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
 * @brief 执行 S05 YMODEM 文件接收、Slot B 写入和镜像验证
 * @return PLATFORM_ERR_OK 表示传输完成且 Slot B 镜像验证为 VALID
 * @return 其他值表示初始化、传输、存储或镜像验证失败
 * @note 本入口会擦除并写入 Slot B，只应由 S05 专用板测启动路径调用。
 */
platform_error_t app_s05_ymodem_test_run(void);
//******************************** Functions ********************************//

#endif
