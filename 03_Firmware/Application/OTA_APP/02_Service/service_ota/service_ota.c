/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file service_ota.c
 * @brief OTA Service 状态机与升级事务实现
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>
#include <string.h>

#include "platform_def.h"
#include "service_ota.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define SERVICE_OTA_PROGRESS_STEP_PERCENT       (5U)
#define SERVICE_OTA_PROGRESS_INTERVAL_MS       (200U)
//******************************** Defines **********************************//

//******************************** Declaring *******************************//
static firmware_slot_state_t *service_ota_get_slot_state(
    firmware_metadata_t *metadata,
    firmware_slot_t slot);
static firmware_slot_t service_ota_get_target_slot(firmware_slot_t confirmedSlot);
static platform_error_t service_ota_load_stable_metadata(service_ota_t *service);
static platform_error_t service_ota_commit_metadata(service_ota_t *service);
static platform_error_t service_ota_fail(
    service_ota_t *service,
    platform_error_t error);
static void service_ota_update_progress(
    service_ota_t *service,
    const ymodem_receiver_status_t *receiverStatus,
    uint32_t nowMs);
static platform_error_t service_ota_finish_receiving(service_ota_t *service);
static platform_bool_t service_ota_receiver_is_active(ymodem_receiver_state_t state);
//******************************** Declaring *******************************//

//******************************** Private Functions ************************//
static firmware_slot_state_t *service_ota_get_slot_state(
    firmware_metadata_t *metadata,
    firmware_slot_t slot)
{
    if (metadata == NULL) {
        return NULL;
    }

    if (slot == FIRMWARE_SLOT_A) {
        return &metadata->slotAState;
    }

    if (slot == FIRMWARE_SLOT_B) {
        return &metadata->slotBState;
    }

    return NULL;
}

static firmware_slot_t service_ota_get_target_slot(firmware_slot_t confirmedSlot)
{
    if (confirmedSlot == FIRMWARE_SLOT_A) {
        return FIRMWARE_SLOT_B;
    }

    if (confirmedSlot == FIRMWARE_SLOT_B) {
        return FIRMWARE_SLOT_A;
    }

    return FIRMWARE_SLOT_NONE;
}

static platform_error_t service_ota_load_stable_metadata(service_ota_t *service)
{
    firmware_slot_state_t *confirmedState;
    platform_error_t result;

    result = firmware_storage_load_metadata(
        service->config.storage,
        &service->context.metadata,
        &service->context.metadataCopy);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    confirmedState = service_ota_get_slot_state(
        &service->context.metadata,
        service->context.metadata.confirmedSlot);
    if ((confirmedState == NULL) ||
        (*confirmedState != FIRMWARE_SLOT_STATE_VALID) ||
        (service->context.metadata.pendingSlot != FIRMWARE_SLOT_NONE) ||
        (service->context.metadata.upgradeState != FIRMWARE_UPGRADE_STATE_NONE)) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    return PLATFORM_ERR_OK;
}

static platform_error_t service_ota_commit_metadata(service_ota_t *service)
{
    firmware_metadata_copy_id_t committedCopy;
    platform_error_t result;

    result = firmware_storage_commit_metadata(
        service->config.storage,
        &service->context.metadata,
        &committedCopy);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    service->context.metadataCopy = committedCopy;

    return firmware_storage_load_metadata(
        service->config.storage,
        &service->context.metadata,
        &service->context.metadataCopy);
}

static platform_error_t service_ota_fail(
    service_ota_t *service,
    platform_error_t error)
{
    if (error == PLATFORM_ERR_OK) {
        error = PLATFORM_ERR_INVALID_STATE;
    }

    service->context.lastError = error;
    service->context.state = SERVICE_OTA_STATE_FAILED;
    service->context.event = SERVICE_OTA_EVENT_FAILED;
    return error;
}

static void service_ota_update_progress(
    service_ota_t *service,
    const ymodem_receiver_status_t *receiverStatus,
    uint32_t nowMs)
{
    uint32_t progressPercent;

    service->context.receivedBytes = receiverStatus->receivedSize;
    service->context.expectedBytes = receiverStatus->block0Metadata.fileSize;
    if (service->context.expectedBytes == 0U) {
        return;
    }

    progressPercent = (uint32_t)(((uint64_t)service->context.receivedBytes * 100U) /
                                 service->context.expectedBytes);
    if (progressPercent > 100U) {
        progressPercent = 100U;
    }
    service->context.progressPercent = progressPercent;

    if ((progressPercent >= service->context.lastProgressPercent) &&
        ((progressPercent - service->context.lastProgressPercent) <
         SERVICE_OTA_PROGRESS_STEP_PERCENT) &&
        ((nowMs - service->context.lastProgressMs) < SERVICE_OTA_PROGRESS_INTERVAL_MS) &&
        (progressPercent != 100U)) {
        return;
    }

    service->context.lastProgressMs = nowMs;
    service->context.lastProgressPercent = progressPercent;
    service->context.event = SERVICE_OTA_EVENT_PROGRESS;
}

