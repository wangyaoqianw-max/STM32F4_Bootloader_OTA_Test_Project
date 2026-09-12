/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s02_flash_board_test.h
 * @brief S02 W25Q64 非破坏性板测入口
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
 * @brief 执行 S02 W25Q64 JEDEC/SR1 非破坏性板测
 * @param[in] spiBus : 已初始化并启动的存储 SPI Bus
 * @return platform_error_t : 板测结果
 */
platform_error_t s02_flash_board_test_run(platform_spi_bus_t *spiBus);
//******************************** Functions ********************************//

#endif
