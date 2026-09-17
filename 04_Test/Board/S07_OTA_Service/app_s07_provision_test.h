/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_s07_provision_test.h
 * @brief S07 Factory Provisioning 板测入口
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_S07_PROVISION_TEST_H
#define APP_S07_PROVISION_TEST_H

//******************************** Includes *********************************//
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 创建 S07 Factory Provisioning 临时板测线程。
 * @return PLATFORM_ERR_OK 表示线程创建成功；其他值表示创建失败。
 * @note 仅允许在空白 External Flash/EEPROM 上运行，不属于正式 Application 启动路径。
 */
platform_error_t app_s07_provision_test_run(void);
//******************************** Functions ********************************//

#endif
