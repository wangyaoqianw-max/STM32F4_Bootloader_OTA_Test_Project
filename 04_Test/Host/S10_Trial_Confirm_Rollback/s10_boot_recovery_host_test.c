/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s10_boot_recovery_host_test.c
 * @brief S10 Bootloader Pending Install/Confirmed Restore Host Test。
 * @author YaoQian Wang
 * @date 2026-09-19
 * @version V1.0
 * @note 本测试不进入 Keil 工程，不参与固件构建。
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "boot_crc32.h"
#include "boot_installer.h"
#include "boot_metadata.h"
#include "boot_prevalidate.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define TEST_IMAGE_SIZE                  (300U)
#define TEST_METADATA_CRC_OFFSET         (0x78U)
#define TEST_METADATA_COMMIT_OFFSET      (0x7CU)
#define TEST_METADATA_CONFIRMED_OFFSET   (0x0DU)
#define TEST_METADATA_PENDING_OFFSET     (0x18U)
#define TEST_METADATA_UPGRADE_OFFSET     (0x19U)
//******************************** Defines **********************************//

//******************************** Variables ********************************//
static uint8_t g_payloadA[TEST_IMAGE_SIZE];
static uint8_t g_payloadB[TEST_IMAGE_SIZE];
static uint8_t g_headerA[BOOT_FIRMWARE_HEADER_SIZE];
static uint8_t g_headerB[BOOT_FIRMWARE_HEADER_SIZE];
static uint8_t g_metadataA[BOOT_METADATA_COPY_SIZE];
static uint8_t g_metadataB[BOOT_METADATA_COPY_SIZE];
static uint8_t g_internalApp[TEST_IMAGE_SIZE];
static uint32_t g_eraseCount;
//******************************** Variables ********************************//

//******************************** Private Functions ************************//
static void test_expect(uint8_t condition, const char *message)
{
    if (condition == 0U) {
        (void)fprintf(stderr, "S10 Bootloader recovery host test failed: %s\n", message);
        (void)fflush(stderr);
        exit(1);
    }
}

static void test_write_u16_le(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
}

static void test_write_u32_le(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
    data[2] = (uint8_t)(value >> 16U);
    data[3] = (uint8_t)(value >> 24U);
}

static void test_build_header(
    uint8_t header[BOOT_FIRMWARE_HEADER_SIZE],
    const uint8_t payload[TEST_IMAGE_SIZE],
    uint16_t major)
{
    (void)memset(header, 0, BOOT_FIRMWARE_HEADER_SIZE);
    test_write_u32_le(&header[0x00U], BOOT_FIRMWARE_IMAGE_MAGIC);
    test_write_u16_le(&header[0x04U], BOOT_FIRMWARE_FORMAT_VERSION);
    test_write_u16_le(&header[0x06U], BOOT_FIRMWARE_HEADER_SIZE);
    test_write_u16_le(&header[0x08U], major);
    test_write_u16_le(&header[0x0AU], 0U);
    test_write_u16_le(&header[0x0CU], 0U);
    test_write_u32_le(&header[0x10U], TEST_IMAGE_SIZE);
    test_write_u32_le(&header[0x14U], boot_crc32_calculate(payload, TEST_IMAGE_SIZE));
    test_write_u32_le(&header[0x3CU], boot_crc32_calculate(header, 0x3CU));
}

static void test_build_image(
    uint8_t payload[TEST_IMAGE_SIZE],
    uint8_t header[BOOT_FIRMWARE_HEADER_SIZE],
    uint16_t major,
    uint8_t fill)
{
    uint32_t index;

    (void)memset(payload, fill, TEST_IMAGE_SIZE);
    payload[0] = 0x00U;
    payload[1] = 0x10U;
    payload[2] = 0x00U;
    payload[3] = 0x20U;
    payload[4] = 0x01U;
    payload[5] = 0x01U;
    payload[6] = 0x01U;
    payload[7] = 0x08U;
    for (index = 8U; index < TEST_IMAGE_SIZE; index++) {
        payload[index] = (uint8_t)(fill + index);
    }
    test_build_header(header, payload, major);
}

