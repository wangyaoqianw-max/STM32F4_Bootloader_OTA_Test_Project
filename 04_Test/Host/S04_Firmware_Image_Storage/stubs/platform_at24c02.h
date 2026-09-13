#ifndef PLATFORM_AT24C02_H
#define PLATFORM_AT24C02_H

#include "platform_error.h"

typedef struct
{
    uint8_t memory[256];
    uint32_t writeCount;
    uint32_t failWriteOnCall;
} platform_at24c02_t;

platform_error_t platform_at24c02_read(
    platform_at24c02_t *eeprom,
    uint32_t address,
    uint8_t *data,
    platform_size_t dataLength);
platform_error_t platform_at24c02_write(
    platform_at24c02_t *eeprom,
    uint32_t address,
    const uint8_t *data,
    platform_size_t dataLength);

#endif
