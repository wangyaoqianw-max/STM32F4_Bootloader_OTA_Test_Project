/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_validate.h
 * @brief Application vector 基础合法性检查接口。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef BOOT_VALIDATE_H
#define BOOT_VALIDATE_H

//******************************** Includes *********************************//
#include <stdint.h>
//******************************** Includes *********************************//

//******************************** Types ************************************//
/**
 * @brief Application 启动向量检查结果。
 */
typedef enum
{
    BOOT_APP_VECTOR_VALID = 0U,
    BOOT_APP_VECTOR_NULL,
    BOOT_APP_VECTOR_ERASED,
    BOOT_APP_VECTOR_MSP_INVALID,
    BOOT_APP_VECTOR_RESET_INVALID
} boot_app_vector_result_t;

/**
 * @brief Application 启动向量表中 Bootloader 需要读取的字段。
 */
typedef struct
{
    uint32_t initialMsp;
    uint32_t resetHandler;
} boot_app_vector_t;
//******************************** Types ************************************//

//******************************** Functions ********************************//
/**
 * @brief 检查 Application 向量表中的 MSP 和 Reset_Handler。
 * @param[out] vector : 输出读取到的 Application 向量表内容。
 * @return 检查结果。
 */
boot_app_vector_result_t boot_validate_app_vector(boot_app_vector_t *vector);
/**
 * @brief 使用指定的 MSP 和 Reset_Handler 检查 Application 向量值。
 * @param[in] initialMsp : 向量表第一个字；必须位于 MCU SRAM 范围
 * @param[in] resetHandler : 向量表第二个字；必须为 APP 内 Thumb 地址
 * @return 检查结果；规则与 boot_validate_app_vector() 相同。
 */
boot_app_vector_result_t boot_validate_vector_values(
    uint32_t initialMsp,
    uint32_t resetHandler);
//******************************** Functions ********************************//

#endif
