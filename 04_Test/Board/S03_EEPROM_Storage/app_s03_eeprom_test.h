/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_s03_eeprom_test.h
 * @brief S03 AT24C02 RTT + EasyLogger 板测入口
 * @author YaoQian Wang
 * @date 2026-09-13
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_S03_EEPROM_TEST_H
#define APP_S03_EEPROM_TEST_H

//******************************** Includes *********************************//
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Functions *********************************//
/**
 * @brief 执行 S03 AT24C02 板级读写与持久性测试
 * @return PLATFORM_ERR_OK : 自动测试项完成且没有发现错误
 * @return 其他值 : 初始化、读写或比较测试失败
 * @note 本函数包含破坏性 EEPROM 测试写入，只应由临时板测固件显式调用。
 */
platform_error_t app_s03_eeprom_test_run(void);
//******************************** Functions *********************************//

#endif
