/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_main_task.c
 * @brief Application 前台长期 Task 实现。
 * @author YaoQian Wang
 * @date 2026-09-11
 * @version V1.0
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include "app_main_task.h"

#define LOG_TAG "app_main_task"

#include "app_health.h"
#include "app_startup.h"
#include "app_system.h"
#include "app_ota_worker.h"
#include "platform_os.h"
#include "platform_mcu_reset.h"
#include "platform_time.h"
#include "platform_watchdog.h"
#include "project_config.h"
#include "diagnostics_fault.h"
#include "service_log.h"
#include "platform_bsp_led.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define APP_MAIN_TASK_STACK_SIZE    (2048U)
//******************************** Defines **********************************//

//******************************** Private Functions *************************//
static void app_main_task_entry(void *argument);
static void app_main_task_terminate(void);
static platform_error_t app_main_task_consume_runtime_ready(
    app_health_context_t *health,
    uint32_t nowMs);
static platform_error_t app_main_task_consume_confirm_result(
    app_health_context_t *health);
static platform_error_t app_main_task_service_health(void);
static void app_main_task_controlled_reset(platform_error_t error);
//******************************** Private Functions *************************//

//******************************** Variables ********************************//
static platform_led_t g_statusLed = PLATFORM_LED_INITIALIZER;
static platform_bool_t g_appMainTaskStarted = PLATFORM_FALSE;
static platform_bool_t g_appMainRuntimeReadyReported = PLATFORM_FALSE;
static platform_bool_t g_appMainConfirmRequested = PLATFORM_FALSE;
static platform_thread_t g_appMainTaskThread = PLATFORM_OS_OBJECT_INITIALIZER;

static const platform_thread_config_t s_app_main_task_config = {
    .name = "appMainTask",
    .entry = app_main_task_entry,
    .argument = (void *)0,
    .stackSizeBytes = APP_MAIN_TASK_STACK_SIZE,
    .priority = PLATFORM_THREAD_PRIORITY_NORMAL,
};
//******************************** Variables ********************************//

