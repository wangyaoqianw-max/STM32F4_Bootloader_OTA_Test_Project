"""CRC-16/XMODEM used by YMODEM packets."""


def crc16_xmodem(data: bytes) -> int:
    """Return the CRC-16/XMODEM value for *data*."""
    crc = 0
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc
