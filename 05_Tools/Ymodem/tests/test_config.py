import sys
from pathlib import Path
import unittest


MODULE_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(MODULE_ROOT))

try:
    import config
    IMPORT_ERROR = None
except ImportError as error:
    config = None
    IMPORT_ERROR = error


class ConfigTest(unittest.TestCase):
    def setUp(self):
        if IMPORT_ERROR is not None:
            self.fail(f"config module is not available: {IMPORT_ERROR}")

    def test_protocol_defaults(self):
        self.assertEqual(config.DEFAULT_BAUDRATE, 115200)
        self.assertEqual(config.DEFAULT_TIMEOUT_SECONDS, 1.0)
        self.assertEqual(config.DEFAULT_MAX_RETRIES, 5)
        self.assertEqual(config.BLOCK0_DATA_SIZE, 128)
        self.assertEqual(config.DATA_PACKET_SIZE, 1024)

    def test_exit_codes_are_stable(self):
        self.assertEqual(config.EXIT_SUCCESS, 0)
        self.assertEqual(config.EXIT_INVALID_ARGUMENT, 1)
        self.assertEqual(config.EXIT_PORT_NOT_FOUND, 2)
        self.assertEqual(config.EXIT_SERIAL_OPEN_FAILED, 3)
        self.assertEqual(config.EXIT_RECEIVER_TIMEOUT, 4)
        self.assertEqual(config.EXIT_TRANSFER_FAILED, 5)
        self.assertEqual(config.EXIT_RECEIVER_CANCELLED, 6)


if __name__ == "__main__":
    unittest.main()
