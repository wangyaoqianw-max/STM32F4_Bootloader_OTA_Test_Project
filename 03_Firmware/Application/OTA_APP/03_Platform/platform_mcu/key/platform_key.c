/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * @file platform_key.c
 * @brief Platform 通用按键事件分发实现。
 *****************************************************************************/

#include "platform_key.h"

#include "platform_def.h"

static platform_key_event_callback_t g_platformKeyCallback = (void *)0;
static void *g_platformKeyContext = (void *)0;
static platform_bool_t g_platformKeyInitialized = PLATFORM_FALSE;

platform_error_t platform_key_init(platform_key_event_callback_t callback,
                                    void *context)
{
    if (callback == (void *)0) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (g_platformKeyInitialized == PLATFORM_TRUE) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }

    g_platformKeyCallback = callback;
    g_platformKeyContext = context;
    g_platformKeyInitialized = PLATFORM_TRUE;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_key_event_from_isr(platform_key_id_t key)
{
    if (g_platformKeyInitialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    if (key >= PLATFORM_KEY_ID_MAX) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    g_platformKeyCallback(key, g_platformKeyContext);
    return PLATFORM_ERR_OK;
}
