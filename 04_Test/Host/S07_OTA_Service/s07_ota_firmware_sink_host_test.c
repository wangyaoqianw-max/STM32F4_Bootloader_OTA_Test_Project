/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s07_ota_firmware_sink_host_test.c
 * @brief S07 Production OTA Firmware Sink Host Test
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "crc.h"
#include "firmware_image.h"
#include "ota_firmware_sink.h"
//******************************** Includes *********************************//

//******************************** Types ***********************************//
typedef struct
{
    platform_error_t eraseResult;
    platform_error_t payloadResult;
    platform_error_t headerResult;
    uint32_t eraseCount;
    uint32_t payloadWriteCount;
    uint32_t headerWriteCount;
    firmware_slot_t eraseSlot;
    firmware_slot_t payloadSlot;
    firmware_slot_t headerSlot;
    uint32_t erasePayloadSize;
    uint32_t payloadOffset;
    uint32_t payloadLength;
    uint8_t payload[32];
    uint8_t header[FIRMWARE_IMAGE_HEADER_SIZE];
} fake_storage_operation_t;
//******************************** Types ***********************************//

//******************************** Variables ********************************//
static fake_storage_operation_t g_storageOperation;
//******************************** Variables ********************************//

//******************************** Functions *********************************//
platform_error_t firmware_storage_erase_slot(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    uint32_t payloadSize)
{
    (void)storage;
    g_storageOperation.eraseCount++;
    g_storageOperation.eraseSlot = slot;
    g_storageOperation.erasePayloadSize = payloadSize;
    return g_storageOperation.eraseResult;
}

platform_error_t firmware_storage_write_payload(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    uint32_t payloadOffset,
    const uint8_t *data,
    uint32_t length)
{
    (void)storage;
    g_storageOperation.payloadWriteCount++;
    g_storageOperation.payloadSlot = slot;
    g_storageOperation.payloadOffset = payloadOffset;
    g_storageOperation.payloadLength = length;
    if (length <= sizeof(g_storageOperation.payload)) {
        (void)memcpy(g_storageOperation.payload, data, length);
    }
    return g_storageOperation.payloadResult;
}

platform_error_t firmware_storage_write_header(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    const uint8_t rawHeader[FIRMWARE_IMAGE_HEADER_SIZE])
{
    (void)storage;
    g_storageOperation.headerWriteCount++;
    g_storageOperation.headerSlot = slot;
    (void)memcpy(g_storageOperation.header, rawHeader, sizeof(g_storageOperation.header));
    return g_storageOperation.headerResult;
}

static void build_image(
    uint8_t *image,
    uint32_t payloadSize,
    uint8_t payloadSeed)
{
    firmware_image_header_t header = {0};
    uint32_t index;

    for (index = 0U; index < payloadSize; index++) {
        image[FIRMWARE_IMAGE_HEADER_SIZE + index] = (uint8_t)(payloadSeed + index);
    }

    header.version.major = 1U;
    header.version.minor = 2U;
    header.version.patch = 3U;
    header.imageSize = payloadSize;
    header.payloadCrc32 = crc32_iso_hdlc_calculate(
        &image[FIRMWARE_IMAGE_HEADER_SIZE], payloadSize);
    (void)firmware_image_encode_header(&header, image);
}

static int init_sink(
    ota_firmware_sink_t *sink,
    ymodem_sink_t *contract,
    firmware_storage_t *storage,
    firmware_slot_t slot)
{
    if ((ota_firmware_sink_init(sink, storage) != PLATFORM_ERR_OK) ||
        (ota_firmware_sink_set_target_slot(sink, slot) != PLATFORM_ERR_OK) ||
        (ota_firmware_sink_get_contract(sink, contract) != PLATFORM_ERR_OK)) {
        return 1;
    }

    return 0;
}

static int test_dynamic_target_and_header_last(firmware_slot_t slot)
{
    uint8_t image[FIRMWARE_IMAGE_HEADER_SIZE + 5U] = {0U};
    ota_firmware_sink_t sink;
    ymodem_sink_t contract;
    firmware_storage_t storage = FIRMWARE_STORAGE_INITIALIZER;
    uint8_t *payload = &image[FIRMWARE_IMAGE_HEADER_SIZE];

    (void)memset(&g_storageOperation, 0, sizeof(g_storageOperation));
    build_image(image, 5U, 0x20U);
    if ((init_sink(&sink, &contract, &storage, slot) != 0) ||
        (contract.begin(contract.context, "firmware.img", sizeof(image)) != PLATFORM_ERR_OK) ||
        (contract.write(contract.context, image, 63U) != PLATFORM_ERR_OK) ||
        (g_storageOperation.eraseCount != 0U) ||
        (g_storageOperation.payloadWriteCount != 0U) ||
        (contract.write(contract.context, &image[63], sizeof(image) - 63U) != PLATFORM_ERR_OK) ||
        (g_storageOperation.eraseCount != 1U) ||
        (g_storageOperation.eraseSlot != slot) ||
        (g_storageOperation.erasePayloadSize != 5U) ||
        (g_storageOperation.payloadWriteCount != 1U) ||
        (g_storageOperation.payloadSlot != slot) ||
        (g_storageOperation.payloadOffset != 0U) ||
        (g_storageOperation.payloadLength != 5U) ||
        (memcmp(g_storageOperation.payload, payload, 5U) != 0) ||
        (g_storageOperation.headerWriteCount != 0U) ||
        (contract.end(contract.context) != PLATFORM_ERR_OK) ||
        (g_storageOperation.headerWriteCount != 1U) ||
        (g_storageOperation.headerSlot != slot) ||
        (memcmp(g_storageOperation.header, image, FIRMWARE_IMAGE_HEADER_SIZE) != 0)) {
        return 1;
    }

    return 0;
}

