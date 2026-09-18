/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_prevalidate.h
 * @brief S09 Candidate 破坏性操作前预校验接口。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef BOOT_PREVALIDATE_H
#define BOOT_PREVALIDATE_H

//******************************** Includes *********************************//
#include "boot_at24c02.h"
#include "boot_image.h"
#include "boot_metadata.h"
#include "boot_validate.h"
#include "boot_w25q64.h"
//******************************** Includes *********************************//

//******************************** Types ***********************************//
/**
 * @brief Candidate 破坏性操作前预校验结果。
 */
typedef enum
{
    BOOT_PREVALIDATE_INVALID = 0,
    BOOT_PREVALIDATE_METADATA_READ_FAILED,
    BOOT_PREVALIDATE_METADATA_INVALID,
    BOOT_PREVALIDATE_NO_PENDING,
    BOOT_PREVALIDATE_PENDING_SLOT_INVALID,
    BOOT_PREVALIDATE_HEADER_INVALID,
    BOOT_PREVALIDATE_SIZE_INVALID,
    BOOT_PREVALIDATE_EXTERNAL_READ_FAILED,
    BOOT_PREVALIDATE_PAYLOAD_CRC_INVALID,
    BOOT_PREVALIDATE_VECTOR_INVALID,
    BOOT_PREVALIDATE_VALID
} boot_prevalidate_result_t;

/**
 * @brief Candidate 预校验所需的外部设备引用。
 * @note 设备必须在调用前按 Bus → Device 顺序初始化。
 */
typedef struct
{
    /** W25Q64 只读设备；由调用者拥有。 */
    boot_w25q64_t *flash;
    /** AT24C02 Metadata 设备；由调用者拥有。 */
    boot_at24c02_t *eeprom;
} boot_prevalidate_context_t;

/**
 * @brief 预校验成功后供 Installer 使用的 Candidate 快照。
 */
typedef struct
{
    /** 预校验时选择的最新 Metadata。 */
    boot_firmware_metadata_t metadata;
    /** 预校验时使用的 Metadata Copy。 */
    boot_metadata_copy_id_t metadataCopy;
    /** Metadata.pendingSlot 指定的 Candidate Slot。 */
    boot_firmware_slot_t candidateSlot;
    /** Candidate Header V1。 */
    boot_firmware_image_header_t header;
    /** Candidate Payload 的 MSP/Reset_Handler。 */
    boot_app_vector_t vector;
} boot_candidate_t;
//******************************** Types ***********************************//

//******************************** Functions ********************************//
/**
 * @brief 在擦除 Internal APP 前完成 Candidate 全量预校验。
 * @param[in] context : 已初始化的 W25Q64 和 AT24C02 引用。
 * @param[out] candidate : 校验成功时输出 Candidate 快照；失败时内容不可用。
 * @return 预校验结果；本函数不调用 Internal Flash 擦除或写入 API。
 * @note 会检查 Metadata PENDING、Header、Payload CRC 以及 MSP/Reset_Handler。
 */
boot_prevalidate_result_t boot_prevalidate_candidate(
    const boot_prevalidate_context_t *context,
    boot_candidate_t *candidate);
//******************************** Functions ********************************//

#endif
