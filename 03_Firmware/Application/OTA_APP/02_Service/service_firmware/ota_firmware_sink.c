/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file ota_firmware_sink.c
 * @brief Production OTA Firmware Image Sink 实现
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>
#include <string.h>

#include "platform_def.h"
#include "ota_firmware_sink.h"
//******************************** Includes *********************************//

//******************************** Declaring *******************************//
static void ota_firmware_sink_reset_runtime(ota_firmware_sink_t *sink);
static platform_error_t ota_firmware_sink_fail(
    ota_firmware_sink_t *sink,
    platform_error_t error);
static platform_error_t ota_firmware_sink_prepare_payload(
    ota_firmware_sink_t *sink);
static platform_error_t ota_firmware_sink_begin(
    void *context,
    const char *filename,
    uint32_t fileSize);
static platform_error_t ota_firmware_sink_consume_header(
    ota_firmware_sink_t *sink,
    const uint8_t *data,
    uint32_t *dataOffset,
    uint32_t *remaining);
static platform_error_t ota_firmware_sink_consume_payload(
    ota_firmware_sink_t *sink,
    const uint8_t *data,
    uint32_t *dataOffset,
    uint32_t *remaining);
static platform_error_t ota_firmware_sink_write(
    void *context,
    const uint8_t *data,
    uint32_t length);
static platform_error_t ota_firmware_sink_end(void *context);
static void ota_firmware_sink_abort(void *context);
//******************************** Declaring *******************************//

//******************************** Private Functions ************************//
static void ota_firmware_sink_reset_runtime(ota_firmware_sink_t *sink)
{
    firmware_storage_t *storage = sink->storage;
    firmware_slot_t slot = sink->slot;

    (void)memset(sink, 0, sizeof(*sink));
    sink->storage = storage;
    sink->slot = slot;
    sink->lastError = PLATFORM_ERR_OK;
}

static platform_error_t ota_firmware_sink_fail(
    ota_firmware_sink_t *sink,
    platform_error_t error)
{
    sink->failed = PLATFORM_TRUE;
    sink->lastError = error;
    return error;
}

static platform_error_t ota_firmware_sink_prepare_payload(
    ota_firmware_sink_t *sink)
{
    firmware_image_validation_t validation;
    platform_error_t result;

    validation = firmware_image_validate_header(sink->headerBuffer, &sink->header);
    if (validation != FIRMWARE_IMAGE_VALIDATION_VALID) {
        return ota_firmware_sink_fail(sink, PLATFORM_ERR_INVALID_PARAM);
    }

    if (sink->expectedFileSize != (FIRMWARE_IMAGE_HEADER_SIZE + sink->header.imageSize)) {
        return ota_firmware_sink_fail(sink, PLATFORM_ERR_INVALID_PARAM);
    }

    result = firmware_storage_erase_slot(sink->storage, sink->slot, sink->header.imageSize);
    if (result != PLATFORM_ERR_OK) {
        return ota_firmware_sink_fail(sink, result);
    }

    sink->headerValidated = PLATFORM_TRUE;
    return PLATFORM_ERR_OK;
}

