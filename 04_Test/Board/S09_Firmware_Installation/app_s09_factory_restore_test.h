/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_s09_factory_restore_test.h
 * @brief S09 Factory Restore 板测入口。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef APP_S09_FACTORY_RESTORE_TEST_H
#define APP_S09_FACTORY_RESTORE_TEST_H

//******************************** Includes *********************************//
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 创建 S09 Factory Restore 临时板测线程。
 * @return PLATFORM_ERR_OK 表示线程创建成功；其他值表示创建失败。
 * @note 该入口只由 Toolkit Factory Restore 临时接入，不属于正式启动路径。
 */
platform_error_t app_s09_factory_restore_test_run(void);
//******************************** Functions ********************************//

#endif
