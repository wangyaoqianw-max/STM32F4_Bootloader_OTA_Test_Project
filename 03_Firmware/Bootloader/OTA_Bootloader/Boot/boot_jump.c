/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_jump.c
 * @brief Bootloader 到 Application 的 Cortex-M 跳转实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#include "boot_jump.h"

//******************************** Includes *********************************//
#include "boot_config.h"
#include "stm32f4xx.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
extern void boot_jump_set_msp_and_branch(uint32_t appMsp,
                                          uint32_t appResetHandler);

void boot_jump_to_app(uint32_t appMsp, uint32_t appResetHandler)
{
    uint32_t irqIndex;
    const uint32_t irqRegisterCount =
        (uint32_t)(sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]));

    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk | SCB_ICSR_PENDSTCLR_Msk;

    for (irqIndex = 0U; irqIndex < irqRegisterCount; irqIndex++) {
        NVIC->ICER[irqIndex] = 0xFFFFFFFFUL;
        NVIC->ICPR[irqIndex] = 0xFFFFFFFFUL;
    }

    SCB->VTOR = APP_BASE_ADDR;
    __DSB();
    __ISB();

    boot_jump_set_msp_and_branch(appMsp, appResetHandler);

    while (1) {
    }
}
//******************************** Functions ********************************//
