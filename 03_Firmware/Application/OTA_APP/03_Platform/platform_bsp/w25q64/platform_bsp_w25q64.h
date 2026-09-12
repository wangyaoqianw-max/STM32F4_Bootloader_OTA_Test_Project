/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_bsp_w25q64.h
 * @brief W25Q64JV Platform BSP 构造接口
 * @author YaoQian Wang
 * @date 2026-09-12
 * @version V1.0
 *
 *****************************************************************************/

#ifndef PLATFORM_BSP_W25Q64_H
#define PLATFORM_BSP_W25Q64_H

//******************************** Includes *********************************//
#include "platform_w25q64.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 构造并绑定板载 W25Q64JV Raw Driver 对象
 * @param[in,out] flash : 使用 PLATFORM_W25Q64_INITIALIZER 清零的对象存储
 * @return platform_error_t : 构造结果；本函数不启动 SPI Bus 或 Flash
 */
platform_error_t platform_bsp_w25q64_construct_flash(
    platform_w25q64_t *flash);
//******************************** Functions ********************************//

#endif
