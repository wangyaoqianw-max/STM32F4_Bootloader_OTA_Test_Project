/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_s04_firmware_image_test.h
 * @brief S04 Firmware Image UART 注入板测入口
 * @author YaoQian Wang
 * @date 2026-09-13
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_S04_FIRMWARE_IMAGE_TEST_H
#define APP_S04_FIRMWARE_IMAGE_TEST_H

//******************************** Includes *********************************//
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 执行 S04 UART 到固定 Slot B 的破坏性 Firmware Image 板测
 * @return PLATFORM_ERR_OK 表示镜像写入、完整回读和 Metadata 恢复验证通过
 * @return 其他值表示初始化、UART、存储、CRC 或 Metadata 验证失败
 * @note 本入口必须在 appSystem Task Context 中执行，并独占通信 UART、Storage SPI Bus 和测试 Metadata。
 * @warning 本入口会擦除 Slot B 并改写 AT24C02 Metadata，仅允许由显式阶段测试开关调用。
 */
platform_error_t app_s04_firmware_image_test_run(void);
//******************************** Functions ********************************//

#endif
