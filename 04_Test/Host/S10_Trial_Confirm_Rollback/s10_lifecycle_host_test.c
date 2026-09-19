/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s10_lifecycle_host_test.c
 * @brief S10 Strict Firmware Lifecycle Confirmation Host Test。
 * @author YaoQian Wang
 * @date 2026-09-19
 * @version V1.0
 * @note 本测试不进入 Keil 工程，不参与固件构建。
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "firmware_lifecycle.h"
//******************************** Includes *********************************//

//******************************** Variables ********************************//
static firmware_storage_t g_storage;
static firmware_metadata_t g_metadata;
static firmware_image_header_t g_header;
static platform_error_t g_initialLoadResult;
static platform_error_t g_reloadLoadResult;
static platform_error_t g_validateResult;
static platform_error_t g_commitResult;
static firmware_image_validation_t g_validation;
static firmware_metadata_t g_committedMetadata;
static uint32_t g_loadCount;
static uint32_t g_validateCount;
static uint32_t g_commitCount;
static uint8_t g_commitFailureStage;
static uint8_t g_corruptReload;
//******************************** Variables ********************************//

//******************************** Private Functions ************************//
static void test_expect(uint8_t condition, const char *message)
{
    if (condition == 0U) {
        (void)fprintf(stderr, "S10 lifecycle host test failed: %s\n", message);
        (void)fflush(stderr);
        exit(1);
    }
}

static void test_reset_fixture(void)
{
    (void)memset(&g_storage, 0, sizeof(g_storage));
    (void)memset(&g_metadata, 0, sizeof(g_metadata));
    (void)memset(&g_header, 0, sizeof(g_header));
    (void)memset(&g_committedMetadata, 0, sizeof(g_committedMetadata));

    g_metadata.sequence = 10U;
    g_metadata.confirmedSlot = FIRMWARE_SLOT_A;
    g_metadata.pendingSlot = FIRMWARE_SLOT_B;
    g_metadata.slotAState = FIRMWARE_SLOT_STATE_VALID;
    g_metadata.slotBState = FIRMWARE_SLOT_STATE_VALID;
    g_metadata.upgradeState = FIRMWARE_UPGRADE_STATE_TRIAL;
    g_metadata.confirmedVersion.major = 1U;

    g_header.version.major = 2U;
    g_header.imageSize = 1024U;
    g_validation = FIRMWARE_IMAGE_VALIDATION_VALID;
    g_initialLoadResult = PLATFORM_ERR_OK;
    g_reloadLoadResult = PLATFORM_ERR_OK;
    g_validateResult = PLATFORM_ERR_OK;
    g_commitResult = PLATFORM_ERR_OK;
    g_loadCount = 0U;
    g_validateCount = 0U;
    g_commitCount = 0U;
    g_commitFailureStage = 0U;
    g_corruptReload = 0U;
}

static void test_expect_no_commit(void)
{
    test_expect(g_commitCount == 0U, "invalid confirmation must not commit metadata");
}

static void test_confirm_success(void)
{
    platform_error_t result;

    test_reset_fixture();
    result = firmware_lifecycle_confirm(&g_storage);
    test_expect(result == PLATFORM_ERR_OK, "TRIAL confirmation succeeds");
    test_expect(g_validateCount == 1U, "confirmation validates pending image once");
    test_expect(g_commitCount == 1U, "confirmation commits metadata once");
    test_expect(g_loadCount == 2U, "confirmation reloads metadata after commit");
    test_expect(g_committedMetadata.confirmedSlot == FIRMWARE_SLOT_B,
                "confirmed slot becomes pending slot");
    test_expect(g_committedMetadata.pendingSlot == FIRMWARE_SLOT_NONE,
                "pending slot is cleared after confirmation");
    test_expect(g_committedMetadata.upgradeState == FIRMWARE_UPGRADE_STATE_NONE,
                "upgrade state returns to NONE after confirmation");
    test_expect(g_committedMetadata.confirmedVersion.major == 2U,
                "confirmed version comes from validated image header");
}

static void test_state_gate(void)
{
    platform_error_t result;

    test_reset_fixture();
    g_metadata.upgradeState = FIRMWARE_UPGRADE_STATE_NONE;
    g_metadata.pendingSlot = FIRMWARE_SLOT_NONE;
    result = firmware_lifecycle_confirm(&g_storage);
    test_expect(result != PLATFORM_ERR_OK, "non-TRIAL state is rejected");
    test_expect_no_commit();

    test_reset_fixture();
    g_metadata.pendingSlot = FIRMWARE_SLOT_NONE;
    result = firmware_lifecycle_confirm(&g_storage);
    test_expect(result != PLATFORM_ERR_OK, "missing pending slot is rejected");
    test_expect_no_commit();

    test_reset_fixture();
    g_metadata.confirmedSlot = FIRMWARE_SLOT_B;
    result = firmware_lifecycle_confirm(&g_storage);
    test_expect(result != PLATFORM_ERR_OK, "pending slot equal to confirmed slot is rejected");
    test_expect_no_commit();

    test_reset_fixture();
    g_metadata.pendingSlot = (firmware_slot_t)2U;
    result = firmware_lifecycle_confirm(&g_storage);
    test_expect(result != PLATFORM_ERR_OK, "invalid pending slot is rejected");
    test_expect_no_commit();

    test_reset_fixture();
    g_metadata.slotBState = FIRMWARE_SLOT_STATE_EMPTY;
    result = firmware_lifecycle_confirm(&g_storage);
    test_expect(result != PLATFORM_ERR_OK, "pending slot without VALID state is rejected");
    test_expect_no_commit();
}

