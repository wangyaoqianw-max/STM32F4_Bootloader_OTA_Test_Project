/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file impl_platform_bsp_key.h
 * @brief 当前板级 KEY_1 到 Platform BSP 按键的绑定接口
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

#ifndef IMPL_PLATFORM_BSP_KEY_H
#define IMPL_PLATFORM_BSP_KEY_H

//******************************** Includes *********************************//
#include "platform_types.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 从 STM32 GPIO EXTI 回调识别板级 KEY_1
 * @param[in] gpioPin : HAL GPIO Pin 掩码
 * @return 无
 * @note 仅允许在 ISR 上下文调用，不执行 OTA 或其他业务处理。
 */
void impl_platform_bsp_key_exti_from_isr(uint16_t gpioPin);
//******************************** Functions ********************************//

#endif
