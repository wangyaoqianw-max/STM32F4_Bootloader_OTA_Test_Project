/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_display_task.h
 * @brief S06 displayTask 生命周期接口
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
 * @brief 创建并启动 displayTask，同时创建 Display Queue。
 * @param[out] displayQueue : 调用者提供的 Queue 句柄存储
 * @return platform_error_t : 创建结果
 * @note Queue 运行时由 displayTask 创建和拥有；调用者负责句柄存储生命周期。
 */
platform_error_t app_display_task_start(platform_queue_t *displayQueue);
//******************************** Functions ********************************//

#endif
