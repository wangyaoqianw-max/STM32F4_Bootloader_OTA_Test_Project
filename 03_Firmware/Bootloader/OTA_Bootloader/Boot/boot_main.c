/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_main.c
 * @brief Bootloader 启动决策入口实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>
#include <string.h>

#include "boot_main.h"

#include "boot_at24c02.h"
#include "boot_installer.h"
#include "boot_jump.h"
#include "boot_log.h"
#include "boot_metadata.h"
#include "boot_metadata_commit.h"
#include "boot_soft_i2c.h"
#include "boot_spi.h"
#include "boot_validate.h"
#include "boot_w25q64.h"
#include "main.h"
#include "spi.h"
#include "stm32f4xx_hal.h"
//******************************** Includes *********************************//

//******************************** Private Functions ************************//
static const char *boot_main_upgrade_state_name(boot_upgrade_state_t state)
{
    switch (state) {
        case BOOT_UPGRADE_STATE_NONE:
            return "NONE";
        case BOOT_UPGRADE_STATE_PENDING:
            return "PENDING";
        case BOOT_UPGRADE_STATE_TRIAL:
            return "TRIAL";
        case BOOT_UPGRADE_STATE_ROLLBACK:
            return "ROLLBACK";
        default:
            return "UNKNOWN";
    }
}

static const char *boot_main_driver_status_name(boot_driver_status_t status)
{
    switch (status) {
        case BOOT_DRIVER_OK:
            return "OK";
        case BOOT_DRIVER_ERR_NULL:
            return "NULL";
        case BOOT_DRIVER_ERR_INVALID:
            return "INVALID";
        case BOOT_DRIVER_ERR_NOT_INITIALIZED:
            return "NOT_INITIALIZED";
        case BOOT_DRIVER_ERR_HAL:
            return "HAL";
        case BOOT_DRIVER_ERR_TIMEOUT:
            return "TIMEOUT";
        case BOOT_DRIVER_ERR_BUSY:
            return "BUSY";
        case BOOT_DRIVER_ERR_NOT_FOUND:
            return "NOT_FOUND";
        case BOOT_DRIVER_ERR_RANGE:
            return "RANGE";
        case BOOT_DRIVER_ERR_VERIFY:
            return "VERIFY";
        default:
            return "UNKNOWN";
    }
}

/* 按冻结顺序完成 SPI/Soft-I2C 总线绑定及 W25Q64/AT24C02 探测。 */
static boot_driver_status_t boot_main_init_devices(
    boot_spi_bus_t *spiBus,
    boot_soft_i2c_t *i2c,
    boot_w25q64_t *flash,
    boot_at24c02_t *eeprom)
{
    boot_driver_status_t result;

    result = boot_spi_init(spiBus, &hspi2);
    if (result != BOOT_DRIVER_OK) {
        BOOT_LOG_E("SPI init FAIL: %s", boot_main_driver_status_name(result));
        return result;
    }
    result = boot_soft_i2c_init(i2c,
                                I2C_SCL_GPIO_Port,
                                I2C_SCL_Pin,
                                I2C_SDA_GPIO_Port,
                                I2C_SDA_Pin);
    if (result != BOOT_DRIVER_OK) {
        BOOT_LOG_E("Soft-I2C init FAIL: %s",
                   boot_main_driver_status_name(result));
        return result;
    }
    result = boot_w25q64_init(flash,
                              spiBus,
                              FLASH_CS_GPIO_Port,
                              FLASH_CS_Pin);
    if (result != BOOT_DRIVER_OK) {
        BOOT_LOG_E("W25Q64 init FAIL: %s",
                   boot_main_driver_status_name(result));
        return result;
    }
    BOOT_LOG_I("W25Q64 JEDEC %02x %02x %02x",
               flash->manufacturerId,
               flash->memoryType,
               flash->capacityId);

    result = boot_at24c02_init(eeprom, i2c, BOOT_AT24C02_ADDRESS);
    if (result != BOOT_DRIVER_OK) {
        BOOT_LOG_E("AT24C02 init FAIL: %s",
                   boot_main_driver_status_name(result));
        return result;
    }
    BOOT_LOG_I("external devices init PASS");
    return BOOT_DRIVER_OK;
}

/* 读取双 Metadata Copy，并沿用 sequence 规则选择唯一 latest。 */
static boot_driver_status_t boot_main_load_metadata(
    boot_at24c02_t *eeprom,
    boot_firmware_metadata_t *metadata,
    boot_metadata_copy_id_t *selectedCopy)
{
    uint8_t copyA[BOOT_METADATA_COPY_SIZE];
    uint8_t copyB[BOOT_METADATA_COPY_SIZE];
    boot_driver_status_t resultA;
    boot_driver_status_t resultB;

    if ((eeprom == NULL) || (metadata == NULL) || (selectedCopy == NULL)) {
        return BOOT_DRIVER_ERR_NULL;
    }

    /* 一份 Copy 读失败时仍保留另一份恢复路径；两份都失败才 fail-fast。 */
    (void)memset(copyA, 0xFF, sizeof(copyA));
    (void)memset(copyB, 0xFF, sizeof(copyB));
    resultA = boot_at24c02_read(eeprom,
                                BOOT_METADATA_COPY_A_ADDRESS,
                                copyA,
                                BOOT_METADATA_COPY_SIZE);
    resultB = boot_at24c02_read(eeprom,
                                BOOT_METADATA_COPY_B_ADDRESS,
                                copyB,
                                BOOT_METADATA_COPY_SIZE);
    if ((resultA != BOOT_DRIVER_OK) && (resultB != BOOT_DRIVER_OK)) {
        return BOOT_DRIVER_ERR_HAL;
    }
    return (boot_metadata_select_latest(copyA, copyB, metadata, selectedCopy) ==
            BOOT_CONTRACT_OK) ? BOOT_DRIVER_OK : BOOT_DRIVER_ERR_NOT_FOUND;
}

