/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s07_service_ota_host_test.c
 * @brief S07 OTA Service 状态机与 Metadata 事务 Host Test
 * @author YaoQian Wang
 * @date 2026-09-17
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <stdio.h>
#include <string.h>

#include "crc.h"
#include "firmware_image.h"
#include "service_ota.h"
#include "ymodem_def.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define TEST_IMAGE_PAYLOAD_SIZE          (4U)
#define TEST_IMAGE_FILE_SIZE             (FIRMWARE_IMAGE_HEADER_SIZE + TEST_IMAGE_PAYLOAD_SIZE)
#define TEST_PACKET_OVERHEAD             (5U)
#define TEST_METADATA_SEQUENCE           (10U)
//******************************** Defines **********************************//

//******************************** Types ***********************************//
typedef struct
{
    firmware_metadata_t metadata;
    firmware_metadata_copy_id_t metadataCopy;
    platform_error_t loadResult;
    platform_error_t commitResult;
    platform_error_t eraseResult;
    platform_error_t payloadResult;
    platform_error_t headerResult;
    platform_error_t validateResult;
    firmware_image_validation_t validation;
    uint32_t loadCount;
    uint32_t commitCount;
    uint32_t eraseCount;
    uint32_t payloadCount;
    uint32_t headerCount;
    uint32_t validateCount;
    firmware_slot_t lastEraseSlot;
    firmware_slot_t lastPayloadSlot;
    firmware_slot_t lastHeaderSlot;
    firmware_slot_t lastValidateSlot;
} test_storage_context_t;
//******************************** Types ***********************************//

//******************************** Variables ********************************//
static test_storage_context_t g_storageContext;
//******************************** Variables ********************************//

//******************************** Private Functions ************************//
static void test_storage_reset(void)
{
    (void)memset(&g_storageContext, 0, sizeof(g_storageContext));
    g_storageContext.metadata.sequence = TEST_METADATA_SEQUENCE;
    g_storageContext.metadata.confirmedSlot = FIRMWARE_SLOT_A;
    g_storageContext.metadata.pendingSlot = FIRMWARE_SLOT_NONE;
    g_storageContext.metadata.slotAState = FIRMWARE_SLOT_STATE_VALID;
    g_storageContext.metadata.slotBState = FIRMWARE_SLOT_STATE_EMPTY;
    g_storageContext.metadata.upgradeState = FIRMWARE_UPGRADE_STATE_NONE;
    g_storageContext.metadata.confirmedVersion.major = 1U;
    g_storageContext.metadataCopy = FIRMWARE_METADATA_COPY_A;
    g_storageContext.loadResult = PLATFORM_ERR_OK;
    g_storageContext.commitResult = PLATFORM_ERR_OK;
    g_storageContext.eraseResult = PLATFORM_ERR_OK;
    g_storageContext.payloadResult = PLATFORM_ERR_OK;
    g_storageContext.headerResult = PLATFORM_ERR_OK;
    g_storageContext.validateResult = PLATFORM_ERR_OK;
    g_storageContext.validation = FIRMWARE_IMAGE_VALIDATION_VALID;
}

static void test_build_packet(
    uint8_t control,
    uint8_t blockNumber,
    const uint8_t *data,
    uint16_t dataSize,
    uint8_t *packet,
    uint16_t *packetSize)
{
    uint16_t crc;

    packet[0] = control;
    packet[1] = blockNumber;
    packet[2] = (uint8_t)~blockNumber;
    (void)memcpy(&packet[3], data, dataSize);
    crc = crc16_xmodem_calculate(data, dataSize);
    packet[3U + dataSize] = (uint8_t)(crc >> 8U);
    packet[4U + dataSize] = (uint8_t)crc;
    *packetSize = (uint16_t)(dataSize + TEST_PACKET_OVERHEAD);
}

static void test_build_block0(uint8_t *data, uint32_t fileSize)
{
    const char filename[] = "firmware.img";
    size_t filenameLength = strlen(filename);

    (void)memset(data, 0, YMODEM_PACKET_DATA_SIZE_128);
    (void)memcpy(data, filename, filenameLength);
    (void)snprintf(
        (char *)&data[filenameLength + 1U],
        YMODEM_PACKET_DATA_SIZE_128 - filenameLength - 1U,
        " %lu 15251747316 100644",
        (unsigned long)fileSize);
}

static int test_feed_bytes(
    service_ota_t *service,
    const uint8_t *data,
    uint16_t dataLength,
    uint32_t *nowMs)
{
    uint16_t index;

    for (index = 0U; index < dataLength; index++) {
        if (service_ota_process(service, &data[index], 1U, *nowMs) != PLATFORM_ERR_OK) {
            return (service->context.state == SERVICE_OTA_STATE_FAILED) ? 2 : 1;
        }
        *nowMs += 1U;
    }

    return 0;
}

static int test_send_packet(
    service_ota_t *service,
    uint8_t blockNumber,
    const uint8_t *data,
    uint16_t dataSize,
    uint32_t *nowMs)
{
    uint8_t packet[YMODEM_PACKET_MAX_SIZE];
    uint16_t packetSize;
    uint8_t control;

    control = (dataSize == YMODEM_PACKET_DATA_SIZE_128) ? YMODEM_SOH : YMODEM_STX;
    test_build_packet(control, blockNumber, data, dataSize, packet, &packetSize);
    return test_feed_bytes(service, packet, packetSize, nowMs);
}

static void test_build_image(uint8_t *image)
{
    firmware_image_header_t header = {0};
    const uint8_t payload[TEST_IMAGE_PAYLOAD_SIZE] = {0x11U, 0x22U, 0x33U, 0x44U};

    header.version.major = 1U;
    header.version.minor = 1U;
    header.version.patch = 0U;
    header.imageSize = TEST_IMAGE_PAYLOAD_SIZE;
    header.payloadCrc32 = crc32_iso_hdlc_calculate(payload, sizeof(payload));
    (void)firmware_image_encode_header(&header, image);
    (void)memcpy(&image[FIRMWARE_IMAGE_HEADER_SIZE], payload, sizeof(payload));
}

static int test_send_valid_image(service_ota_t *service, uint32_t *nowMs)
{
    uint8_t block0[YMODEM_PACKET_DATA_SIZE_128];
    uint8_t dataPacket[YMODEM_PACKET_DATA_SIZE_128] = {0U};
    uint8_t emptyBlock[YMODEM_PACKET_DATA_SIZE_128] = {0U};
    uint8_t image[TEST_IMAGE_FILE_SIZE];
    uint8_t eot = YMODEM_EOT;

    test_build_image(image);
    test_build_block0(block0, TEST_IMAGE_FILE_SIZE);
    (void)memcpy(dataPacket, image, sizeof(image));

    if ((test_send_packet(service, 0U, block0, sizeof(block0), nowMs) != 0) ||
        (test_send_packet(service, 1U, dataPacket, sizeof(dataPacket), nowMs) != 0) ||
        (service_ota_process(service, NULL, 0U, *nowMs) != PLATFORM_ERR_OK)) {
        return 1;
    }
    *nowMs += 1U;

    if ((service_ota_process(service, &eot, 1U, *nowMs) != PLATFORM_ERR_OK) ||
        (service_ota_process(service, &eot, 1U, *nowMs + 1U) != PLATFORM_ERR_OK)) {
        return 1;
    }

    return test_send_packet(service, 0U, emptyBlock, sizeof(emptyBlock), nowMs);
}

static int test_prepare_service(
    service_ota_t *service,
    firmware_storage_t *storage,
    service_uart_t *uart)
{
    service_ota_config_t config;

    (void)memset(service, 0, sizeof(*service));
    (void)memset(storage, 0, sizeof(*storage));
    (void)memset(uart, 0, sizeof(*uart));
    config.storage = storage;
    config.uart = uart;

    return service_ota_init(service, &config) == PLATFORM_ERR_OK ? 0 : 1;
}

static int test_normal_two_confirm_flow(void)
{
    firmware_storage_t storage;
    service_uart_t uart;
    service_ota_t service;
    service_ota_status_t status;
    uint32_t nowMs = 0U;

    test_storage_reset();
    if (test_prepare_service(&service, &storage, &uart) != 0) {
        return 1;
    }

    if ((service_ota_start(&service) != PLATFORM_ERR_OK) ||
        (service.context.state != SERVICE_OTA_STATE_RECEIVING) ||
        (service.context.targetSlot != FIRMWARE_SLOT_B) ||
        (g_storageContext.commitCount != 1U) ||
        (g_storageContext.metadata.slotBState != FIRMWARE_SLOT_STATE_INVALID) ||
        (g_storageContext.metadata.pendingSlot != FIRMWARE_SLOT_NONE) ||
        (g_storageContext.eraseCount != 0U)) {
        return 1;
    }

    if ((test_send_valid_image(&service, &nowMs) != 0) ||
        (service_ota_get_status(&service, &status) != PLATFORM_ERR_OK) ||
        (status.state != SERVICE_OTA_STATE_READY_TO_INSTALL) ||
        (g_storageContext.validateCount != 1U) ||
        (g_storageContext.commitCount != 2U) ||
        (g_storageContext.metadata.slotBState != FIRMWARE_SLOT_STATE_VALID) ||
        (g_storageContext.metadata.pendingSlot != FIRMWARE_SLOT_NONE)) {
        return 1;
    }

    if ((service_ota_confirm_install(&service) != PLATFORM_ERR_OK) ||
        (service.context.state != SERVICE_OTA_STATE_REBOOT_REQUIRED) ||
        (g_storageContext.commitCount != 3U) ||
        (g_storageContext.metadata.pendingSlot != FIRMWARE_SLOT_B) ||
        (g_storageContext.metadata.upgradeState != FIRMWARE_UPGRADE_STATE_PENDING) ||
        (service_ota_confirm_install(&service) != PLATFORM_ERR_INVALID_STATE)) {
        return 1;
    }

    return 0;
}

