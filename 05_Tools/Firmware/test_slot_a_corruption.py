import struct
import unittest
import zlib

from slot_a_corruption import (
    HEADER_CRC_OFFSET,
    HEADER_SECTOR_SIZE,
    PAYLOAD_CORRUPTION_OFFSET,
    CorruptionCase,
    mutate_slot,
    validate_compact_image,
)


def build_fixture():
    payload = bytes(range(256)) * 3
    header = bytearray(64)
    struct.pack_into("<I", header, 0x00, 0x4D495746)
    struct.pack_into("<H", header, 0x04, 1)
    struct.pack_into("<H", header, 0x06, 64)
    struct.pack_into("<HHHH", header, 0x08, 1, 0, 0, 0)
    struct.pack_into("<I", header, 0x10, len(payload))
    struct.pack_into("<I", header, 0x14, zlib.crc32(payload) & 0xFFFFFFFF)
    struct.pack_into("<I", header, HEADER_CRC_OFFSET,
                     zlib.crc32(header[:HEADER_CRC_OFFSET]) & 0xFFFFFFFF)
    header_sector = bytes(header) + (b"\xFF" * (HEADER_SECTOR_SIZE - len(header)))
    return header_sector, payload


class SlotACorruptionTests(unittest.TestCase):
    def test_known_good_compact_image_is_validated_and_split(self):
        header_sector, payload = build_fixture()

        header, actual_payload = validate_compact_image(
            header_sector[:64] + payload)

        self.assertEqual(header, header_sector[:64])
        self.assertEqual(actual_payload, payload)

    def test_compact_image_with_changed_payload_is_rejected(self):
        header_sector, payload = build_fixture()
        corrupted = bytearray(header_sector[:64] + payload)
        corrupted[64 + PAYLOAD_CORRUPTION_OFFSET] ^= 0x01

        with self.assertRaises(ValueError):
            validate_compact_image(bytes(corrupted))

    def test_header_invalid_changes_magic_without_republishing_header_crc(self):
        header_sector, payload = build_fixture()

        mutated_header, mutated_payload = mutate_slot(
            header_sector, payload, CorruptionCase.HEADER_INVALID)

        self.assertEqual(mutated_payload, payload)
        self.assertEqual(mutated_header[0], header_sector[0] ^ 0x01)
        self.assertEqual(mutated_header[1:HEADER_CRC_OFFSET],
                         header_sector[1:HEADER_CRC_OFFSET])
        self.assertNotEqual(
            zlib.crc32(mutated_header[:HEADER_CRC_OFFSET]) & 0xFFFFFFFF,
            struct.unpack_from("<I", mutated_header, HEADER_CRC_OFFSET)[0])

    def test_payload_crc_invalid_preserves_vector_and_header_crc(self):
        header_sector, payload = build_fixture()

        mutated_header, mutated_payload = mutate_slot(
            header_sector, payload, CorruptionCase.PAYLOAD_CRC_INVALID)

        self.assertEqual(mutated_header, header_sector)
        self.assertEqual(mutated_payload[:8], payload[:8])
        self.assertEqual(mutated_payload[PAYLOAD_CORRUPTION_OFFSET],
                         payload[PAYLOAD_CORRUPTION_OFFSET] ^ 0x01)
        self.assertNotEqual(mutated_payload, payload)
        expected_crc = struct.unpack_from("<I", mutated_header, 0x14)[0]
        self.assertNotEqual(zlib.crc32(mutated_payload) & 0xFFFFFFFF,
                            expected_crc)

    def test_version_mismatch_recomputes_header_crc_and_preserves_payload(self):
        header_sector, payload = build_fixture()

        mutated_header, mutated_payload = mutate_slot(
            header_sector, payload, CorruptionCase.VERSION_MISMATCH)

        self.assertEqual(mutated_payload, payload)
        self.assertEqual(struct.unpack_from("<H", mutated_header, 0x08)[0], 2)
        self.assertEqual(struct.unpack_from("<HHH", mutated_header, 0x0A),
                         (0, 0, 0))
        header_crc = struct.unpack_from("<I", mutated_header,
                                       HEADER_CRC_OFFSET)[0]
        self.assertEqual(zlib.crc32(mutated_header[:HEADER_CRC_OFFSET]) & 0xFFFFFFFF,
                         header_crc)
        self.assertEqual(struct.unpack_from("<I", mutated_header, 0x14)[0],
                         struct.unpack_from("<I", header_sector, 0x14)[0])

    def test_rejects_header_sector_that_is_not_complete(self):
        _, payload = build_fixture()

        with self.assertRaises(ValueError):
            mutate_slot(b"\xFF" * (HEADER_SECTOR_SIZE - 1), payload,
                        CorruptionCase.HEADER_INVALID)


if __name__ == "__main__":
    unittest.main()
