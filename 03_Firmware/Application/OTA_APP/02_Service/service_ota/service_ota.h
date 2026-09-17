/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file service_ota.h
 * @brief OTA Service 状态机与升级事务公共接口
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

#ifndef SERVICE_OTA_H
#define SERVICE_OTA_H

//******************************** Includes *********************************//
#include <stdint.h>

#include "firmware_storage.h"
#include "ota_firmware_sink.h"
#include "service_uart.h"
#include "ymodem_receiver.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define SERVICE_OTA_INITIALIZER             {0}
//******************************** Defines **********************************//

//******************************** Types ***********************************//
/** @brief OTA Service 当前业务状态。 */
typedef enum
{
    SERVICE_OTA_STATE_IDLE = 0, /**< 已初始化，等待第一次确认按键。 */
    SERVICE_OTA_STATE_PREPARING, /**< 正在检查 Metadata 并准备目标 Slot。 */
    SERVICE_OTA_STATE_RECEIVING, /**< 正在通过 YMODEM 接收镜像。 */
    SERVICE_OTA_STATE_VERIFYING, /**< YMODEM 完成，正在验证 Slot 镜像。 */
    SERVICE_OTA_STATE_READY_TO_INSTALL, /**< 镜像有效，等待第二次确认。 */
    SERVICE_OTA_STATE_COMMITTING, /**< 正在提交 PENDING Metadata。 */
    SERVICE_OTA_STATE_REBOOT_REQUIRED, /**< PENDING 已提交，等待执行层复位。 */
    SERVICE_OTA_STATE_FAILED /**< 当前 Session 失败，可重新 start。 */
} service_ota_state_t;

/** @brief OTA Service 向执行层报告的业务事件。 */
typedef enum
{
    SERVICE_OTA_EVENT_NONE = 0,
    SERVICE_OTA_EVENT_STARTED, /**< 已完成 Metadata 准备并开始等待 YMODEM。 */
    SERVICE_OTA_EVENT_PROGRESS, /**< 已发布一笔节流后的接收进度。 */
    SERVICE_OTA_EVENT_VERIFYING, /**< YMODEM 完成，开始镜像验证。 */
    SERVICE_OTA_EVENT_READY_TO_INSTALL, /**< 镜像已验证并等待第二次确认。 */
    SERVICE_OTA_EVENT_RESET_REQUIRED, /**< PENDING 已提交，执行层可以复位。 */
    SERVICE_OTA_EVENT_FAILED /**< 当前 OTA Session 已失败。 */
} service_ota_event_t;

/** @brief OTA Service 初始化依赖；对象生命周期由调用者持有。 */
typedef struct
{
    /** 已初始化且由调用者持有的 Firmware Storage Service。 */
    firmware_storage_t *storage;
    /** 已初始化且由调用者持有的 UART Service。 */
    service_uart_t *uart;
} service_ota_config_t;

/** @brief OTA Service 当前运行上下文。 */
typedef struct
{
    /** 当前业务状态。 */
    service_ota_state_t state;
    /** 尚未消费的业务事件。 */
    service_ota_event_t event;
    /** 最近一次错误；成功运行时为 PLATFORM_ERR_OK。 */
    platform_error_t lastError;
    /** 当前接收目标 Slot。 */
    firmware_slot_t targetSlot;
    /** 当前已接收的有效文件字节数。 */
    uint32_t receivedBytes;
    /** YMODEM Block 0 声明的文件总字节数。 */
    uint32_t expectedBytes;
    /** 当前节流进度，范围为 0~100。 */
    uint32_t progressPercent;
    /** 上一次发布进度事件的毫秒时间戳。 */
    uint32_t lastProgressMs;
    /** 上一次发布进度事件的百分比。 */
    uint32_t lastProgressPercent;
    /** 当前 Service 使用的 Metadata 快照。 */
    firmware_metadata_t metadata;
    /** 当前 Service 使用的 Metadata 副本标识。 */
    firmware_metadata_copy_id_t metadataCopy;
} service_ota_context_t;

