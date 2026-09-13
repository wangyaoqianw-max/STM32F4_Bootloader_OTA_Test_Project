/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_s03_eeprom_test.c
 * @brief S03 AT24C02 RTT + EasyLogger 板测实现
 * @author YaoQian Wang
 * @date 2026-09-13
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "app_s03_eeprom_test.h"

#define LOG_TAG "s03_eeprom"

#include "platform_at24c02.h"
#include "platform_bsp_gpio.h"
#include "platform_def.h"
#include "platform_i2c.h"
#include "project_config.h"
#include "service_log.h"

#include <stddef.h>
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define S03_EEPROM_TEST_MAX_DATA_LENGTH       (10U)
#define S03_EEPROM_TEST_PERSISTENCE_ADDRESS  (0xF0U)
#define S03_EEPROM_TEST_PERSISTENCE_LENGTH    (4U)
//******************************** Defines *********************************//

//******************************** Variables ********************************//
static platform_gpio_t g_s03EepromScl = PLATFORM_GPIO_INITIALIZER;
static platform_gpio_t g_s03EepromSda = PLATFORM_GPIO_INITIALIZER;
static platform_i2c_t g_s03EepromI2c = PLATFORM_I2C_INITIALIZER;
static platform_at24c02_t g_s03Eeprom = PLATFORM_AT24C02_INITIALIZER;

static const uint8_t s_resetPersistenceMarker[
    S03_EEPROM_TEST_PERSISTENCE_LENGTH] = {'S', '0', '3', 'A'};
static const uint8_t s_powerCyclePersistenceMarker[
    S03_EEPROM_TEST_PERSISTENCE_LENGTH] = {'S', '0', '3', 'P'};
//******************************** Variables ********************************//

//******************************** Private Functions *************************//
static void s03_eeprom_log_failure(
    const char *testName,
    uint32_t address,
    platform_size_t dataLength,
    platform_error_t result)
{
    SERVICE_LOG_E(
        "[S03][EEPROM] %s: FAIL address=0x%02lX length=%lu error=%d",
        testName,
        (unsigned long)address,
        (unsigned long)dataLength,
        (int)result);
}

static void s03_eeprom_log_pass(
    const char *testName,
    uint32_t address,
    platform_size_t dataLength,
    platform_error_t result)
{
    SERVICE_LOG_I(
        "[S03][EEPROM] %s: PASS address=0x%02lX length=%lu error=%d",
        testName,
        (unsigned long)address,
        (unsigned long)dataLength,
        (int)result);
}

static platform_error_t s03_eeprom_compare(
    const char *testName,
    uint32_t address,
    const uint8_t *expected,
    const uint8_t *actual,
    platform_size_t dataLength)
{
    platform_size_t index = 0U;

    for (index = 0U; index < dataLength; index++) {
        if (expected[index] != actual[index]) {
            SERVICE_LOG_E(
                "[S03][EEPROM] %s: FAIL address=0x%02lX length=%lu "
                "error=%d index=%lu expected=0x%02X actual=0x%02X",
                testName,
                (unsigned long)address,
                (unsigned long)dataLength,
                (int)PLATFORM_ERR_CHECKSUM,
                (unsigned long)index,
                (unsigned int)expected[index],
                (unsigned int)actual[index]);
            return PLATFORM_ERR_CHECKSUM;
        }
    }

    return PLATFORM_ERR_OK;
}

static platform_error_t s03_eeprom_transfer_test(
    const char *testName,
    uint32_t address,
    const uint8_t *expected,
    platform_size_t dataLength)
{
    uint8_t actual[S03_EEPROM_TEST_MAX_DATA_LENGTH] = {0U};
    platform_error_t result = PLATFORM_ERR_OK;

    if ((expected == NULL) || (dataLength == 0U) ||
        (dataLength > S03_EEPROM_TEST_MAX_DATA_LENGTH)) {
        s03_eeprom_log_failure(testName,
                               address,
                               dataLength,
                               PLATFORM_ERR_INVALID_PARAM);
        return PLATFORM_ERR_INVALID_PARAM;
    }

    result = platform_at24c02_write(&g_s03Eeprom,
                                    address,
                                    expected,
                                    dataLength);
    if (result != PLATFORM_ERR_OK) {
        s03_eeprom_log_failure(testName, address, dataLength, result);
        return result;
    }

    result = platform_at24c02_read(&g_s03Eeprom,
                                   address,
                                   actual,
                                   dataLength);
    if (result != PLATFORM_ERR_OK) {
        s03_eeprom_log_failure(testName, address, dataLength, result);
        return result;
    }

    result = s03_eeprom_compare(testName,
                                address,
                                expected,
                                actual,
                                dataLength);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    s03_eeprom_log_pass(testName, address, dataLength, result);
    return PLATFORM_ERR_OK;
}

