/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_main.c
 * @brief Bootloader 启动决策入口实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#include "boot_main.h"

//******************************** Includes *********************************//
#include "boot_jump.h"
#include "boot_log.h"
#include "boot_validate.h"
#include "stm32f4xx_hal.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
static const char *boot_main_validation_result_name(
    boot_app_vector_result_t result)
{
    switch (result) {
        case BOOT_APP_VECTOR_VALID:
            return "VALID";
        case BOOT_APP_VECTOR_NULL:
            return "NULL";
        case BOOT_APP_VECTOR_ERASED:
            return "ERASED";
        case BOOT_APP_VECTOR_MSP_INVALID:
            return "MSP_INVALID";
        case BOOT_APP_VECTOR_RESET_INVALID:
            return "RESET_INVALID";
        default:
            return "UNKNOWN";
    }
}

void boot_main_run(void)
{
    boot_app_vector_t appVector = {0};
    boot_app_vector_result_t validationResult;

    BOOT_LOG_I("BOOT start");
    validationResult = boot_validate_app_vector(&appVector);
    BOOT_LOG_I("APP vector MSP = 0x%08lx",
               (unsigned long)appVector.initialMsp);
    BOOT_LOG_I("APP vector Reset_Handler = 0x%08lx",
               (unsigned long)appVector.resetHandler);

    if (validationResult != BOOT_APP_VECTOR_VALID) {
        BOOT_LOG_E("APP validation FAIL: %s",
                   boot_main_validation_result_name(validationResult));
        while (1) {
        }
    }

    BOOT_LOG_I("APP validation PASS");
    BOOT_LOG_I("jump start");
    boot_jump_to_app(appVector.initialMsp, appVector.resetHandler);
}

/**
 * @brief 为 Bootloader 的 HAL 初始化和时钟切换提供 SysTick 时基。
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
//******************************** Functions ********************************//
