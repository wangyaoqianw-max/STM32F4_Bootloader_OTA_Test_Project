/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_health.c
 * @brief Application Trial Runtime Health 状态与权限实现。
 * @author YaoQian Wang
 * @date 2026-09-19
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_health.h"

#include "platform_def.h"
//******************************** Includes *********************************//

//******************************** Private Functions *************************//
static platform_bool_t app_health_is_valid_bool(platform_bool_t value)
{
    return ((value == PLATFORM_FALSE) || (value == PLATFORM_TRUE)) ?
           PLATFORM_TRUE : PLATFORM_FALSE;
}

static platform_bool_t app_health_deadline_reached(
    uint32_t nowMs,
    uint32_t deadlineMs)
{
    return ((int32_t)(nowMs - deadlineMs) >= 0) ?
           PLATFORM_TRUE : PLATFORM_FALSE;
}

static platform_bool_t app_health_is_ready_flag(uint32_t readyFlag)
{
    return ((readyFlag == APP_HEALTH_READY_MAIN) ||
            (readyFlag == APP_HEALTH_READY_OTA) ||
            (readyFlag == APP_HEALTH_READY_DISPLAY)) ?
           PLATFORM_TRUE : PLATFORM_FALSE;
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t app_health_initialize(
    app_health_context_t *context,
    platform_bool_t trial,
    uint32_t nowMs)
{
    if (context == (app_health_context_t *)0) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (app_health_is_valid_bool(trial) != PLATFORM_TRUE) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    (void)nowMs;
    context->state = APP_HEALTH_STATE_STARTUP;
    context->trial = trial;
    context->readyMask = 0U;
    context->readyDeadlineMs = 0U;
    context->observationDeadlineMs = 0U;
    return PLATFORM_ERR_OK;
}

platform_error_t app_health_startup_complete(
    app_health_context_t *context,
    platform_bool_t systemRunning,
    platform_bool_t systemFailed,
    uint32_t nowMs)
{
    if (context == (app_health_context_t *)0) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if ((app_health_is_valid_bool(systemRunning) != PLATFORM_TRUE) ||
        (app_health_is_valid_bool(systemFailed) != PLATFORM_TRUE)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (context->state != APP_HEALTH_STATE_STARTUP) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    if ((systemRunning == PLATFORM_TRUE) &&
        (systemFailed == PLATFORM_TRUE)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (systemFailed == PLATFORM_TRUE) {
        context->state = APP_HEALTH_STATE_FAILED;
        return PLATFORM_ERR_OK;
    }

    if (context->trial != PLATFORM_TRUE) {
        context->state = APP_HEALTH_STATE_STABLE;
        return PLATFORM_ERR_OK;
    }

    if (systemRunning != PLATFORM_TRUE) {
        context->state = APP_HEALTH_STATE_FAILED;
        return PLATFORM_ERR_OK;
    }

    context->readyMask = 0U;
    context->readyDeadlineMs =
        nowMs + APP_HEALTH_RUNTIME_READY_TIMEOUT_MS;
    context->observationDeadlineMs = 0U;
    context->state = APP_HEALTH_STATE_WAIT_RUNTIME_READY;
    return PLATFORM_ERR_OK;
}

platform_error_t app_health_mark_runtime_ready(
    app_health_context_t *context,
    uint32_t readyFlag,
    uint32_t nowMs)
{
    platform_error_t result;

    if (context == (app_health_context_t *)0) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (app_health_is_ready_flag(readyFlag) != PLATFORM_TRUE) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    result = app_health_update(context, nowMs);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (context->state != APP_HEALTH_STATE_WAIT_RUNTIME_READY) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    if ((context->readyMask & readyFlag) != 0U) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }

    context->readyMask |= readyFlag;
    if (context->readyMask == APP_HEALTH_READY_ALL) {
        context->observationDeadlineMs =
            nowMs + APP_HEALTH_OBSERVATION_WINDOW_MS;
        context->state = APP_HEALTH_STATE_OBSERVING;
    }

    return PLATFORM_ERR_OK;
}

platform_error_t app_health_update(
    app_health_context_t *context,
    uint32_t nowMs)
{
    if (context == (app_health_context_t *)0) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (context->state == APP_HEALTH_STATE_WAIT_RUNTIME_READY) {
        if (app_health_deadline_reached(
                nowMs,
                context->readyDeadlineMs) == PLATFORM_TRUE) {
            context->state = APP_HEALTH_STATE_FAILED;
            return PLATFORM_ERR_TIMEOUT;
        }
    } else if (context->state == APP_HEALTH_STATE_OBSERVING) {
        if (app_health_deadline_reached(
                nowMs,
                context->observationDeadlineMs) == PLATFORM_TRUE) {
            context->state = APP_HEALTH_STATE_CONFIRM_REQUIRED;
        }
    }

    return PLATFORM_ERR_OK;
}

platform_bool_t app_health_feed_allowed(
    app_health_context_t *context,
    uint32_t nowMs)
{
    if (context == (app_health_context_t *)0) {
        return PLATFORM_FALSE;
    }

    if (app_health_update(context, nowMs) == PLATFORM_ERR_TIMEOUT) {
        return PLATFORM_FALSE;
    }

    switch (context->state) {
        case APP_HEALTH_STATE_STARTUP:
        case APP_HEALTH_STATE_WAIT_RUNTIME_READY:
        case APP_HEALTH_STATE_OBSERVING:
        case APP_HEALTH_STATE_STABLE:
            return PLATFORM_TRUE;

        case APP_HEALTH_STATE_CONFIRM_REQUIRED:
        case APP_HEALTH_STATE_CONFIRMING:
        case APP_HEALTH_STATE_FAILED:
        default:
            return PLATFORM_FALSE;
    }
}

platform_bool_t app_health_confirm_allowed(
    const app_health_context_t *context)
{
    if (context == (const app_health_context_t *)0) {
        return PLATFORM_FALSE;
    }

    return ((context->trial == PLATFORM_TRUE) &&
            (context->state == APP_HEALTH_STATE_CONFIRM_REQUIRED)) ?
           PLATFORM_TRUE : PLATFORM_FALSE;
}

platform_error_t app_health_begin_confirm(
    app_health_context_t *context)
{
    if (context == (app_health_context_t *)0) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (app_health_confirm_allowed(context) != PLATFORM_TRUE) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    context->state = APP_HEALTH_STATE_CONFIRMING;
    return PLATFORM_ERR_OK;
}

platform_error_t app_health_finish_confirm(
    app_health_context_t *context,
    platform_error_t result)
{
    if (context == (app_health_context_t *)0) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (context->state != APP_HEALTH_STATE_CONFIRMING) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    context->state = (result == PLATFORM_ERR_OK) ?
                     APP_HEALTH_STATE_STABLE :
                     APP_HEALTH_STATE_FAILED;
    return result;
}

platform_error_t app_health_get_state(
    const app_health_context_t *context,
    app_health_state_t *state)
{
    if ((context == (const app_health_context_t *)0) ||
        (state == (app_health_state_t *)0)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    *state = context->state;
    return PLATFORM_ERR_OK;
}
//******************************** Functions *********************************//
