/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file Loader_Src.h
 * @brief STM32CubeProgrammer External Loader 入口声明
 * @author YaoQian Wang
 * @date 2026-09-21
 * @version V1.0
 ******************************************************************************/

#ifndef W25Q64_STM32F411_LOADER_SRC_H
#define W25Q64_STM32F411_LOADER_SRC_H

#include <stdint.h>

int Init(void);
int Write(uint32_t address, uint32_t size, uint8_t *buffer);
int Read(uint32_t address, uint32_t size, uint8_t *buffer);
int SectorErase(uint32_t eraseStartAddress, uint32_t eraseEndAddress);
int MassErase(uint32_t parallelism);
uint32_t CheckSum(uint32_t startAddress, uint32_t size, uint32_t initValue);
uint64_t Verify(
    uint32_t memoryAddress,
    uint32_t ramBufferAddress,
    uint32_t size,
    uint32_t missalignment);

#endif
