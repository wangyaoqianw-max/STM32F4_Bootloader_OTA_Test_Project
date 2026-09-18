/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file cmbacktrace_port.c
 * @brief Bootloader CmBacktrace 工程适配实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#include "cmbacktrace_port.h"

//******************************** Includes *********************************//
#include "cm_backtrace.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
void cmbacktrace_port_init(void)
{
    cm_backtrace_init("OTA_BOOTLOADER", "STM32F411CE", "S08.1");
}
//******************************** Functions ********************************//
