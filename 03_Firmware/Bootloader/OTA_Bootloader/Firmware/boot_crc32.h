/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_crc32.h
 * @brief Bootloader 最小 CRC-32/ISO-HDLC 接口。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef BOOT_CRC32_H
#define BOOT_CRC32_H

//******************************** Includes *********************************//
#include <stdint.h>
//******************************** Includes *********************************//

//******************************** Types ***********************************//
typedef struct
{
    uint32_t value;
} boot_crc32_context_t;
//******************************** Types ***********************************//

//******************************** Functions ********************************//
void boot_crc32_init(boot_crc32_context_t *context);
void boot_crc32_update(boot_crc32_context_t *context, const uint8_t *data, uint32_t length);
uint32_t boot_crc32_finalize(const boot_crc32_context_t *context);
uint32_t boot_crc32_calculate(const uint8_t *data, uint32_t length);
//******************************** Functions ********************************//

#endif
