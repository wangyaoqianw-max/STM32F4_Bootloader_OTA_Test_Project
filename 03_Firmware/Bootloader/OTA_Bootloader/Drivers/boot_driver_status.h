/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_driver_status.h
 * @brief Bootloader 最小外设驱动统一状态码。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef BOOT_DRIVER_STATUS_H
#define BOOT_DRIVER_STATUS_H

//******************************** Types ***********************************//
/**
 * @brief Bootloader 外设驱动统一状态码。
 * @note 该类型只服务 Bootloader，不引入 Application platform_error。
 */
typedef enum
{
    BOOT_DRIVER_OK = 0,
    BOOT_DRIVER_ERR_NULL,
    BOOT_DRIVER_ERR_INVALID,
    BOOT_DRIVER_ERR_NOT_INITIALIZED,
    BOOT_DRIVER_ERR_HAL,
    BOOT_DRIVER_ERR_TIMEOUT,
    BOOT_DRIVER_ERR_BUSY,
    BOOT_DRIVER_ERR_NOT_FOUND,
    BOOT_DRIVER_ERR_RANGE,
    BOOT_DRIVER_ERR_VERIFY
} boot_driver_status_t;
//******************************** Types ***********************************//

#endif
