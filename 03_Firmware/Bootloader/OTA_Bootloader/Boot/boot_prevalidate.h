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

typedef struct
{
    boot_w25q64_t *flash;
    boot_at24c02_t *eeprom;
} boot_prevalidate_context_t;

typedef struct
{
    boot_firmware_metadata_t metadata;
    boot_metadata_copy_id_t metadataCopy;
    boot_firmware_slot_t candidateSlot;
    boot_firmware_image_header_t header;
    boot_app_vector_t vector;
} boot_candidate_t;
//******************************** Types ***********************************//

//******************************** Functions ********************************//
boot_prevalidate_result_t boot_prevalidate_candidate(
    const boot_prevalidate_context_t *context,
    boot_candidate_t *candidate);
//******************************** Functions ********************************//

#endif
