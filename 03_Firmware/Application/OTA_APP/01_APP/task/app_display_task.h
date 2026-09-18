/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_display_task.h
 * @brief displayTask 生命周期接口
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_DISPLAY_TASK_H
#define APP_DISPLAY_TASK_H

//******************************** Includes *********************************//
#include "app_runtime_contract.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 绑定共享 Display Queue 并启动 displayTask。
 * @param[in] displayQueue : 由系统 Bootstrap 预先创建的 Display Queue
 * @return platform_error_t : 创建结果
 * @note Queue 属于 Application Runtime 生命周期，由 system 负责创建。
 */
platform_error_t app_display_task_start(platform_queue_t *displayQueue);
//******************************** Functions ********************************//

#endif
