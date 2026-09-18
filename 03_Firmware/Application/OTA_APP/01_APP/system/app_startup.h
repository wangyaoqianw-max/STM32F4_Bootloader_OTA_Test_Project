/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_startup.h
 * @brief Application Startup Context、Barrier 与启动状态接口。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_STARTUP_H
#define APP_STARTUP_H

//******************************** Includes *********************************//
#include "platform_event_flags.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define APP_STARTUP_DONE_MAIN       (1UL << 0)
#define APP_STARTUP_DONE_OTA        (1UL << 1)
#define APP_STARTUP_DONE_DISPLAY    (1UL << 2)

#define APP_STARTUP_RUN             (1UL << 8)
#define APP_STARTUP_ABORT           (1UL << 9)

#define APP_STARTUP_DONE_ALL        \
    (APP_STARTUP_DONE_MAIN |       \
     APP_STARTUP_DONE_OTA |        \
     APP_STARTUP_DONE_DISPLAY)

#define APP_SYSTEM_STARTUP_TIMEOUT_MS    (5000U)
//******************************** Defines **********************************//

//******************************** Types ************************************//
typedef enum
{
    APP_SYSTEM_STATE_STARTING = 0,
    APP_SYSTEM_STATE_RUNNING,
    APP_SYSTEM_STATE_DEGRADED,
    APP_SYSTEM_STATE_FAILED
} app_system_state_t;

/**
 * @brief Application 启动阶段的静态生命周期上下文。
 * @note Event Flags 在 Application 生命周期内保持有效，不随 defaultTask 删除。
 */
typedef struct
{
    platform_event_flags_t events;
    platform_error_t mainResult;
    platform_error_t otaResult;
    platform_error_t displayResult;
    app_system_state_t systemState;
} app_startup_context_t;
//******************************** Types ************************************//

//******************************** Functions ********************************//
/**
 * @brief 创建并初始化 Application Startup Context。
 * @return platform_error_t : 初始化结果
 */
platform_error_t app_startup_initialize(void);

/**
 * @brief 获取 Application Startup Context 的只读视图。
 * @return Application Startup Context 指针
 */
const app_startup_context_t *app_startup_get_context(void);

/**
 * @brief 记录 appMainTask 初始化结果并发布 MAIN_DONE。
 * @param[in] result : appMainTask 初始化结果
 * @return platform_error_t : 上报结果
 */
platform_error_t app_startup_report_main(platform_error_t result);

/**
 * @brief 记录 otaWorker 初始化结果并发布 OTA_DONE。
 * @param[in] result : otaWorker 初始化结果
 * @return platform_error_t : 上报结果
 */
platform_error_t app_startup_report_ota(platform_error_t result);

/**
 * @brief 记录 displayTask 初始化结果并发布 DISPLAY_DONE。
 * @param[in] result : displayTask 初始化结果
 * @return platform_error_t : 上报结果
 */
platform_error_t app_startup_report_display(platform_error_t result);

/**
 * @brief 等待三个长期 Task 均完成本地初始化。
 * @param[in] timeoutMs : 超时时间，单位为毫秒
 * @return platform_error_t : Barrier 等待结果
 */
platform_error_t app_startup_wait_for_components(uint32_t timeoutMs);

/**
 * @brief 判断三个长期 Task 的本地初始化是否全部成功。
 * @return PLATFORM_TRUE 表示全部成功，否则返回 PLATFORM_FALSE
 */
platform_bool_t app_startup_components_succeeded(void);

/**
 * @brief 发布 RUNNING、DEGRADED 或 FAILED 决策。
 * @param[in] state : 系统启动裁决状态
 * @return platform_error_t : 发布结果
 */
platform_error_t app_startup_publish_decision(app_system_state_t state);

/**
 * @brief 等待广播的 SYSTEM_RUN 或 SYSTEM_ABORT 决策。
 * @return platform_error_t : 决策结果；SYSTEM_ABORT 映射为 CANCELED
 * @note 该接口在决策发布前阻塞，且不会清除广播事件位。
 */
platform_error_t app_startup_wait_for_decision(void);
//******************************** Functions ********************************//

#endif
