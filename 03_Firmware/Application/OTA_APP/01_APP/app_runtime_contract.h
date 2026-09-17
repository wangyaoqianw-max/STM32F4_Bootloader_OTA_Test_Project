/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_runtime_contract.h
 * @brief S06 Application Runtime 任务间最小数据契约
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_RUNTIME_CONTRACT_H
#define APP_RUNTIME_CONTRACT_H

//******************************** Includes *********************************//
#include "platform_os.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define APP_OTA_NOTIFY_UART_RX       (1U << 0)
#define APP_OTA_NOTIFY_START         (1U << 1)
#define APP_OTA_NOTIFY_CANCEL        (1U << 2)
#define APP_OTA_NOTIFY_SHUTDOWN      (1U << 3)
#define APP_OTA_NOTIFY_KEY_1         (1U << 4)

#define APP_DISPLAY_FIRMWARE_VERSION_BUFFER_SIZE  (16U)
#define APP_DISPLAY_SYSTEM_STATE_BUFFER_SIZE      (16U)
//******************************** Defines **********************************//

//******************************** Types ************************************//
/** @brief otaWorker 发往 displayTask 的业务语义事件类型。 */
typedef enum
{
    APP_DISPLAY_EVENT_OTA_IDLE = 0U,
    APP_DISPLAY_EVENT_OTA_RECEIVING,
    APP_DISPLAY_EVENT_OTA_VERIFYING,
    APP_DISPLAY_EVENT_OTA_SUCCESS,
    APP_DISPLAY_EVENT_OTA_FAILED,
    APP_DISPLAY_EVENT_MAX
} app_display_event_type_t;

/** @brief Display Model 当前固定目标 Slot。 */
typedef enum
{
    APP_DISPLAY_TARGET_SLOT_UNKNOWN = 0U,
    APP_DISPLAY_TARGET_SLOT_A,
    APP_DISPLAY_TARGET_SLOT_B,
    APP_DISPLAY_TARGET_SLOT_MAX
} app_display_target_slot_t;

/**
 * @brief Display Queue 中传递的紧凑 OTA 事件。
 * @note 不携带 YMODEM 原始 Packet、Flash 原始地址或 UART DMA 内部状态。
 */
typedef struct
{
    app_display_event_type_t type;
    uint32_t progress;
    uint32_t imageSize;
    platform_error_t errorCode;
} app_display_event_t;

/**
 * @brief displayTask 私有显示模型。
 * @note 模型由 displayTask 独占维护，其他 Task 只能发送 Display Event。
 */
typedef struct
{
    char_t firmwareVersion[APP_DISPLAY_FIRMWARE_VERSION_BUFFER_SIZE];
    char_t systemState[APP_DISPLAY_SYSTEM_STATE_BUFFER_SIZE];
    app_display_event_type_t otaState;
    uint32_t otaProgress;
    app_display_target_slot_t targetSlot;
    platform_error_t lastError;
} display_model_t;
//******************************** Types ************************************//

#endif
