/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file firmware_lifecycle.h
 * @brief Firmware Runtime Lifecycle 公共接口
 * @author YaoQian Wang
 * @date 2026-09-19
 * @version V1.0
 *
 *****************************************************************************/

#ifndef FIRMWARE_LIFECYCLE_H
#define FIRMWARE_LIFECYCLE_H

//******************************** Includes *********************************//
#include "firmware_storage.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
/**
 * @brief 严格确认当前 Trial Firmware
 * @param[in,out] storage : 已初始化的 Firmware Storage；底层 Driver 由调用者拥有
 * @return PLATFORM_ERR_OK 表示已原子确认；其他值表示确认未完成
 * @note 本接口只允许在 otaWorker 所属上下文执行，不负责 Health 策略或 Watchdog。
 *       提交前会重新读取 Metadata 并完整验证 pending image，提交后会重新读取并核对结果。
 */
platform_error_t firmware_lifecycle_confirm(firmware_storage_t *storage);
//******************************** Functions ********************************//

#endif
