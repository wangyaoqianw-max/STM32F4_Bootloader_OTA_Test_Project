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
/**
 * @brief 擦除完整 Application Region，即 STM32F411 Sector 4~7。
 * @return 驱动状态；Bootloader Region 不在本接口可表达范围内。
 * @warning 这是破坏性操作，调用前必须完成 Candidate 预校验。
 */
boot_driver_status_t boot_internal_flash_erase_app(void);
/**
 * @brief 从 Application Region 读取数据。
 * @param[in] appOffset : 相对 APP_BASE_ADDR 的字节偏移。
 * @param[out] data : 接收 Buffer；不得为空。
 * @param[in] length : 读取字节数；必须完全位于 APP Region。
 * @return 驱动状态。
 */
boot_driver_status_t boot_internal_flash_read(
    uint32_t appOffset,
    uint8_t *data,
    uint32_t length);
/**
 * @brief 向已擦除的 Application Region 写入数据并逐 Word read-back。
 * @param[in] appOffset : 相对 APP_BASE_ADDR 的字节偏移。
 * @param[in] data : 写入 Buffer；不得为空。
 * @param[in] length : 写入字节数；必须完全位于 APP Region。
 * @return 驱动状态；接口不会触及 Bootloader Region。
 */
boot_driver_status_t boot_internal_flash_write(
    uint32_t appOffset,
    const uint8_t *data,
    uint32_t length);
//******************************** Functions ********************************//

#endif
