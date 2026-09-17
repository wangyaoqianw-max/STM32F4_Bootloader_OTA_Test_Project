/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * @file impl_platform_key.c
 * @brief KEY_1 STM32 GPIO 到 Platform Key 的绑定实现。
 *****************************************************************************/

#include "impl_platform_key.h"

#include "main.h"
#include "platform_key.h"

void impl_platform_key_exti_from_isr(uint16_t gpioPin)
{
    if (gpioPin == KEY_1_Pin) {
        (void)platform_key_event_from_isr(PLATFORM_KEY_ID_1);
    }
}
