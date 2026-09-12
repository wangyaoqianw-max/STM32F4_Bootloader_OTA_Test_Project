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

#include "project_config.h"
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
    uint8_t eraseRead[PLATFORM_W25Q64_PAGE_SIZE_BYTES] = {0U};
    uint8_t tx[64U] = {0U};
    uint8_t pageRead[64U] = {0U};
    uint8_t crossTx[300U] = {0U};
    uint8_t crossRead[300U] = {0U};
    uint8_t status = 0U;
    uint32_t offset = 0U;
    uint32_t index = 0U;
    platform_size_t readLength = 0U;
    platform_bool_t allErased = PLATFORM_TRUE;
    platform_bool_t pageMatch = PLATFORM_TRUE;
    platform_bool_t crossMatch = PLATFORM_TRUE;
    platform_bool_t boundaryPass = PLATFORM_TRUE;
    platform_error_t boundaryResult = PLATFORM_ERR_OK;

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

    if (result == PLATFORM_ERR_OK) {
        result = platform_w25q64_sector_erase(
            &flash,
            PROJECT_FLASH_TEST_SECTOR_ADDRESS);
    }
    SERVICE_LOG_I("[S02] erase sector=0x%06X result=%d",
                  (unsigned int)PROJECT_FLASH_TEST_SECTOR_ADDRESS,
                  result);

    if (result == PLATFORM_ERR_OK) {
        for (offset = 0U;
             offset < PLATFORM_W25Q64_SECTOR_SIZE_BYTES;
             offset += sizeof(eraseRead)) {
            readLength = PLATFORM_W25Q64_SECTOR_SIZE_BYTES - offset;
            if (readLength > sizeof(eraseRead)) {
                readLength = sizeof(eraseRead);
            }

            result = platform_w25q64_read(
                &flash,
                PROJECT_FLASH_TEST_SECTOR_ADDRESS + offset,
                eraseRead,
                readLength);
            if (result != PLATFORM_ERR_OK) {
                break;
            }

            for (index = 0U; index < readLength; index++) {
                if (eraseRead[index] != 0xFFU) {
                    allErased = PLATFORM_FALSE;
                    break;
                }
            }
            if (allErased != PLATFORM_TRUE) {
                result = PLATFORM_ERR_IO;
                break;
            }
        }
    }
    SERVICE_LOG_I("[S02] erase readback all_ff=%d %s",
                  allErased,
                  (result == PLATFORM_ERR_OK) ? "PASS" : "FAIL");

    if (result == PLATFORM_ERR_OK) {
        for (index = 0U; index < sizeof(tx); index++) {
            tx[index] = (uint8_t)(0xA5U ^ index);
        }

        result = platform_w25q64_page_program(
            &flash,
            PROJECT_FLASH_TEST_SECTOR_ADDRESS,
            tx,
            sizeof(tx));
    }
    SERVICE_LOG_I("[S02] page program addr=0x%06X len=%u result=%d",
                  (unsigned int)PROJECT_FLASH_TEST_SECTOR_ADDRESS,
                  (unsigned int)sizeof(tx),
                  result);

    if (result == PLATFORM_ERR_OK) {
        result = platform_w25q64_read(
            &flash,
            PROJECT_FLASH_TEST_SECTOR_ADDRESS,
            pageRead,
            sizeof(pageRead));
        if (result == PLATFORM_ERR_OK) {
            for (index = 0U; index < sizeof(pageRead); index++) {
                if (pageRead[index] != tx[index]) {
                    pageMatch = PLATFORM_FALSE;
                    break;
                }
            }
            if (pageMatch != PLATFORM_TRUE) {
                result = PLATFORM_ERR_IO;
            }
        }
    }
    SERVICE_LOG_I("[S02] page compare %s",
                  (result == PLATFORM_ERR_OK) ? "PASS" : "FAIL");

    if (result == PLATFORM_ERR_OK) {
        for (index = 0U; index < sizeof(crossTx); index++) {
            crossTx[index] = (uint8_t)(0x5AU ^ index);
        }

        result = platform_w25q64_write(
            &flash,
            PROJECT_FLASH_TEST_SECTOR_ADDRESS + 0xF0U,
            crossTx,
            sizeof(crossTx));
    }

    if (result == PLATFORM_ERR_OK) {
        result = platform_w25q64_read(
            &flash,
            PROJECT_FLASH_TEST_SECTOR_ADDRESS + 0xF0U,
            crossRead,
            sizeof(crossRead));
        if (result == PLATFORM_ERR_OK) {
            for (index = 0U; index < sizeof(crossRead); index++) {
                if (crossRead[index] != crossTx[index]) {
                    crossMatch = PLATFORM_FALSE;
                    break;
                }
            }
            if (crossMatch != PLATFORM_TRUE) {
                result = PLATFORM_ERR_IO;
            }
        }
    }
    SERVICE_LOG_I("[S02] cross-page write addr=0x%06X len=%u %s",
                  (unsigned int)(PROJECT_FLASH_TEST_SECTOR_ADDRESS + 0xF0U),
                  (unsigned int)sizeof(crossTx),
                  (result == PLATFORM_ERR_OK) ? "PASS" : "FAIL");

    boundaryResult = platform_w25q64_read(
        &flash,
        PLATFORM_W25Q64_TOTAL_SIZE_BYTES,
        pageRead,
        1U);
    if (boundaryResult == PLATFORM_ERR_OK) {
        boundaryPass = PLATFORM_FALSE;
    }
    SERVICE_LOG_I("[S02] out-of-range read rejected %s",
                  (boundaryResult != PLATFORM_ERR_OK) ? "PASS" : "FAIL");

    boundaryResult = platform_w25q64_read(
        &flash,
        PLATFORM_W25Q64_ADDRESS_MAX,
        pageRead,
        2U);
    if (boundaryResult == PLATFORM_ERR_OK) {
        boundaryPass = PLATFORM_FALSE;
    }
    SERVICE_LOG_I("[S02] crossing-end read rejected %s",
                  (boundaryResult != PLATFORM_ERR_OK) ? "PASS" : "FAIL");

    boundaryResult = platform_w25q64_write(
        &flash,
        PLATFORM_W25Q64_ADDRESS_MAX,
        crossTx,
        2U);
    if (boundaryResult == PLATFORM_ERR_OK) {
        boundaryPass = PLATFORM_FALSE;
    }
    SERVICE_LOG_I("[S02] out-of-range write rejected %s",
                  (boundaryResult != PLATFORM_ERR_OK) ? "PASS" : "FAIL");

    boundaryResult = platform_w25q64_page_program(
        &flash,
        PROJECT_FLASH_TEST_SECTOR_ADDRESS + 0xF0U,
        tx,
        32U);
    if (boundaryResult == PLATFORM_ERR_OK) {
        boundaryPass = PLATFORM_FALSE;
    }
    SERVICE_LOG_I("[S02] atomic page-cross rejected %s",
                  (boundaryResult != PLATFORM_ERR_OK) ? "PASS" : "FAIL");

    boundaryResult = platform_w25q64_sector_erase(
        &flash,
        PROJECT_FLASH_TEST_SECTOR_ADDRESS + 1U);
    if (boundaryResult == PLATFORM_ERR_OK) {
        boundaryPass = PLATFORM_FALSE;
    }
    SERVICE_LOG_I("[S02] unaligned erase rejected %s",
                  (boundaryResult != PLATFORM_ERR_OK) ? "PASS" : "FAIL");

    boundaryResult = platform_w25q64_read(
        &flash,
        PLATFORM_W25Q64_ADDRESS_MAX,
        pageRead,
        1U);
    if (boundaryResult != PLATFORM_ERR_OK) {
        boundaryPass = PLATFORM_FALSE;
    }
    SERVICE_LOG_I("[S02] legal tail read accepted %s",
                  (boundaryResult == PLATFORM_ERR_OK) ? "PASS" : "FAIL");

    if ((result == PLATFORM_ERR_OK) &&
        (boundaryPass != PLATFORM_TRUE)) {
        result = PLATFORM_ERR_IO;
    }

    deinitResult = platform_w25q64_deinit(&flash);
    if (result == PLATFORM_ERR_OK) {
        result = deinitResult;
    }

    return result;
}
//******************************** Functions ********************************//
