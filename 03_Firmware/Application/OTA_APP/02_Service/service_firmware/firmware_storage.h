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

//******************************** Includes *********************************//
#include "firmware_image.h"
#include "firmware_metadata.h"
#include "platform_at24c02.h"
#include "platform_w25q64.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define FIRMWARE_STORAGE_INITIALIZER       {0}
//******************************** Defines **********************************//

//******************************** Types ***********************************//
/** @brief Firmware Storage Service 对象；底层 Driver 与总线生命周期由调用者持有。 */
typedef struct
{
    /** 已初始化且由调用者持有的 W25Q64 Raw Driver。 */
    platform_w25q64_t *flash;
    /** 已初始化且由调用者持有的 AT24C02 Raw Driver。 */
    platform_at24c02_t *eeprom;
    /** Service 是否已完成依赖绑定。 */
    platform_bool_t initialized;
} firmware_storage_t;
//******************************** Types ***********************************//

//******************************** Declaring *******************************//
/**
 * @brief 绑定已初始化的 W25Q64 与 AT24C02 Raw Driver
 * @param[in,out] storage : 使用 FIRMWARE_STORAGE_INITIALIZER 初始化的对象
 * @param[in] flash : 调用者持有的已初始化 Flash Driver
 * @param[in] eeprom : 调用者持有的已初始化 EEPROM Driver
 * @return PLATFORM_ERR_OK 成功；其他值表示空指针错误。
 */
platform_error_t firmware_storage_init(
    firmware_storage_t *storage,
    platform_w25q64_t *flash,
    platform_at24c02_t *eeprom);
/** @brief 仅读取并验证指定 Slot 的 Header；I/O 失败时 validation 保持 UNKNOWN。 */
platform_error_t firmware_storage_read_header(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    firmware_image_header_t *header,
    firmware_image_validation_t *validation);
/** @brief 分块重读 Payload 并校验 CRC；本接口不提交 Metadata。 */
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
/** @brief 原子提交 Metadata；目标副本始终最后写入 committed marker。 */
platform_error_t firmware_storage_commit_metadata(
    firmware_storage_t *storage,
    const firmware_metadata_t *metadata,
    firmware_metadata_copy_id_t *committedCopy);
/** @brief 擦除 Header Sector 与容纳 payloadSize 所需的 Payload Sector。 */
platform_error_t firmware_storage_erase_slot(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    uint32_t payloadSize);

//******************************** Declaring *******************************//

#endif
