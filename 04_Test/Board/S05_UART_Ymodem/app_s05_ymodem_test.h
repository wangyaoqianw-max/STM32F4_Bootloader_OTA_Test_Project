/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_s05_ymodem_test.h
 * @brief S05 UART YMODEM 到 Slot B 板测入口
 * @author YaoQian Wang
 * @date 2026-09-14
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_S05_YMODEM_TEST_H
#define APP_S05_YMODEM_TEST_H

//******************************** Includes *********************************//
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 创建 S05 专用线程执行 YMODEM 文件接收、Slot B 写入和镜像验证
 * @return PLATFORM_ERR_OK 表示 S05 专用线程创建成功
 * @return 其他值表示线程创建失败
 * @note 本入口会擦除并写入 Slot B，只应由 S05 专用板测启动路径调用。
 */
platform_error_t app_s05_ymodem_test_run(void);
//******************************** Functions ********************************//

#endif
