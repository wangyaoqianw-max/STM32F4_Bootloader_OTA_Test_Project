/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_metadata_commit.h
 * @brief S09/S10 Metadata 生命周期原子提交接口。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef BOOT_METADATA_COMMIT_H
#define BOOT_METADATA_COMMIT_H

//******************************** Includes *********************************//
#include "boot_at24c02.h"
#include "boot_metadata.h"
//******************************** Includes *********************************//

//******************************** Types ***********************************//
/**
 * @brief Metadata 生命周期原子提交结果。
 */
typedef enum
{
    BOOT_METADATA_COMMIT_OK = 0,
    BOOT_METADATA_COMMIT_READ_FAILED,
    BOOT_METADATA_COMMIT_STATE_INVALID,
    BOOT_METADATA_COMMIT_WRITE_FAILED,
    BOOT_METADATA_COMMIT_VERIFY_FAILED
} boot_metadata_commit_result_t;
//******************************** Types ***********************************//

//******************************** Functions ********************************//
/**
 * @brief 将已安装 Candidate 对应的 Metadata 从 PENDING 原子提交为 TRIAL。
 * @param[in] eeprom : 已初始化的 AT24C02；由调用者拥有。
 * @param[in] installedSlot : 本次已完成安装的 Candidate Slot。
 * @param[out] committedMetadata : 成功时输出重新加载并验证的最新 Metadata。
 * @return 原子提交结果。
 * @note 只改变 upgradeState 和 sequence；confirmed/pending/slot/version 字段保持不变。
 * @note commit marker 必须最后写入；失败时旧 Copy 仍保持 PENDING。
 */
boot_metadata_commit_result_t boot_metadata_commit_trial(
    boot_at24c02_t *eeprom,
    boot_firmware_slot_t installedSlot,
    boot_firmware_metadata_t *committedMetadata);
/**
 * @brief 将 TRIAL Metadata 原子持久化为 ROLLBACK。
 * @param[in] eeprom : 已初始化的 AT24C02；由调用者拥有。
 * @param[out] committedMetadata : 成功时输出重新加载并验证的最新 Metadata。
 * @return 原子提交结果。
 * @note 只改变 upgradeState 和 sequence；confirmed/pending/slot/version 字段保持不变。
 * @note 必须在任何 destructive restore 前完成，失败时旧 Copy 仍保持 TRIAL。
 */
boot_metadata_commit_result_t boot_metadata_commit_rollback_begin(
    boot_at24c02_t *eeprom,
    boot_firmware_metadata_t *committedMetadata);
/**
 * @brief 将 ROLLBACK Metadata 原子完成为 NONE。
 * @param[in] eeprom : 已初始化的 AT24C02；由调用者拥有。
 * @param[out] committedMetadata : 成功时输出重新加载并验证的最新 Metadata。
 * @return 原子提交结果。
 * @note 只改变 upgradeState、pendingSlot 和 sequence；confirmed/slot/version 字段保持不变。
 * @note 失败时旧 Copy 仍保持 ROLLBACK，允许下次 Boot 从零重试恢复。
 */
boot_metadata_commit_result_t boot_metadata_commit_rollback_complete(
    boot_at24c02_t *eeprom,
    boot_firmware_metadata_t *committedMetadata);
//******************************** Functions ********************************//

#endif
