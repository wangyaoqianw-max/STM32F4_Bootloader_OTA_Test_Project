/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_log.c
 * @brief Bootloader 基于 SEGGER RTT 的轻量日志实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#include "boot_log.h"

//******************************** Includes *********************************//
#include <stdarg.h>

#include "SEGGER_RTT.h"
//******************************** Includes *********************************//

//******************************** Variables ********************************//
static const char *const g_bootLogLevelNames[] =
{
    "D",
    "I",
    "W",
    "E"
};
//******************************** Variables ********************************//

//******************************** Functions ********************************//
void boot_log_init(void)
{
    (void)SEGGER_RTT_WriteString(0U, "\r\n[BOOT_LOG] ready\r\n");
}

void boot_log_write(boot_log_level_t level, const char *format, ...)
{
    va_list arguments;

    if ((level < BOOT_CONFIG_LOG_MIN_LEVEL) ||
        (level > BOOT_LOG_LEVEL_ERROR) ||
        (format == (const char *)0)) {
        return;
    }

    (void)SEGGER_RTT_printf(0U, "[BOOT][%s] ", g_bootLogLevelNames[level]);
    va_start(arguments, format);
    (void)SEGGER_RTT_vprintf(0U, format, &arguments);
    va_end(arguments);
    (void)SEGGER_RTT_WriteString(0U, "\r\n");
}
//******************************** Functions ********************************//
