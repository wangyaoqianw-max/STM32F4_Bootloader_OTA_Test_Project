"""Create controlled corruption artifacts for the physical Slot A image."""

import argparse
import enum
import json
import pathlib
import struct
import zlib


HEADER_SIZE = 64
HEADER_SECTOR_SIZE = 0x1000
PAYLOAD_CORRUPTION_OFFSET = 0x100
MAGIC = 0x4D495746
FORMAT_VERSION = 1
HEADER_CRC_OFFSET = 0x3C


class CorruptionCase(str, enum.Enum):
    HEADER_INVALID = "header-invalid"
    PAYLOAD_CRC_INVALID = "payload-crc-invalid"
    VERSION_MISMATCH = "version-mismatch"


def _crc32(data):
    return zlib.crc32(data) & 0xFFFFFFFF


def _u16(data, offset):
    return struct.unpack_from("<H", data, offset)[0]


def _u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def validate_compact_image(image, expected_version=(1, 0, 0)):
    """Validate and split a compact [64-byte Header][Payload] image."""
    if len(image) < HEADER_SIZE:
        raise ValueError("compact image is shorter than the 64-byte Header")

    header = image[:HEADER_SIZE]
    payload = image[HEADER_SIZE:]
    if _u32(header, 0x00) != MAGIC:
        raise ValueError("image Header magic is invalid")
    if _u16(header, 0x04) != FORMAT_VERSION:
        raise ValueError("image Header format version is invalid")
    if _u16(header, 0x06) != HEADER_SIZE:
        raise ValueError("image Header size is invalid")
    if tuple(_u16(header, offset) for offset in (0x08, 0x0A, 0x0C)) != expected_version:
        raise ValueError("image version is not the expected Known-Good version")
    if _u16(header, 0x0E) != 0:
        raise ValueError("image version reserved field is not zero")
    if any(header[offset] != 0 for offset in range(0x18, HEADER_CRC_OFFSET)):
        raise ValueError("image Header reserved bytes are not zero")

    image_size = _u32(header, 0x10)
    if image_size == 0 or image_size != len(payload):
        raise ValueError("image size does not match the compact payload")
    if _u32(header, 0x14) != _crc32(payload):
        raise ValueError("image Payload CRC is invalid")
    if _u32(header, HEADER_CRC_OFFSET) != _crc32(header[:HEADER_CRC_OFFSET]):
        raise ValueError("image Header CRC is invalid")
    return header, payload


def mutate_slot(header_sector, payload, corruption_case):
    """Mutate only the selected physical Slot A corruption target."""
    if len(header_sector) != HEADER_SECTOR_SIZE:
        raise ValueError("Header Sector must be exactly 4096 bytes")
    if len(payload) <= PAYLOAD_CORRUPTION_OFFSET:
        raise ValueError("Payload is too short for the controlled corruption offset")

    try:
        case = (corruption_case if isinstance(corruption_case, CorruptionCase)
                else CorruptionCase(corruption_case))
    except ValueError as error:
        raise ValueError(f"unsupported corruption case: {corruption_case}") from error

    mutated_header = bytearray(header_sector)
    mutated_payload = bytearray(payload)

    if case is CorruptionCase.HEADER_INVALID:
        mutated_header[0] ^= 0x01
    elif case is CorruptionCase.PAYLOAD_CRC_INVALID:
        mutated_payload[PAYLOAD_CORRUPTION_OFFSET] ^= 0x01
    else:
        major = _u16(mutated_header, 0x08)
        if major == 0xFFFF:
            raise ValueError("cannot increment a saturated Header version")
        struct.pack_into("<H", mutated_header, 0x08, major + 1)
        struct.pack_into(
            "<I", mutated_header, HEADER_CRC_OFFSET,
            _crc32(mutated_header[:HEADER_CRC_OFFSET]))

    return bytes(mutated_header), bytes(mutated_payload)


def _main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--validate-image", type=pathlib.Path)
    parser.add_argument("--case", choices=[case.value for case in CorruptionCase])
    parser.add_argument("--header-sector", type=pathlib.Path)
    parser.add_argument("--payload", type=pathlib.Path)
    parser.add_argument("--output-header-sector", type=pathlib.Path)
    parser.add_argument("--output-payload", type=pathlib.Path)
    parser.add_argument("--manifest", type=pathlib.Path)
    arguments = parser.parse_args()

    if arguments.validate_image is not None:
        header, payload = validate_compact_image(
            arguments.validate_image.read_bytes())
        print(json.dumps({
            "valid": True,
            "header_size": len(header),
            "payload_size": len(payload),
        }))
        return

    required = (
        arguments.case,
        arguments.header_sector,
        arguments.payload,
        arguments.output_header_sector,
        arguments.output_payload,
    )
    if any(value is None for value in required):
        parser.error("mutation mode requires --case, input and output paths")

    header_sector = arguments.header_sector.read_bytes()
    payload = arguments.payload.read_bytes()
    mutated_header, mutated_payload = mutate_slot(
        header_sector, payload, arguments.case)
    arguments.output_header_sector.write_bytes(mutated_header)
    arguments.output_payload.write_bytes(mutated_payload)

    manifest = {
        "case": arguments.case,
        "header_sector_size": len(mutated_header),
        "payload_size": len(mutated_payload),
        "payload_corruption_offset": (
            PAYLOAD_CORRUPTION_OFFSET
            if arguments.case == CorruptionCase.PAYLOAD_CRC_INVALID.value
            else None
        ),
    }
    if arguments.manifest is not None:
        arguments.manifest.write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8")
    print(json.dumps(manifest, ensure_ascii=False))


if __name__ == "__main__":
    _main()
