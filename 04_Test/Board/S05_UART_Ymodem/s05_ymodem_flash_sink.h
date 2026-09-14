/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s05_ymodem_flash_sink.h
 * @brief S05 Slot B YMODEM Flash Sink 公共接口
 * @author YaoQian Wang
 * @date 2026-09-14
 * @version V1.0
 *
 *****************************************************************************/

#ifndef S05_YMODEM_FLASH_SINK_H
#define S05_YMODEM_FLASH_SINK_H

//******************************** Includes *********************************//
#include "firmware_storage.h"
#include "ymodem_config.h"
#include "ymodem_sink.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define S05_YMODEM_FLASH_SINK_INITIALIZER    {0}
//******************************** Defines **********************************//

//******************************** Types ***********************************//
/** @brief S05 将 YMODEM 文件流转换为 Slot B Header/Payload 写入的上下文。 */
typedef struct
{
    firmware_storage_t *storage; /**< 已初始化的 Firmware Storage Service。 */
    firmware_slot_t slot; /**< 固定写入的目标 Slot，必须为 FIRMWARE_SLOT_B。 */
    uint32_t expectedFileSize; /**< YMODEM Block 0 声明的完整文件长度。 */
    uint32_t receivedFileBytes; /**< 已接收的 Header 与 Payload 总字节数。 */
    uint8_t headerBuffer[FIRMWARE_IMAGE_HEADER_SIZE]; /**< 尚未提交的原始 Header。 */
    uint32_t headerFillCount; /**< Header Buffer 已填充字节数。 */
    firmware_image_header_t header; /**< 已验证的 Header 解码结果。 */
    uint32_t payloadWritten; /**< 已写入 Slot Payload 区域的有效字节数。 */
    platform_bool_t headerValidated; /**< Header 已验证且 Slot 已完成擦除。 */
    platform_bool_t headerCommitted; /**< Header 已在 Payload 完成后成功写入。 */
    platform_bool_t started; /**< begin 已成功且尚未 end/abort。 */
    platform_bool_t failed; /**< 当前传输已发生不可恢复错误。 */
    platform_error_t lastError; /**< 最近一次 Sink 错误。 */
    char filename[YMODEM_CFG_FILENAME_MAX_LEN + 1U]; /**< 仅用于诊断的文件名。 */
} s05_ymodem_flash_sink_t;
//******************************** Types ***********************************//

//******************************** Declaring *******************************//
/**
 * @brief 初始化固定写入 Slot B 的 S05 Flash Sink
 * @param[out] sink : Sink 上下文
 * @param[in] storage : 调用者持有的已初始化 Storage Service
 * @return PLATFORM_ERR_OK 成功；其他值表示参数错误。
 */
platform_error_t s05_ymodem_flash_sink_init(
    s05_ymodem_flash_sink_t *sink,
    firmware_storage_t *storage);
/**
 * @brief 获取绑定到 S05 Flash Sink 的 YMODEM Sink 合同
 * @param[in] sink : 已初始化的 Sink 上下文
 * @param[out] contract : Receiver 使用的回调合同
 * @return PLATFORM_ERR_OK 成功；其他值表示参数或状态错误。
 */
platform_error_t s05_ymodem_flash_sink_get_contract(
    s05_ymodem_flash_sink_t *sink,
    ymodem_sink_t *contract);
//******************************** Declaring *******************************//

#endif
