/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s07a_startup_contract_host_test.c
 * @brief S07A Startup Context / Barrier Contract Host Test
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stdio.h>

#include "app_startup.h"
#include "platform_def.h"
//******************************** Includes *********************************//

//******************************** Variables ********************************//
static uint32_t g_eventFlags = 0U;
//******************************** Variables ********************************//

//******************************** Platform Event Flags Stubs ***************//
platform_error_t platform_event_flags_create(
    platform_event_flags_t *eventFlags)
{
    if (eventFlags == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    eventFlags->native = (void *)1;
    g_eventFlags = 0U;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_event_flags_set(
    platform_event_flags_t *eventFlags,
    uint32_t flags)
{
    if ((eventFlags == NULL) || (eventFlags->native == NULL)) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    g_eventFlags |= flags;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_event_flags_wait(
    platform_event_flags_t *eventFlags,
    uint32_t flags,
    platform_bool_t waitAll,
    platform_bool_t clearOnExit,
    uint32_t timeoutMs,
    uint32_t *receivedFlags)
{
    uint32_t matchedFlags;

    (void)timeoutMs;

    if ((eventFlags == NULL) || (eventFlags->native == NULL) ||
        (receivedFlags == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    matchedFlags = g_eventFlags & flags;
    if (((waitAll != PLATFORM_FALSE) && (matchedFlags != flags)) ||
        ((waitAll == PLATFORM_FALSE) && (matchedFlags == 0U))) {
        return PLATFORM_ERR_TIMEOUT;
    }

    *receivedFlags = matchedFlags;
    if (clearOnExit != PLATFORM_FALSE) {
        g_eventFlags &= ~matchedFlags;
    }

    return PLATFORM_ERR_OK;
}

platform_error_t platform_event_flags_delete(
    platform_event_flags_t *eventFlags)
{
    if (eventFlags == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    eventFlags->native = NULL;
    return PLATFORM_ERR_OK;
}
//******************************** Platform Event Flags Stubs ***************//

//******************************** Private Functions *************************//
static int test_startup_contract(void)
{
    const app_startup_context_t *context;

    if (app_startup_initialize() != PLATFORM_ERR_OK) {
        return 1;
    }

    context = app_startup_get_context();
    if ((context == NULL) ||
        (context->systemState != APP_SYSTEM_STATE_STARTING)) {
        return 1;
    }

    if (app_startup_wait_for_components(100U) != PLATFORM_ERR_TIMEOUT) {
        return 1;
    }

    if ((app_startup_report_main(PLATFORM_ERR_OK) != PLATFORM_ERR_OK) ||
        (app_startup_report_ota(PLATFORM_ERR_IO) != PLATFORM_ERR_OK) ||
        (app_startup_report_display(PLATFORM_ERR_OK) != PLATFORM_ERR_OK)) {
        return 1;
    }

    if (app_startup_wait_for_components(100U) != PLATFORM_ERR_OK) {
        return 1;
    }

    if ((context->mainResult != PLATFORM_ERR_OK) ||
        (context->otaResult != PLATFORM_ERR_IO) ||
        (context->displayResult != PLATFORM_ERR_OK) ||
        (g_eventFlags & APP_STARTUP_DONE_ALL) != APP_STARTUP_DONE_ALL ||
        app_startup_components_succeeded() != PLATFORM_FALSE) {
        return 1;
    }

    if (app_startup_publish_decision(APP_SYSTEM_STATE_DEGRADED) !=
        PLATFORM_ERR_OK) {
        return 1;
    }

    if (app_startup_wait_for_decision() != PLATFORM_ERR_OK) {
        return 1;
    }

    if ((context->systemState != APP_SYSTEM_STATE_DEGRADED) ||
        (g_eventFlags & APP_STARTUP_RUN) != APP_STARTUP_RUN) {
        return 1;
    }

    if ((app_startup_publish_decision(APP_SYSTEM_STATE_FAILED) !=
         PLATFORM_ERR_OK) ||
        (app_startup_wait_for_decision() != PLATFORM_ERR_CANCELED) ||
        ((g_eventFlags & APP_STARTUP_ABORT) != APP_STARTUP_ABORT)) {
        return 1;
    }

    return 0;
}
//******************************** Private Functions *************************//

//******************************** Public Functions **************************//
int main(void)
{
    if (test_startup_contract() != 0) {
        (void)printf("S07A Startup contract host test failed.\n");
        return 1;
    }

    (void)printf("S07A Startup contract host test passed.\n");
    return 0;
}
//******************************** Public Functions **************************//
