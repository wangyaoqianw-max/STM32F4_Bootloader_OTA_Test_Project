"""Build S04 Firmware Image V1: [64 Byte Header][Payload]."""

import argparse
import pathlib
import struct
import zlib


HEADER_SIZE = 64
PAYLOAD_CAPACITY = 0x07F000
MAGIC = 0x4D495746
FORMAT_VERSION = 1


def parse_version(value):
    parts = value.split(".")
    if len(parts) != 3:
        raise ValueError("version must be major.minor.patch")
    try:
        version = tuple(int(part, 10) for part in parts)
    except ValueError as error:
        raise ValueError("version must contain decimal integers") from error
    if any(part < 0 or part > 0xFFFF for part in version):
        raise ValueError("version components must be 0..65535")
    return version


def build_image(payload, version):
    if not payload:
        raise ValueError("payload must not be empty")
    if len(payload) > PAYLOAD_CAPACITY:
        raise ValueError("payload exceeds S04 Slot capacity")
    if len(version) != 3:
        raise ValueError("version must contain three components")

    header = bytearray(HEADER_SIZE)
    struct.pack_into("<I", header, 0x00, MAGIC)
    struct.pack_into("<H", header, 0x04, FORMAT_VERSION)
    struct.pack_into("<H", header, 0x06, HEADER_SIZE)
    struct.pack_into("<HHH", header, 0x08, *version)
    struct.pack_into("<H", header, 0x0E, 0)
    struct.pack_into("<I", header, 0x10, len(payload))
    struct.pack_into("<I", header, 0x14, zlib.crc32(payload) & 0xFFFFFFFF)
    struct.pack_into("<I", header, 0x3C, zlib.crc32(header[:0x3C]) & 0xFFFFFFFF)
    return bytes(header) + payload


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--version", required=True)
    arguments = parser.parse_args()
    image = build_image(arguments.input.read_bytes(), parse_version(arguments.version))
    arguments.output.write_bytes(image)


if __name__ == "__main__":
    main()