static platform_error_t s03_eeprom_test_range_reject(void)
{
    const uint8_t baseline[4] = {0xA5U, 0x5AU, 0x3CU, 0xC3U};
    const uint8_t invalidData[5] = {0x11U, 0x22U, 0x33U, 0x44U, 0x55U};
    uint8_t actual[4] = {0U};
    platform_error_t result = PLATFORM_ERR_OK;

    result = platform_at24c02_write(&g_s03Eeprom,
                                    0xFCU,
                                    baseline,
                                    sizeof(baseline));
    if (result != PLATFORM_ERR_OK) {
        s03_eeprom_log_failure("out-of-range-baseline",
                               0xFCU,
                               sizeof(baseline),
                               result);
        return result;
    }

    result = platform_at24c02_write(&g_s03Eeprom,
                                    0xFCU,
                                    invalidData,
                                    sizeof(invalidData));
    if (result != PLATFORM_ERR_INVALID_PARAM) {
        s03_eeprom_log_failure("out-of-range-reject",
                               0xFCU,
                               sizeof(invalidData),
                               result);
        return PLATFORM_ERR_CHECKSUM;
    }
    s03_eeprom_log_pass("out-of-range-reject",
                        0xFCU,
                        sizeof(invalidData),
                        result);

    result = platform_at24c02_read(&g_s03Eeprom,
                                   0xFCU,
                                   actual,
                                   sizeof(actual));
    if (result != PLATFORM_ERR_OK) {
        s03_eeprom_log_failure("out-of-range-preservation",
                               0xFCU,
                               sizeof(actual),
                               result);
        return result;
    }

    result = s03_eeprom_compare("out-of-range-preservation",
                                0xFCU,
                                baseline,
                                actual,
                                sizeof(baseline));
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    s03_eeprom_log_pass("out-of-range-preservation",
                        0xFCU,
                        sizeof(actual),
                        result);
    return PLATFORM_ERR_OK;
}

static platform_bool_t s03_eeprom_marker_matches(
    const uint8_t *actual,
    const uint8_t *expected)
{
    platform_size_t index = 0U;

    for (index = 0U; index < S03_EEPROM_TEST_PERSISTENCE_LENGTH; index++) {
        if (actual[index] != expected[index]) {
            return PLATFORM_FALSE;
        }
    }

    return PLATFORM_TRUE;
}

static platform_error_t s03_eeprom_test_persistence(void)
{
    uint8_t actual[S03_EEPROM_TEST_PERSISTENCE_LENGTH] = {0U};
    const uint8_t *nextMarker = s_resetPersistenceMarker;
    platform_error_t result = PLATFORM_ERR_OK;

    result = platform_at24c02_read(&g_s03Eeprom,
                                   S03_EEPROM_TEST_PERSISTENCE_ADDRESS,
                                   actual,
                                   S03_EEPROM_TEST_PERSISTENCE_LENGTH);
    if (result != PLATFORM_ERR_OK) {
        s03_eeprom_log_failure("persistence-marker-read",
                               S03_EEPROM_TEST_PERSISTENCE_ADDRESS,
                               S03_EEPROM_TEST_PERSISTENCE_LENGTH,
                               result);
        return result;
    }

    if (s03_eeprom_marker_matches(actual, s_resetPersistenceMarker) !=
        PLATFORM_FALSE) {
        s03_eeprom_log_pass("reset-persistence",
                            S03_EEPROM_TEST_PERSISTENCE_ADDRESS,
                            S03_EEPROM_TEST_PERSISTENCE_LENGTH,
                            PLATFORM_ERR_OK);
        nextMarker = s_powerCyclePersistenceMarker;
        SERVICE_LOG_I(
            "[S03][EEPROM] power-cycle-persistence: ARMED "
            "address=0x%02lX length=%lu error=%d",
            (unsigned long)S03_EEPROM_TEST_PERSISTENCE_ADDRESS,
            (unsigned long)S03_EEPROM_TEST_PERSISTENCE_LENGTH,
            (int)PLATFORM_ERR_OK);
    } else if (s03_eeprom_marker_matches(
                   actual,
                   s_powerCyclePersistenceMarker) != PLATFORM_FALSE) {
        s03_eeprom_log_pass("power-cycle-persistence",
                            S03_EEPROM_TEST_PERSISTENCE_ADDRESS,
                            S03_EEPROM_TEST_PERSISTENCE_LENGTH,
                            PLATFORM_ERR_OK);
        return PLATFORM_ERR_OK;
    } else {
        SERVICE_LOG_I(
            "[S03][EEPROM] reset-persistence: ARMED "
            "address=0x%02lX length=%lu error=%d "
            "expected=%02X%02X%02X%02X actual=%02X%02X%02X%02X",
            (unsigned long)S03_EEPROM_TEST_PERSISTENCE_ADDRESS,
            (unsigned long)S03_EEPROM_TEST_PERSISTENCE_LENGTH,
            (int)PLATFORM_ERR_OK,
            (unsigned int)s_resetPersistenceMarker[0],
            (unsigned int)s_resetPersistenceMarker[1],
            (unsigned int)s_resetPersistenceMarker[2],
            (unsigned int)s_resetPersistenceMarker[3],
            (unsigned int)actual[0],
            (unsigned int)actual[1],
            (unsigned int)actual[2],
            (unsigned int)actual[3]);
    }

    result = platform_at24c02_write(&g_s03Eeprom,
                                    S03_EEPROM_TEST_PERSISTENCE_ADDRESS,
                                    nextMarker,
                                    S03_EEPROM_TEST_PERSISTENCE_LENGTH);
    if (result != PLATFORM_ERR_OK) {
        s03_eeprom_log_failure("persistence-marker-write",
                               S03_EEPROM_TEST_PERSISTENCE_ADDRESS,
                               S03_EEPROM_TEST_PERSISTENCE_LENGTH,
                               result);
        return result;
    }

    SERVICE_LOG_I(
        "[S03][EEPROM] persistence-marker-write: PASS "
        "address=0x%02lX length=%lu error=%d",
        (unsigned long)S03_EEPROM_TEST_PERSISTENCE_ADDRESS,
        (unsigned long)S03_EEPROM_TEST_PERSISTENCE_LENGTH,
        (int)result);
    return PLATFORM_ERR_OK;
}

