/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file ymodem_receiver.c
 * @brief YMODEM Receiver 状态机实现
 * @author YaoQian Wang
 * @date 2026-09-14
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include <string.h>

#include "platform_def.h"
#include "ymodem_receiver.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define YMODEM_CANCEL_SEQUENCE_SIZE          (2U)
//******************************** Defines **********************************//

//******************************** Private Functions ************************//
static platform_bool_t ymodem_receiver_is_active(const ymodem_receiver_t *receiver)
{
    return ((receiver->context.state == YMODEM_RECEIVER_STATE_WAIT_HEADER) ||
            (receiver->context.state == YMODEM_RECEIVER_STATE_RECEIVE_DATA) ||
            (receiver->context.state == YMODEM_RECEIVER_STATE_WAIT_EOT_CONFIRM) ||
            (receiver->context.state == YMODEM_RECEIVER_STATE_WAIT_END_HEADER)) ?
           PLATFORM_TRUE : PLATFORM_FALSE;
}

static platform_error_t ymodem_receiver_send(
    ymodem_receiver_t *receiver,
    const uint8_t *data,
    uint32_t length)
{
    if ((receiver == NULL) || (receiver->config.uart == NULL) || (data == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    return service_uart_write(
        receiver->config.uart,
        data,
        length,
        YMODEM_CFG_PACKET_TIMEOUT_MS);
}

static platform_error_t ymodem_receiver_send_control(
    ymodem_receiver_t *receiver,
    uint8_t control)
{
    return ymodem_receiver_send(receiver, &control, 1U);
}

/** @brief 调用 Sink abort 并清除当前文件生命周期标记。 */
static void ymodem_receiver_abort_sink(ymodem_receiver_t *receiver)
{
    if ((receiver->context.fileStarted != 0U) && (receiver->config.sink.abort != NULL)) {
        receiver->config.sink.abort(receiver->config.sink.context);
        receiver->context.fileStarted = PLATFORM_FALSE;
    }
}

/** @brief 记录不可恢复错误、放弃文件并进入 ERROR 状态。 */
static platform_error_t ymodem_receiver_fail(
    ymodem_receiver_t *receiver,
    platform_error_t error)
{
    ymodem_receiver_abort_sink(receiver);
    receiver->context.lastError = error;
    receiver->context.state = YMODEM_RECEIVER_STATE_ERROR;
    return error;
}

static void ymodem_receiver_reset_retry(ymodem_receiver_t *receiver)
{
    receiver->context.retryCount = 0U;
}

/** @brief 执行一次有界控制字节重试，超限则结束 Session。 */
static platform_error_t ymodem_receiver_retry(
    ymodem_receiver_t *receiver,
    uint8_t control,
    platform_error_t reason)
{
    platform_error_t result;

    if (receiver->context.retryCount >= YMODEM_CFG_MAX_RETRY) {
        return ymodem_receiver_fail(receiver, reason);
    }

    receiver->context.retryCount++;
    receiver->statistics.retryCount++;
    result = ymodem_receiver_send_control(receiver, control);
    if (result != PLATFORM_ERR_OK) {
        return ymodem_receiver_fail(receiver, result);
    }

    return PLATFORM_ERR_OK;
}

/** @brief 在 Block 0 数据区内有界解析文件名和十进制文件大小。 */
static platform_error_t ymodem_receiver_parse_block0(
    ymodem_receiver_t *receiver,
    const ymodem_packet_t *packet,
    uint32_t *fileSize)
{
    uint16_t filenameLength = 0U;
    uint16_t fileSizeIndex;
    uint16_t index;
    uint32_t parsedSize = 0U;
    uint8_t digit;
    platform_bool_t hasDigit = PLATFORM_FALSE;

    while ((filenameLength < packet->dataSize) && (packet->data[filenameLength] != '\0')) {
        filenameLength++;
    }

    if ((filenameLength == 0U) ||
        (filenameLength > YMODEM_CFG_FILENAME_MAX_LEN) ||
        (filenameLength >= packet->dataSize)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    fileSizeIndex = (uint16_t)(filenameLength + 1U);
    if (fileSizeIndex >= packet->dataSize) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    for (index = fileSizeIndex; index < packet->dataSize; index++) {
        if (packet->data[index] == '\0') {
            break;
        }

        if ((packet->data[index] < '0') || (packet->data[index] > '9')) {
            return PLATFORM_ERR_INVALID_PARAM;
        }

        digit = (uint8_t)(packet->data[index] - '0');
        if (parsedSize > ((0xFFFFFFFFUL - digit) / 10UL)) {
            return PLATFORM_ERR_OVERFLOW;
        }

        parsedSize = (parsedSize * 10UL) + digit;
        hasDigit = PLATFORM_TRUE;
    }

    if ((hasDigit == PLATFORM_FALSE) || (parsedSize == 0U)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    (void)memcpy(receiver->context.filename, packet->data, filenameLength);
    receiver->context.filename[filenameLength] = '\0';
    *fileSize = parsedSize;
    return PLATFORM_ERR_OK;
}

static platform_bool_t ymodem_receiver_is_empty_block0(const ymodem_packet_t *packet)
{
    uint16_t index;

    for (index = 0U; index < packet->dataSize; index++) {
        if (packet->data[index] != 0U) {
            return PLATFORM_FALSE;
        }
    }

    return PLATFORM_TRUE;
}

/** @brief 校验并接受首个非空 Block 0，启动 Sink 文件生命周期。 */
static platform_error_t ymodem_receiver_accept_header(
    ymodem_receiver_t *receiver,
    const ymodem_packet_t *packet,
    uint32_t nowMs)
{
    uint32_t fileSize;
    platform_error_t result;

    result = ymodem_receiver_parse_block0(receiver, packet, &fileSize);
    if (result != PLATFORM_ERR_OK) {
        return ymodem_receiver_fail(receiver, result);
    }

    result = receiver->config.sink.begin(
        receiver->config.sink.context,
        receiver->context.filename,
        fileSize);
    if (result != PLATFORM_ERR_OK) {
        receiver->config.sink.abort(receiver->config.sink.context);
        return ymodem_receiver_fail(receiver, result);
    }

    receiver->context.fileStarted = PLATFORM_TRUE;
    receiver->context.fileSize = fileSize;
    receiver->context.receivedSize = 0U;
    receiver->context.expectedBlock = 1U;
    ymodem_receiver_reset_retry(receiver);

    result = ymodem_receiver_send_control(receiver, YMODEM_ACK);
    if (result == PLATFORM_ERR_OK) {
        result = ymodem_receiver_send_control(receiver, YMODEM_C);
    }
    if (result != PLATFORM_ERR_OK) {
        return ymodem_receiver_fail(receiver, result);
    }

    receiver->context.state = YMODEM_RECEIVER_STATE_RECEIVE_DATA;
    receiver->context.lastActivityMs = nowMs;
    receiver->statistics.packetAcceptedCount++;
    return PLATFORM_ERR_OK;
}

/** @brief 校验数据序号、写入有效长度并确认当前数据 Packet。 */
static platform_error_t ymodem_receiver_accept_data(
    ymodem_receiver_t *receiver,
    const ymodem_packet_t *packet,
    uint32_t nowMs)
{
    uint32_t remaining;
    uint32_t writeLength;
    platform_error_t result;

    if (packet->blockNumber == (uint8_t)(receiver->context.expectedBlock - 1U)) {
        receiver->statistics.duplicatePacketCount++;
        return ymodem_receiver_send_control(receiver, YMODEM_ACK);
    }

    if (packet->blockNumber != receiver->context.expectedBlock) {
        receiver->statistics.sequenceErrorCount++;
        return ymodem_receiver_retry(receiver, YMODEM_NAK, PLATFORM_ERR_INVALID_STATE);
    }

    if (receiver->context.receivedSize > receiver->context.fileSize) {
        return ymodem_receiver_fail(receiver, PLATFORM_ERR_INVALID_STATE);
    }

    remaining = receiver->context.fileSize - receiver->context.receivedSize;
    writeLength = (packet->dataSize > remaining) ? remaining : packet->dataSize;
    if (writeLength == 0U) {
        return ymodem_receiver_fail(receiver, PLATFORM_ERR_INVALID_PARAM);
    }

    result = receiver->config.sink.write(
        receiver->config.sink.context,
        packet->data,
        writeLength);
    if (result != PLATFORM_ERR_OK) {
        return ymodem_receiver_fail(receiver, result);
    }

    receiver->context.receivedSize += writeLength;
    receiver->statistics.bytesReceived += writeLength;
    receiver->statistics.bytesWritten += writeLength;
    receiver->statistics.packetAcceptedCount++;
    receiver->context.expectedBlock++;
    receiver->context.lastActivityMs = nowMs;
    ymodem_receiver_reset_retry(receiver);
    return ymodem_receiver_send_control(receiver, YMODEM_ACK);
}

/** @brief 接受空 Block 0，结束并提交 Sink 文件生命周期。 */
static platform_error_t ymodem_receiver_accept_end_header(
    ymodem_receiver_t *receiver,
    const ymodem_packet_t *packet)
{
    platform_error_t result;

    if ((packet->blockNumber != 0U) ||
        (ymodem_receiver_is_empty_block0(packet) == PLATFORM_FALSE)) {
        return ymodem_receiver_fail(receiver, PLATFORM_ERR_INVALID_STATE);
    }

    result = receiver->config.sink.end(receiver->config.sink.context);
    if (result != PLATFORM_ERR_OK) {
        return ymodem_receiver_fail(receiver, result);
    }

    receiver->context.fileStarted = PLATFORM_FALSE;
    result = ymodem_receiver_send_control(receiver, YMODEM_ACK);
    if (result != PLATFORM_ERR_OK) {
        return ymodem_receiver_fail(receiver, result);
    }

    receiver->context.state = YMODEM_RECEIVER_STATE_FINISHED;
    receiver->statistics.packetAcceptedCount++;
    return PLATFORM_ERR_OK;
}

/** @brief 根据当前状态分派一个已验证 Packet。 */
static platform_error_t ymodem_receiver_handle_packet(
    ymodem_receiver_t *receiver,
    const ymodem_packet_t *packet,
    uint32_t nowMs)
{
    receiver->statistics.packetReceivedCount++;

    if (receiver->context.state == YMODEM_RECEIVER_STATE_WAIT_HEADER) {
        if (packet->blockNumber != 0U) {
            receiver->statistics.sequenceErrorCount++;
            return ymodem_receiver_retry(receiver, YMODEM_NAK, PLATFORM_ERR_INVALID_STATE);
        }

        return ymodem_receiver_accept_header(receiver, packet, nowMs);
    }

    if (receiver->context.state == YMODEM_RECEIVER_STATE_RECEIVE_DATA) {
        return ymodem_receiver_accept_data(receiver, packet, nowMs);
    }

    if (receiver->context.state == YMODEM_RECEIVER_STATE_WAIT_END_HEADER) {
        return ymodem_receiver_accept_end_header(receiver, packet);
    }

    return PLATFORM_ERR_INVALID_STATE;
}

/** @brief 处理首个或确认阶段的 EOT 握手。 */
static platform_error_t ymodem_receiver_handle_eot(
    ymodem_receiver_t *receiver,
    uint32_t nowMs)
{
    platform_error_t result;

    if (receiver->context.state == YMODEM_RECEIVER_STATE_RECEIVE_DATA) {
        if (receiver->context.receivedSize != receiver->context.fileSize) {
            return ymodem_receiver_fail(receiver, PLATFORM_ERR_INVALID_PARAM);
        }

        result = ymodem_receiver_send_control(receiver, YMODEM_NAK);
        if (result != PLATFORM_ERR_OK) {
            return ymodem_receiver_fail(receiver, result);
        }

        receiver->context.state = YMODEM_RECEIVER_STATE_WAIT_EOT_CONFIRM;
        receiver->context.lastActivityMs = nowMs;
        ymodem_receiver_reset_retry(receiver);
        return PLATFORM_ERR_OK;
    }

    if (receiver->context.state == YMODEM_RECEIVER_STATE_WAIT_EOT_CONFIRM) {
        result = ymodem_receiver_send_control(receiver, YMODEM_ACK);
        if (result == PLATFORM_ERR_OK) {
            result = ymodem_receiver_send_control(receiver, YMODEM_C);
        }
        if (result != PLATFORM_ERR_OK) {
            return ymodem_receiver_fail(receiver, result);
        }

        receiver->context.state = YMODEM_RECEIVER_STATE_WAIT_END_HEADER;
        receiver->context.lastActivityMs = nowMs;
        ymodem_receiver_reset_retry(receiver);
        return PLATFORM_ERR_OK;
    }

    if ((receiver->context.state == YMODEM_RECEIVER_STATE_WAIT_HEADER) ||
        (receiver->context.state == YMODEM_RECEIVER_STATE_WAIT_END_HEADER)) {
        return ymodem_receiver_retry(receiver, YMODEM_C, PLATFORM_ERR_INVALID_STATE);
    }

    return PLATFORM_ERR_INVALID_STATE;
}

/** @brief 将 Parser 事件分派到 Receiver 状态机处理路径。 */
static platform_error_t ymodem_receiver_handle_event(
    ymodem_receiver_t *receiver,
    ymodem_parser_event_t event,
    const ymodem_packet_t *packet,
    uint32_t nowMs)
{
    if (event == YMODEM_PARSER_EVENT_PACKET) {
        return ymodem_receiver_handle_packet(receiver, packet, nowMs);
    }

    if (event == YMODEM_PARSER_EVENT_PACKET_ERROR) {
        receiver->statistics.crcErrorCount++;
        if (receiver->context.state == YMODEM_RECEIVER_STATE_WAIT_END_HEADER) {
            return ymodem_receiver_retry(receiver, YMODEM_C, PLATFORM_ERR_CHECKSUM);
        }

        return ymodem_receiver_retry(receiver, YMODEM_NAK, PLATFORM_ERR_CHECKSUM);
    }

    if (event == YMODEM_PARSER_EVENT_EOT) {
        return ymodem_receiver_handle_eot(receiver, nowMs);
    }

    if (event == YMODEM_PARSER_EVENT_CAN) {
        receiver->statistics.cancelCount++;
        ymodem_receiver_abort_sink(receiver);
        receiver->context.state = YMODEM_RECEIVER_STATE_ABORTED;
        receiver->context.lastError = PLATFORM_ERR_CANCELED;
        return PLATFORM_ERR_OK;
    }

    return PLATFORM_ERR_OK;
}

/** @brief 根据当前状态和 Parser 是否处于半包状态选择超时阈值。 */
static uint32_t ymodem_receiver_timeout_ms(const ymodem_receiver_t *receiver)
{
    if ((receiver->context.state == YMODEM_RECEIVER_STATE_RECEIVE_DATA) &&
        (receiver->context.parser.state == YMODEM_PARSER_STATE_COLLECT_PACKET)) {
        return YMODEM_CFG_INTERBYTE_TIMEOUT_MS;
    }

    return YMODEM_CFG_PACKET_TIMEOUT_MS;
}

/** @brief 为当前等待阶段选择重试控制字节。 */
static uint8_t ymodem_receiver_timeout_control(const ymodem_receiver_t *receiver)
{
    return ((receiver->context.state == YMODEM_RECEIVER_STATE_WAIT_HEADER) ||
            (receiver->context.state == YMODEM_RECEIVER_STATE_WAIT_END_HEADER)) ?
           YMODEM_C : YMODEM_NAK;
}
//******************************** Private Functions ************************//

//******************************** Public Functions *************************//
platform_error_t ymodem_receiver_init(
    ymodem_receiver_t *receiver,
    const ymodem_receiver_config_t *config)
{
    if ((receiver == NULL) || (config == NULL) || (config->uart == NULL) ||
        (config->sink.begin == NULL) || (config->sink.write == NULL) ||
        (config->sink.end == NULL) || (config->sink.abort == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    receiver->config = *config;
    (void)memset(&receiver->context, 0, sizeof(receiver->context));
    (void)memset(&receiver->statistics, 0, sizeof(receiver->statistics));
    ymodem_parser_init(&receiver->context.parser);
    receiver->context.state = YMODEM_RECEIVER_STATE_IDLE;
    receiver->context.lastError = PLATFORM_ERR_OK;
    return PLATFORM_ERR_OK;
}

platform_error_t ymodem_receiver_start(ymodem_receiver_t *receiver)
{
    platform_error_t result;

    if (receiver == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (receiver->context.state != YMODEM_RECEIVER_STATE_IDLE) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    ymodem_parser_reset(&receiver->context.parser);
    receiver->context.expectedBlock = 0U;
    receiver->context.retryCount = 0U;
    receiver->context.fileSize = 0U;
    receiver->context.receivedSize = 0U;
    receiver->context.fileStarted = PLATFORM_FALSE;
    receiver->context.lastError = PLATFORM_ERR_OK;
    result = ymodem_receiver_send_control(receiver, YMODEM_C);
    if (result != PLATFORM_ERR_OK) {
        return ymodem_receiver_fail(receiver, result);
    }

    receiver->context.state = YMODEM_RECEIVER_STATE_WAIT_HEADER;
    return PLATFORM_ERR_OK;
}

platform_error_t ymodem_receiver_feed_byte(
    ymodem_receiver_t *receiver,
    uint8_t byte,
    uint32_t nowMs)
{
    ymodem_parser_event_t event;
    ymodem_packet_t packet;
    platform_error_t result;

    if (receiver == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (ymodem_receiver_is_active(receiver) == PLATFORM_FALSE) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    result = ymodem_parser_feed_byte(&receiver->context.parser, byte, &event, &packet);
    if (result != PLATFORM_ERR_OK) {
        return ymodem_receiver_fail(receiver, result);
    }

    receiver->context.lastActivityMs = nowMs;
    return ymodem_receiver_handle_event(receiver, event, &packet, nowMs);
}

platform_error_t ymodem_receiver_tick(ymodem_receiver_t *receiver, uint32_t nowMs)
{
    uint8_t control;

    if (receiver == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (ymodem_receiver_is_active(receiver) == PLATFORM_FALSE) {
        return PLATFORM_ERR_OK;
    }

    if ((nowMs - receiver->context.lastActivityMs) < ymodem_receiver_timeout_ms(receiver)) {
        return PLATFORM_ERR_OK;
    }

    receiver->statistics.timeoutCount++;
    ymodem_parser_reset(&receiver->context.parser);
    control = ymodem_receiver_timeout_control(receiver);
    receiver->context.lastActivityMs = nowMs;
    return ymodem_receiver_retry(receiver, control, PLATFORM_ERR_TIMEOUT);
}

platform_error_t ymodem_receiver_cancel(ymodem_receiver_t *receiver)
{
    uint8_t cancelSequence[YMODEM_CANCEL_SEQUENCE_SIZE] = {YMODEM_CAN, YMODEM_CAN};
    platform_error_t result;

    if (receiver == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (ymodem_receiver_is_active(receiver) == PLATFORM_FALSE) {
        return PLATFORM_ERR_INVALID_STATE;
    }

    result = ymodem_receiver_send(receiver, cancelSequence, sizeof(cancelSequence));
    ymodem_receiver_abort_sink(receiver);
    receiver->statistics.cancelCount++;
    receiver->context.state = YMODEM_RECEIVER_STATE_ABORTED;
    receiver->context.lastError = PLATFORM_ERR_CANCELED;
    return result;
}

platform_error_t ymodem_receiver_get_status(
    const ymodem_receiver_t *receiver,
    ymodem_receiver_status_t *status)
{
    if ((receiver == NULL) || (status == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    status->state = receiver->context.state;
    status->lastError = receiver->context.lastError;
    status->fileSize = receiver->context.fileSize;
    status->receivedSize = receiver->context.receivedSize;
    (void)strcpy(status->filename, receiver->context.filename);
    return PLATFORM_ERR_OK;
}

platform_error_t ymodem_receiver_get_statistics(
    const ymodem_receiver_t *receiver,
    ymodem_receiver_statistics_t *statistics)
{
    if ((receiver == NULL) || (statistics == NULL)) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    *statistics = receiver->statistics;
    return PLATFORM_ERR_OK;
}
//******************************** Public Functions *************************//
