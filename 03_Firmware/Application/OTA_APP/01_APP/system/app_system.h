/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_system.h
 * @brief Application System Composition / Bootstrap 接口。
 * @author YaoQian Wang
 * @date 2026-09-12
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_SYSTEM_H
#define APP_SYSTEM_H

//******************************** Includes *********************************//
#include "app_health.h"
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define APP_SYSTEM_HEALTH_CONFIRM_RESULT    (1UL << 3)
//******************************** Defines **********************************//

//******************************** Functions ********************************//
/**
 * @brief 在 defaultTask 上完成 Application 系统装配和启动裁决。
 * @return PLATFORM_ERR_OK 表示进入 RUNNING 或 DEGRADED；其他值表示启动基础设施失败。
 */
platform_error_t app_system_bootstrap(void);

/**
 * @brief 从长期 Task 上报一个 Runtime Ready 事件。
 * @param[in] readyFlag : APP_HEALTH_READY_* 中的单个标志
 * @return platform_error_t : 上报结果
 */
platform_error_t app_system_report_runtime_ready(uint32_t readyFlag);

/**
 * @brief 由 appMainTask 非阻塞取出待处理的 Runtime Ready 事件。
 * @param[out] readyMask : 本次取出的 Ready 标志
 * @return platform_error_t : 读取结果；无事件时返回 PLATFORM_ERR_TIMEOUT
 */
platform_error_t app_system_take_runtime_ready(uint32_t *readyMask);

/**
 * @brief 由 otaWorker 上报 Strict Confirm 结果。
 * @param[in] result : Confirm 事务结果
 * @return platform_error_t : 上报结果
 */
platform_error_t app_system_report_confirm_result(platform_error_t result);

/**
 * @brief 由 appMainTask 非阻塞取出 Strict Confirm 结果。
 * @param[out] result : Confirm 事务结果
 * @return platform_error_t : 读取结果；无结果时返回 PLATFORM_ERR_TIMEOUT
 */
platform_error_t app_system_take_confirm_result(platform_error_t *result);

/**
 * @brief 获取由 appMainTask 独占推进的 Health Context。
 * @return Health Context；基础设施尚未初始化时返回 NULL
 */
app_health_context_t *app_system_get_health_context(void);
//******************************** Functions ********************************//

#endif
