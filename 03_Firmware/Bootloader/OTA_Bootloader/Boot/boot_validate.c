/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_validate.c
 * @brief Application 启动向量基础合法性检查实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#include "boot_validate.h"

//******************************** Includes *********************************//
#include "boot_config.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define BOOT_VECTOR_ERASED_VALUE       (0xFFFFFFFFUL)
#define BOOT_STACK_ALIGNMENT_MASK      (0x00000007UL)
#define BOOT_THUMB_BIT_MASK            (0x00000001UL)
//******************************** Defines **********************************//

//******************************** Functions ********************************//
boot_app_vector_result_t boot_validate_app_vector(boot_app_vector_t *vector)
{
    volatile const uint32_t *appVector;
    uint32_t resetAddress;

    if (vector == (boot_app_vector_t *)0) {
        return BOOT_APP_VECTOR_NULL;
    }

    appVector = (volatile const uint32_t *)APP_BASE_ADDR;
    vector->initialMsp = appVector[0];
    vector->resetHandler = appVector[1];

    if ((vector->initialMsp == BOOT_VECTOR_ERASED_VALUE) ||
        (vector->resetHandler == BOOT_VECTOR_ERASED_VALUE)) {
        return BOOT_APP_VECTOR_ERASED;
    }

    if ((vector->initialMsp < MCU_SRAM_BASE_ADDR) ||
        (vector->initialMsp > MCU_SRAM_END_ADDR) ||
        ((vector->initialMsp & BOOT_STACK_ALIGNMENT_MASK) != 0U)) {
        return BOOT_APP_VECTOR_MSP_INVALID;
    }

    if ((vector->resetHandler & BOOT_THUMB_BIT_MASK) == 0U) {
        return BOOT_APP_VECTOR_RESET_INVALID;
    }

    resetAddress = vector->resetHandler & ~BOOT_THUMB_BIT_MASK;
    if ((resetAddress < APP_BASE_ADDR) ||
        (resetAddress >= APP_FLASH_END_ADDR)) {
        return BOOT_APP_VECTOR_RESET_INVALID;
    }

    return BOOT_APP_VECTOR_VALID;
}
//******************************** Functions ********************************//
