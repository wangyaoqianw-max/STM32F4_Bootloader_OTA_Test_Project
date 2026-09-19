/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s09_installer_host_test.c
 * @brief S09 Installer destructive gate 与分块安装 Host Test。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 * @note 本测试不进入 Keil 工程，不参与固件构建。
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "boot_crc32.h"
#include "boot_installer.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define TEST_PAYLOAD_SIZE (300U)
#define TEST_CANDIDATE_ADDRESS \
    (BOOT_FIRMWARE_SLOT_B_BASE + BOOT_FIRMWARE_PAYLOAD_OFFSET)
//******************************** Defines **********************************//

//******************************** Variables ********************************//
static uint8_t g_payload[TEST_PAYLOAD_SIZE];
static uint8_t g_internalApp[TEST_PAYLOAD_SIZE];
static uint8_t g_prevalidatePass;
static uint8_t g_externalReadFail;
static uint8_t g_internalWriteFail;
static uint8_t g_internalReadbackCorrupt;
static uint32_t g_eraseCount;
static uint32_t g_writeCount;
//******************************** Variables ********************************//

//******************************** Private Functions ************************//
static void test_expect(uint8_t condition, const char *message)
{
    if (condition == 0U) {
        (void)fprintf(stderr, "S09 installer host test failed: %s\n", message);
        (void)fflush(stderr);
        exit(1);
    }
}

static void test_build_payload(void)
{
    uint32_t index;

    (void)memset(g_payload, 0, sizeof(g_payload));
    g_payload[0] = 0x00U;
    g_payload[1] = 0x10U;
    g_payload[2] = 0x00U;
    g_payload[3] = 0x20U;
    g_payload[4] = 0x01U;
    g_payload[5] = 0x10U;
    g_payload[6] = 0x00U;
    g_payload[7] = 0x08U;
    for (index = 8U; index < sizeof(g_payload); index++) {
        g_payload[index] = (uint8_t)(index ^ 0x5AU);
    }
}

static void test_reset_state(void)
{
    g_prevalidatePass = 1U;
    g_externalReadFail = 0U;
    g_internalWriteFail = 0U;
    g_internalReadbackCorrupt = 0U;
    g_eraseCount = 0U;
    g_writeCount = 0U;
    (void)memset(g_internalApp, 0xFF, sizeof(g_internalApp));
}

static void test_prepare_candidate(boot_prevalidated_image_t *candidate)
{
    (void)memset(candidate, 0, sizeof(*candidate));
    candidate->sourceSlot = BOOT_FIRMWARE_SLOT_B;
    candidate->header.imageSize = TEST_PAYLOAD_SIZE;
    candidate->header.payloadCrc32 = boot_crc32_calculate(
        g_payload, sizeof(g_payload));
    candidate->vector.initialMsp = 0x20001000UL;
    candidate->vector.resetHandler = 0x08001001UL;
}

static void test_prevalidation_gate(void)
{
    boot_installer_context_t context = {0};
    boot_prevalidated_image_t candidate;
    boot_installer_result_t result;

    test_reset_state();
    g_prevalidatePass = 0U;
    result = boot_installer_install_pending(&context, &candidate);
    test_expect(result == BOOT_INSTALLER_PREVALIDATION_FAILED,
                "failed prevalidation stops installer");
    test_expect(g_eraseCount == 0U, "failed prevalidation does not erase APP");
}

static void test_successful_install(void)
{
    boot_installer_context_t context = {0};
    boot_w25q64_t flash = {0};
    boot_at24c02_t eeprom = {0};
    boot_prevalidated_image_t candidate;
    boot_installer_result_t result;

    test_reset_state();
    context.flash = &flash;
    context.eeprom = &eeprom;
    result = boot_installer_install_pending(&context, &candidate);
    test_expect(result == BOOT_INSTALLER_OK, "valid candidate installs successfully");
    test_expect(g_eraseCount == 1U, "valid candidate erases APP once");
    test_expect(g_writeCount == 2U, "300-byte payload uses two bounded writes");
    test_expect(memcmp(g_internalApp, g_payload, sizeof(g_payload)) == 0,
                "internal APP equals candidate payload");
}

