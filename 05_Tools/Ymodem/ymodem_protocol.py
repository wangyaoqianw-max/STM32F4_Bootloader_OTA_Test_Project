"""Single-file YMODEM-1K sender independent from the serial implementation."""

from dataclasses import dataclass
from pathlib import Path
import time

from config import (
    BLOCK0_DATA_SIZE,
    DATA_PACKET_SIZE,
    DEFAULT_FILE_MODE,
    DEFAULT_SERIAL_NUMBER,
    MAX_UINT32,
    PADDING_BYTE,
)
from crc16 import crc16_xmodem


SOH = 0x01
STX = 0x02
EOT = 0x04
ACK = 0x06
NAK = 0x15
CAN = 0x18
CRC_REQUEST = 0x43


class YModemError(Exception):
    """Base class for protocol-level failures."""


class ReceiverTimeoutError(YModemError):
    """The receiver did not provide the expected initial CRC request."""


class TransferFailedError(YModemError):
    """A transfer phase exhausted its bounded retries."""


class ReceiverCancelledError(YModemError):
    """The receiver sent CAN."""


@dataclass
class TransferStats:
    bytes_sent: int = 0
    blocks_sent: int = 0
    retry_count: int = 0


def _build_packet(start_byte: int, block_number: int, data: bytes) -> bytes:
    if not 0 <= block_number <= 0xFF:
        raise ValueError("block number must be in range 0..255")
    if len(data) not in (BLOCK0_DATA_SIZE, DATA_PACKET_SIZE):
        raise ValueError("packet data must be 128 or 1024 bytes")
    header = bytes((start_byte, block_number, block_number ^ 0xFF))
    checksum = crc16_xmodem(data)
    return header + data + checksum.to_bytes(2, "big")


def build_block0(
    filename: str,
    file_size: int,
    modification_time: int,
    file_mode: int,
    serial_number: int,
) -> bytes:
    """Build a 128-byte-data YMODEM Block 0 packet."""
    if not filename or "\x00" in filename:
        raise ValueError("filename must be non-empty and must not contain NUL")
    try:
        filename_bytes = filename.encode("ascii")
    except UnicodeEncodeError as error:
        raise ValueError("filename must contain ASCII characters") from error
    for name, value in (
        ("file size", file_size),
        ("modification time", modification_time),
        ("file mode", file_mode),
        ("serial number", serial_number),
    ):
        if not isinstance(value, int) or not 0 <= value <= MAX_UINT32:
            raise ValueError(f"{name} must be an unsigned 32-bit integer")

    metadata = (
        filename_bytes
        + b"\x00 "
        + str(file_size).encode("ascii")
        + b" "
        + format(modification_time, "o").encode("ascii")
        + b" "
        + format(file_mode, "o").encode("ascii")
        + b" "
        + format(serial_number, "o").encode("ascii")
        + b"\x00"
    )
    if len(metadata) > BLOCK0_DATA_SIZE:
        raise ValueError("Block 0 metadata exceeds 128 bytes")
    return _build_packet(SOH, 0, metadata.ljust(BLOCK0_DATA_SIZE, b"\x00"))


def build_empty_block0() -> bytes:
    """Build the zero-filled Block 0 that closes a single-file session."""
    return _build_packet(SOH, 0, b"\x00" * BLOCK0_DATA_SIZE)


def build_packet(block_number: int, data: bytes, packet_size: int = DATA_PACKET_SIZE) -> bytes:
    """Build a padded YMODEM data packet."""
    if packet_size != DATA_PACKET_SIZE:
        raise ValueError("V1 data packets must use 1024 bytes")
    if not data or len(data) > packet_size:
        raise ValueError("data packet payload must contain 1..1024 bytes")
    return _build_packet(STX, block_number, data.ljust(packet_size, bytes((PADDING_BYTE,))))


def _control_name(value: int) -> str:
    return {
        CRC_REQUEST: "C",
        ACK: "ACK",
        NAK: "NAK",
        CAN: "CAN",
    }.get(value, f"0x{value:02X}")


class YModemSender:
    """Send one file over a transport exposing write/read_byte."""

    def __init__(self, transport, timeout: float, max_retries: int, logger=None):
        if timeout <= 0:
            raise ValueError("timeout must be positive")
        if max_retries < 0:
            raise ValueError("max_retries must not be negative")
        self.transport = transport
        self.timeout = timeout
        self.max_retries = max_retries
        self.logger = logger
        self.stats = TransferStats()

    def _log(self, message: str) -> None:
        if self.logger is None:
            return
        if callable(self.logger):
            self.logger(message)
        else:
            self.logger.info(message)

    def _read_until(self, accepted: set[int]) -> int:
        deadline = time.monotonic() + self.timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("YMODEM response timeout")
            value = self.transport.read_byte(remaining)
            self._log(f"[RX] {_control_name(value)}")
            if value == CAN:
                raise ReceiverCancelledError("receiver sent CAN")
            if value in accepted:
                return value

    def _cancel_best_effort(self) -> None:
        try:
            self.transport.write(bytes((CAN, CAN)))
        except Exception:
            pass

    def _retry(self, label: str, attempt: int) -> None:
        self.stats.retry_count += 1
        self._log(f"[RETRY] {label} retry {attempt}/{self.max_retries}")

    def _send_packet_with_ack(self, packet: bytes, label: str, follow_up: int | None = None) -> None:
        for attempt in range(self.max_retries + 1):
            self.transport.write(packet)
            response = None
            try:
                response = self._read_until({ACK, NAK})
                if response == ACK and follow_up is not None:
                    self._read_until({follow_up})
                    return
                if response == ACK:
                    return
            except ReceiverCancelledError:
                raise
            except TimeoutError:
                response = None

            if attempt >= self.max_retries:
                self._cancel_best_effort()
                raise TransferFailedError(f"{label} retry limit exceeded")
            self._retry(label, attempt + 1)

    def _wait_for_initial_crc(self) -> None:
        self._log("[WAIT] Receiver C")
        try:
            self._read_until({CRC_REQUEST})
        except ReceiverCancelledError:
            raise
        except TimeoutError as error:
            raise ReceiverTimeoutError("receiver did not send initial C") from error

    def _send_eot_handshake(self) -> None:
        first_eot_received = False
        for attempt in range(self.max_retries + 1):
            self.transport.write(bytes((EOT,)))
            self._log("[TX] EOT")
            try:
                response = self._read_until({ACK, NAK})
            except ReceiverCancelledError:
                raise
            except TimeoutError:
                response = None
            if response == NAK:
                first_eot_received = True
                break
            if attempt >= self.max_retries:
                self._cancel_best_effort()
                raise TransferFailedError("first EOT was not acknowledged with NAK")
            self._retry("EOT", attempt + 1)

        if not first_eot_received:
            raise TransferFailedError("first EOT handshake failed")

        for attempt in range(self.max_retries + 1):
            self.transport.write(bytes((EOT,)))
            self._log("[TX] EOT")
            try:
                response = self._read_until({ACK, NAK})
                if response == ACK:
                    self._read_until({CRC_REQUEST})
                    return
            except ReceiverCancelledError:
                raise
            except TimeoutError:
                pass
            if attempt >= self.max_retries:
                self._cancel_best_effort()
                raise TransferFailedError("second EOT was not acknowledged")
            self._retry("EOT", attempt + 1)

    def send(self, file_path) -> TransferStats:
        path = Path(file_path)
        if not path.is_file():
            raise ValueError(f"firmware file does not exist: {path}")
        file_size = path.stat().st_size
        if file_size == 0:
            raise ValueError("firmware file must not be empty")
        if file_size > MAX_UINT32:
            raise ValueError("firmware file is too large for YMODEM Block 0")

        self.stats = TransferStats()
        self._wait_for_initial_crc()
        header = build_block0(
            path.name,
            file_size,
            int(path.stat().st_mtime),
            DEFAULT_FILE_MODE,
            DEFAULT_SERIAL_NUMBER,
        )
        self._log("[TX] Block 0")
        self._send_packet_with_ack(header, "Block 0", follow_up=CRC_REQUEST)

        with path.open("rb") as source:
            block_number = 1
            remaining = file_size
            while remaining:
                chunk = source.read(min(DATA_PACKET_SIZE, remaining))
                if not chunk:
                    self._cancel_best_effort()
                    raise TransferFailedError("source file changed while reading")
                packet = build_packet(block_number, chunk)
                self._log(f"[TX] Block {block_number}")
                self._send_packet_with_ack(packet, f"Block {block_number}")
                self.stats.bytes_sent += len(chunk)
                self.stats.blocks_sent += 1
                remaining -= len(chunk)
                block_number = (block_number + 1) & 0xFF

        self._send_eot_handshake()
        self._log("[TX] Block 0 (end)")
        self._send_packet_with_ack(build_empty_block0(), "end Block 0")
        return self.stats
