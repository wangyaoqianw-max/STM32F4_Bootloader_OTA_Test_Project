/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_startup.c
 * @brief Application Startup Context、Barrier 与启动状态实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_startup.h"

#include "platform_def.h"
#include "platform_os.h"
//******************************** Includes *********************************//

//******************************** Private Functions *************************//
static platform_error_t app_startup_report_component(
    platform_error_t result,
    uint32_t doneFlag,
    platform_error_t *storedResult);
//******************************** Private Functions *************************//

//******************************** Variables ********************************//
static app_startup_context_t g_appStartupContext = {
    .events = PLATFORM_OS_OBJECT_INITIALIZER,
    .mainResult = PLATFORM_ERR_NOT_INITIALIZED,
    .otaResult = PLATFORM_ERR_NOT_INITIALIZED,
    .displayResult = PLATFORM_ERR_NOT_INITIALIZED,
    .systemState = APP_SYSTEM_STATE_STARTING
};
static platform_bool_t g_appStartupInitialized = PLATFORM_FALSE;
//******************************** Variables ********************************//

//******************************** Private Functions *************************//
static platform_error_t app_startup_report_component(
    platform_error_t result,
    uint32_t doneFlag,
    platform_error_t *storedResult)
{
    if (storedResult == (platform_error_t *)0) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (g_appStartupInitialized == PLATFORM_FALSE) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    *storedResult = result;
    return platform_event_flags_set(
        &g_appStartupContext.events,
        doneFlag);
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t app_startup_initialize(void)
{
    platform_error_t result;

    if (g_appStartupInitialized == PLATFORM_TRUE) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }

    g_appStartupContext.mainResult = PLATFORM_ERR_NOT_INITIALIZED;
    g_appStartupContext.otaResult = PLATFORM_ERR_NOT_INITIALIZED;
    g_appStartupContext.displayResult = PLATFORM_ERR_NOT_INITIALIZED;
    g_appStartupContext.systemState = APP_SYSTEM_STATE_STARTING;

    result = platform_event_flags_create(&g_appStartupContext.events);
    if (result == PLATFORM_ERR_OK) {
        g_appStartupInitialized = PLATFORM_TRUE;
    } else {
        g_appStartupContext.systemState = APP_SYSTEM_STATE_FAILED;
    }
    return result;
}

const app_startup_context_t *app_startup_get_context(void)
{
    return &g_appStartupContext;
}

platform_error_t app_startup_report_main(platform_error_t result)
{
    return app_startup_report_component(
        result,
        APP_STARTUP_DONE_MAIN,
        &g_appStartupContext.mainResult);
}

platform_error_t app_startup_report_ota(platform_error_t result)
{
    return app_startup_report_component(
        result,
        APP_STARTUP_DONE_OTA,
        &g_appStartupContext.otaResult);
}

platform_error_t app_startup_report_display(platform_error_t result)
{
    return app_startup_report_component(
        result,
        APP_STARTUP_DONE_DISPLAY,
        &g_appStartupContext.displayResult);
}

platform_error_t app_startup_wait_for_components(uint32_t timeoutMs)
{
    uint32_t receivedFlags;

    if (g_appStartupInitialized == PLATFORM_FALSE) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    return platform_event_flags_wait(
        &g_appStartupContext.events,
        APP_STARTUP_DONE_ALL,
        PLATFORM_TRUE,
        PLATFORM_FALSE,
        timeoutMs,
        &receivedFlags);
}

platform_bool_t app_startup_components_succeeded(void)
{
    return ((g_appStartupContext.mainResult == PLATFORM_ERR_OK) &&
            (g_appStartupContext.otaResult == PLATFORM_ERR_OK) &&
            (g_appStartupContext.displayResult == PLATFORM_ERR_OK)) ?
           PLATFORM_TRUE : PLATFORM_FALSE;
}

platform_error_t app_startup_publish_decision(app_system_state_t state)
{
    uint32_t decisionFlag;

    if (g_appStartupInitialized == PLATFORM_FALSE) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    if ((state != APP_SYSTEM_STATE_RUNNING) &&
        (state != APP_SYSTEM_STATE_DEGRADED) &&
        (state != APP_SYSTEM_STATE_FAILED)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    g_appStartupContext.systemState = state;
    decisionFlag = (state == APP_SYSTEM_STATE_FAILED) ?
                   APP_STARTUP_ABORT : APP_STARTUP_RUN;
    return platform_event_flags_set(
        &g_appStartupContext.events,
        decisionFlag);
}

platform_error_t app_startup_wait_for_decision(void)
{
    uint32_t receivedFlags;
    platform_error_t result;

    if (g_appStartupInitialized == PLATFORM_FALSE) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    result = platform_event_flags_wait(
        &g_appStartupContext.events,
        APP_STARTUP_RUN | APP_STARTUP_ABORT,
        PLATFORM_FALSE,
        PLATFORM_FALSE,
        PLATFORM_OS_WAIT_FOREVER,
        &receivedFlags);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return ((receivedFlags & APP_STARTUP_ABORT) != 0U) ?
           PLATFORM_ERR_CANCELED : PLATFORM_ERR_OK;
}
//******************************** Functions *********************************//