//******************************** Private Functions *************************//
static platform_error_t app_main_task_consume_runtime_ready(
    app_health_context_t *health,
    uint32_t nowMs)
{
    app_health_state_t state;
    uint32_t readyMask;
    platform_error_t result;

    if (health == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    result = app_system_take_runtime_ready(&readyMask);
    if (result == PLATFORM_ERR_TIMEOUT) {
        return PLATFORM_ERR_OK;
    }
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = app_health_get_state(health, &state);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (state != APP_HEALTH_STATE_WAIT_RUNTIME_READY) {
        return PLATFORM_ERR_OK;
    }

    if ((readyMask & APP_HEALTH_READY_MAIN) != 0U) {
        result = app_health_mark_runtime_ready(
            health,
            APP_HEALTH_READY_MAIN,
            nowMs);
        if ((result != PLATFORM_ERR_OK) &&
            (result != PLATFORM_ERR_ALREADY_INITIALIZED)) {
            return result;
        }
    }

    if ((readyMask & APP_HEALTH_READY_OTA) != 0U) {
        result = app_health_mark_runtime_ready(
            health,
            APP_HEALTH_READY_OTA,
            nowMs);
        if ((result != PLATFORM_ERR_OK) &&
            (result != PLATFORM_ERR_ALREADY_INITIALIZED)) {
            return result;
        }
    }

    if ((readyMask & APP_HEALTH_READY_DISPLAY) != 0U) {
        result = app_health_mark_runtime_ready(
            health,
            APP_HEALTH_READY_DISPLAY,
            nowMs);
        if ((result != PLATFORM_ERR_OK) &&
            (result != PLATFORM_ERR_ALREADY_INITIALIZED)) {
            return result;
        }
    }

    return PLATFORM_ERR_OK;
}

static platform_error_t app_main_task_consume_confirm_result(
    app_health_context_t *health)
{
    platform_error_t confirmResult;
    platform_error_t result;

    if (health == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    result = app_system_take_confirm_result(&confirmResult);
    if (result == PLATFORM_ERR_TIMEOUT) {
        return PLATFORM_ERR_OK;
    }
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = app_health_finish_confirm(health, confirmResult);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return confirmResult;
}

static platform_error_t app_main_task_service_health(void)
{
    app_health_context_t *health;
    app_health_state_t state;
    uint32_t nowMs;
    platform_error_t result;

    health = app_system_get_health_context();
    if (health == NULL) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    result = platform_time_get_ms(&nowMs);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = app_main_task_consume_runtime_ready(health, nowMs);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = app_health_update(health, nowMs);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = app_main_task_consume_confirm_result(health);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = app_health_get_state(health, &state);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (state == APP_HEALTH_STATE_FAILED) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    if ((g_appMainConfirmRequested == PLATFORM_FALSE) &&
        (app_health_confirm_allowed(health) == PLATFORM_TRUE)) {
        result = app_health_begin_confirm(health);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }

        g_appMainConfirmRequested = PLATFORM_TRUE;
        result = app_ota_worker_request_confirm();
        if (result != PLATFORM_ERR_OK) {
            (void)app_health_finish_confirm(health, result);
            return result;
        }
    }

    if (app_health_feed_allowed(health, nowMs) == PLATFORM_TRUE) {
        return platform_watchdog_feed();
    }

    return PLATFORM_ERR_OK;
}

static void app_main_task_controlled_reset(platform_error_t error)
{
    SERVICE_LOG_E("Health control requests reset: %d", (int)error);
    platform_mcu_reset();

    for (;;) {
        (void)platform_time_delay_ms(1000U);
    }
}

static void app_main_task_terminate(void)
{
    platform_error_t result;

    result = platform_thread_terminate(&g_appMainTaskThread);
    if (result != PLATFORM_ERR_OK) {
        SERVICE_LOG_E("Application task termination failed: %d", (int)result);
    }

    for (;;) {
        (void)platform_time_delay_ms(1000U);
    }
}

/* 构造并启动 Application 前台基础资源。 */
static platform_error_t app_main_init(void)
{
    platform_error_t result = platform_bsp_led_construct_status_led(
        &g_statusLed);

    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_led_init(&g_statusLed);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return result;
}

static void app_main_task_entry(void *argument)
{
    platform_error_t appResult;
    platform_error_t result;
    const app_startup_context_t *startupContext;
    uint32_t freeStackBytes;

    (void)argument;

    SERVICE_LOG_I("Application Foundation start");

    appResult = app_main_init();
    SERVICE_LOG_I("Application init result: %d", appResult);

    if (appResult != PLATFORM_ERR_OK) {
        SERVICE_LOG_E("Application init failed: %d", appResult);
    }

    result = app_startup_report_main(appResult);
    if (result != PLATFORM_ERR_OK) {
        SERVICE_LOG_E("Application startup report failed: %d", result);
        app_main_task_terminate();
    }

    result = app_startup_wait_for_decision();
    if (result != PLATFORM_ERR_OK) {
        SERVICE_LOG_I("Application startup aborted: %d", result);
        app_main_task_terminate();
    }

    startupContext = app_startup_get_context();
    if ((appResult != PLATFORM_ERR_OK) ||
        (startupContext->systemState == APP_SYSTEM_STATE_FAILED)) {
        app_main_task_terminate();
    }

    result = platform_thread_get_stack_space(
        &g_appMainTaskThread,
        &freeStackBytes);
    if (result == PLATFORM_ERR_OK) {
        SERVICE_LOG_I("appMainTask stack free=%lu B",
                      (unsigned long)freeStackBytes);
    } else {
        SERVICE_LOG_W("appMainTask stack query failed: %d", (int)result);
    }

#if (DIAG_FAULT_TEST_ENABLE != 0U)
    (void)platform_time_delay_ms(DIAG_FAULT_TEST_DELAY_MS);
    diagnostics_fault_trigger(DIAG_FAULT_TEST_TYPE);
#endif

    for (;;) {
        (void)platform_led_on(&g_statusLed);
        (void)platform_time_delay_ms(PROJECT_STATUS_LED_BLINK_ON_MS);
        (void)platform_led_off(&g_statusLed);
        (void)platform_time_delay_ms(PROJECT_STATUS_LED_BLINK_OFF_MS);

        if (g_appMainRuntimeReadyReported == PLATFORM_FALSE) {
            result = app_system_report_runtime_ready(APP_HEALTH_READY_MAIN);
            if (result != PLATFORM_ERR_OK) {
                app_main_task_controlled_reset(result);
            }
            g_appMainRuntimeReadyReported = PLATFORM_TRUE;
        }

        result = app_main_task_service_health();
        if (result != PLATFORM_ERR_OK) {
            app_main_task_controlled_reset(result);
        }
    }
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t app_main_task_start(void)
{
    platform_error_t result;

    if (g_appMainTaskStarted == PLATFORM_TRUE) {
        return PLATFORM_ERR_OK;
    }

    result = platform_thread_create(
        &g_appMainTaskThread,
        &s_app_main_task_config);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    g_appMainTaskStarted = PLATFORM_TRUE;
    return PLATFORM_ERR_OK;
}
//******************************** Functions *********************************//
