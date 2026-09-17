/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file impl_platform_bsp_key.c
 * @brief 当前板级 KEY_1 到 Platform BSP 按键的绑定实现
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "impl_platform_bsp_key.h"

#include "main.h"
#include "platform_key.h"
//******************************** Includes *********************************//

//******************************** Functions *********************************//
void impl_platform_bsp_key_exti_from_isr(uint16_t gpioPin)
{
    if (gpioPin == KEY_1_Pin) {
        (void)platform_key_event_from_isr(PLATFORM_KEY_ID_1);
    }
}
//******************************** Functions *********************************//