/* 记录不可继续的启动错误；S09 不在此处清除 PENDING 或执行恢复。 */
static void boot_main_halt(const char *reason)
{
    BOOT_LOG_E("BOOT halt: %s", reason);
    while (1) {
    }
}

static const char *boot_main_validation_result_name(
    boot_app_vector_result_t result)
{
    switch (result) {
        case BOOT_APP_VECTOR_VALID:
            return "VALID";
        case BOOT_APP_VECTOR_NULL:
            return "NULL";
        case BOOT_APP_VECTOR_ERASED:
            return "ERASED";
        case BOOT_APP_VECTOR_MSP_INVALID:
            return "MSP_INVALID";
        case BOOT_APP_VECTOR_RESET_INVALID:
            return "RESET_INVALID";
        default:
            return "UNKNOWN";
    }
}

/* 复用 S08 向量检查与 Jump Contract，验证通过后立即跳转。 */
static void boot_main_validate_and_jump(void)
{
    boot_app_vector_t appVector = {0};
    boot_app_vector_result_t validationResult;

    validationResult = boot_validate_app_vector(&appVector);
    BOOT_LOG_I("APP vector MSP = 0x%08lx",
               (unsigned long)appVector.initialMsp);
    BOOT_LOG_I("APP vector Reset_Handler = 0x%08lx",
               (unsigned long)appVector.resetHandler);

    if (validationResult != BOOT_APP_VECTOR_VALID) {
        BOOT_LOG_E("APP validation FAIL: %s",
                   boot_main_validation_result_name(validationResult));
        boot_main_halt("invalid APP vector");
    }

    BOOT_LOG_I("APP validation PASS");
    BOOT_LOG_I("jump start");
    boot_jump_to_app(appVector.initialMsp, appVector.resetHandler);
}

//******************************** Functions ********************************//
void boot_main_run(void)
{
    boot_spi_bus_t spiBus = {0};
    boot_soft_i2c_t i2c = {0};
    boot_w25q64_t flash = {0};
    boot_at24c02_t eeprom = {0};
    boot_firmware_metadata_t metadata = {0};
    boot_firmware_metadata_t committedMetadata = {0};
    boot_metadata_copy_id_t selectedCopy = BOOT_METADATA_COPY_NONE;
    boot_installer_context_t installerContext;
    boot_candidate_t candidate = {0};
    boot_installer_result_t installerResult;
    boot_metadata_commit_result_t commitResult;
    boot_driver_status_t driverResult;

    BOOT_LOG_I("BOOT start");
    driverResult = boot_main_init_devices(&spiBus, &i2c, &flash, &eeprom);
    if (driverResult != BOOT_DRIVER_OK) {
        boot_main_halt("external device init");
    }

    driverResult = boot_main_load_metadata(&eeprom, &metadata, &selectedCopy);
    if (driverResult != BOOT_DRIVER_OK) {
        BOOT_LOG_E("Metadata load FAIL: %s",
                   boot_main_driver_status_name(driverResult));
        boot_main_halt("Metadata load");
    }
    BOOT_LOG_I("Metadata copy=%c sequence=%lu state=%s pending=%u",
               (selectedCopy == BOOT_METADATA_COPY_A) ? 'A' : 'B',
               (unsigned long)metadata.sequence,
               boot_main_upgrade_state_name(metadata.upgradeState),
               (unsigned int)metadata.pendingSlot);

    if (metadata.upgradeState == BOOT_UPGRADE_STATE_PENDING) {
        installerContext.flash = &flash;
        installerContext.eeprom = &eeprom;
        installerResult = boot_installer_run(&installerContext, &candidate);
        if (installerResult != BOOT_INSTALLER_OK) {
            BOOT_LOG_E("Installer FAIL: %u", (unsigned int)installerResult);
            boot_main_halt("Candidate installation");
        }
        BOOT_LOG_I("installed APP CRC/vector PASS");

        commitResult = boot_metadata_commit_trial(&eeprom,
                                                  candidate.candidateSlot,
                                                  &committedMetadata);
        if (commitResult != BOOT_METADATA_COMMIT_OK) {
            BOOT_LOG_E("Metadata PENDING -> TRIAL FAIL: %u",
                       (unsigned int)commitResult);
            boot_main_halt("TRIAL commit");
        }
        BOOT_LOG_I("Metadata PENDING -> TRIAL PASS sequence=%lu",
                   (unsigned long)committedMetadata.sequence);
    } else if (metadata.upgradeState == BOOT_UPGRADE_STATE_ROLLBACK) {
        BOOT_LOG_E("ROLLBACK is owned by S10; no recovery action in S09");
        boot_main_halt("S10 rollback state");
    } else if (metadata.upgradeState == BOOT_UPGRADE_STATE_TRIAL) {
        BOOT_LOG_I("TRIAL state: skip reinstall");
    } else {
        BOOT_LOG_I("NONE state: no pending installation");
    }

    boot_main_validate_and_jump();
}

/**
 * @brief 为 Bootloader 的 HAL 初始化和时钟切换提供 SysTick 时基。
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
//******************************** Functions ********************************//
