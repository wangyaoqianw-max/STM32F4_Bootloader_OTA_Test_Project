/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * @file impl_platform_key.h
 * @brief KEY_1 STM32 GPIO 到 Platform Key 的绑定接口。
 *****************************************************************************/

#ifndef IMPL_PLATFORM_KEY_H
#define IMPL_PLATFORM_KEY_H

#include "platform_types.h"

void impl_platform_key_exti_from_isr(uint16_t gpioPin);

#endif
