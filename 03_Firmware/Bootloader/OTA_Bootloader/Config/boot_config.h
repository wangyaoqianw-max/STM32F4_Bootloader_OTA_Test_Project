/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_config.h
 * @brief Bootloader 静态配置。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef BOOT_CONFIG_H
#define BOOT_CONFIG_H

//******************************** Includes *********************************//
#include "memory_layout.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/* Bootloader 基础日志默认开启，实际输出级别由 Diagnostics 层控制。 */
#define BOOT_CONFIG_LOG_ENABLE       (1U)
#define BOOT_CONFIG_LOG_MIN_LEVEL    (1U)

/* CmBacktrace 和启动日志使用的固件身份与版本标识。 */
#define BOOT_CONFIG_IDENTITY         "OTA_BOOTLOADER"
#define BOOT_CONFIG_VERSION          "S09.1"
//******************************** Defines **********************************//

#endif
