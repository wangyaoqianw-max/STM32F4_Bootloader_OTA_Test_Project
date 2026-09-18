/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_installer.h
 * @brief S09 W25Q64 Candidate 到 Internal APP 的安装事务接口。
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
 * @brief Candidate 安装事务结果。
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
 * @brief 执行 Candidate 预校验、擦除、分块安装和安装后验证。
 * @param[in] context : 已初始化的 W25Q64 和 AT24C02 引用。
 * @param[out] candidate : 成功时输出本次安装使用的 Candidate 快照。
 * @return 安装事务结果；失败时不产生 TRIAL Metadata。
 * @note 本函数只负责 W25Q64 → Internal APP，不执行 PENDING→TRIAL 提交。
 */
boot_installer_result_t boot_installer_run(
    const boot_installer_context_t *context,
    boot_candidate_t *candidate);
//******************************** Functions ********************************//

#endif