static void test_image_gate(void)
{
    platform_error_t result;

    test_reset_fixture();
    g_validation = FIRMWARE_IMAGE_VALIDATION_INVALID_HEADER_CRC;
    result = firmware_lifecycle_confirm(&g_storage);
    test_expect(result != PLATFORM_ERR_OK, "invalid header is rejected");
    test_expect_no_commit();

    test_reset_fixture();
    g_header.imageSize = FIRMWARE_SLOT_PAYLOAD_CAPACITY + 1U;
    result = firmware_lifecycle_confirm(&g_storage);
    test_expect(result != PLATFORM_ERR_OK, "image size overflow is rejected");
    test_expect_no_commit();

    test_reset_fixture();
    g_validation = FIRMWARE_IMAGE_VALIDATION_INVALID_PAYLOAD_CRC;
    result = firmware_lifecycle_confirm(&g_storage);
    test_expect(result != PLATFORM_ERR_OK, "payload CRC failure is rejected");
    test_expect_no_commit();

    test_reset_fixture();
    g_validateResult = PLATFORM_ERR_IO;
    result = firmware_lifecycle_confirm(&g_storage);
    test_expect(result == PLATFORM_ERR_IO, "image validation I/O failure propagates");
    test_expect_no_commit();
}

static void test_commit_gate(void)
{
    platform_error_t result;

    test_reset_fixture();
    g_commitFailureStage = 1U;
    g_commitResult = PLATFORM_ERR_IO;
    result = firmware_lifecycle_confirm(&g_storage);
    test_expect(result == PLATFORM_ERR_IO, "metadata body failure propagates");
    test_expect(g_commitCount == 1U, "metadata body failure is observed");

    test_reset_fixture();
    g_commitFailureStage = 2U;
    g_commitResult = PLATFORM_ERR_IO;
    result = firmware_lifecycle_confirm(&g_storage);
    test_expect(result == PLATFORM_ERR_IO, "metadata marker failure propagates");
    test_expect(g_commitCount == 1U, "metadata marker failure is observed");

    test_reset_fixture();
    g_corruptReload = 1U;
    result = firmware_lifecycle_confirm(&g_storage);
    test_expect(result != PLATFORM_ERR_OK, "post-commit metadata mismatch is rejected");
    test_expect(g_commitCount == 1U, "post-commit verification follows one commit");
}
//******************************** Private Functions ************************//

//******************************** Test Doubles ******************************//
platform_error_t firmware_storage_load_metadata(
    firmware_storage_t *storage,
    firmware_metadata_t *metadata,
    firmware_metadata_copy_id_t *sourceCopy)
{
    if ((storage == NULL) || (metadata == NULL) || (sourceCopy == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    g_loadCount++;
    if ((g_loadCount == 1U) && (g_initialLoadResult != PLATFORM_ERR_OK)) {
        return g_initialLoadResult;
    }
    if ((g_loadCount > 1U) && (g_reloadLoadResult != PLATFORM_ERR_OK)) {
        return g_reloadLoadResult;
    }

    if ((g_loadCount > 1U) && (g_corruptReload != 0U)) {
        *metadata = g_committedMetadata;
        metadata->upgradeState = FIRMWARE_UPGRADE_STATE_TRIAL;
    } else if (g_commitCount != 0U) {
        *metadata = g_committedMetadata;
    } else {
        *metadata = g_metadata;
    }
    *sourceCopy = FIRMWARE_METADATA_COPY_A;
    return PLATFORM_ERR_OK;
}

platform_error_t firmware_storage_validate_image(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    firmware_image_header_t *header,
    firmware_image_validation_t *validation)
{
    (void)slot;
    if ((storage == NULL) || (header == NULL) || (validation == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    g_validateCount++;
    *header = g_header;
    *validation = g_validation;
    return g_validateResult;
}

platform_error_t firmware_storage_commit_metadata(
    firmware_storage_t *storage,
    const firmware_metadata_t *metadata,
    firmware_metadata_copy_id_t *committedCopy)
{
    if ((storage == NULL) || (metadata == NULL) || (committedCopy == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    g_commitCount++;
    if ((g_commitFailureStage == 1U) || (g_commitFailureStage == 2U)) {
        return g_commitResult;
    }

    g_committedMetadata = *metadata;
    *committedCopy = FIRMWARE_METADATA_COPY_B;
    return PLATFORM_ERR_OK;
}
//******************************** Test Doubles ******************************//

//******************************** Main Function ****************************//
int main(void)
{
    test_confirm_success();
    test_state_gate();
    test_image_gate();
    test_commit_gate();
    (void)printf("S10 lifecycle host test passed; strict confirmation transaction verified.\n");
    return 0;
}
//******************************** Main Function ****************************//
