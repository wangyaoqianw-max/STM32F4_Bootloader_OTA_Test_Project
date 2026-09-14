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
/**
 * @brief 仅读取并验证指定 Slot 的 64 Byte Header
 * @param[in,out] storage : 已初始化的 Storage Service
 * @param[in] slot : 目标 Slot，只允许 Slot A 或 Slot B
 * @param[out] header : 解码后的 Header 输出；不得为空
 * @param[out] validation : Header 验证结果输出；不得为空
 * @return PLATFORM_ERR_OK 表示读取流程完成；其他值表示底层 I/O 或参数错误。
 * @note 本接口不读取 Payload、不修改 EEPROM；I/O 失败时 validation 保持 UNKNOWN。
 */
platform_error_t firmware_storage_read_header(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    firmware_image_header_t *header,
    firmware_image_validation_t *validation);
/**
 * @brief 完整验证指定 Slot 的 Header 与 Payload
 * @param[in,out] storage : 已初始化的 Storage Service
 * @param[in] slot : 目标 Slot，只允许 Slot A 或 Slot B
 * @param[out] header : 已验证 Header 输出；不得为空
 * @param[out] validation : 镜像验证结果输出；不得为空
 * @return PLATFORM_ERR_OK 表示验证流程完成；其他值表示底层 I/O 或参数错误。
 * @note Payload 使用固定小 Buffer 流式 CRC；本接口是只读操作，不提交 Metadata。
 */
platform_error_t firmware_storage_validate_image(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    firmware_image_header_t *header,
    firmware_image_validation_t *validation);
/**
 * @brief 读取 EEPROM 双副本并选择最新有效 Metadata
 * @param[in,out] storage : 已初始化的 Storage Service
 * @param[out] metadata : 选择出的 Metadata 输出；不得为空
 * @param[out] sourceCopy : 被选择副本输出；不得为空
 * @return PLATFORM_ERR_OK 成功；无有效副本返回 PLATFORM_ERR_NOT_FOUND。
 */
platform_error_t firmware_storage_load_metadata(
    firmware_storage_t *storage,
    firmware_metadata_t *metadata,
    firmware_metadata_copy_id_t *sourceCopy);
/**
 * @brief 原子提交一份新的 Metadata
 * @param[in,out] storage : 已初始化的 Storage Service
 * @param[in] metadata : 待提交业务状态；不得为空，sequence 在已有副本时由本接口递增
 * @param[out] committedCopy : 实际提交目标副本输出；不得为空
 * @return PLATFORM_ERR_OK 成功；其他值表示 I/O、格式或参数错误。
 * @note 提交流程固定为失效 marker、Body/CRC、回读、最终 committed marker、再次回读。
 */
platform_error_t firmware_storage_commit_metadata(
    firmware_storage_t *storage,
    const firmware_metadata_t *metadata,
    firmware_metadata_copy_id_t *committedCopy);
/**
 * @brief 擦除指定 Slot 的 Header Sector 和 Payload 所需 Sector
 * @param[in,out] storage : 已初始化的 Storage Service
 * @param[in] slot : 目标 Slot，只允许 Slot A 或 Slot B
 * @param[in] payloadSize : 将要写入的 Payload 字节数，范围为 1 至 Slot Payload Capacity
 * @return PLATFORM_ERR_OK 成功；其他值表示参数或底层擦除错误。
 * @note 所有范围计算在第一笔擦除前完成。
 */
platform_error_t firmware_storage_erase_slot(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    uint32_t payloadSize);

//******************************** Declaring *******************************//

#endif
