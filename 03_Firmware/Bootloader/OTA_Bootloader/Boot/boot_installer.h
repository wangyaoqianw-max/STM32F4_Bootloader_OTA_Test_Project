/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_installer.h
 * @brief S10 Pending Install/Confirmed Restore 安装事务接口。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef BOOT_INSTALLER_H
#define BOOT_INSTALLER_H

//******************************** Includes *********************************//
#include "boot_prevalidate.h"
#include "boot_internal_flash.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define BOOT_INSTALLER_BUFFER_SIZE (256U)
//******************************** Defines **********************************//

//******************************** Types ***********************************//
/**
 * @brief 外部镜像安装事务结果。
 */
typedef enum
{
    BOOT_INSTALLER_OK = 0,
    BOOT_INSTALLER_PREVALIDATION_FAILED,
    BOOT_INSTALLER_ERASE_FAILED,
    BOOT_INSTALLER_EXTERNAL_READ_FAILED,
    BOOT_INSTALLER_INTERNAL_WRITE_FAILED,
    BOOT_INSTALLER_READBACK_FAILED,
    BOOT_INSTALLER_INTERNAL_CRC_FAILED,
    BOOT_INSTALLER_VECTOR_FAILED
} boot_installer_result_t;

/**
 * @brief Installer 使用的外部设备引用。
 */
typedef struct
{
    /** W25Q64 Candidate 来源；安装期间只读。 */
    boot_w25q64_t *flash;
    /** AT24C02 Metadata 来源；安装完成前不写入。 */
    boot_at24c02_t *eeprom;
} boot_installer_context_t;
//******************************** Types ***********************************//

//******************************** Functions ********************************//
/**
 * @brief 预校验 Pending Image、安装并完成 Internal APP 验证。
 * @param[in] context : 已初始化的 W25Q64 和 AT24C02 引用。
 * @param[out] image : 成功时输出本次安装使用的 Pending 镜像快照。
 * @return 安装事务结果；失败时不产生 TRIAL Metadata。
 * @note 本函数只负责 W25Q64 → Internal APP，不执行 PENDING→TRIAL 提交。
 */
boot_installer_result_t boot_installer_install_pending(
    const boot_installer_context_t *context,
    boot_prevalidated_image_t *image);
/**
 * @brief 预校验 Confirmed Image、恢复并完成 Internal APP 验证。
 * @param[in] context : 已初始化的 W25Q64 和 AT24C02 引用。
 * @param[out] image : 成功时输出本次恢复使用的 Confirmed 镜像快照。
 * @return 安装事务结果；Confirmed 预校验失败时不得擦除 Internal APP。
 * @note 只接受 TRIAL/ROLLBACK recovery path，不公开任意 Slot 安装入口。
 */
boot_installer_result_t boot_installer_restore_confirmed(
    const boot_installer_context_t *context,
    boot_prevalidated_image_t *image);
//******************************** Functions ********************************//

#endif
