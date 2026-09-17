/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s07_irq_host_test.c
 * @brief S07 Platform MCU IRQ Host Contract Test
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stdio.h>

#include "platform_mcu_irq.h"
//******************************** Includes *********************************//

static int test_irq_contract(void)
{
    if (PLATFORM_MCU_IRQ_KEY_EXTI0 != 0) {
        return 1;
    }

    if (PLATFORM_MCU_IRQ_USART1 != 1) {
        return 1;
    }

    if (PLATFORM_MCU_IRQ_DMA2_STREAM2 != 2) {
        return 1;
    }

    if (PLATFORM_MCU_IRQ_DMA2_STREAM7 != 3) {
        return 1;
    }

    if (PLATFORM_MCU_IRQ_FREERTOS_SAFE_PRIORITY != 5U) {
        return 1;
    }

    return (PLATFORM_MCU_IRQ_MAX == 4) ? 0 : 1;
}

int main(void)
{
    if (test_irq_contract() != 0) {
        (void)printf("S07 MCU IRQ host contract test failed.\n");
        return 1;
    }

    (void)printf("S07 MCU IRQ host contract test passed.\n");
    return 0;
}
