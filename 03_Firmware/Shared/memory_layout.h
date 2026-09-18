/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file memory_layout.h
 * @brief Application 与 Bootloader 共用的 STM32F411 内部 Flash / SRAM 地址合同。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef MEMORY_LAYOUT_H
#define MEMORY_LAYOUT_H

//******************************** Defines **********************************//
#define BOOT_FLASH_BASE_ADDR    (0x08000000UL)
#define BOOT_FLASH_SIZE         (0x00010000UL)
#define BOOT_FLASH_END_ADDR     (BOOT_FLASH_BASE_ADDR + BOOT_FLASH_SIZE)

#define APP_BASE_ADDR           (0x08010000UL)
#define APP_FLASH_SIZE          (0x00070000UL)
#define APP_FLASH_END_ADDR      (APP_BASE_ADDR + APP_FLASH_SIZE)

#define MCU_SRAM_BASE_ADDR      (0x20000000UL)
#define MCU_SRAM_SIZE           (0x00020000UL)
#define MCU_SRAM_END_ADDR       (MCU_SRAM_BASE_ADDR + MCU_SRAM_SIZE)
//******************************** Defines **********************************//

#endif
