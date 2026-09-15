/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_main.c
 * @brief S01 Application 基础运行入口实现。
 * @author YaoQian Wang
 * @date 2026-09-11
 * @version V1.0
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include "app_main.h"

#define LOG_TAG "app_main"

#include "platform_time.h"
#include "project_config.h"
#include "diagnostics_fault.h"
#include "service_log.h"
#if (PROJECT_ENABLE_S05_YMODEM_BOARD_TEST != 0U)
#include "app_s05_ymodem_test.h"
#endif
#include "platform_bsp_led.h"
#include "platform_bsp_spi.h"
#include "platform_spi.h"
//******************************** Includes *********************************//

//******************************** Variables ********************************//
#if (PROJECT_ENABLE_S05_YMODEM_BOARD_TEST == 0U)
static platform_led_t g_statusLed = PLATFORM_LED_INITIALIZER;
static platform_spi_bus_t g_storageSpiBus = PLATFORM_SPI_BUS_INITIALIZER;
#endif
//******************************** Variables ********************************//

//******************************** Private Functions *************************//
#if (PROJECT_ENABLE_S05_YMODEM_BOARD_TEST == 0U)
/* 构造并启动 Application 基础资源与共享 Storage SPI Bus。 */
static platform_error_t app_main_init(void)
{
    platform_error_t result = platform_bsp_led_construct_status_led(
        &g_statusLed);

    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_led_init(&g_statusLed);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_bsp_spi_construct_storage_bus(&g_storageSpiBus);
    SERVICE_LOG_I("Storage SPI construct result: %d", result);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_spi_bus_lifecycle_init(&g_storageSpiBus);
    SERVICE_LOG_I("Storage SPI init result: %d", result);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_spi_bus_lifecycle_start(&g_storageSpiBus);
    SERVICE_LOG_I("Storage SPI start result: %d", result);
    return result;
}
#endif
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
void app_main(void)
{
#if (PROJECT_ENABLE_S05_YMODEM_BOARD_TEST != 0U)
    platform_error_t testResult;

    testResult = app_s05_ymodem_test_run();
    SERVICE_LOG_I("[S05] board test thread start result=%d", (int)testResult);
    for (;;) {
        (void)platform_time_delay_ms(PROJECT_STATUS_LED_BLINK_OFF_MS);
    }
#else
    platform_error_t appResult;

    SERVICE_LOG_I("Application Foundation start");

    appResult = app_main_init();
    SERVICE_LOG_I("Application init result: %d", appResult);

    if (appResult != PLATFORM_ERR_OK) {
        SERVICE_LOG_E("Application init failed: %d", appResult);

        for (;;) {
            (void)platform_time_delay_ms(PROJECT_STATUS_LED_BLINK_OFF_MS);
        }
    }

#if (DIAG_FAULT_TEST_ENABLE != 0U)
    (void)platform_time_delay_ms(DIAG_FAULT_TEST_DELAY_MS);
    diagnostics_fault_trigger(DIAG_FAULT_TEST_TYPE);
#endif

    for (;;) {
        (void)platform_led_on(&g_statusLed);
        (void)platform_time_delay_ms(PROJECT_STATUS_LED_BLINK_ON_MS);
        (void)platform_led_off(&g_statusLed);
        (void)platform_time_delay_ms(PROJECT_STATUS_LED_BLINK_OFF_MS);
    }
#endif
}
//******************************** Functions *********************************//
