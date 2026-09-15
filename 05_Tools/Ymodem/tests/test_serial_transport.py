import sys
from pathlib import Path
import unittest


MODULE_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(MODULE_ROOT))

try:
    from serial_transport import SerialTransport, TransportOpenError, TransportTimeout
    IMPORT_ERROR = None
except ImportError as error:
    SerialTransport = None
    TransportOpenError = None
    TransportTimeout = None
    IMPORT_ERROR = error


class FakeSerial:
    def __init__(self):
        self.read_values = []
        self.writes = []
        self.closed = False

    def write(self, data):
        self.writes.append(bytes(data))
        return len(data)

    def read(self, size):
        del size
        if not self.read_values:
            return b""
        return self.read_values.pop(0)

    def close(self):
        self.closed = True


class SerialTransportTest(unittest.TestCase):
    def setUp(self):
        if IMPORT_ERROR is not None:
            self.fail(f"serial transport module is not available: {IMPORT_ERROR}")
        self.fake = FakeSerial()
        self.factory_calls = []

        def factory(**kwargs):
            self.factory_calls.append(kwargs)
            return self.fake

        self.transport = SerialTransport("COM9", 115200, write_timeout=1.0, serial_factory=factory)

    def test_open_write_read_and_close(self):
        self.fake.read_values.append(b"C")
        self.transport.open()
        self.transport.write(b"abc")
        self.assertEqual(self.transport.read_byte(0.1), ord("C"))
        self.transport.close()

        self.assertEqual(self.factory_calls[0]["port"], "COM9")
        self.assertEqual(self.factory_calls[0]["baudrate"], 115200)
        self.assertEqual(self.fake.writes, [b"abc"])
        self.assertTrue(self.fake.closed)

    def test_empty_read_is_transport_timeout(self):
        self.transport.open()
        with self.assertRaises(TransportTimeout):
            self.transport.read_byte(0.01)

    def test_open_failure_is_wrapped(self):
        def failing_factory(**kwargs):
            del kwargs
            raise OSError("port busy")

        transport = SerialTransport("COM9", 115200, serial_factory=failing_factory)
        with self.assertRaises(TransportOpenError):
            transport.open()


if __name__ == "__main__":
    unittest.main()