/** @brief OTA Service 对象；不创建 Task，也不拥有外部依赖。 */
typedef struct
{
    /** 初始化后不变的依赖配置。 */
    service_ota_config_t config;
    /** 当前状态机、Metadata 与进度上下文。 */
    service_ota_context_t context;
    /** 绑定 Firmware Storage 的生产 Sink。 */
    ota_firmware_sink_t sink;
    /** 驱动 YMODEM 文件接收的协议对象。 */
    ymodem_receiver_t receiver;
    /** Receiver 初始化时使用的配置副本。 */
    ymodem_receiver_config_t receiverConfig;
} service_ota_t;

/** @brief OTA Service 对外状态快照。 */
typedef struct
{
    service_ota_state_t state; /**< 当前业务状态。 */
    service_ota_event_t event; /**< 当前尚未消费的业务事件。 */
    platform_error_t lastError; /**< 最近一次错误。 */
    firmware_slot_t targetSlot; /**< 当前接收目标 Slot。 */
    uint32_t receivedBytes; /**< 已接收的有效文件字节数。 */
    uint32_t expectedBytes; /**< Block 0 声明的文件总字节数。 */
    uint32_t progressPercent; /**< 当前节流进度，范围为 0~100。 */
} service_ota_status_t;
//******************************** Types ***********************************//

//******************************** Declaring *******************************//
/**
 * @brief 初始化 OTA Service 并绑定 Storage、UART 和生产 Sink
 * @param[out] service : 使用 SERVICE_OTA_INITIALIZER 初始化的 Service 对象
 * @param[in] config : 调用者持有的 Storage 与 UART 依赖
 * @return PLATFORM_ERR_OK 成功；其他值表示参数或依赖绑定错误。
 * @note 本函数不创建 Task、不启动 UART、不执行擦除或 Metadata 提交。
 */
platform_error_t service_ota_init(
    service_ota_t *service,
    const service_ota_config_t *config);

/**
 * @brief 开始一次新的 OTA 接收 Session
 * @param[in,out] service : 已初始化的 OTA Service
 * @return PLATFORM_ERR_OK 成功；其他值表示状态、Metadata 或 UART 错误。
 * @note 只允许稳定 Metadata 状态或 FAILED 状态重试；开始擦除前先将目标 Slot 提交为 INVALID。
 */
platform_error_t service_ota_start(service_ota_t *service);

/**
 * @brief 驱动 OTA Service 处理一段收到的 UART 数据
 * @param[in,out] service : 正在 RECEIVING 的 OTA Service
 * @param[in] data : UART 输入数据；dataLength 为 0 时可为空
 * @param[in] dataLength : 输入字节数
 * @param[in] nowMs : 调用者提供的单调毫秒时间戳
 * @return PLATFORM_ERR_OK 成功；其他值表示状态、协议、存储或验证错误。
 * @note 本函数不读取 UART RingBuffer，不执行 RTOS 等待，也不直接复位 MCU。
 */
platform_error_t service_ota_process(
    service_ota_t *service,
    const uint8_t *data,
    uint32_t dataLength,
    uint32_t nowMs);

/**
 * @brief 确认安装已验证镜像并提交 PENDING Metadata
 * @param[in,out] service : 状态为 READY_TO_INSTALL 的 OTA Service
 * @return PLATFORM_ERR_OK 成功；其他值表示状态或 Metadata 提交错误。
 * @note 成功仅报告 REBOOT_REQUIRED，不在 Service 内直接调用复位实现。
 */
platform_error_t service_ota_confirm_install(service_ota_t *service);

/**
 * @brief 获取 OTA Service 当前状态快照
 * @param[in] service : 已初始化的 OTA Service
 * @param[out] status : 状态快照输出
 * @return PLATFORM_ERR_OK 成功；其他值表示参数错误。
 */
platform_error_t service_ota_get_status(
    const service_ota_t *service,
    service_ota_status_t *status);

/**
 * @brief 清除当前已读取的业务事件
 * @param[in,out] service : 已初始化的 OTA Service
 * @return PLATFORM_ERR_OK 成功；其他值表示参数错误。
 * @note 清除事件不改变状态、Metadata 或最近一次错误。
 */
platform_error_t service_ota_clear_event(service_ota_t *service);
//******************************** Declaring *******************************//

#endif
