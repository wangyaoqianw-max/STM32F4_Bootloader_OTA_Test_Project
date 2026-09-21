/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file Dev_Inf.h
 * @brief STM32CubeProgrammer External Loader StorageInfo 接口
 * @author YaoQian Wang
 * @date 2026-09-21
 * @version V1.0
 ******************************************************************************/

#ifndef W25Q64_STM32F411_DEV_INF_H
#define W25Q64_STM32F411_DEV_INF_H

#define MCU_FLASH       1U
#define NAND_FLASH      2U
#define NOR_FLASH       3U
#define SRAM            4U
#define PSRAM           5U
#define PC_CARD         6U
#define SPI_FLASH       7U
#define I2C_FLASH       8U
#define SDRAM           9U
#define I2C_EEPROM      10U

#define SECTOR_NUM      10U

struct DeviceSectors
{
    unsigned long SectorNum;
    unsigned long SectorSize;
};

struct StorageInfo
{
    char DeviceName[100];
    unsigned short DeviceType;
    unsigned long DeviceStartAddress;
    unsigned long DeviceSize;
    unsigned long PageSize;
    unsigned char EraseValue;
    struct DeviceSectors sectors[SECTOR_NUM];
};

extern struct StorageInfo const StorageInfo;

#endif
