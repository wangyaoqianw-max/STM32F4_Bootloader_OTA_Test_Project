import importlib.util
import pathlib
import struct
import tempfile
import unittest
import zlib


MODULE_PATH = pathlib.Path(__file__).with_name("pack_firmware.py")
SPEC = importlib.util.spec_from_file_location("pack_firmware", MODULE_PATH)
pack_firmware = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(pack_firmware)


class PackFirmwareTest(unittest.TestCase):
    def test_build_image_uses_fixed_header_and_payload(self):
        payload = b"\x01\x02\x03"
        image = pack_firmware.build_image(payload, (1, 2, 3))
        self.assertEqual(len(image), 67)
        self.assertEqual(struct.unpack_from("<I", image, 0)[0], 0x4D495746)
        self.assertEqual(struct.unpack_from("<H", image, 4)[0], 1)
        self.assertEqual(struct.unpack_from("<H", image, 6)[0], 64)
        self.assertEqual(struct.unpack_from("<HHH", image, 8), (1, 2, 3))
        self.assertEqual(struct.unpack_from("<I", image, 16)[0], 3)
        self.assertEqual(struct.unpack_from("<I", image, 20)[0], zlib.crc32(payload) & 0xFFFFFFFF)
        self.assertEqual(struct.unpack_from("<I", image, 60)[0], zlib.crc32(image[:60]) & 0xFFFFFFFF)
        self.assertEqual(image[64:], payload)

    def test_rejects_invalid_inputs(self):
        with self.assertRaises(ValueError):
            pack_firmware.parse_version("1.2")
        with self.assertRaises(ValueError):
            pack_firmware.build_image(b"", (1, 0, 0))
        with self.assertRaises(ValueError):
            pack_firmware.build_image(b"x" * (0x07F000 + 1), (1, 0, 0))


if __name__ == "__main__":
    unittest.main()
