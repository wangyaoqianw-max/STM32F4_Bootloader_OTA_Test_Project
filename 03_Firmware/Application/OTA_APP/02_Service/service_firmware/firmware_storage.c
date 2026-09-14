/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file firmware_storage.c
 * @brief Firmware Storage Service 实现
 * @author YaoQian Wang
 * @date 2026-09-13
 * @version V1.0
 *
 *****************************************************************************/

#include <stddef.h>
#include <string.h>

//******************************** Includes *********************************//
#include "crc.h"
#include "firmware_storage.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define FIRMWARE_STORAGE_CRC_BUFFER_SIZE       (256U)
//******************************** Defines **********************************//

//******************************** Private Functions ************************//

/**
 * @brief 解析固定 Slot 基地址
 * @note 仅接受 A/B，NONE 不能映射到物理存储地址。
 */
static platform_error_t firmware_storage_get_slot_base(firmware_slot_t slot, uint32_t *slotBase)
{
    if (slotBase == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (slot == FIRMWARE_SLOT_A) {
        *slotBase = FIRMWARE_SLOT_A_BASE;
        return PLATFORM_ERR_OK;
    }

    if (slot == FIRMWARE_SLOT_B) {
        *slotBase = FIRMWARE_SLOT_B_BASE;
        return PLATFORM_ERR_OK;
    }

    return PLATFORM_ERR_INVALID_PARAM;
}

/**
 * @brief 检查 Service 依赖是否已由调用者绑定
 * @note 本 Service 不拥有 Raw Driver 或底层总线生命周期。
 */
static platform_error_t firmware_storage_check_ready(const firmware_storage_t *storage)
{
    if (storage == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if ((storage->initialized == 0U) || (storage->flash == NULL) || (storage->eeprom == NULL)) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    return PLATFORM_ERR_OK;
}

//******************************** Public Functions *************************//
platform_error_t firmware_storage_init(
    firmware_storage_t *storage,
    platform_w25q64_t *flash,
    platform_at24c02_t *eeprom)
{
    if ((storage == NULL) || (flash == NULL) || (eeprom == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    storage->flash = flash;
    storage->eeprom = eeprom;
    storage->initialized = (platform_bool_t)1U;
    return PLATFORM_ERR_OK;
}

platform_error_t firmware_storage_read_header(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    firmware_image_header_t *header,
    firmware_image_validation_t *validation)
{
    uint8_t rawHeader[FIRMWARE_IMAGE_HEADER_SIZE];
    uint32_t slotBase;
    platform_error_t result;

    if ((header == NULL) || (validation == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    *validation = FIRMWARE_IMAGE_VALIDATION_UNKNOWN;
    result = firmware_storage_check_ready(storage);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = firmware_storage_get_slot_base(slot, &slotBase);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_w25q64_read(storage->flash, slotBase, rawHeader, sizeof(rawHeader));
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    *validation = firmware_image_validate_header(rawHeader, header);
    return PLATFORM_ERR_OK;
}

platform_error_t firmware_storage_validate_image(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    firmware_image_header_t *header,
    firmware_image_validation_t *validation)
{
    crc32_iso_hdlc_context_t crcContext;
    uint8_t buffer[FIRMWARE_STORAGE_CRC_BUFFER_SIZE];
    uint32_t remaining;
    uint32_t address;
    uint32_t readLength;
    platform_error_t result;

    result = firmware_storage_read_header(storage, slot, header, validation);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if (*validation != FIRMWARE_IMAGE_VALIDATION_VALID) {
        return PLATFORM_ERR_OK;
    }

    address = (slot == FIRMWARE_SLOT_A ? FIRMWARE_SLOT_A_BASE : FIRMWARE_SLOT_B_BASE) + FIRMWARE_PAYLOAD_OFFSET;
    remaining = header->imageSize;
    crc32_iso_hdlc_init(&crcContext);
    while (remaining != 0U) {
        readLength = (remaining > FIRMWARE_STORAGE_CRC_BUFFER_SIZE) ? FIRMWARE_STORAGE_CRC_BUFFER_SIZE : remaining;
        result = platform_w25q64_read(storage->flash, address, buffer, readLength);
        if (result != PLATFORM_ERR_OK) {
            *validation = FIRMWARE_IMAGE_VALIDATION_UNKNOWN;
            return result;
        }

        crc32_iso_hdlc_update(&crcContext, buffer, readLength);
        address += readLength;
        remaining -= readLength;
    }

    if (crc32_iso_hdlc_finalize(&crcContext) != header->payloadCrc32) {
        *validation = FIRMWARE_IMAGE_VALIDATION_INVALID_PAYLOAD_CRC;
    }

    return PLATFORM_ERR_OK;
}

platform_error_t firmware_storage_load_metadata(
    firmware_storage_t *storage,
    firmware_metadata_t *metadata,
    firmware_metadata_copy_id_t *sourceCopy)
{
    uint8_t copyA[FIRMWARE_METADATA_COPY_SIZE];
    uint8_t copyB[FIRMWARE_METADATA_COPY_SIZE];
    platform_error_t result;

    if ((metadata == NULL) || (sourceCopy == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    result = firmware_storage_check_ready(storage);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_at24c02_read(storage->eeprom, FIRMWARE_METADATA_COPY_A_ADDRESS, copyA, sizeof(copyA));
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_at24c02_read(storage->eeprom, FIRMWARE_METADATA_COPY_B_ADDRESS, copyB, sizeof(copyB));
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return firmware_metadata_select_latest(copyA, copyB, metadata, sourceCopy);
}

platform_error_t firmware_storage_commit_metadata(
    firmware_storage_t *storage,
    const firmware_metadata_t *metadata,
    firmware_metadata_copy_id_t *committedCopy)
{
    firmware_metadata_t currentMetadata;
    firmware_metadata_t targetMetadata;
    firmware_metadata_copy_id_t sourceCopy;
    firmware_metadata_copy_id_t targetCopy;
    uint8_t raw[FIRMWARE_METADATA_COPY_SIZE];
    uint8_t readBack[FIRMWARE_METADATA_COPY_SIZE];
    uint32_t targetAddress;
    uint8_t marker[4] = {0x43U, 0x4DU, 0x49U, 0x54U};
    uint8_t invalidMarker[4] = {0xFFU, 0xFFU, 0xFFU, 0xFFU};
    platform_error_t result;

    if ((metadata == NULL) || (committedCopy == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    result = firmware_storage_check_ready(storage);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    targetMetadata = *metadata;
    result = firmware_storage_load_metadata(storage, &currentMetadata, &sourceCopy);
    if (result == PLATFORM_ERR_OK) {
        targetMetadata.sequence = currentMetadata.sequence + 1U;
        targetCopy = (sourceCopy == FIRMWARE_METADATA_COPY_A) ?
                     FIRMWARE_METADATA_COPY_B : FIRMWARE_METADATA_COPY_A;
    } else if (result == PLATFORM_ERR_NOT_FOUND) {
        targetCopy = FIRMWARE_METADATA_COPY_A;
    } else {
        return result;
    }

    targetAddress = (targetCopy == FIRMWARE_METADATA_COPY_A) ?
                    FIRMWARE_METADATA_COPY_A_ADDRESS : FIRMWARE_METADATA_COPY_B_ADDRESS;
    result = firmware_metadata_encode_uncommitted(&targetMetadata, raw);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_at24c02_write(storage->eeprom, targetAddress + 0x7CU,
                                    invalidMarker, sizeof(invalidMarker));
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_at24c02_write(storage->eeprom, targetAddress, raw, 0x7CU);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_at24c02_read(storage->eeprom, targetAddress, readBack, sizeof(readBack));
    if ((result != PLATFORM_ERR_OK) || (memcmp(raw, readBack, 0x7CU) != 0)) {
        return (result != PLATFORM_ERR_OK) ? result : PLATFORM_ERR_IO;
    }

    result = platform_at24c02_write(storage->eeprom, targetAddress + 0x7CU,
                                    marker, sizeof(marker));
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_at24c02_read(storage->eeprom, targetAddress + 0x7CU,
                                   &readBack[0x7CU], sizeof(marker));
    if ((result != PLATFORM_ERR_OK) ||
        (memcmp(marker, &readBack[0x7CU], sizeof(marker)) != 0)) {
        return (result != PLATFORM_ERR_OK) ? result : PLATFORM_ERR_IO;
    }

    *committedCopy = targetCopy;
    return PLATFORM_ERR_OK;
}

platform_error_t firmware_storage_erase_slot(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    uint32_t payloadSize)
{
    uint32_t slotBase;
    uint32_t sectorCount;
    uint32_t index;
    platform_error_t result;

    result = firmware_storage_check_ready(storage);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = firmware_storage_get_slot_base(slot, &slotBase);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    if ((payloadSize == 0U) || (payloadSize > FIRMWARE_SLOT_PAYLOAD_CAPACITY)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    sectorCount = 1U + ((payloadSize + FIRMWARE_HEADER_SECTOR_SIZE - 1U) / FIRMWARE_HEADER_SECTOR_SIZE);
    for (index = 0U; index < sectorCount; index++) {
        result = platform_w25q64_sector_erase(storage->flash, slotBase + (index * FIRMWARE_HEADER_SECTOR_SIZE));
        if (result != PLATFORM_ERR_OK) {
            return result;
        }
    }

    return PLATFORM_ERR_OK;
}
