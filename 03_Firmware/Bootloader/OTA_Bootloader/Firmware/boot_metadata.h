/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_metadata.h
 * @brief Bootloader Metadata V1/V2 固定格式访问接口。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef BOOT_METADATA_H
#define BOOT_METADATA_H

//******************************** Includes *********************************//
#include "boot_firmware_def.h"
//******************************** Includes *********************************//

//******************************** Types ***********************************//
/**
 * @brief 被选择的 Metadata Copy 标识。
 */
typedef enum
{
    BOOT_METADATA_COPY_NONE = 0,
    BOOT_METADATA_COPY_A,
    BOOT_METADATA_COPY_B
} boot_metadata_copy_id_t;
//******************************** Types ***********************************//

//******************************** Functions ********************************//
/**
 * @brief 编码尚未提交的 Metadata V2 副本。
 * @param[in] metadata : Metadata；字段必须符合 S07 V2 合同。
 * @param[out] raw : 128 Byte 输出 Buffer；不得为空。
 * @return Bootloader 合同状态；commit marker 固定保持 invalid。
 */
boot_contract_status_t boot_metadata_encode_uncommitted(
    const boot_firmware_metadata_t *metadata,
    uint8_t raw[BOOT_METADATA_COPY_SIZE]);
/**
 * @brief 验证并解码已提交的 Metadata V1/V2 副本。
 * @param[in] raw : 128 Byte 输入 Buffer；不得为空。
 * @param[out] metadata : 解码输出；不得为空。
 * @return Bootloader 合同状态；会检查 marker、保留区、字段和 CRC。
 */
boot_contract_status_t boot_metadata_decode_committed(
    const uint8_t raw[BOOT_METADATA_COPY_SIZE],
    boot_firmware_metadata_t *metadata);
/**
 * @brief 按 uint32_t 环绕规则比较两个 Metadata sequence。
 * @param[in] candidate : 待判断序列号。
 * @param[in] reference : 当前参考序列号。
 * @return 非零表示 candidate 较新。
 */
uint8_t boot_metadata_sequence_is_newer(uint32_t candidate, uint32_t reference);
/**
 * @brief 从两个 Copy 中选择最新有效副本。
 * @param[in] copyA : Copy A 的 128 Byte raw buffer。
 * @param[in] copyB : Copy B 的 128 Byte raw buffer。
 * @param[out] metadata : 被选择的 Metadata。
 * @param[out] selectedCopy : 被选择的 Copy 标识。
 * @return Bootloader 合同状态；两个副本均无效时返回 NOT_FOUND。
 */
boot_contract_status_t boot_metadata_select_latest(
    const uint8_t copyA[BOOT_METADATA_COPY_SIZE],
    const uint8_t copyB[BOOT_METADATA_COPY_SIZE],
    boot_firmware_metadata_t *metadata,
    boot_metadata_copy_id_t *selectedCopy);
//******************************** Functions ********************************//

#endif
