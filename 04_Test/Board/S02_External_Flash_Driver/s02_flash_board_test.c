/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file s02_flash_board_test.c
 * @brief S02 W25Q64 非破坏性板测入口实现
 * @author YaoQian Wang
 * @date 2026-09-12
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "s02_flash_board_test.h"

#define LOG_TAG "s02_flash_test"

#include "platform_bsp_w25q64.h"
#include "platform_w25q64.h"
#include "service_log.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
platform_error_t s02_flash_board_test_run(platform_spi_bus_t *spiBus)
{
    platform_error_t result = PLATFORM_ERR_OK;
    platform_error_t deinitResult = PLATFORM_ERR_OK;
    platform_w25q64_t flash = PLATFORM_W25Q64_INITIALIZER;
    platform_w25q64_jedec_id_t jedecId = {0U};
    uint8_t status = 0U;

    if (spiBus == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    result = platform_bsp_w25q64_construct_flash(&flash);
    if (result != PLATFORM_ERR_OK) {
        SERVICE_LOG_E("[S02] w25q64 construct result=%d", result);
        return result;
    }

    result = platform_w25q64_init(&flash, spiBus);
    SERVICE_LOG_I("[S02] w25q64 init result=%d", result);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_w25q64_read_jedec_id(&flash, &jedecId);
    if (result == PLATFORM_ERR_OK) {
        result = ((jedecId.manufacturerId == 0xEFU) &&
                  (jedecId.memoryType == 0x40U) &&
                  (jedecId.capacityId == 0x17U)) ?
                 PLATFORM_ERR_OK : PLATFORM_ERR_IO;
    }
    SERVICE_LOG_I("[S02] jedec=%02X %02X %02X result=%d %s",
                  jedecId.manufacturerId,
                  jedecId.memoryType,
                  jedecId.capacityId,
                  result,
                  (result == PLATFORM_ERR_OK) ? "PASS" : "FAIL");

    if (result == PLATFORM_ERR_OK) {
        result = platform_w25q64_read_status1(&flash, &status);
    }
    SERVICE_LOG_I("[S02] sr1=%02X result=%d %s",
                  status,
                  result,
                  (result == PLATFORM_ERR_OK) ? "PASS" : "FAIL");

    deinitResult = platform_w25q64_deinit(&flash);
    if (result == PLATFORM_ERR_OK) {
        result = deinitResult;
    }

    return result;
}
//******************************** Functions ********************************//
