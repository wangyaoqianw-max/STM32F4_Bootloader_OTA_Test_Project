/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file cmb_user_cfg.h
 * @brief Bootloader CmBacktrace Bare-metal Cortex-M4 配置。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef CMB_USER_CFG_H
#define CMB_USER_CFG_H

//******************************** Includes *********************************//
#include "SEGGER_RTT.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
/* CmBacktrace Fault 输出直接使用 RTT，不依赖 Bootloader 日志封装。 */
#define cmb_println(...)                                                       \
    do {                                                                       \
        (void)SEGGER_RTT_printf(0U, __VA_ARGS__);                              \
        (void)SEGGER_RTT_WriteString(0U, "\r\n");                             \
    } while (0)

#define CMB_USING_BARE_METAL_PLATFORM
#define CMB_CPU_PLATFORM_TYPE         CMB_CPU_ARM_CORTEX_M4
#define CMB_USING_DUMP_STACK_INFO
#define CMB_PRINT_LANGUAGE            CMB_PRINT_LANGUAGE_ENGLISH
//******************************** Defines **********************************//

#endif