static platform_error_t service_ota_finish_receiving(service_ota_t *service)
{
    firmware_image_header_t header = {0};
    firmware_image_validation_t validation;
    firmware_slot_state_t *targetState;
    platform_error_t result;

    service->context.state = SERVICE_OTA_STATE_VERIFYING;
    service->context.event = SERVICE_OTA_EVENT_VERIFYING;
    result = firmware_storage_validate_image(
        service->config.storage,
        service->context.targetSlot,
        &header,
        &validation);
    if (result != PLATFORM_ERR_OK) {
        return service_ota_fail(service, result);
    }

    if (validation != FIRMWARE_IMAGE_VALIDATION_VALID) {
        return service_ota_fail(service, PLATFORM_ERR_INVALID_PARAM);
    }

    targetState = service_ota_get_slot_state(
        &service->context.metadata,
        service->context.targetSlot);
    if (targetState == NULL) {
        return service_ota_fail(service, PLATFORM_ERR_INVALID_PARAM);
    }

    *targetState = FIRMWARE_SLOT_STATE_VALID;
    service->context.metadata.pendingSlot = FIRMWARE_SLOT_NONE;
    service->context.metadata.upgradeState = FIRMWARE_UPGRADE_STATE_NONE;
    result = service_ota_commit_metadata(service);
    if (result != PLATFORM_ERR_OK) {
        return service_ota_fail(service, result);
    }

    service->context.progressPercent = 100U;
    service->context.lastProgressPercent = 100U;
    service->context.state = SERVICE_OTA_STATE_READY_TO_INSTALL;
    service->context.event = SERVICE_OTA_EVENT_READY_TO_INSTALL;
    service->context.lastError = PLATFORM_ERR_OK;
    return PLATFORM_ERR_OK;
}

static platform_bool_t service_ota_receiver_is_active(ymodem_receiver_state_t state)
{
    if ((state == YMODEM_RECEIVER_STATE_WAIT_HEADER) ||
        (state == YMODEM_RECEIVER_STATE_RECEIVE_DATA) ||
        (state == YMODEM_RECEIVER_STATE_WAIT_EOT_CONFIRM) ||
        (state == YMODEM_RECEIVER_STATE_WAIT_END_HEADER)) {
        return PLATFORM_TRUE;
    }

    return PLATFORM_FALSE;
}
//******************************** Private Functions ************************//