static platform_error_t ota_firmware_sink_begin(
    void *context,
    const char *filename,
    uint32_t fileSize)
{
    ota_firmware_sink_t *sink = (ota_firmware_sink_t *)context;

    if ((sink == NULL) || (filename == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (sink->storage == NULL) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    if (sink->started != PLATFORM_FALSE) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    if ((sink->slot != FIRMWARE_SLOT_A) && (sink->slot != FIRMWARE_SLOT_B)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    ota_firmware_sink_reset_runtime(sink);
    if ((fileSize < (FIRMWARE_IMAGE_HEADER_SIZE + 1U)) ||
        (fileSize > (FIRMWARE_IMAGE_HEADER_SIZE + FIRMWARE_SLOT_PAYLOAD_CAPACITY))) {
        return ota_firmware_sink_fail(sink, PLATFORM_ERR_INVALID_PARAM);
    }

    (void)strncpy(sink->filename, filename, sizeof(sink->filename) - 1U);
    sink->filename[sizeof(sink->filename) - 1U] = '\0';
    sink->expectedFileSize = fileSize;
    sink->started = PLATFORM_TRUE;
    return PLATFORM_ERR_OK;
}

/** @brief 在一次回调内处理 Header 边界，并在完整后触发校验和擦除。 */
static platform_error_t ota_firmware_sink_consume_header(
    ota_firmware_sink_t *sink,
    const uint8_t *data,
    uint32_t *dataOffset,
    uint32_t *remaining)
{
    uint32_t copyLength;
    platform_error_t result;

    copyLength = FIRMWARE_IMAGE_HEADER_SIZE - sink->headerFillCount;
    if (copyLength > *remaining) {
        copyLength = *remaining;
    }

    (void)memcpy(
        &sink->headerBuffer[sink->headerFillCount],
        &data[*dataOffset],
        copyLength);
    sink->headerFillCount += copyLength;
    *dataOffset += copyLength;
    *remaining -= copyLength;

    if (sink->headerFillCount == FIRMWARE_IMAGE_HEADER_SIZE) {
        result = ota_firmware_sink_prepare_payload(sink);
        if (result != PLATFORM_ERR_OK) {
            return result;
        }
    }

    return PLATFORM_ERR_OK;
}

static platform_error_t ota_firmware_sink_consume_payload(
    ota_firmware_sink_t *sink,
    const uint8_t *data,
    uint32_t *dataOffset,
    uint32_t *remaining)
{
    uint32_t payloadLength;
    platform_error_t result;

    if ((sink->headerValidated == PLATFORM_FALSE) ||
        (sink->payloadWritten >= sink->header.imageSize)) {
        return ota_firmware_sink_fail(sink, PLATFORM_ERR_INVALID_PARAM);
    }

    payloadLength = sink->header.imageSize - sink->payloadWritten;
    if (payloadLength > *remaining) {
        payloadLength = *remaining;
    }

    result = firmware_storage_write_payload(
        sink->storage,
        sink->slot,
        sink->payloadWritten,
        &data[*dataOffset],
        payloadLength);
    if (result != PLATFORM_ERR_OK) {
        return ota_firmware_sink_fail(sink, result);
    }

    sink->payloadWritten += payloadLength;
    *dataOffset += payloadLength;
    *remaining -= payloadLength;
    return PLATFORM_ERR_OK;
}

static platform_error_t ota_firmware_sink_write(
    void *context,
    const uint8_t *data,
    uint32_t length)
{
    ota_firmware_sink_t *sink = (ota_firmware_sink_t *)context;
    uint32_t dataOffset = 0U;
    uint32_t remaining = length;
    platform_error_t result;

    if ((sink == NULL) || (data == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (length == 0U) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if ((sink->started == PLATFORM_FALSE) || (sink->failed != PLATFORM_FALSE)) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    if ((sink->receivedFileBytes > sink->expectedFileSize) ||
        (length > (sink->expectedFileSize - sink->receivedFileBytes))) {
        return ota_firmware_sink_fail(sink, PLATFORM_ERR_INVALID_PARAM);
    }

    while (remaining != 0U) {
        if (sink->headerFillCount < FIRMWARE_IMAGE_HEADER_SIZE) {
            result = ota_firmware_sink_consume_header(
                sink, data, &dataOffset, &remaining);
        } else {
            result = ota_firmware_sink_consume_payload(
                sink, data, &dataOffset, &remaining);
        }

        if (result != PLATFORM_ERR_OK) {
            return result;
        }
    }

    sink->receivedFileBytes += length;
    return PLATFORM_ERR_OK;
}

static platform_error_t ota_firmware_sink_end(void *context)
{
    ota_firmware_sink_t *sink = (ota_firmware_sink_t *)context;
    platform_error_t result;

    if (sink == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if ((sink->started == PLATFORM_FALSE) || (sink->failed != PLATFORM_FALSE)) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    if ((sink->receivedFileBytes != sink->expectedFileSize) ||
        (sink->headerFillCount != FIRMWARE_IMAGE_HEADER_SIZE) ||
        (sink->headerValidated == PLATFORM_FALSE) ||
        (sink->payloadWritten != sink->header.imageSize)) {
        return ota_firmware_sink_fail(sink, PLATFORM_ERR_INVALID_STATE);
    }

    result = firmware_storage_write_header(sink->storage, sink->slot, sink->headerBuffer);
    if (result != PLATFORM_ERR_OK) {
        return ota_firmware_sink_fail(sink, result);
    }

    sink->headerCommitted = PLATFORM_TRUE;
    sink->started = PLATFORM_FALSE;
    return PLATFORM_ERR_OK;
}

static void ota_firmware_sink_abort(void *context)
{
    ota_firmware_sink_t *sink = (ota_firmware_sink_t *)context;

    if (sink != NULL) {
        ota_firmware_sink_reset_runtime(sink);
    }
}

//******************************** Public Functions *************************//
platform_error_t ota_firmware_sink_init(
    ota_firmware_sink_t *sink,
    firmware_storage_t *storage)
{
    if ((sink == NULL) || (storage == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    (void)memset(sink, 0, sizeof(*sink));
    sink->storage = storage;
    sink->slot = FIRMWARE_SLOT_NONE;
    sink->lastError = PLATFORM_ERR_OK;
    return PLATFORM_ERR_OK;
}

platform_error_t ota_firmware_sink_set_target_slot(
    ota_firmware_sink_t *sink,
    firmware_slot_t slot)
{
    if (sink == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if ((slot != FIRMWARE_SLOT_A) && (slot != FIRMWARE_SLOT_B)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (sink->started != PLATFORM_FALSE) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    sink->slot = slot;
    return PLATFORM_ERR_OK;
}

platform_error_t ota_firmware_sink_get_contract(
    ota_firmware_sink_t *sink,
    ymodem_sink_t *contract)
{
    if ((sink == NULL) || (contract == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (sink->storage == NULL) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    contract->begin = ota_firmware_sink_begin;
    contract->write = ota_firmware_sink_write;
    contract->end = ota_firmware_sink_end;
    contract->abort = ota_firmware_sink_abort;
    contract->context = sink;
    return PLATFORM_ERR_OK;
}
