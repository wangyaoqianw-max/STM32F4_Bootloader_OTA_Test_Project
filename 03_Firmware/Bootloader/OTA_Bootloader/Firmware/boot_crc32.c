/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_crc32.c
 * @brief Bootloader CRC-32/ISO-HDLC 软件 bitwise 实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>

#include "boot_crc32.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define BOOT_CRC32_POLYNOMIAL (0xEDB88320UL)
//******************************** Defines **********************************//

void boot_crc32_init(boot_crc32_context_t *context)
{
    if (context != NULL) {
        context->value = 0xFFFFFFFFUL;
    }
}

void boot_crc32_update(boot_crc32_context_t *context, const uint8_t *data, uint32_t length)
{
    uint8_t bitIndex;
    uint32_t dataIndex;

    if ((context == NULL) || ((data == NULL) && (length != 0U))) {
        return;
    }

    for (dataIndex = 0U; dataIndex < length; dataIndex++) {
        context->value ^= (uint32_t)data[dataIndex];
        for (bitIndex = 0U; bitIndex < 8U; bitIndex++) {
            if ((context->value & 1UL) != 0UL) {
                context->value = (context->value >> 1U) ^ BOOT_CRC32_POLYNOMIAL;
            } else {
                context->value >>= 1U;
            }
        }
    }
}

uint32_t boot_crc32_finalize(const boot_crc32_context_t *context)
{
    return (context == NULL) ? 0U : (context->value ^ 0xFFFFFFFFUL);
}

uint32_t boot_crc32_calculate(const uint8_t *data, uint32_t length)
{
    boot_crc32_context_t context;

    boot_crc32_init(&context);
    boot_crc32_update(&context, data, length);
    return boot_crc32_finalize(&context);
}
