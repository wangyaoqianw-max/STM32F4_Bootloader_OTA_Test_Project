/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * @file impl_platform_mcu_irq.c
 * @brief STM32 CMSIS NVIC Platform MCU IRQ Impl。
 *****************************************************************************/

#include "impl_platform_mcu_irq.h"

#include "stm32f4xx_hal.h"

static platform_error_t impl_platform_mcu_irq_map(
    platform_mcu_irq_id_t irq,
    IRQn_Type *irqn)
{
    if (irqn == (void *)0) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    switch (irq) {
        case PLATFORM_MCU_IRQ_KEY_EXTI0:
            *irqn = EXTI0_IRQn;
            return PLATFORM_ERR_OK;

        case PLATFORM_MCU_IRQ_USART1:
            *irqn = USART1_IRQn;
            return PLATFORM_ERR_OK;

        case PLATFORM_MCU_IRQ_DMA2_STREAM2:
            *irqn = DMA2_Stream2_IRQn;
            return PLATFORM_ERR_OK;

        case PLATFORM_MCU_IRQ_DMA2_STREAM7:
            *irqn = DMA2_Stream7_IRQn;
            return PLATFORM_ERR_OK;

        default:
            return PLATFORM_ERR_INVALID_PARAM;
    }
}

platform_error_t platform_mcu_irq_enable(platform_mcu_irq_id_t irq)
{
    IRQn_Type irqn;

    if (impl_platform_mcu_irq_map(irq, &irqn) != PLATFORM_ERR_OK) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    NVIC_EnableIRQ(irqn);
    return PLATFORM_ERR_OK;
}

platform_error_t platform_mcu_irq_disable(platform_mcu_irq_id_t irq)
{
    IRQn_Type irqn;

    if (impl_platform_mcu_irq_map(irq, &irqn) != PLATFORM_ERR_OK) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    NVIC_DisableIRQ(irqn);
    return PLATFORM_ERR_OK;
}

platform_error_t platform_mcu_irq_set_priority(platform_mcu_irq_id_t irq,
                                                uint32_t priority)
{
    IRQn_Type irqn;

    if (impl_platform_mcu_irq_map(irq, &irqn) != PLATFORM_ERR_OK) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if ((priority < PLATFORM_MCU_IRQ_FREERTOS_SAFE_PRIORITY) ||
        (priority > 15U)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    NVIC_SetPriority(irqn, priority);
    return PLATFORM_ERR_OK;
}

platform_error_t platform_mcu_irq_clear_pending(platform_mcu_irq_id_t irq)
{
    IRQn_Type irqn;

    if (impl_platform_mcu_irq_map(irq, &irqn) != PLATFORM_ERR_OK) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    NVIC_ClearPendingIRQ(irqn);
    return PLATFORM_ERR_OK;
}
