/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file firmware_storage.h
 * @brief Firmware Storage Service 公共接口
 * @author YaoQian Wang
 * @date 2026-09-13
 * @version V1.0
 *
 *****************************************************************************/

#ifndef FIRMWARE_STORAGE_H
#define FIRMWARE_STORAGE_H

#include "firmware_image.h"
#include "firmware_metadata.h"
#include "platform_at24c02.h"
#include "platform_w25q64.h"

#define FIRMWARE_STORAGE_INITIALIZER       {0}

typedef struct
{
    /** 已初始化且由调用者持有的 W25Q64 Raw Driver。 */
    platform_w25q64_t *flash;
    /** 已初始化且由调用者持有的 AT24C02 Raw Driver。 */
    platform_at24c02_t *eeprom;
    /** Service 是否已完成依赖绑定。 */
    platform_bool_t initialized;
} firmware_storage_t;

/** @brief 绑定已初始化的 W25Q64 与 AT24C02 Raw Driver，不接管其生命周期。 */
platform_error_t firmware_storage_init(
    firmware_storage_t *storage,
    platform_w25q64_t *flash,
    platform_at24c02_t *eeprom);
/** @brief 仅读取并验证指定 Slot 的 64 Byte Header，不修改 Metadata。 */
platform_error_t firmware_storage_read_header(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    firmware_image_header_t *header,
    firmware_image_validation_t *validation);
/** @brief 分块重读指定 Slot Payload，并区分镜像无效与底层 I/O 错误。 */
platform_error_t firmware_storage_validate_image(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    firmware_image_header_t *header,
    firmware_image_validation_t *validation);
/** @brief 读取 EEPROM 双副本并选择最新有效 Metadata。 */
platform_error_t firmware_storage_load_metadata(
    firmware_storage_t *storage,
    firmware_metadata_t *metadata,
    firmware_metadata_copy_id_t *sourceCopy);
/** @brief 按失效标记、Body/CRC、回读、最终标记顺序提交 Metadata。 */
platform_error_t firmware_storage_commit_metadata(
    firmware_storage_t *storage,
    const firmware_metadata_t *metadata,
    firmware_metadata_copy_id_t *committedCopy);
/** @brief 擦除 Header Sector 与容纳 payloadSize 所需的 Payload Sector。 */
platform_error_t firmware_storage_erase_slot(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    uint32_t payloadSize);

#endif