static void test_mark_metadata_committed(uint8_t raw[BOOT_METADATA_COPY_SIZE])
{
    test_write_u32_le(&raw[TEST_METADATA_COMMIT_OFFSET], BOOT_METADATA_COMMIT_MARKER);
}

static void test_build_metadata(boot_upgrade_state_t upgradeState, uint16_t confirmedMajor)
{
    boot_firmware_metadata_t metadata = {0};

    metadata.sequence = 1U;
    metadata.confirmedSlot = BOOT_FIRMWARE_SLOT_A;
    metadata.pendingSlot = (upgradeState == BOOT_UPGRADE_STATE_NONE) ?
                           BOOT_FIRMWARE_SLOT_NONE : BOOT_FIRMWARE_SLOT_B;
    metadata.slotAState = BOOT_FIRMWARE_SLOT_STATE_VALID;
    metadata.slotBState = (upgradeState == BOOT_UPGRADE_STATE_NONE) ?
                          BOOT_FIRMWARE_SLOT_STATE_EMPTY :
                          BOOT_FIRMWARE_SLOT_STATE_VALID;
    metadata.upgradeState = upgradeState;
    metadata.confirmedVersion.major = confirmedMajor;
    test_expect(boot_metadata_encode_uncommitted(&metadata, g_metadataA) == BOOT_CONTRACT_OK,
                "metadata fixture encodes");
    test_mark_metadata_committed(g_metadataA);
    (void)memcpy(g_metadataB, g_metadataA, sizeof(g_metadataB));
}

static void test_reset_fixture(
    boot_upgrade_state_t upgradeState,
    uint16_t confirmedMajor,
    uint16_t imageAMajor,
    uint16_t imageBMajor)
{
    test_build_image(g_payloadA, g_headerA, imageAMajor, 0x20U);
    test_build_image(g_payloadB, g_headerB, imageBMajor, 0x80U);
    test_build_metadata(upgradeState, confirmedMajor);
    (void)memset(g_internalApp, 0xFF, sizeof(g_internalApp));
    g_eraseCount = 0U;
}

static boot_prevalidate_context_t test_context(void)
{
    static boot_w25q64_t flash = {0};
    static boot_at24c02_t eeprom = {0};
    boot_prevalidate_context_t context = {&flash, &eeprom};

    return context;
}

static void test_mutate_metadata_same_slot(void)
{
    uint32_t metadataCrc32;

    g_metadataA[TEST_METADATA_PENDING_OFFSET] = BOOT_FIRMWARE_SLOT_A;
    metadataCrc32 = boot_crc32_calculate(g_metadataA, TEST_METADATA_CRC_OFFSET);
    test_write_u32_le(&g_metadataA[TEST_METADATA_CRC_OFFSET], metadataCrc32);
    test_mark_metadata_committed(g_metadataA);
    (void)memcpy(g_metadataB, g_metadataA, sizeof(g_metadataB));
}

static void test_rebuild_payload_header_a(void)
{
    test_build_header(g_headerA, g_payloadA, 1U);
}
//******************************** Private Functions ************************//

