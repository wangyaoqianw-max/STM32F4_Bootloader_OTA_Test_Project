/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file diagnostics_config.h
 * @brief Cortex-M Fault 受控测试配置。
 * @author YaoQian Wang
 * @date 2026-09-15
 * @version V1.0
 ******************************************************************************/

#ifndef DIAGNOSTICS_CONFIG_H
#define DIAGNOSTICS_CONFIG_H

#ifndef DIAG_FAULT_TEST_ENABLE
#define DIAG_FAULT_TEST_ENABLE          (0U)
#endif

#ifndef DIAG_FAULT_TEST_TYPE
#define DIAG_FAULT_TEST_TYPE            DIAG_FAULT_INVALID_ADDRESS
#endif

#ifndef DIAG_FAULT_TEST_DELAY_MS
#define DIAG_FAULT_TEST_DELAY_MS        (1000U)
#endif

#define DIAG_FAULT_INVALID_ADDRESS_VALUE (0xFFFFFFF0U)
#define DIAG_FAULT_WRITE_PATTERN         (0xDEADBEEFU)
#define DIAG_FAULT_UNDEFINED_OPCODE      (0xDEADU)

#endif
