/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_internal_flash.h
 * @brief Bootloader Application Flash Region 专用接口。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef BOOT_INTERNAL_FLASH_H
#define BOOT_INTERNAL_FLASH_H

//******************************** Includes *********************************//
#include <stdint.h>

#include "boot_driver_status.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
boot_driver_status_t boot_internal_flash_erase_app(void);
boot_driver_status_t boot_internal_flash_read(
    uint32_t appOffset,
    uint8_t *data,
    uint32_t length);
boot_driver_status_t boot_internal_flash_write(
    uint32_t appOffset,
    const uint8_t *data,
    uint32_t length);
//******************************** Functions ********************************//

#endif
