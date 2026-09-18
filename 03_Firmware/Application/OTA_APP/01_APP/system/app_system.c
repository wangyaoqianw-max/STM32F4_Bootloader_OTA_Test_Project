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
#include "app_ota_worker.h"
#include "app_runtime_contract.h"
#include "app_startup.h"
#include "platform_os.h"
#include "service_log.h"
//******************************** Includes *********************************//

//******************************** Variables ********************************//
static platform_bool_t g_appSystemBootstrapped = PLATFORM_FALSE;
static platform_queue_t g_displayQueue = PLATFORM_OS_OBJECT_INITIALIZER;
//******************************** Variables ********************************//

//******************************** Private Functions *************************//
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
//******************************** Functions *********************************//
