/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file spi.h
 * @brief Host Test 用 STM32 HAL SPI 替身接口
 * @author YaoQian Wang
 * @date 2026-09-12
 * @version V1.0
 * @note 本文件不是 Vendor HAL 头文件，只在 Host Test 编译时通过 -I 顺序
 *       覆盖 Core/Inc/spi.h，用于在 PC 上编译 impl_platform_spi.c。
 *
 *****************************************************************************/

#ifndef STUB_SPI_H
#define STUB_SPI_H

//******************************** Includes *********************************//
#include "platform_types.h"
//******************************** Includes *********************************//

//******************************** Defines *********************************//
/*与 STM32F4 HAL 保持一致的 Status / State / Init 常量值。*/
typedef enum
{
    HAL_OK      = 0x00U,
    HAL_ERROR   = 0x01U,
    HAL_BUSY    = 0x02U,
    HAL_TIMEOUT = 0x03U
} HAL_StatusTypeDef;

typedef enum
{
    HAL_SPI_STATE_RESET   = 0x00U,
    HAL_SPI_STATE_READY   = 0x01U,
    HAL_SPI_STATE_BUSY    = 0x02U,
    HAL_SPI_STATE_BUSY_TX = 0x03U,
    HAL_SPI_STATE_BUSY_RX = 0x04U,
    HAL_SPI_STATE_ERROR   = 0x05U
} HAL_SPI_StateTypeDef;

#define SPI_POLARITY_LOW                (0x00000000U)
#define SPI_POLARITY_HIGH               (0x00000002U)
#define SPI_PHASE_1EDGE                 (0x00000000U)
#define SPI_PHASE_2EDGE                 (0x00000001U)
#define SPI_FIRSTBIT_MSB                (0x00000000U)
#define SPI_FIRSTBIT_LSB                (0x00000080U)
#define SPI_DATASIZE_8BIT               (0x00000000U)

#define SPI_BAUDRATEPRESCALER_2         (0x00000000U)
#define SPI_BAUDRATEPRESCALER_4         (0x00000008U)
#define SPI_BAUDRATEPRESCALER_8         (0x00000010U)
#define SPI_BAUDRATEPRESCALER_16        (0x00000018U)
#define SPI_BAUDRATEPRESCALER_32        (0x00000020U)
#define SPI_BAUDRATEPRESCALER_64        (0x00000028U)
#define SPI_BAUDRATEPRESCALER_128       (0x00000030U)
#define SPI_BAUDRATEPRESCALER_256       (0x00000038U)
//******************************** Defines *********************************//

//******************************** Declaring *********************************//
/*最小 SPI 寄存器块替身；Host Test 只使用地址身份，不访问寄存器。*/
typedef struct
{
    uint32_t reserved;
} SPI_TypeDef;

typedef struct
{
    uint32_t CLKPolarity;
    uint32_t CLKPhase;
    uint32_t FirstBit;
    uint32_t BaudRatePrescaler;
    uint32_t DataSize;
} SPI_InitTypeDef;

typedef struct
{
    SPI_TypeDef *Instance;
    SPI_InitTypeDef Init;
} SPI_HandleTypeDef;

extern SPI_TypeDef *const g_stubSpi1;
extern SPI_TypeDef *const g_stubSpi2;

#define SPI1    (g_stubSpi1)
#define SPI2    (g_stubSpi2)

/*CubeMX 生成的两个 SPI Handle，仅提供符号，由 Host Test 自行定义。*/
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;

HAL_StatusTypeDef HAL_SPI_Transmit(
    SPI_HandleTypeDef *hspi,
    uint8_t *pData,
    uint16_t Size,
    uint32_t Timeout);

HAL_StatusTypeDef HAL_SPI_Receive(
    SPI_HandleTypeDef *hspi,
    uint8_t *pData,
    uint16_t Size,
    uint32_t Timeout);

HAL_SPI_StateTypeDef HAL_SPI_GetState(SPI_HandleTypeDef *hspi);

uint32_t HAL_RCC_GetPCLK1Freq(void);
uint32_t HAL_RCC_GetPCLK2Freq(void);
//******************************** Declaring *********************************//

#endif

