/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_key.c
 * @brief Platform BSP 按键事件分发实现
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "platform_key.h"

#include "platform_def.h"
//******************************** Includes *********************************//

//******************************** Variables ********************************//
static platform_key_event_callback_t g_platformKeyCallback = NULL;
static void *g_platformKeyContext = NULL;
static platform_bool_t g_platformKeyInitialized = PLATFORM_FALSE;
//******************************** Variables ********************************//

//******************************** Functions *********************************//
platform_error_t platform_key_init(platform_key_event_callback_t callback,
                                    void *context)
{
    if (callback == NULL) {
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

    if (key != PLATFORM_KEY_ID_1) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    g_platformKeyCallback(key, g_platformKeyContext);
    return PLATFORM_ERR_OK;
}
//******************************** Functions *********************************//