static int test_invalid_image_keeps_target_invalid(void)
{
    firmware_storage_t storage;
    service_uart_t uart;
    service_ota_t service;
    uint32_t nowMs = 0U;

    test_storage_reset();
    g_storageContext.validation = FIRMWARE_IMAGE_VALIDATION_INVALID_PAYLOAD_CRC;
    if ((test_prepare_service(&service, &storage, &uart) != 0) ||
        (service_ota_start(&service) != PLATFORM_ERR_OK) ||
        (test_send_valid_image(&service, &nowMs) != 2) ||
        (service.context.state != SERVICE_OTA_STATE_FAILED) ||
        (g_storageContext.metadata.slotBState != FIRMWARE_SLOT_STATE_INVALID) ||
        (g_storageContext.metadata.pendingSlot != FIRMWARE_SLOT_NONE) ||
        (g_storageContext.commitCount != 1U)) {
        return 1;
    }

    return 0;
}

static int test_metadata_failure_can_retry(void)
{
    firmware_storage_t storage;
    service_uart_t uart;
    service_ota_t service;

    test_storage_reset();
    g_storageContext.commitResult = PLATFORM_ERR_IO;
    if ((test_prepare_service(&service, &storage, &uart) != 0) ||
        (service_ota_start(&service) != PLATFORM_ERR_IO) ||
        (service.context.state != SERVICE_OTA_STATE_FAILED)) {
        return 1;
    }

    g_storageContext.commitResult = PLATFORM_ERR_OK;
    return (service_ota_start(&service) == PLATFORM_ERR_OK) &&
                   (service.context.state == SERVICE_OTA_STATE_RECEIVING) ? 0 : 1;
}

static int test_invalid_transitions_and_power_loss_state(void)
{
    firmware_storage_t storage;
    service_uart_t uart;
    service_ota_t service;
    service_ota_t restartedService;
    service_ota_status_t status;
    uint8_t cancel = YMODEM_CAN;

    test_storage_reset();
    if ((test_prepare_service(&service, &storage, &uart) != 0) ||
        (service_ota_confirm_install(&service) != PLATFORM_ERR_INVALID_STATE) ||
        (service_ota_start(&service) != PLATFORM_ERR_OK) ||
        (service_ota_start(&service) != PLATFORM_ERR_INVALID_STATE)) {
        return 1;
    }

    if ((service_ota_process(&service, &cancel, 1U, 0U) != PLATFORM_ERR_CANCELED) ||
        (service.context.state != SERVICE_OTA_STATE_FAILED) ||
        (g_storageContext.metadata.pendingSlot != FIRMWARE_SLOT_NONE) ||
        (service_ota_start(&service) != PLATFORM_ERR_OK) ||
        (service.context.state != SERVICE_OTA_STATE_RECEIVING)) {
        return 1;
    }

    g_storageContext.metadata.slotBState = FIRMWARE_SLOT_STATE_VALID;
    g_storageContext.metadata.pendingSlot = FIRMWARE_SLOT_B;
    g_storageContext.metadata.upgradeState = FIRMWARE_UPGRADE_STATE_PENDING;
    if (test_prepare_service(&restartedService, &storage, &uart) != 0) {
        return 1;
    }

    if ((service_ota_get_status(&restartedService, &status) != PLATFORM_ERR_OK) ||
        (status.state != SERVICE_OTA_STATE_IDLE) ||
        (service_ota_start(&restartedService) != PLATFORM_ERR_INVALID_STATE) ||
        (restartedService.context.state != SERVICE_OTA_STATE_FAILED) ||
        (g_storageContext.metadata.pendingSlot != FIRMWARE_SLOT_B) ||
        (g_storageContext.metadata.upgradeState != FIRMWARE_UPGRADE_STATE_PENDING)) {
        return 1;
    }

    return 0;
}

