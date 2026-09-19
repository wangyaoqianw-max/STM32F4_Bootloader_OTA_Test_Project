/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_installer.c
 * @brief S10 Pending Install/Confirmed Restore 安装事务实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>

#include "boot_crc32.h"
#include "boot_installer.h"
#include "memory_layout.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define BOOT_INSTALLER_VECTOR_SIZE (8U)
//******************************** Defines **********************************//

//******************************** Private Functions ************************//
static uint32_t boot_installer_slot_base(boot_firmware_slot_t slot)
{
    return (slot == BOOT_FIRMWARE_SLOT_A) ?
           BOOT_FIRMWARE_SLOT_A_BASE : BOOT_FIRMWARE_SLOT_B_BASE;
}

static boot_installer_result_t boot_installer_copy_payload(
    const boot_installer_context_t *context,
    const boot_prevalidated_image_t *image)
{
    uint8_t buffer[BOOT_INSTALLER_BUFFER_SIZE];
    boot_crc32_context_t internalCrc;
    uint32_t slotAddress = boot_installer_slot_base(image->sourceSlot) +
                           BOOT_FIRMWARE_PAYLOAD_OFFSET;
    uint32_t appOffset = 0U;
    uint32_t remaining = image->header.imageSize;
    uint16_t chunkLength;
    uint32_t expectedChunkCrc;
    uint32_t actualChunkCrc;
    uint32_t actualInternalCrc;
    boot_driver_status_t driverResult;

    /* 每块先保存期望 CRC，再写入并复读同一块，避免申请第二个大 Buffer。 */
    while (remaining > 0U) {
        chunkLength = (remaining > sizeof(buffer)) ?
                      (uint16_t)sizeof(buffer) : (uint16_t)remaining;
        driverResult = boot_w25q64_read(context->flash,
                                        slotAddress,
                                        buffer,
                                        chunkLength);
        if (driverResult != BOOT_DRIVER_OK) {
            return BOOT_INSTALLER_EXTERNAL_READ_FAILED;
        }
        expectedChunkCrc = boot_crc32_calculate(buffer, chunkLength);
        driverResult = boot_internal_flash_write(appOffset, buffer, chunkLength);
        if (driverResult != BOOT_DRIVER_OK) {
            return BOOT_INSTALLER_INTERNAL_WRITE_FAILED;
        }
        driverResult = boot_internal_flash_read(appOffset, buffer, chunkLength);
        if (driverResult != BOOT_DRIVER_OK) {
            return BOOT_INSTALLER_READBACK_FAILED;
        }
        actualChunkCrc = boot_crc32_calculate(buffer, chunkLength);
        if (actualChunkCrc != expectedChunkCrc) {
            return BOOT_INSTALLER_READBACK_FAILED;
        }
        slotAddress += chunkLength;
        appOffset += chunkLength;
        remaining -= chunkLength;
    }

    /* 安装完成后重新从 Internal APP 计算 whole-image CRC。 */
    boot_crc32_init(&internalCrc);
    appOffset = 0U;
    remaining = image->header.imageSize;
    while (remaining > 0U) {
        chunkLength = (remaining > sizeof(buffer)) ?
                      (uint16_t)sizeof(buffer) : (uint16_t)remaining;
        driverResult = boot_internal_flash_read(appOffset, buffer, chunkLength);
        if (driverResult != BOOT_DRIVER_OK) {
            return BOOT_INSTALLER_INTERNAL_CRC_FAILED;
        }
        boot_crc32_update(&internalCrc, buffer, chunkLength);
        appOffset += chunkLength;
        remaining -= chunkLength;
    }
    actualInternalCrc = boot_crc32_finalize(&internalCrc);
    if (actualInternalCrc != image->header.payloadCrc32) {
        return BOOT_INSTALLER_INTERNAL_CRC_FAILED;
    }

    return BOOT_INSTALLER_OK;
}

static boot_installer_result_t boot_installer_validate_installed_vector(
    boot_prevalidated_image_t *image)
{
    uint8_t vectorBytes[BOOT_INSTALLER_VECTOR_SIZE];

    if (boot_internal_flash_read(0U, vectorBytes, sizeof(vectorBytes)) !=
        BOOT_DRIVER_OK) {
        return BOOT_INSTALLER_VECTOR_FAILED;
    }
    image->vector.initialMsp = (uint32_t)vectorBytes[0] |
                                   ((uint32_t)vectorBytes[1] << 8U) |
                                   ((uint32_t)vectorBytes[2] << 16U) |
                                   ((uint32_t)vectorBytes[3] << 24U);
    image->vector.resetHandler = (uint32_t)vectorBytes[4] |
                                     ((uint32_t)vectorBytes[5] << 8U) |
                                     ((uint32_t)vectorBytes[6] << 16U) |
                                     ((uint32_t)vectorBytes[7] << 24U);
    return (boot_validate_vector_values(image->vector.initialMsp,
                                        image->vector.resetHandler) ==
            BOOT_APP_VECTOR_VALID) ?
           BOOT_INSTALLER_OK : BOOT_INSTALLER_VECTOR_FAILED;
}
//******************************** Private Functions ************************//
static boot_installer_result_t boot_installer_install_core(
    const boot_installer_context_t *context,
    boot_prevalidated_image_t *image)
{
    boot_installer_result_t result;

    if (boot_internal_flash_erase_app() != BOOT_DRIVER_OK) {
        return BOOT_INSTALLER_ERASE_FAILED;
    }
    result = boot_installer_copy_payload(context, image);
    if (result != BOOT_INSTALLER_OK) {
        return result;
    }
    return boot_installer_validate_installed_vector(image);
}

//******************************** Functions ********************************//

boot_installer_result_t boot_installer_install_pending(
    const boot_installer_context_t *context,
    boot_prevalidated_image_t *image)
{
    boot_prevalidate_context_t prevalidateContext;
    boot_prevalidated_image_t validatedImage;

    if ((context == NULL) || (image == NULL) ||
        (context->flash == NULL) || (context->eeprom == NULL)) {
        return BOOT_INSTALLER_PREVALIDATION_FAILED;
    }
    /* Pending 入口必须在同一调用中重新完成只读预校验。 */
    prevalidateContext.flash = context->flash;
    prevalidateContext.eeprom = context->eeprom;
    if (boot_prevalidate_candidate(&prevalidateContext, &validatedImage) !=
        BOOT_PREVALIDATE_VALID) {
        return BOOT_INSTALLER_PREVALIDATION_FAILED;
    }
    *image = validatedImage;
    return boot_installer_install_core(context, image);
}

boot_installer_result_t boot_installer_restore_confirmed(
    const boot_installer_context_t *context,
    boot_prevalidated_image_t *image)
{
    boot_prevalidate_context_t prevalidateContext;
    boot_prevalidated_image_t validatedImage;

    if ((context == NULL) || (image == NULL) ||
        (context->flash == NULL) || (context->eeprom == NULL)) {
        return BOOT_INSTALLER_PREVALIDATION_FAILED;
    }
    /* Confirmed 入口同样必须在 destructive gate 前重新验证 Metadata/Image。 */
    prevalidateContext.flash = context->flash;
    prevalidateContext.eeprom = context->eeprom;
    if (boot_prevalidate_confirmed(&prevalidateContext, &validatedImage) !=
        BOOT_PREVALIDATE_VALID) {
        return BOOT_INSTALLER_PREVALIDATION_FAILED;
    }
    *image = validatedImage;
    return boot_installer_install_core(context, image);
}
