/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_s09_metadata_baseline_test.h
 * @brief S09 AT24C02 Metadata 基线板测入口。
 * @author YaoQian Wang
 * @date 2026-09-21
 * @version V1.0
 *
 ******************************************************************************/

#ifndef APP_S09_METADATA_BASELINE_TEST_H
#define APP_S09_METADATA_BASELINE_TEST_H

//******************************** Includes *********************************//
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 创建 S09 Metadata 基线临时板测线程。
 * @return PLATFORM_ERR_OK 表示线程创建成功；其他值表示创建失败。
 * @note W25Q64 必须已经由 External Loader 预烧录；本入口只读 W25Q64，
 *       写入并验证 AT24C02 Metadata。
 */
platform_error_t app_s09_metadata_baseline_test_run(void);
//******************************** Functions ********************************//

#endif
