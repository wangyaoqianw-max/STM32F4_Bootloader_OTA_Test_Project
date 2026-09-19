/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file project_config.h
 * @brief 定义 S01 Application 基础工程的静态配置。
 * @author YaoQian Wang
 * @date 2026-09-11
 * @version V1.0
 *
 ******************************************************************************/

#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

//******************************** Includes *********************************//
#include "platform_gpio_types.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
/* Software I2C 的半周期和时钟拉伸等待上限，单位为微秒。 */
#define PROJECT_SOFT_I2C_HALF_PERIOD_US       (5U)
#define PROJECT_SOFT_I2C_SCL_TIMEOUT_US       (100U)
#define PROJECT_AT24C02_I2C_ADDRESS            (0x50U)

/* Status LED 的有效电平和基础闪烁节奏，时间单位为毫秒。 */
#define PROJECT_STATUS_LED_ACTIVE_LEVEL       PLATFORM_GPIO_LEVEL_LOW
#define PROJECT_STATUS_LED_BLINK_ON_MS        (500U)
#define PROJECT_STATUS_LED_BLINK_OFF_MS       (500U)

/* 当前 ST7789 面板的逻辑几何、显存偏移和 SPI 时钟配置。 */
#define PROJECT_DISPLAY_WIDTH                 (240U)
#define PROJECT_DISPLAY_HEIGHT                (280U)
#define PROJECT_DISPLAY_X_OFFSET              (0U)
#define PROJECT_DISPLAY_Y_OFFSET              (20U)
#define PROJECT_DISPLAY_MADCTL                (0x00U)
#define PROJECT_DISPLAY_SPI_MAX_CLOCK_HZ      (12500000U)

/* W25Q64 Platform 校验上限。 */
#define PROJECT_FLASH_SPI_MAX_CLOCK_HZ        (50000000U)

/* Application IWDG 目标超时时间，实际值受 LSI 误差影响。 */
#define PROJECT_WATCHDOG_TIMEOUT_MS           (10000U)

//******************************** Defines *********************************//

#endif
