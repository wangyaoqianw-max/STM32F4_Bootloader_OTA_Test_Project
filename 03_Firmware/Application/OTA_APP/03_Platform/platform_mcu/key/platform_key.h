/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * @file platform_key.h
 * @brief Platform 通用按键事件接口。
 *****************************************************************************/

#ifndef PLATFORM_KEY_H
#define PLATFORM_KEY_H

#include "platform_error.h"

typedef enum
{
    PLATFORM_KEY_ID_1 = 0,
    PLATFORM_KEY_ID_MAX
} platform_key_id_t;

typedef void (*platform_key_event_callback_t)(platform_key_id_t key,
                                               void *context);

platform_error_t platform_key_init(platform_key_event_callback_t callback,
                                    void *context);
platform_error_t platform_key_event_from_isr(platform_key_id_t key);

#endif