static int test_runtime_abort_enters_failed(void)
{
    firmware_storage_t storage;
    service_uart_t uart;
    service_ota_t service;

    test_storage_reset();
    if ((test_prepare_service(&service, &storage, &uart) != 0) ||
        (service_ota_start(&service) != PLATFORM_ERR_OK) ||
        (service_ota_abort(&service, PLATFORM_ERR_IO) != PLATFORM_ERR_IO) ||
        (service.context.state != SERVICE_OTA_STATE_FAILED) ||
        (service.context.event != SERVICE_OTA_EVENT_FAILED) ||
        (g_storageContext.metadata.pendingSlot != FIRMWARE_SLOT_NONE) ||
        (service_ota_start(&service) != PLATFORM_ERR_OK)) {
        return 1;
    }

    return 0;
}
//******************************** Private Functions ************************//

//******************************** Public Functions *************************//
platform_error_t service_uart_write(
    service_uart_t *service,
    const uint8_t *data,
    platform_size_t dataLength,
    uint32_t timeoutMs)
{
    (void)timeoutMs;

    if ((service == NULL) || (data == NULL) || (dataLength > sizeof(service->txData))) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (service->txLength + dataLength > sizeof(service->txData)) {
        return PLATFORM_ERR_OVERFLOW;
    }

    (void)memcpy(&service->txData[service->txLength], data, dataLength);
    service->txLength += dataLength;
    return PLATFORM_ERR_OK;
}

platform_error_t firmware_storage_load_metadata(
    firmware_storage_t *storage,
    firmware_metadata_t *metadata,
    firmware_metadata_copy_id_t *sourceCopy)
{
    (void)storage;

    if ((metadata == NULL) || (sourceCopy == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    g_storageContext.loadCount++;
    if (g_storageContext.loadResult != PLATFORM_ERR_OK) {
        return g_storageContext.loadResult;
    }

    *metadata = g_storageContext.metadata;
    *sourceCopy = g_storageContext.metadataCopy;
    return PLATFORM_ERR_OK;
}

platform_error_t firmware_storage_commit_metadata(
    firmware_storage_t *storage,
    const firmware_metadata_t *metadata,
    firmware_metadata_copy_id_t *committedCopy)
{
    (void)storage;

    if ((metadata == NULL) || (committedCopy == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    g_storageContext.commitCount++;
    if (g_storageContext.commitResult != PLATFORM_ERR_OK) {
        return g_storageContext.commitResult;
    }

    g_storageContext.metadata = *metadata;
    g_storageContext.metadata.sequence++;
    g_storageContext.metadataCopy =
        (g_storageContext.metadataCopy == FIRMWARE_METADATA_COPY_A) ?
        FIRMWARE_METADATA_COPY_B : FIRMWARE_METADATA_COPY_A;
    *committedCopy = g_storageContext.metadataCopy;
    return PLATFORM_ERR_OK;
}

platform_error_t firmware_storage_erase_slot(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    uint32_t payloadSize)
{
    (void)storage;
    (void)payloadSize;
    g_storageContext.eraseCount++;
    g_storageContext.lastEraseSlot = slot;
    return g_storageContext.eraseResult;
}

platform_error_t firmware_storage_write_payload(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    uint32_t payloadOffset,
    const uint8_t *data,
    uint32_t length)
{
    (void)storage;
    (void)payloadOffset;
    (void)data;
    (void)length;
    g_storageContext.payloadCount++;
    g_storageContext.lastPayloadSlot = slot;
    return g_storageContext.payloadResult;
}

platform_error_t firmware_storage_write_header(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    const uint8_t rawHeader[FIRMWARE_IMAGE_HEADER_SIZE])
{
    (void)storage;
    (void)rawHeader;
    g_storageContext.headerCount++;
    g_storageContext.lastHeaderSlot = slot;
    return g_storageContext.headerResult;
}

platform_error_t firmware_storage_validate_image(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    firmware_image_header_t *header,
    firmware_image_validation_t *validation)
{
    (void)storage;
    (void)slot;

    if ((header == NULL) || (validation == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    g_storageContext.validateCount++;
    *validation = g_storageContext.validation;
    return g_storageContext.validateResult;
}

int main(void)
{
    if (test_normal_two_confirm_flow() != 0) {
        (void)printf("S07 OTA Service host test failed.\n");
        return 1;
    }
    if (test_invalid_image_keeps_target_invalid() != 0) {
        (void)printf("S07 OTA Service host test failed.\n");
        return 1;
    }
    if (test_metadata_failure_can_retry() != 0) {
        (void)printf("S07 OTA Service host test failed.\n");
        return 1;
    }
    if (test_invalid_transitions_and_power_loss_state() != 0) {
        (void)printf("S07 OTA Service host test failed.\n");
        return 1;
    }
    if (test_runtime_abort_enters_failed() != 0) {
        (void)printf("S07 OTA Service host test failed.\n");
        return 1;
    }

    (void)printf("S07 OTA Service host test passed.\n");
    return 0;
}
//******************************** Public Functions *************************//
