/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_main_task.h
 * @brief Application 前台长期 Task 接口。
 * @author YaoQian Wang
 * @date 2026-09-11
 * @version V1.0
 *
 ******************************************************************************/

#ifndef APP_MAIN_TASK_H
#define APP_MAIN_TASK_H

//******************************** Includes *********************************//
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 启动 Application 基础资源、板测和 Status LED 周期闪烁。
 * @return platform_error_t : Task 创建结果
 */
platform_error_t app_main_task_start(void);
//******************************** Functions ********************************//

#endif
