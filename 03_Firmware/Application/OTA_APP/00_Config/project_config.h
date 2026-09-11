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

/* Status LED 的有效电平和基础闪烁节奏，时间单位为毫秒。 */
#define PROJECT_STATUS_LED_ACTIVE_LEVEL       PLATFORM_GPIO_LEVEL_LOW
#define PROJECT_STATUS_LED_BLINK_ON_MS        (500U)
#define PROJECT_STATUS_LED_BLINK_OFF_MS       (500U)
//******************************** Defines *********************************//

#endif
