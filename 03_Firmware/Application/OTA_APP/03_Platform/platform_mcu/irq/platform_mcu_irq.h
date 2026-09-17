/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * @file platform_mcu_irq.h
 * @brief Platform MCU 通用 IRQ 控制接口。
 *****************************************************************************/

#ifndef PLATFORM_MCU_IRQ_H
#define PLATFORM_MCU_IRQ_H

#include "platform_error.h"

/* 这些 IRQ 会调用 FreeRTOS FromISR 接口，优先级不得高于该安全边界。 */
#define PLATFORM_MCU_IRQ_FREERTOS_SAFE_PRIORITY    (5U)

typedef enum
{
    PLATFORM_MCU_IRQ_KEY_EXTI0 = 0,
    PLATFORM_MCU_IRQ_USART1,
    PLATFORM_MCU_IRQ_DMA2_STREAM2,
    PLATFORM_MCU_IRQ_DMA2_STREAM7,
    PLATFORM_MCU_IRQ_MAX
} platform_mcu_irq_id_t;

platform_error_t platform_mcu_irq_enable(platform_mcu_irq_id_t irq);
platform_error_t platform_mcu_irq_disable(platform_mcu_irq_id_t irq);
platform_error_t platform_mcu_irq_set_priority(platform_mcu_irq_id_t irq,
                                                uint32_t priority);
platform_error_t platform_mcu_irq_clear_pending(platform_mcu_irq_id_t irq);

#endif
