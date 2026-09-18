/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_log.h
 * @brief Bootloader 基于 SEGGER RTT 的轻量日志接口。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef BOOT_LOG_H
#define BOOT_LOG_H

//******************************** Includes *********************************//
#include "boot_config.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#if (BOOT_CONFIG_LOG_ENABLE != 0U)
#define BOOT_LOG_D(...) boot_log_write(BOOT_LOG_LEVEL_DEBUG, __VA_ARGS__)
#define BOOT_LOG_I(...) boot_log_write(BOOT_LOG_LEVEL_INFO, __VA_ARGS__)
#define BOOT_LOG_W(...) boot_log_write(BOOT_LOG_LEVEL_WARNING, __VA_ARGS__)
#define BOOT_LOG_E(...) boot_log_write(BOOT_LOG_LEVEL_ERROR, __VA_ARGS__)
#else
#define BOOT_LOG_D(...) do { } while (0)
#define BOOT_LOG_I(...) do { } while (0)
#define BOOT_LOG_W(...) do { } while (0)
#define BOOT_LOG_E(...) do { } while (0)
#endif
//******************************** Defines **********************************//

//******************************** Types ************************************//
typedef enum
{
    BOOT_LOG_LEVEL_DEBUG = 0U,
    BOOT_LOG_LEVEL_INFO,
    BOOT_LOG_LEVEL_WARNING,
    BOOT_LOG_LEVEL_ERROR
} boot_log_level_t;
//******************************** Types ************************************//

//******************************** Functions ********************************//
/**
 * @brief 初始化 Bootloader 日志输出。
 */
void boot_log_init(void);

/**
 * @brief 输出一条 Bootloader 日志。
 * @param[in] level : 日志级别。
 * @param[in] format : printf 风格格式串。
 */
void boot_log_write(boot_log_level_t level, const char *format, ...);
//******************************** Functions ********************************//

#endif
