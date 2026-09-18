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
typedef enum
{
    BOOT_METADATA_COPY_NONE = 0,
    BOOT_METADATA_COPY_A,
    BOOT_METADATA_COPY_B
} boot_metadata_copy_id_t;
//******************************** Types ***********************************//

//******************************** Functions ********************************//
boot_contract_status_t boot_metadata_encode_uncommitted(
    const boot_firmware_metadata_t *metadata,
    uint8_t raw[BOOT_METADATA_COPY_SIZE]);
boot_contract_status_t boot_metadata_decode_committed(
    const uint8_t raw[BOOT_METADATA_COPY_SIZE],
    boot_firmware_metadata_t *metadata);
uint8_t boot_metadata_sequence_is_newer(uint32_t candidate, uint32_t reference);
boot_contract_status_t boot_metadata_select_latest(
    const uint8_t copyA[BOOT_METADATA_COPY_SIZE],
    const uint8_t copyB[BOOT_METADATA_COPY_SIZE],
    boot_firmware_metadata_t *metadata,
    boot_metadata_copy_id_t *selectedCopy);
//******************************** Functions ********************************//

#endif