static int test_invalid_input_and_abort(void)
{
    uint8_t image[FIRMWARE_IMAGE_HEADER_SIZE + 5U] = {0U};
    ota_firmware_sink_t sink;
    ymodem_sink_t contract;
    firmware_storage_t storage = FIRMWARE_STORAGE_INITIALIZER;

    (void)memset(&g_storageOperation, 0, sizeof(g_storageOperation));
    build_image(image, 5U, 0x30U);
    image[10] ^= 1U;
    if ((init_sink(&sink, &contract, &storage, FIRMWARE_SLOT_A) != 0) ||
        (contract.begin(contract.context, "firmware.img", sizeof(image)) != PLATFORM_ERR_OK) ||
        (contract.write(contract.context, image, sizeof(image)) == PLATFORM_ERR_OK) ||
        (g_storageOperation.eraseCount != 0U) ||
        (g_storageOperation.headerWriteCount != 0U)) {
        return 1;
    }

    (void)memset(&g_storageOperation, 0, sizeof(g_storageOperation));
    if ((init_sink(&sink, &contract, &storage, FIRMWARE_SLOT_NONE) == 0) ||
        (ota_firmware_sink_init(&sink, &storage) != PLATFORM_ERR_OK) ||
        (ota_firmware_sink_get_contract(&sink, &contract) != PLATFORM_ERR_OK) ||
        (contract.begin(contract.context, "firmware.img", sizeof(image)) == PLATFORM_ERR_OK)) {
        return 1;
    }

    if ((ota_firmware_sink_set_target_slot(&sink, FIRMWARE_SLOT_B) != PLATFORM_ERR_OK) ||
        (contract.begin(contract.context, "firmware.img", FIRMWARE_IMAGE_HEADER_SIZE) == PLATFORM_ERR_OK) ||
        (contract.begin(contract.context,
                        "firmware.img",
                        FIRMWARE_IMAGE_HEADER_SIZE + FIRMWARE_SLOT_PAYLOAD_CAPACITY + 1U) ==
         PLATFORM_ERR_OK)) {
        return 1;
    }

    build_image(image, 5U, 0x30U);
    (void)memset(&g_storageOperation, 0, sizeof(g_storageOperation));
    if ((contract.begin(contract.context, "firmware.img", sizeof(image)) != PLATFORM_ERR_OK) ||
        (contract.write(contract.context, image, FIRMWARE_IMAGE_HEADER_SIZE) != PLATFORM_ERR_OK) ||
        (contract.end(contract.context) == PLATFORM_ERR_OK) ||
        (g_storageOperation.headerWriteCount != 0U)) {
        return 1;
    }

    contract.abort(contract.context);
    if ((contract.begin(contract.context, "firmware.img", sizeof(image)) != PLATFORM_ERR_OK) ||
        (contract.write(contract.context, image, sizeof(image)) != PLATFORM_ERR_OK)) {
        return 1;
    }

    contract.abort(contract.context);
    return (g_storageOperation.headerWriteCount == 0U) ? 0 : 1;
}

static int test_storage_failures(void)
{
    uint8_t image[FIRMWARE_IMAGE_HEADER_SIZE + 5U] = {0U};
    ota_firmware_sink_t sink;
    ymodem_sink_t contract;
    firmware_storage_t storage = FIRMWARE_STORAGE_INITIALIZER;

    build_image(image, 5U, 0x50U);
    (void)memset(&g_storageOperation, 0, sizeof(g_storageOperation));
    g_storageOperation.eraseResult = PLATFORM_ERR_IO;
    if ((init_sink(&sink, &contract, &storage, FIRMWARE_SLOT_A) != 0) ||
        (contract.begin(contract.context, "firmware.img", sizeof(image)) != PLATFORM_ERR_OK) ||
        (contract.write(contract.context, image, sizeof(image)) == PLATFORM_ERR_OK) ||
        (g_storageOperation.headerWriteCount != 0U)) {
        return 1;
    }

    (void)memset(&g_storageOperation, 0, sizeof(g_storageOperation));
    g_storageOperation.payloadResult = PLATFORM_ERR_IO;
    if ((init_sink(&sink, &contract, &storage, FIRMWARE_SLOT_B) != 0) ||
        (contract.begin(contract.context, "firmware.img", sizeof(image)) != PLATFORM_ERR_OK) ||
        (contract.write(contract.context, image, sizeof(image)) == PLATFORM_ERR_OK) ||
        (g_storageOperation.headerWriteCount != 0U)) {
        return 1;
    }

    (void)memset(&g_storageOperation, 0, sizeof(g_storageOperation));
    g_storageOperation.headerResult = PLATFORM_ERR_IO;
    if ((init_sink(&sink, &contract, &storage, FIRMWARE_SLOT_A) != 0) ||
        (contract.begin(contract.context, "firmware.img", sizeof(image)) != PLATFORM_ERR_OK) ||
        (contract.write(contract.context, image, sizeof(image)) != PLATFORM_ERR_OK) ||
        (contract.end(contract.context) == PLATFORM_ERR_OK) ||
        (g_storageOperation.headerWriteCount != 1U)) {
        return 1;
    }

    return 0;
}

int main(void)
{
    if ((test_dynamic_target_and_header_last(FIRMWARE_SLOT_A) != 0) ||
        (test_dynamic_target_and_header_last(FIRMWARE_SLOT_B) != 0) ||
        (test_invalid_input_and_abort() != 0) ||
        (test_storage_failures() != 0)) {
        (void)printf("S07 OTA Firmware Sink host test failed.\n");
        return 1;
    }

    (void)printf("S07 OTA Firmware Sink host test passed.\n");
    return 0;
}
//******************************** Functions *********************************//
