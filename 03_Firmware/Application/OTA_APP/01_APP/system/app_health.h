/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_health.h
 * @brief Application Trial Runtime Health 状态与权限接口。
 * @author YaoQian Wang
 * @date 2026-09-19
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_HEALTH_H
#define APP_HEALTH_H

//******************************** Includes *********************************//
#include "platform_error.h"
#include "platform_types.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define APP_HEALTH_READY_MAIN       (1UL << 0)
#define APP_HEALTH_READY_OTA        (1UL << 1)
#define APP_HEALTH_READY_DISPLAY    (1UL << 2)
#define APP_HEALTH_READY_ALL        \
    (APP_HEALTH_READY_MAIN |       \
     APP_HEALTH_READY_OTA |        \
     APP_HEALTH_READY_DISPLAY)

#define APP_HEALTH_RUNTIME_READY_TIMEOUT_MS  (5000U)
#define APP_HEALTH_OBSERVATION_WINDOW_MS     (5000U)
//******************************** Defines **********************************//

//******************************** Types ************************************//
/** @brief Trial Runtime Health 当前阶段。 */
typedef enum
{
    APP_HEALTH_STATE_STARTUP = 0U,
    APP_HEALTH_STATE_WAIT_RUNTIME_READY,
    APP_HEALTH_STATE_OBSERVING,
    APP_HEALTH_STATE_CONFIRM_REQUIRED,
    APP_HEALTH_STATE_CONFIRMING,
    APP_HEALTH_STATE_STABLE,
    APP_HEALTH_STATE_FAILED
} app_health_state_t;

/**
 * @brief Application Health 的最小运行上下文。
 * @note 不包含 Firmware Storage、W25Q64 或 EEPROM 指针。
 */
typedef struct
{
    app_health_state_t state;
    platform_bool_t trial;
    uint32_t readyMask;
    uint32_t readyDeadlineMs;
    uint32_t observationDeadlineMs;
} app_health_context_t;
//******************************** Types ************************************//

//******************************** Functions ********************************//
/**
 * @brief 初始化 Health Context。
 * @param[out] context : Health Context
 * @param[in] trial : PLATFORM_TRUE 表示当前固件处于 Trial
 * @param[in] nowMs : 当前单调毫秒计数，保留用于启动时间基准
 * @return platform_error_t : 初始化结果
 */
platform_error_t app_health_initialize(
    app_health_context_t *context,
    platform_bool_t trial,
    uint32_t nowMs);

/**
 * @brief 提交 Startup RUNNING/DEGRADED/FAILED 的 Health 结果。
 * @param[in,out] context : Health Context
 * @param[in] systemRunning : SYSTEM_RUN 是否为 RUNNING
 * @param[in] systemFailed : Startup Infrastructure 是否失败
 * @param[in] nowMs : 当前单调毫秒计数
 * @return platform_error_t : 处理结果
 * @note Trial 只有 RUNNING 能进入 Runtime Ready；NONE 保持稳定固件语义。
 */
platform_error_t app_health_startup_complete(
    app_health_context_t *context,
    platform_bool_t systemRunning,
    platform_bool_t systemFailed,
    uint32_t nowMs);

/**
 * @brief 报告一个长期 Task 已进入 Runtime Ready。
 * @param[in,out] context : Health Context
 * @param[in] readyFlag : APP_HEALTH_READY_* 中的单个标志
 * @param[in] nowMs : 当前单调毫秒计数
 * @return platform_error_t : 处理结果
 */
platform_error_t app_health_mark_runtime_ready(
    app_health_context_t *context,
    uint32_t readyFlag,
    uint32_t nowMs);

/**
 * @brief 根据当前时间推进 Health deadline/state。
 * @param[in,out] context : Health Context
 * @param[in] nowMs : 当前单调毫秒计数
 * @return platform_error_t : 处理结果；Ready 超时返回 PLATFORM_ERR_TIMEOUT
 */
platform_error_t app_health_update(
    app_health_context_t *context,
    uint32_t nowMs);

/**
 * @brief 判断当前时刻是否允许长期 Feed。
 * @param[in,out] context : Health Context
 * @param[in] nowMs : 当前单调毫秒计数
 * @return PLATFORM_TRUE 表示允许，PLATFORM_FALSE 表示禁止
 */
platform_bool_t app_health_feed_allowed(
    app_health_context_t *context,
    uint32_t nowMs);

/**
 * @brief 判断当前是否允许发起严格 Confirm。
 * @param[in] context : Health Context
 * @return PLATFORM_TRUE 表示允许，PLATFORM_FALSE 表示禁止
 */
platform_bool_t app_health_confirm_allowed(
    const app_health_context_t *context);

/**
 * @brief 将 Health 状态切换到 CONFIRMING。
 * @param[in,out] context : Health Context
 * @return platform_error_t : 状态切换结果
 */
platform_error_t app_health_begin_confirm(
    app_health_context_t *context);

/**
 * @brief 提交 Confirm 事务结果并完成状态切换。
 * @param[in,out] context : Health Context
 * @param[in] result : Confirm 事务结果
 * @return platform_error_t : 原样返回 Confirm 结果
 */
platform_error_t app_health_finish_confirm(
    app_health_context_t *context,
    platform_error_t result);

/**
 * @brief 读取当前 Health State。
 * @param[in] context : Health Context
 * @param[out] state : State 输出
 * @return platform_error_t : 读取结果
 */
platform_error_t app_health_get_state(
    const app_health_context_t *context,
    app_health_state_t *state);
//******************************** Functions ********************************//

#endif
