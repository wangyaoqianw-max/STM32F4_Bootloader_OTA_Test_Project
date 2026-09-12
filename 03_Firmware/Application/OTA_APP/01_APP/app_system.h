/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file app_system.h
 * @brief Application 系统任务启动接口。
 * @author YaoQian Wang
 * @date 2026-09-12
 * @version V1.0
 *
 *****************************************************************************/

#ifndef APP_SYSTEM_H
#define APP_SYSTEM_H

//******************************** Includes *********************************//
#include "platform_error.h"
//******************************** Includes *********************************//

//******************************** Declaring *******************************//
/**
 * @brief 创建 Application 系统任务。
 * @return PLATFORM_ERR_OK 表示创建成功，其他 platform_error_t 表示失败。
 */
platform_error_t app_system_start(void);
//******************************** Declaring *******************************//

#endif
