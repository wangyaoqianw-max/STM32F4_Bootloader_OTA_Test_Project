#ifndef PLATFORM_W25Q64_H
#define PLATFORM_W25Q64_H

#include "platform_error.h"

#define PLATFORM_W25Q64_INITIALIZER {0}

typedef struct
{
    uint8_t *memory;
    uint32_t baseAddress;
    uint32_t sizeBytes;
    platform_bool_t failRead;
    platform_bool_t failErase;
    uint32_t readCount;
} platform_w25q64_t;

platform_error_t platform_w25q64_read(
    platform_w25q64_t *flash,
    uint32_t address,
    uint8_t *data,
    platform_size_t dataLength);
platform_error_t platform_w25q64_write(
    platform_w25q64_t *flash,
    uint32_t address,
    const uint8_t *data,
    platform_size_t dataLength);
platform_error_t platform_w25q64_sector_erase(platform_w25q64_t *flash, uint32_t sectorAddress);

#endif
