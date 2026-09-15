import sys
import tempfile
from pathlib import Path
import unittest


MODULE_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(MODULE_ROOT))

try:
    from crc16 import crc16_xmodem
    from ymodem_protocol import (
        EOT,
        SOH,
        STX,
        ACK,
        NAK,
        CAN,
        ReceiverCancelledError,
        ReceiverTimeoutError,
        TransferFailedError,
        YModemSender,
        build_block0,
        build_packet,
    )
    IMPORT_ERROR = None
except ImportError as error:
    crc16_xmodem = None
    YModemSender = None
    IMPORT_ERROR = error


class ScriptedTransport:
    def __init__(self, responses):
        self.responses = list(responses)
        self.writes = []

    def write(self, data):
        self.writes.append(bytes(data))

    def read_byte(self, timeout):
        del timeout
        if not self.responses:
            raise TimeoutError("scripted transport has no response")
        response = self.responses.pop(0)
        if isinstance(response, BaseException):
            raise response
        return response


class ProtocolTest(unittest.TestCase):
    def setUp(self):
        if IMPORT_ERROR is not None:
            self.fail(f"YMODEM protocol module is not available: {IMPORT_ERROR}")

    def test_block0_packet_contains_standard_metadata(self):
        packet = build_block0("app.bin", 5, 8, 0o100644, 0)

        self.assertEqual(len(packet), 133)
        self.assertEqual(packet[0], SOH)
        self.assertEqual(packet[1:3], b"\x00\xff")
        data = packet[3:131]
        self.assertTrue(data.startswith(b"app.bin\x00 5 10 100644 0\x00"))
        self.assertEqual(
            int.from_bytes(packet[131:133], "big"),
            crc16_xmodem(data),
        )

    def test_data_packet_is_one_kilobyte_and_padded(self):
        packet = build_packet(1, b"abc", 1024)

        self.assertEqual(len(packet), 1029)
        self.assertEqual(packet[0], STX)
        self.assertEqual(packet[1:3], b"\x01\xfe")
        self.assertEqual(packet[3:6], b"abc")
        self.assertEqual(packet[6:1027], b"\x1a" * 1021)
        self.assertEqual(
            int.from_bytes(packet[1027:1029], "big"),
            crc16_xmodem(packet[3:1027]),
        )

    def test_normal_single_file_flow(self):
        transport = ScriptedTransport([ord("C"), ACK, ord("C"), ACK, NAK, ACK, ACK, ord("C"), ACK])
        sender = YModemSender(transport, timeout=0.01, max_retries=2)

        with tempfile.TemporaryDirectory() as directory:
            file_path = Path(directory) / "app.bin"
            file_path.write_bytes(b"hello")
            stats = sender.send(file_path)

        self.assertEqual(stats.bytes_sent, 5)
        self.assertEqual(stats.blocks_sent, 1)
        self.assertEqual(stats.retry_count, 0)
        self.assertEqual(len(transport.writes), 5)
        self.assertEqual(transport.writes[1][0], STX)
        self.assertEqual(transport.writes[2], bytes([EOT]))
        self.assertEqual(transport.writes[3], bytes([EOT]))
        self.assertEqual(transport.writes[4][0], SOH)
        self.assertEqual(transport.writes[4][3:131], b"\x00" * 128)

    def test_nak_retries_current_data_packet(self):
        transport = ScriptedTransport([ord("C"), ACK, ord("C"), NAK, ACK, NAK, ACK, ord("C"), ACK])
        sender = YModemSender(transport, timeout=0.01, max_retries=2)

        with tempfile.TemporaryDirectory() as directory:
            file_path = Path(directory) / "app.bin"
            file_path.write_bytes(b"hello")
            stats = sender.send(file_path)

        self.assertEqual(stats.retry_count, 1)
        self.assertEqual(transport.writes[1], transport.writes[2])

    def test_initial_receiver_timeout_is_distinct(self):
        transport = ScriptedTransport([])
        sender = YModemSender(transport, timeout=0.01, max_retries=1)

        with tempfile.TemporaryDirectory() as directory:
            file_path = Path(directory) / "app.bin"
            file_path.write_bytes(b"hello")
            with self.assertRaises(ReceiverTimeoutError):
                sender.send(file_path)

    def test_receiver_can_cancels_transfer(self):
        transport = ScriptedTransport([ord("C"), ACK, ord("C"), CAN])
        sender = YModemSender(transport, timeout=0.01, max_retries=1)

        with tempfile.TemporaryDirectory() as directory:
            file_path = Path(directory) / "app.bin"
            file_path.write_bytes(b"hello")
            with self.assertRaises(ReceiverCancelledError):
                sender.send(file_path)

    def test_retry_exhaustion_is_transfer_failure(self):
        transport = ScriptedTransport([ord("C"), ACK, ord("C"), NAK, NAK, NAK])
        sender = YModemSender(transport, timeout=0.01, max_retries=2)

        with tempfile.TemporaryDirectory() as directory:
            file_path = Path(directory) / "app.bin"
            file_path.write_bytes(b"hello")
            with self.assertRaises(TransferFailedError):
                sender.send(file_path)


if __name__ == "__main__":
    unittest.main()
