#ifndef SERVICE_UART_H
#define SERVICE_UART_H

#include "platform_error.h"

typedef struct
{
    uint8_t txData[64];
    platform_size_t txLength;
    platform_bool_t failWrite;
} service_uart_t;

platform_error_t service_uart_write(
    service_uart_t *service,
    const uint8_t *data,
    platform_size_t dataLength,
    uint32_t timeoutMs);

#endif
