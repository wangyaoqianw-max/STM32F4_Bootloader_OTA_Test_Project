/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_jump.h
 * @brief Bootloader 到 Application 的 Cortex-M 跳转接口。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef BOOT_JUMP_H
#define BOOT_JUMP_H

//******************************** Includes *********************************//
#include <stdint.h>
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 设置 Application 运行环境并跳转到 Reset_Handler。
 * @param[in] appMsp : Application 初始主堆栈指针。
 * @param[in] appResetHandler : Application Reset_Handler 地址，必须保留
 *                              Thumb bit。
 */
void boot_jump_to_app(uint32_t appMsp, uint32_t appResetHandler);
//******************************** Functions ********************************//

#endif
