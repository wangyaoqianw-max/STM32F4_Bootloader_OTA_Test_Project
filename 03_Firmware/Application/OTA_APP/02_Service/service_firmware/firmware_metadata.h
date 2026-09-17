/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file firmware_metadata.h
 * @brief Firmware Metadata V2 公共接口
 * @author YaoQian Wang
 * @date 2026-09-13
 * @version V1.0
 *
 *****************************************************************************/

#ifndef FIRMWARE_METADATA_H
#define FIRMWARE_METADATA_H

//******************************** Includes *********************************//
#include "firmware_def.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define FIRMWARE_METADATA_COPY_SIZE           (128U)
#define FIRMWARE_METADATA_COPY_A_ADDRESS      (0x00U)
#define FIRMWARE_METADATA_COPY_B_ADDRESS      (0x80U)
#define FIRMWARE_METADATA_MAGIC               (0x444D5746UL)
#define FIRMWARE_METADATA_FORMAT_VERSION_V1   (1U)
#define FIRMWARE_METADATA_FORMAT_VERSION_V2   (2U)
#define FIRMWARE_METADATA_FORMAT_VERSION      FIRMWARE_METADATA_FORMAT_VERSION_V2
#define FIRMWARE_METADATA_COMMIT_MARKER       (0x54494D43UL)
#define FIRMWARE_METADATA_INVALID_MARKER      (0xFFFFFFFFUL)
//******************************** Defines **********************************//

//******************************** Types ***********************************//
typedef enum
{
    FIRMWARE_METADATA_COPY_NONE = 0,
    FIRMWARE_METADATA_COPY_A,
    FIRMWARE_METADATA_COPY_B
} firmware_metadata_copy_id_t;

/**
 * @brief Firmware Metadata 跨 Reset 升级生命周期
 */
typedef enum
{
    FIRMWARE_UPGRADE_STATE_NONE = 0,
    FIRMWARE_UPGRADE_STATE_PENDING,
    FIRMWARE_UPGRADE_STATE_TRIAL,
    FIRMWARE_UPGRADE_STATE_ROLLBACK
} firmware_upgrade_state_t;

/**
 * @brief Firmware Metadata V2 的内存表达
 * @note 持久化与传输必须使用 fixed-offset raw buffer 编解码，不使用本结构体原始内存布局。
 */
typedef struct
{
    /** 双副本提交序列号；使用 32-bit wrap-around-safe 比较。 */
    uint32_t sequence;
    /** 已确认 Slot；可为 FIRMWARE_SLOT_NONE。 */
    firmware_slot_t confirmedSlot;
    /** Bootloader 下一次需要安装的 Slot；无待安装镜像时为 FIRMWARE_SLOT_NONE。 */
    firmware_slot_t pendingSlot;
    /** Slot A 的基础镜像状态。 */
    firmware_slot_state_t slotAState;
    /** Slot B 的基础镜像状态。 */
    firmware_slot_state_t slotBState;
    /** 跨 Reset 保存的升级生命周期状态。 */
    firmware_upgrade_state_t upgradeState;
    /** 已确认版本基准；不替代 Image Header 的实际版本。 */
    firmware_version_t confirmedVersion;
} firmware_metadata_t;
//******************************** Types ***********************************//

//******************************** Declaring *******************************//
/**
 * @brief 编码尚未提交的 Metadata V2 副本
 * @param[in] metadata : 源 Metadata；不得为空，全部字段必须符合 V2 范围
 * @param[out] raw : 固定 128 Byte 输出 Buffer；不得为空
 * @return PLATFORM_ERR_OK 成功；其他值表示参数或字段错误。
 * @note 输出中的 commit marker 固定为 FIRMWARE_METADATA_INVALID_MARKER。
 *       下一次正常提交会将读取到的 V1 Metadata 自然编码为 V2。
 */
platform_error_t firmware_metadata_encode_uncommitted(
    const firmware_metadata_t *metadata,
    uint8_t raw[FIRMWARE_METADATA_COPY_SIZE]);
/**
 * @brief 验证并解码已提交的 Metadata V1/V2 副本
 * @param[in] raw : 固定 128 Byte 输入 Buffer；不得为空
 * @param[out] metadata : 解码输出；不得为空
 * @return PLATFORM_ERR_OK 成功；其他值表示格式、字段、CRC 或提交标记错误。
 * @note V1 副本读取后将 pendingSlot 与 upgradeState 初始化为 NONE，不主动写回 EEPROM。
 */
platform_error_t firmware_metadata_decode_committed(
    const uint8_t raw[FIRMWARE_METADATA_COPY_SIZE],
    firmware_metadata_t *metadata);
/**
 * @brief 使用 uint32_t 环绕规则比较两个 Metadata 序列号
 * @param[in] candidate : 待判断序列号
 * @param[in] reference : 当前参考序列号
 * @return 非零表示 candidate 较新；相等或相差 0x80000000 时返回 0。
 */
platform_bool_t firmware_metadata_sequence_is_newer(uint32_t candidate, uint32_t reference);
/**
 * @brief 从两个 Metadata Copy 中选择最新有效副本
 * @param[in] copyA : Copy A 的固定 128 Byte Buffer；不得为空
 * @param[in] copyB : Copy B 的固定 128 Byte Buffer；不得为空
 * @param[out] metadata : 被选择的 Metadata 输出；不得为空
 * @param[out] selectedCopy : 被选择副本的标识；不得为空
 * @return PLATFORM_ERR_OK 成功；两个副本均无效时返回 PLATFORM_ERR_NOT_FOUND。
 * @note 两个有效副本 sequence 相同时固定选择 Copy A，保证结果确定。
 */
platform_error_t firmware_metadata_select_latest(
    const uint8_t copyA[FIRMWARE_METADATA_COPY_SIZE],
    const uint8_t copyB[FIRMWARE_METADATA_COPY_SIZE],
    firmware_metadata_t *metadata,
    firmware_metadata_copy_id_t *selectedCopy);
//******************************** Declaring *******************************//

#endif