//******************************** Test Doubles ******************************//
boot_driver_status_t boot_w25q64_read(
    boot_w25q64_t *flash,
    uint32_t address,
    uint8_t *data,
    uint32_t length)
{
    uint32_t slotBase;
    uint32_t offset;
    const uint8_t *source;

    (void)flash;
    if (data == NULL) {
        return BOOT_DRIVER_ERR_NULL;
    }
    if ((address == BOOT_FIRMWARE_SLOT_A_BASE) &&
        (length == BOOT_FIRMWARE_HEADER_SIZE)) {
        (void)memcpy(data, g_headerA, length);
        return BOOT_DRIVER_OK;
    }
    if ((address == BOOT_FIRMWARE_SLOT_B_BASE) &&
        (length == BOOT_FIRMWARE_HEADER_SIZE)) {
        (void)memcpy(data, g_headerB, length);
        return BOOT_DRIVER_OK;
    }
    if ((address >= (BOOT_FIRMWARE_SLOT_A_BASE + BOOT_FIRMWARE_PAYLOAD_OFFSET)) &&
        (address < (BOOT_FIRMWARE_SLOT_A_BASE + BOOT_FIRMWARE_PAYLOAD_OFFSET +
                    TEST_IMAGE_SIZE))) {
        slotBase = BOOT_FIRMWARE_SLOT_A_BASE;
        source = g_payloadA;
    } else if ((address >= (BOOT_FIRMWARE_SLOT_B_BASE + BOOT_FIRMWARE_PAYLOAD_OFFSET)) &&
               (address < (BOOT_FIRMWARE_SLOT_B_BASE + BOOT_FIRMWARE_PAYLOAD_OFFSET +
                           TEST_IMAGE_SIZE))) {
        slotBase = BOOT_FIRMWARE_SLOT_B_BASE;
        source = g_payloadB;
    } else {
        return BOOT_DRIVER_ERR_RANGE;
    }
    offset = address - slotBase - BOOT_FIRMWARE_PAYLOAD_OFFSET;
    if ((offset > TEST_IMAGE_SIZE) || (length > (TEST_IMAGE_SIZE - offset))) {
        return BOOT_DRIVER_ERR_RANGE;
    }
    (void)memcpy(data, &source[offset], length);
    return BOOT_DRIVER_OK;
}

boot_driver_status_t boot_at24c02_read(
    boot_at24c02_t *eeprom,
    uint32_t address,
    uint8_t *data,
    uint16_t length)
{
    (void)eeprom;
    if ((data == NULL) || (length != BOOT_METADATA_COPY_SIZE)) {
        return BOOT_DRIVER_ERR_RANGE;
    }
    if (address == BOOT_METADATA_COPY_A_ADDRESS) {
        (void)memcpy(data, g_metadataA, length);
        return BOOT_DRIVER_OK;
    }
    if (address == BOOT_METADATA_COPY_B_ADDRESS) {
        (void)memcpy(data, g_metadataB, length);
        return BOOT_DRIVER_OK;
    }
    return BOOT_DRIVER_ERR_RANGE;
}

boot_driver_status_t boot_internal_flash_erase_app(void)
{
    g_eraseCount++;
    (void)memset(g_internalApp, 0xFF, sizeof(g_internalApp));
    return BOOT_DRIVER_OK;
}

boot_driver_status_t boot_internal_flash_write(
    uint32_t appOffset,
    const uint8_t *data,
    uint32_t length)
{
    if ((data == NULL) || (appOffset > sizeof(g_internalApp)) ||
        (length > (sizeof(g_internalApp) - appOffset))) {
        return BOOT_DRIVER_ERR_RANGE;
    }
    (void)memcpy(&g_internalApp[appOffset], data, length);
    return BOOT_DRIVER_OK;
}

boot_driver_status_t boot_internal_flash_read(
    uint32_t appOffset,
    uint8_t *data,
    uint32_t length)
{
    if ((data == NULL) || (appOffset > sizeof(g_internalApp)) ||
        (length > (sizeof(g_internalApp) - appOffset))) {
        return BOOT_DRIVER_ERR_RANGE;
    }
    (void)memcpy(data, &g_internalApp[appOffset], length);
    return BOOT_DRIVER_OK;
}
//******************************** Test Doubles ******************************//