//******************************** Public Functions *************************//
platform_error_t service_ota_init(
    service_ota_t *service,
    const service_ota_config_t *config)
{
    ymodem_sink_t sinkContract;
    platform_error_t result;

    if ((service == NULL) || (config == NULL) ||
        (config->storage == NULL) || (config->uart == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    (void)memset(service, 0, sizeof(*service));
    service->config = *config;
    service->context.state = SERVICE_OTA_STATE_IDLE;
    service->context.targetSlot = FIRMWARE_SLOT_NONE;
    service->context.lastError = PLATFORM_ERR_OK;

    result = ota_firmware_sink_init(&service->sink, config->storage);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = ota_firmware_sink_get_contract(&service->sink, &sinkContract);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    service->receiverConfig.uart = config->uart;
    service->receiverConfig.sink = sinkContract;
    return ymodem_receiver_init(&service->receiver, &service->receiverConfig);
}

platform_error_t service_ota_start(service_ota_t *service)
{
    firmware_slot_state_t *targetState;
    platform_error_t result;

    if (service == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if ((service->config.storage == NULL) || (service->config.uart == NULL)) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    if ((service->context.state != SERVICE_OTA_STATE_IDLE) &&
        (service->context.state != SERVICE_OTA_STATE_FAILED)) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    service->context.event = SERVICE_OTA_EVENT_NONE;
    service->context.lastError = PLATFORM_ERR_OK;
    service->context.targetSlot = FIRMWARE_SLOT_NONE;
    service->context.receivedBytes = 0U;
    service->context.expectedBytes = 0U;
    service->context.progressPercent = 0U;
    service->context.lastProgressMs = 0U;
    service->context.lastProgressPercent = 0U;
    service->context.state = SERVICE_OTA_STATE_PREPARING;

    result = service_ota_load_stable_metadata(service);
    if (result != PLATFORM_ERR_OK) {
        return service_ota_fail(service, result);
    }

    service->context.targetSlot = service_ota_get_target_slot(
        service->context.metadata.confirmedSlot);
    if (service->context.targetSlot == FIRMWARE_SLOT_NONE) {
        return service_ota_fail(service, PLATFORM_ERR_INVALID_STATE);
    }

    targetState = service_ota_get_slot_state(
        &service->context.metadata,
        service->context.targetSlot);
    if (targetState == NULL) {
        return service_ota_fail(service, PLATFORM_ERR_INVALID_STATE);
    }

    *targetState = FIRMWARE_SLOT_STATE_INVALID;
    service->context.metadata.pendingSlot = FIRMWARE_SLOT_NONE;
    service->context.metadata.upgradeState = FIRMWARE_UPGRADE_STATE_NONE;
    result = service_ota_commit_metadata(service);
    if (result != PLATFORM_ERR_OK) {
        return service_ota_fail(service, result);
    }

    result = ota_firmware_sink_set_target_slot(
        &service->sink,
        service->context.targetSlot);
    if (result != PLATFORM_ERR_OK) {
        return service_ota_fail(service, result);
    }

    result = ota_firmware_sink_get_contract(&service->sink, &service->receiverConfig.sink);
    if (result != PLATFORM_ERR_OK) {
        return service_ota_fail(service, result);
    }

    result = ymodem_receiver_init(&service->receiver, &service->receiverConfig);
    if (result != PLATFORM_ERR_OK) {
        return service_ota_fail(service, result);
    }

    result = ymodem_receiver_start(&service->receiver);
    if (result != PLATFORM_ERR_OK) {
        return service_ota_fail(service, result);
    }

    service->context.state = SERVICE_OTA_STATE_RECEIVING;
    service->context.event = SERVICE_OTA_EVENT_STARTED;
    return PLATFORM_ERR_OK;
}

platform_error_t service_ota_process(
    service_ota_t *service,
    const uint8_t *data,
    uint32_t dataLength,
    uint32_t nowMs)
{
    ymodem_receiver_status_t receiverStatus;
    platform_error_t result;
    uint32_t index;

    if (service == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (service->context.state != SERVICE_OTA_STATE_RECEIVING) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    if ((data == NULL) && (dataLength != 0U)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    for (index = 0U; index < dataLength; index++) {
        result = ymodem_receiver_feed_byte(&service->receiver, data[index], nowMs);
        if (result != PLATFORM_ERR_OK) {
            return service_ota_fail(service, result);
        }
        nowMs++;
    }

    result = ymodem_receiver_tick(&service->receiver, nowMs);
    if (result != PLATFORM_ERR_OK) {
        return service_ota_fail(service, result);
    }

    result = ymodem_receiver_get_status(&service->receiver, &receiverStatus);
    if (result != PLATFORM_ERR_OK) {
        return service_ota_fail(service, result);
    }

    service_ota_update_progress(service, &receiverStatus, nowMs);
    if (receiverStatus.state == YMODEM_RECEIVER_STATE_FINISHED) {
        return service_ota_finish_receiving(service);
    }

    if (service_ota_receiver_is_active(receiverStatus.state) == PLATFORM_FALSE) {
        result = receiverStatus.lastError;
        if (result == PLATFORM_ERR_OK) {
            result = PLATFORM_ERR_INVALID_STATE;
        }
        return service_ota_fail(service, result);
    }

    return PLATFORM_ERR_OK;
}

platform_error_t service_ota_abort(
    service_ota_t *service,
    platform_error_t error)
{
    if (service == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (service->context.state != SERVICE_OTA_STATE_RECEIVING) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    (void)ymodem_receiver_cancel(&service->receiver);
    return service_ota_fail(service, error);
}

platform_error_t service_ota_confirm_install(service_ota_t *service)
{
    platform_error_t result;

    if (service == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (service->context.state != SERVICE_OTA_STATE_READY_TO_INSTALL) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    service->context.state = SERVICE_OTA_STATE_COMMITTING;
    service->context.event = SERVICE_OTA_EVENT_NONE;
    service->context.metadata.pendingSlot = service->context.targetSlot;
    service->context.metadata.upgradeState = FIRMWARE_UPGRADE_STATE_PENDING;
    result = service_ota_commit_metadata(service);
    if (result != PLATFORM_ERR_OK) {
        return service_ota_fail(service, result);
    }

    service->context.state = SERVICE_OTA_STATE_REBOOT_REQUIRED;
    service->context.event = SERVICE_OTA_EVENT_RESET_REQUIRED;
    service->context.lastError = PLATFORM_ERR_OK;
    return PLATFORM_ERR_OK;
}

platform_error_t service_ota_get_status(
    const service_ota_t *service,
    service_ota_status_t *status)
{
    if ((service == NULL) || (status == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    status->state = service->context.state;
    status->event = service->context.event;
    status->lastError = service->context.lastError;
    status->targetSlot = service->context.targetSlot;
    status->receivedBytes = service->context.receivedBytes;
    status->expectedBytes = service->context.expectedBytes;
    status->progressPercent = service->context.progressPercent;
    return PLATFORM_ERR_OK;
}

platform_error_t service_ota_clear_event(service_ota_t *service)
{
    if (service == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    service->context.event = SERVICE_OTA_EVENT_NONE;
    return PLATFORM_ERR_OK;
}
//******************************** Public Functions *************************//
