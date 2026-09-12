/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s02_flash_board_test.h
 * @brief S02 W25Q64 破坏性板测与重启持久化验证入口
 * @author YaoQian Wang
 * @date 2026-09-12
 * @version V1.0
 *
 *****************************************************************************/

#ifndef S02_FLASH_BOARD_TEST_H
#define S02_FLASH_BOARD_TEST_H

//******************************** Includes *********************************//
#include "platform_spi.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 执行 S02 W25Q64 完整破坏性板测与持久化验证
 * @param[in] spiBus : 已初始化并启动的存储 SPI Bus
 * @return platform_error_t : 板测结果
 * @note 本入口包含擦除、编程和持久化检查，仅允许由显式阶段配置调用。
 */
platform_error_t s02_flash_board_test_run(platform_spi_bus_t *spiBus);
//******************************** Functions ********************************//

#endif