static void test_internal_write_failure(void)
{
    boot_installer_context_t context = {0};
    boot_w25q64_t flash = {0};
    boot_at24c02_t eeprom = {0};
    boot_prevalidated_image_t candidate;
    boot_installer_result_t result;

    test_reset_state();
    g_internalWriteFail = 1U;
    context.flash = &flash;
    context.eeprom = &eeprom;
    result = boot_installer_install_pending(&context, &candidate);
    test_expect(result == BOOT_INSTALLER_INTERNAL_WRITE_FAILED,
                "internal write failure is reported");
    test_expect(g_eraseCount == 1U, "write failure occurs after destructive gate");
}

static void test_readback_failure(void)
{
    boot_installer_context_t context = {0};
    boot_w25q64_t flash = {0};
    boot_at24c02_t eeprom = {0};
    boot_prevalidated_image_t candidate;
    boot_installer_result_t result;

    test_reset_state();
    g_internalReadbackCorrupt = 1U;
    context.flash = &flash;
    context.eeprom = &eeprom;
    result = boot_installer_install_pending(&context, &candidate);
    test_expect(result == BOOT_INSTALLER_READBACK_FAILED,
                "read-back mismatch is reported");
}
//******************************** Private Functions ************************//

//******************************** Test Doubles ******************************//
boot_prevalidate_result_t boot_prevalidate_candidate(
    const boot_prevalidate_context_t *context,
    boot_prevalidated_image_t *candidate)
{
    (void)context;
    if ((g_prevalidatePass == 0U) || (candidate == NULL)) {
        return BOOT_PREVALIDATE_INVALID;
    }
    test_prepare_candidate(candidate);
    return BOOT_PREVALIDATE_VALID;
}

boot_prevalidate_result_t boot_prevalidate_confirmed(
    const boot_prevalidate_context_t *context,
    boot_prevalidated_image_t *image)
{
    (void)context;
    (void)image;
    return BOOT_PREVALIDATE_NO_RECOVERY;
}

boot_driver_status_t boot_w25q64_read(
    boot_w25q64_t *flash,
    uint32_t address,
    uint8_t *data,
    uint32_t length)
{
    uint32_t offset;

    (void)flash;
    if ((g_externalReadFail != 0U) || (data == NULL) ||
        (address < TEST_CANDIDATE_ADDRESS)) {
        return BOOT_DRIVER_ERR_RANGE;
    }
    offset = address - TEST_CANDIDATE_ADDRESS;
    if (length > (sizeof(g_payload) - offset)) {
        return BOOT_DRIVER_ERR_RANGE;
    }
    (void)memcpy(data, &g_payload[offset], length);
    return BOOT_DRIVER_OK;
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
    if ((g_internalWriteFail != 0U) || (data == NULL) ||
        (length > (sizeof(g_internalApp) - appOffset))) {
        return BOOT_DRIVER_ERR_RANGE;
    }
    g_writeCount++;
    (void)memcpy(&g_internalApp[appOffset], data, length);
    return BOOT_DRIVER_OK;
}

boot_driver_status_t boot_internal_flash_read(
    uint32_t appOffset,
    uint8_t *data,
    uint32_t length)
{
    if ((data == NULL) || (length > (sizeof(g_internalApp) - appOffset))) {
        return BOOT_DRIVER_ERR_RANGE;
    }
    (void)memcpy(data, &g_internalApp[appOffset], length);
    if (g_internalReadbackCorrupt != 0U) {
        data[0] ^= 0x01U;
    }
    return BOOT_DRIVER_OK;
}

boot_app_vector_result_t boot_validate_vector_values(
    uint32_t initialMsp,
    uint32_t resetHandler)
{
    (void)initialMsp;
    (void)resetHandler;
    return BOOT_APP_VECTOR_VALID;
}
//******************************** Test Doubles ******************************//

//******************************** Main Function ****************************//
int main(void)
{
    test_build_payload();
    test_prevalidation_gate();
    test_successful_install();
    test_internal_write_failure();
    test_readback_failure();
    (void)printf("S09 installer host test passed; destructive gate and read-back verified.\n");
    return 0;
}
//******************************** Main Function ****************************//
