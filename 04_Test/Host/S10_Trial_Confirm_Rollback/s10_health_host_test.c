/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s10_health_host_test.c
 * @brief S10 Application Runtime Health 状态机 Host Test。
 * @author YaoQian Wang
 * @date 2026-09-19
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stdio.h>

#include "app_health.h"
#include "platform_def.h"
//******************************** Includes *********************************//

//******************************** Private Functions *************************//
static int s10_health_expect_state(
    app_health_context_t *context,
    app_health_state_t expected)
{
    app_health_state_t actual;

    if (app_health_get_state(context, &actual) != PLATFORM_ERR_OK) {
        return 1;
    }

    return (actual == expected) ? 0 : 1;
}

static int s10_health_test_trial_flow(void)
{
    app_health_context_t context;

    if (app_health_initialize(&context, PLATFORM_TRUE, 100U) !=
        PLATFORM_ERR_OK) {
        return 1;
    }

    if (app_health_startup_complete(
            &context,
            PLATFORM_TRUE,
            PLATFORM_FALSE,
            100U) != PLATFORM_ERR_OK) {
        return 1;
    }

    if ((s10_health_expect_state(
             &context,
             APP_HEALTH_STATE_WAIT_RUNTIME_READY) != 0) ||
        (app_health_feed_allowed(&context, 5099U) != PLATFORM_TRUE)) {
        return 1;
    }

    if ((app_health_mark_runtime_ready(
             &context,
             APP_HEALTH_READY_MAIN,
             1000U) != PLATFORM_ERR_OK) ||
        (app_health_mark_runtime_ready(
             &context,
             APP_HEALTH_READY_OTA,
             1200U) != PLATFORM_ERR_OK) ||
        (app_health_mark_runtime_ready(
             &context,
             APP_HEALTH_READY_DISPLAY,
             1500U) != PLATFORM_ERR_OK) ||
        (s10_health_expect_state(
             &context,
             APP_HEALTH_STATE_OBSERVING) != 0)) {
        return 1;
    }

    if ((app_health_feed_allowed(&context, 6499U) != PLATFORM_TRUE) ||
        (app_health_update(&context, 6500U) != PLATFORM_ERR_OK) ||
        (s10_health_expect_state(
             &context,
             APP_HEALTH_STATE_CONFIRM_REQUIRED) != 0) ||
        (app_health_feed_allowed(&context, 6500U) != PLATFORM_FALSE) ||
        (app_health_confirm_allowed(&context) != PLATFORM_TRUE)) {
        return 1;
    }

    if ((app_health_begin_confirm(&context) != PLATFORM_ERR_OK) ||
        (s10_health_expect_state(
             &context,
             APP_HEALTH_STATE_CONFIRMING) != 0) ||
        (app_health_feed_allowed(&context, 6501U) != PLATFORM_FALSE) ||
        (app_health_finish_confirm(&context, PLATFORM_ERR_OK) !=
         PLATFORM_ERR_OK) ||
        (s10_health_expect_state(
             &context,
             APP_HEALTH_STATE_STABLE) != 0) ||
        (app_health_feed_allowed(&context, 7000U) != PLATFORM_TRUE)) {
        return 1;
    }

    return 0;
}

static int s10_health_test_failures_and_none(void)
{
    app_health_context_t context;

    if ((app_health_initialize(&context, PLATFORM_TRUE, 0U) !=
         PLATFORM_ERR_OK) ||
        (app_health_startup_complete(
             &context,
             PLATFORM_FALSE,
             PLATFORM_FALSE,
             0U) != PLATFORM_ERR_OK) ||
        (s10_health_expect_state(
             &context,
             APP_HEALTH_STATE_FAILED) != 0) ||
        (app_health_confirm_allowed(&context) != PLATFORM_FALSE)) {
        return 1;
    }

    if ((app_health_initialize(&context, PLATFORM_TRUE, 0U) !=
         PLATFORM_ERR_OK) ||
        (app_health_startup_complete(
             &context,
             PLATFORM_FALSE,
             PLATFORM_TRUE,
             0U) != PLATFORM_ERR_OK) ||
        (s10_health_expect_state(
             &context,
             APP_HEALTH_STATE_FAILED) != 0)) {
        return 1;
    }

    if ((app_health_initialize(&context, PLATFORM_FALSE, 0U) !=
         PLATFORM_ERR_OK) ||
        (app_health_startup_complete(
             &context,
             PLATFORM_FALSE,
             PLATFORM_FALSE,
             0U) != PLATFORM_ERR_OK) ||
        (s10_health_expect_state(
             &context,
             APP_HEALTH_STATE_STABLE) != 0) ||
        (app_health_feed_allowed(&context, 5000U) != PLATFORM_TRUE)) {
        return 1;
    }

    return 0;
}

static int s10_health_test_deadline_wrap(void)
{
    app_health_context_t context;

    if ((app_health_initialize(
             &context,
             PLATFORM_TRUE,
             0xFFFFFF00U) != PLATFORM_ERR_OK) ||
        (app_health_startup_complete(
             &context,
             PLATFORM_TRUE,
             PLATFORM_FALSE,
             0xFFFFFF00U) != PLATFORM_ERR_OK) ||
        (app_health_update(&context, 0x00001200U) != PLATFORM_ERR_OK) ||
        (s10_health_expect_state(
             &context,
             APP_HEALTH_STATE_WAIT_RUNTIME_READY) != 0) ||
        (app_health_update(&context, 0x00001300U) != PLATFORM_ERR_TIMEOUT) ||
        (s10_health_expect_state(
             &context,
             APP_HEALTH_STATE_FAILED) != 0)) {
        return 1;
    }

    return 0;
}
//******************************** Private Functions *************************//

//******************************** Public Functions **************************//
int main(void)
{
    if ((s10_health_test_trial_flow() != 0) ||
        (s10_health_test_failures_and_none() != 0) ||
        (s10_health_test_deadline_wrap() != 0)) {
        (void)printf("S10 Health host test failed.\n");
        return 1;
    }

    (void)printf("S10 Health host test passed.\n");
    return 0;
}
//******************************** Public Functions **************************//
