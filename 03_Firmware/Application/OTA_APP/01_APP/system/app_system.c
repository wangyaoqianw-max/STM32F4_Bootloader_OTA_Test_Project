/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_system.c
 * @brief Application System Composition / Bootstrap 实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_system.h"

#define LOG_TAG "app_system"

#include "app_display_task.h"
#include "app_main_task.h"
#include "app_ota_runtime.h"
#include "app_ota_worker.h"
#include "app_runtime_contract.h"
#include "app_startup.h"
#include "platform_os.h"
#include "platform_time.h"
#include "service_log.h"

#include <stddef.h>
//******************************** Includes *********************************//

//******************************** Variables ********************************//
static platform_bool_t g_appSystemBootstrapped = PLATFORM_FALSE;
static platform_queue_t g_displayQueue = PLATFORM_OS_OBJECT_INITIALIZER;
static platform_event_flags_t g_appHealthEvents = PLATFORM_OS_OBJECT_INITIALIZER;
static app_health_context_t g_appHealthContext = {0};
static platform_error_t g_appHealthConfirmResult = PLATFORM_ERR_UNKNOWN;
static platform_bool_t g_appHealthEventsInitialized = PLATFORM_FALSE;
static platform_bool_t g_appHealthInitialized = PLATFORM_FALSE;
//******************************** Variables ********************************//

//******************************** Private Functions *************************//
static platform_bool_t app_system_is_ready_flag(uint32_t readyFlag)
{
    return ((readyFlag == APP_HEALTH_READY_MAIN) ||
            (readyFlag == APP_HEALTH_READY_OTA) ||
            (readyFlag == APP_HEALTH_READY_DISPLAY)) ?
           PLATFORM_TRUE : PLATFORM_FALSE;
}

static platform_error_t app_system_initialize_health(
    app_system_state_t systemState)
{
    platform_bool_t trial;
    uint32_t nowMs;
    platform_error_t result;

    result = app_ota_runtime_get_trial_status(&trial);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_time_get_ms(&nowMs);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = app_health_initialize(&g_appHealthContext, trial, nowMs);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = app_health_startup_complete(
        &g_appHealthContext,
        (systemState == APP_SYSTEM_STATE_RUNNING) ? PLATFORM_TRUE : PLATFORM_FALSE,
        (systemState == APP_SYSTEM_STATE_FAILED) ? PLATFORM_TRUE : PLATFORM_FALSE,
        nowMs);
    if (result == PLATFORM_ERR_OK) {
        g_appHealthInitialized = PLATFORM_TRUE;
    }
    return result;
}

static platform_error_t app_system_abort(platform_error_t error)
{
    platform_error_t abortResult;

    abortResult = app_startup_publish_decision(APP_SYSTEM_STATE_FAILED);
    if (abortResult != PLATFORM_ERR_OK) {
        SERVICE_LOG_E("Startup abort publication failed: %d",
                      (int)abortResult);
    }

    return error;
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t app_system_bootstrap(void)
{
    const app_startup_context_t *startupContext;
    app_system_state_t systemState;
    platform_error_t result;

    if (g_appSystemBootstrapped == PLATFORM_TRUE) {
        return PLATFORM_ERR_OK;
    }

    result = app_startup_initialize();
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    g_appSystemBootstrapped = PLATFORM_TRUE;

    result = platform_event_flags_create(&g_appHealthEvents);
    if (result != PLATFORM_ERR_OK) {
        return app_system_abort(result);
    }
    g_appHealthEventsInitialized = PLATFORM_TRUE;

    result = platform_queue_create(
        &g_displayQueue,
        APP_DISPLAY_QUEUE_LENGTH,
        sizeof(app_display_event_t));
    if (result != PLATFORM_ERR_OK) {
        return app_system_abort(result);
    }

    result = app_main_task_start();
    if (result != PLATFORM_ERR_OK) {
        return app_system_abort(result);
    }

    result = app_ota_worker_start(&g_displayQueue);
    if (result != PLATFORM_ERR_OK) {
        return app_system_abort(result);
    }

    result = app_display_task_start(&g_displayQueue);
    if (result != PLATFORM_ERR_OK) {
        return app_system_abort(result);
    }

    result = app_startup_wait_for_components(
        APP_SYSTEM_STARTUP_TIMEOUT_MS);
    if (result != PLATFORM_ERR_OK) {
        SERVICE_LOG_E("Startup barrier failed: %d", (int)result);
        return app_system_abort(result);
    }

    systemState = app_startup_components_succeeded() == PLATFORM_TRUE ?
                  APP_SYSTEM_STATE_RUNNING : APP_SYSTEM_STATE_DEGRADED;

    result = app_system_initialize_health(systemState);
    if (result != PLATFORM_ERR_OK) {
        SERVICE_LOG_E("Health initialization failed: %d", (int)result);
        return app_system_abort(result);
    }

    result = app_startup_publish_decision(systemState);
    if (result != PLATFORM_ERR_OK) {
        return app_system_abort(result);
    }

    startupContext = app_startup_get_context();
    SERVICE_LOG_I(
        "Startup state=%d main=%d ota=%d display=%d",
        (int)startupContext->systemState,
        (int)startupContext->mainResult,
        (int)startupContext->otaResult,
        (int)startupContext->displayResult);
    return PLATFORM_ERR_OK;
}

platform_error_t app_system_report_runtime_ready(uint32_t readyFlag)
{
    if (app_system_is_ready_flag(readyFlag) != PLATFORM_TRUE) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (g_appHealthEventsInitialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    return platform_event_flags_set(&g_appHealthEvents, readyFlag);
}

platform_error_t app_system_take_runtime_ready(uint32_t *readyMask)
{
    if (readyMask == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (g_appHealthEventsInitialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    return platform_event_flags_wait(
        &g_appHealthEvents,
        APP_HEALTH_READY_ALL,
        PLATFORM_FALSE,
        PLATFORM_TRUE,
        PLATFORM_OS_NO_WAIT,
        readyMask);
}

platform_error_t app_system_report_confirm_result(platform_error_t result)
{
    if (g_appHealthEventsInitialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    g_appHealthConfirmResult = result;
    return platform_event_flags_set(
        &g_appHealthEvents,
        APP_SYSTEM_HEALTH_CONFIRM_RESULT);
}

platform_error_t app_system_take_confirm_result(platform_error_t *result)
{
    uint32_t receivedFlags;
    platform_error_t waitResult;

    if (result == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (g_appHealthEventsInitialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    waitResult = platform_event_flags_wait(
        &g_appHealthEvents,
        APP_SYSTEM_HEALTH_CONFIRM_RESULT,
        PLATFORM_FALSE,
        PLATFORM_TRUE,
        PLATFORM_OS_NO_WAIT,
        &receivedFlags);
    if (waitResult != PLATFORM_ERR_OK) {
        return waitResult;
    }

    *result = g_appHealthConfirmResult;
    return PLATFORM_ERR_OK;
}

app_health_context_t *app_system_get_health_context(void)
{
    return (g_appHealthInitialized == PLATFORM_TRUE) ?
           &g_appHealthContext : NULL;
}
//******************************** Functions *********************************//
