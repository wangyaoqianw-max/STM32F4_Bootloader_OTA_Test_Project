"""Small pyserial adapter used by the YMODEM protocol layer."""


class TransportOpenError(Exception):
    """The serial port could not be opened."""


class TransportTimeout(TimeoutError):
    """No byte arrived before the requested timeout."""


class TransportError(Exception):
    """A serial read or write failed."""


class SerialTransport:
    def __init__(self, port: str, baudrate: int, write_timeout: float = 1.0, serial_factory=None):
        self.port = port
        self.baudrate = baudrate
        self.write_timeout = write_timeout
        self.serial_factory = serial_factory
        self._serial = None

    def open(self) -> None:
        factory = self.serial_factory
        if factory is None:
            try:
                import serial
            except ImportError as error:
                raise TransportOpenError("pyserial is required for serial transport") from error
            factory = serial.Serial
        try:
            self._serial = factory(
                port=self.port,
                baudrate=self.baudrate,
                timeout=None,
                write_timeout=self.write_timeout,
            )
        except Exception as error:
            raise TransportOpenError(f"failed to open serial port {self.port}: {error}") from error

    def write(self, data: bytes) -> None:
        if self._serial is None:
            raise TransportError("serial transport is not open")
        offset = 0
        while offset < len(data):
            try:
                written = self._serial.write(data[offset:])
            except Exception as error:
                raise TransportError(f"serial write failed: {error}") from error
            if not written:
                raise TransportError("serial write returned zero bytes")
            offset += written

    def read_byte(self, timeout: float) -> int:
        if self._serial is None:
            raise TransportError("serial transport is not open")
        previous_timeout = getattr(self._serial, "timeout", None)
        try:
            self._serial.timeout = timeout
            data = self._serial.read(1)
        except Exception as error:
            raise TransportError(f"serial read failed: {error}") from error
        finally:
            if hasattr(self._serial, "timeout"):
                self._serial.timeout = previous_timeout
        if not data:
            raise TransportTimeout("serial read timed out")
        return data[0]

    def close(self) -> None:
        if self._serial is not None:
            self._serial.close()
            self._serial = None
