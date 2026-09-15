/*
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file cmbacktrace_port.c
 * @brief CmBacktrace 工程适配实现。
 */

#include "cmbacktrace_port.h"

#include "cm_backtrace.h"

void cmbacktrace_port_init(void)
{
    cm_backtrace_init("OTA_APP", "STM32F411CE", "V1.0");
}
