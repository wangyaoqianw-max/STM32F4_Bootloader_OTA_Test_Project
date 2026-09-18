/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file diagnostics_config.h
 * @brief Bootloader Cortex-M Fault 诊断静态配置。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef DIAGNOSTICS_CONFIG_H
#define DIAGNOSTICS_CONFIG_H

//******************************** Defines **********************************//
/* 默认关闭受控 Fault，板级验证时通过本配置显式开启。 */
#define DIAG_FAULT_TEST_ENABLE          (0U)
#define DIAG_FAULT_TEST_TYPE            (DIAG_FAULT_INVALID_ADDRESS)

#define DIAG_FAULT_INVALID_ADDRESS_VALUE (0xFFFFFFF0U)
#define DIAG_FAULT_WRITE_PATTERN         (0xDEADBEEFU)
#define DIAG_FAULT_UNDEFINED_OPCODE      (0xDEADU)
//******************************** Defines **********************************//

#endif
