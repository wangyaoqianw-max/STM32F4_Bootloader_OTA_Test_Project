/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_key.h
 * @brief Platform BSP 按键事件接口
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

#ifndef PLATFORM_KEY_H
#define PLATFORM_KEY_H

//******************************** Includes *********************************//
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Types ************************************//
typedef enum
{
    PLATFORM_KEY_ID_1 = 0,
    PLATFORM_KEY_ID_MAX
} platform_key_id_t;

typedef void (*platform_key_event_callback_t)(platform_key_id_t key,
                                               void *context);
//******************************** Types ************************************//

//******************************** Functions ********************************//
/**
 * @brief 初始化 Platform BSP 按键事件分发
 * @param[in] callback : 按键事件回调；回调必须满足 ISR 上下文约束
 * @param[in] context : 回调上下文，可为 NULL
 * @return platform_error_t : 初始化结果
 * @note 本模块只支持一次初始化，按键事件回调由调用者长期持有。
 */
platform_error_t platform_key_init(platform_key_event_callback_t callback,
                                    void *context);

/**
 * @brief 从 ISR 上下文分发一次按键事件
 * @param[in] key : Platform BSP 按键标识
 * @return platform_error_t : 分发结果
 * @note 本函数不得阻塞；回调不得执行 Flash、日志、延时或协议处理。
 */
platform_error_t platform_key_event_from_isr(platform_key_id_t key);
//******************************** Functions ********************************//

#endif