//******************************** Functions ********************************//
int main(void)
{
    boot_prevalidate_context_t context;
    boot_prevalidated_image_t image = {0};
    boot_installer_context_t installerContext = {0};
    boot_installer_result_t installerResult;

    test_reset_fixture(BOOT_UPGRADE_STATE_PENDING, 1U, 1U, 2U);
    context = test_context();
    test_expect(boot_prevalidate_candidate(&context, &image) == BOOT_PREVALIDATE_VALID,
                "PENDING prevalidation succeeds");
    test_expect(image.sourceSlot == BOOT_FIRMWARE_SLOT_B,
                "PENDING source is pending Slot B");

    test_reset_fixture(BOOT_UPGRADE_STATE_TRIAL, 1U, 1U, 2U);
    context = test_context();
    test_expect(boot_prevalidate_confirmed(&context, &image) == BOOT_PREVALIDATE_VALID,
                "TRIAL confirmed prevalidation succeeds");
    test_expect(image.sourceSlot == BOOT_FIRMWARE_SLOT_A,
                "TRIAL source is confirmed Slot A");

    test_reset_fixture(BOOT_UPGRADE_STATE_ROLLBACK, 1U, 1U, 2U);
    context = test_context();
    test_expect(boot_prevalidate_confirmed(&context, &image) == BOOT_PREVALIDATE_VALID,
                "ROLLBACK confirmed prevalidation succeeds");

    test_reset_fixture(BOOT_UPGRADE_STATE_NONE, 1U, 1U, 2U);
    context = test_context();
    test_expect(boot_prevalidate_confirmed(&context, &image) ==
                    BOOT_PREVALIDATE_NO_RECOVERY,
                "NONE is not a recovery state");

    test_reset_fixture(BOOT_UPGRADE_STATE_TRIAL, 1U, 2U, 2U);
    context = test_context();
    test_expect(boot_prevalidate_confirmed(&context, &image) ==
                    BOOT_PREVALIDATE_CONFIRMED_VERSION_MISMATCH,
                "confirmed version mismatch is rejected");

    test_reset_fixture(BOOT_UPGRADE_STATE_TRIAL, 1U, 1U, 2U);
    g_payloadA[40] ^= 0x01U;
    context = test_context();
    test_expect(boot_prevalidate_confirmed(&context, &image) ==
                    BOOT_PREVALIDATE_PAYLOAD_CRC_INVALID,
                "confirmed payload CRC is checked");

    test_reset_fixture(BOOT_UPGRADE_STATE_TRIAL, 1U, 1U, 2U);
    g_payloadA[4] = 0x00U;
    test_rebuild_payload_header_a();
    context = test_context();
    test_expect(boot_prevalidate_confirmed(&context, &image) ==
                    BOOT_PREVALIDATE_VECTOR_INVALID,
                "confirmed vector is checked");

    test_reset_fixture(BOOT_UPGRADE_STATE_PENDING, 1U, 1U, 2U);
    test_mutate_metadata_same_slot();
    context = test_context();
    test_expect(boot_prevalidate_candidate(&context, &image) ==
                    BOOT_PREVALIDATE_METADATA_INVALID,
                "same-slot pending is rejected before image access");

    test_reset_fixture(BOOT_UPGRADE_STATE_PENDING, 1U, 1U, 2U);
    context = test_context();
    installerContext.flash = context.flash;
    installerContext.eeprom = context.eeprom;
    installerResult = boot_installer_install_pending(&installerContext, &image);
    test_expect(installerResult == BOOT_INSTALLER_OK,
                "pending installer succeeds");
    test_expect((image.sourceSlot == BOOT_FIRMWARE_SLOT_B) &&
                    (memcmp(g_internalApp, g_payloadB, sizeof(g_internalApp)) == 0),
                "pending installer uses Slot B source");

    test_reset_fixture(BOOT_UPGRADE_STATE_TRIAL, 1U, 1U, 2U);
    installerResult = boot_installer_restore_confirmed(&installerContext, &image);
    test_expect(installerResult == BOOT_INSTALLER_OK,
                "confirmed restore succeeds");
    test_expect((image.sourceSlot == BOOT_FIRMWARE_SLOT_A) &&
                    (memcmp(g_internalApp, g_payloadA, sizeof(g_internalApp)) == 0),
                "confirmed restore uses Slot A source");

    test_reset_fixture(BOOT_UPGRADE_STATE_TRIAL, 1U, 2U, 2U);
    installerResult = boot_installer_restore_confirmed(&installerContext, &image);
    test_expect(installerResult == BOOT_INSTALLER_PREVALIDATION_FAILED,
                "confirmed prevalidation failure stops restore");
    test_expect(g_eraseCount == 0U,
                "confirmed prevalidation failure does not erase APP");

    (void)printf("S10 Bootloader recovery host test passed; shared validation/install core verified.\n");
    return 0;
}
//******************************** Functions ********************************//
