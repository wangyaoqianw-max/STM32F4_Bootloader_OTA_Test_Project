import sys
from pathlib import Path
import unittest


MODULE_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(MODULE_ROOT))

try:
    from crc16 import crc16_xmodem
    IMPORT_ERROR = None
except ImportError as error:
    crc16_xmodem = None
    IMPORT_ERROR = error


class Crc16Test(unittest.TestCase):
    def test_crc16_xmodem_known_vector(self):
        if IMPORT_ERROR is not None:
            self.fail(f"crc16 module is not available: {IMPORT_ERROR}")
        self.assertEqual(crc16_xmodem(b"123456789"), 0x31C3)

    def test_crc16_xmodem_empty_data(self):
        if IMPORT_ERROR is not None:
            self.fail(f"crc16 module is not available: {IMPORT_ERROR}")
        self.assertEqual(crc16_xmodem(b""), 0x0000)


if __name__ == "__main__":
    unittest.main()