static platform_error_t s03_eeprom_test_init(void)
{
    platform_error_t result = platform_bsp_gpio_construct_soft_i2c_scl(
        &g_s03EepromScl);

    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_gpio_construct_soft_i2c_sda(&g_s03EepromSda);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_i2c_init(&g_s03EepromI2c,
                               "s03_eeprom_i2c",
                               &g_s03EepromScl,
                               &g_s03EepromSda);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_at24c02_init(&g_s03Eeprom,
                                   &g_s03EepromI2c,
                                   PROJECT_AT24C02_I2C_ADDRESS);
    SERVICE_LOG_I(
        "[S03][EEPROM] init/probe address=0x%02X error=%d",
        (unsigned int)PROJECT_AT24C02_I2C_ADDRESS,
        (int)result);
    return result;
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t app_s03_eeprom_test_run(void)
{
    static const uint8_t singleByteData[] = {0x5AU};
    static const uint8_t inPageData[] = {
        0x10U, 0x21U, 0x32U, 0x43U, 0x54U, 0x65U, 0x76U, 0x87U};
    static const uint8_t crossPageData[] = {
        0xA0U, 0xA1U, 0xA2U, 0xA3U, 0xA4U,
        0xA5U, 0xA6U, 0xA7U, 0xA8U, 0xA9U};
    static const uint8_t unalignedCrossPageData[] = {
        0xB0U, 0xB1U, 0xB2U, 0xB3U, 0xB4U};
    static const uint8_t lastByteData[] = {0xFFU};
    platform_error_t firstError = PLATFORM_ERR_OK;
    platform_error_t result = s03_eeprom_test_init();

    if (result != PLATFORM_ERR_OK) {
        s03_eeprom_log_failure("init/probe",
                               PROJECT_AT24C02_I2C_ADDRESS,
                               0U,
                               result);
        return result;
    }

    result = s03_eeprom_transfer_test("single-byte",
                                      0x00U,
                                      singleByteData,
                                      sizeof(singleByteData));
    if ((firstError == PLATFORM_ERR_OK) && (result != PLATFORM_ERR_OK)) {
        firstError = result;
    }

    result = s03_eeprom_transfer_test("in-page",
                                      0x10U,
                                      inPageData,
                                      sizeof(inPageData));
    if ((firstError == PLATFORM_ERR_OK) && (result != PLATFORM_ERR_OK)) {
        firstError = result;
    }

    result = s03_eeprom_transfer_test("cross-page-0x06-plus-10",
                                      0x06U,
                                      crossPageData,
                                      sizeof(crossPageData));
    if ((firstError == PLATFORM_ERR_OK) && (result != PLATFORM_ERR_OK)) {
        firstError = result;
    }

    result = s03_eeprom_transfer_test("unaligned-cross-page",
                                      0x0DU,
                                      unalignedCrossPageData,
                                      sizeof(unalignedCrossPageData));
    if ((firstError == PLATFORM_ERR_OK) && (result != PLATFORM_ERR_OK)) {
        firstError = result;
    }

    result = s03_eeprom_transfer_test("last-byte-0xff",
                                      0xFFU,
                                      lastByteData,
                                      sizeof(lastByteData));
    if ((firstError == PLATFORM_ERR_OK) && (result != PLATFORM_ERR_OK)) {
        firstError = result;
    }

    result = s03_eeprom_test_range_reject();
    if ((firstError == PLATFORM_ERR_OK) && (result != PLATFORM_ERR_OK)) {
        firstError = result;
    }

    result = s03_eeprom_test_persistence();
    if ((firstError == PLATFORM_ERR_OK) && (result != PLATFORM_ERR_OK)) {
        firstError = result;
    }

    if (firstError == PLATFORM_ERR_OK) {
        SERVICE_LOG_I("[S03][EEPROM] automated suite: PASS error=%d",
                      (int)firstError);
    } else {
        SERVICE_LOG_E("[S03][EEPROM] automated suite: FAIL error=%d",
                      (int)firstError);
    }

    return firstError;
}
//******************************** Functions *********************************//
