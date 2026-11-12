
#include "protocol.h"
#include "uart.h"
#include "crc32.h"

/*
 * Receive one byte with timeout.
 *
 * Maps UART receive status to protocol status.
 */
static ProtocolStatus Protocol_ReceiveByte(uint8_t *byte)
{
    uint8_t status;

    status = UART1_ReceiveCharTimeout(
        byte,
        PROTOCOL_BYTE_TIMEOUT_MS
    );

    if (status == UART_RX_OK)
    {
        return PROTOCOL_OK;
    }

    if (status == UART_RX_TIMEOUT)
    {
        return PROTOCOL_TIMEOUT;
    }

    return PROTOCOL_ERROR;
}


/*
 * Receive a complete protocol packet.
 *
 * Packet structure:
 *
 * SYNC1 | SYNC2 | TYPE | SEQ_L | SEQ_H |
 * LEN_L | LEN_H | PAYLOAD | CRC32
 *
 * CRC32 covers TYPE through PAYLOAD.
 */
ProtocolStatus Protocol_ReceivePacket(ProtocolPacket *packet)
{
    uint8_t byte;
    uint8_t state = 0U;

    uint8_t type;
    uint16_t sequence;
    uint16_t length;

    uint32_t received_crc;
    uint32_t calculated_crc;

    uint8_t crc_buffer[5U + PROTOCOL_MAX_PAYLOAD];
    uint16_t index;

    ProtocolStatus status;

    if (packet == 0)
    {
        return PROTOCOL_INVALID_ARGUMENT;
    }

    /* STEP 1: Synchronize with packet start */

    while (state < 2U)
    {
        status = Protocol_ReceiveByte(&byte);

        if (status != PROTOCOL_OK)
        {
            return status;
        }

        if (state == 0U)
        {
            if (byte == PROTOCOL_SYNC_1)
            {
                state = 1U;
            }
        }
        else
        {
            if (byte == PROTOCOL_SYNC_2)
            {
                state = 2U;
            }
            else if (byte == PROTOCOL_SYNC_1)
            {
                state = 1U;
            }
            else
            {
                state = 0U;
            }
        }
    }

    /* STEP 2: Receive packet header */

    status = Protocol_ReceiveByte(&type);
    if (status != PROTOCOL_OK) return status;

    status = Protocol_ReceiveByte(&byte);
    if (status != PROTOCOL_OK) return status;

    sequence = byte;

    status = Protocol_ReceiveByte(&byte);
    if (status != PROTOCOL_OK) return status;

    sequence |= (uint16_t)byte << 8U;

    status = Protocol_ReceiveByte(&byte);
    if (status != PROTOCOL_OK) return status;

    length = byte;

    status = Protocol_ReceiveByte(&byte);
    if (status != PROTOCOL_OK) return status;

    length |= (uint16_t)byte << 8U;

    /* Reject oversized payloads */

    if (length > PROTOCOL_MAX_PAYLOAD)
    {
        return PROTOCOL_INVALID_LENGTH;
    }

    /* STEP 3: Store header for CRC calculation */

    crc_buffer[0] = type;
    crc_buffer[1] = (uint8_t)(sequence & 0xFFU);
    crc_buffer[2] = (uint8_t)(sequence >> 8U);
    crc_buffer[3] = (uint8_t)(length & 0xFFU);
    crc_buffer[4] = (uint8_t)(length >> 8U);

    /* STEP 4: Receive payload */

    for (index = 0U; index < length; index++)
    {
        status = Protocol_ReceiveByte(&byte);

        if (status != PROTOCOL_OK)
        {
            return status;
        }

        packet->payload[index] = byte;
        crc_buffer[5U + index] = byte;
    }

    /* STEP 5: Receive transmitted CRC32 */

    received_crc = 0U;

    for (index = 0U; index < 4U; index++)
    {
        status = Protocol_ReceiveByte(&byte);

        if (status != PROTOCOL_OK)
        {
            return status;
        }

        received_crc |= (uint32_t)byte << (8U * index);
    }

    /* STEP 6: Calculate and verify CRC32 */

    calculated_crc = CRC32_Calculate(
        crc_buffer,
        5U + length
    );

    if (calculated_crc != received_crc)
    {
        return PROTOCOL_CRC_ERROR;
    }

    /* STEP 7: Packet successfully validated */

    packet->type = type;
    packet->sequence = sequence;
    packet->length = length;

    return PROTOCOL_OK;
}


/*
 * Transmit a complete protocol packet.
 *
 * CRC32 covers TYPE + SEQUENCE + LENGTH + PAYLOAD.
 */
ProtocolStatus Protocol_SendPacket(
    uint8_t type,
    uint16_t sequence,
    const uint8_t *payload,
    uint16_t length)
{
    uint8_t crc_buffer[5U + PROTOCOL_MAX_PAYLOAD];

    uint32_t crc;
    uint16_t index;

    if (length > PROTOCOL_MAX_PAYLOAD)
    {
        return PROTOCOL_INVALID_LENGTH;
    }

    if ((length > 0U) && (payload == 0))
    {
        return PROTOCOL_INVALID_ARGUMENT;
    }

    /* Construct CRC calculation buffer */

    crc_buffer[0] = type;
    crc_buffer[1] = (uint8_t)(sequence & 0xFFU);
    crc_buffer[2] = (uint8_t)(sequence >> 8U);
    crc_buffer[3] = (uint8_t)(length & 0xFFU);
    crc_buffer[4] = (uint8_t)(length >> 8U);

    for (index = 0U; index < length; index++)
    {
        crc_buffer[5U + index] = payload[index];
    }

    /* Calculate CRC32 */

    crc = CRC32_Calculate(crc_buffer, 5U + length);

    /* Send synchronization bytes */

    UART1_SendChar((char)PROTOCOL_SYNC_1);
    UART1_SendChar((char)PROTOCOL_SYNC_2);

    /* Send packet header */

    UART1_SendChar((char)type);
    UART1_SendChar((char)(sequence & 0xFFU));
    UART1_SendChar((char)(sequence >> 8U));
    UART1_SendChar((char)(length & 0xFFU));
    UART1_SendChar((char)(length >> 8U));

    /* Send payload */

    for (index = 0U; index < length; index++)
    {
        UART1_SendChar((char)payload[index]);
    }

    /* Send CRC32 in little-endian format */

    UART1_SendChar((char)(crc & 0xFFU));
    UART1_SendChar((char)((crc >> 8U) & 0xFFU));
    UART1_SendChar((char)((crc >> 16U) & 0xFFU));
    UART1_SendChar((char)((crc >> 24U) & 0xFFU));

    return PROTOCOL_OK;
}


/* Send ACK: empty payload */

ProtocolStatus Protocol_SendACK(uint16_t sequence)
{
    return Protocol_SendPacket(
        CMD_ACK,
        sequence,
        0,
        0U
    );
}


/* Send NACK: one-byte reason code */

ProtocolStatus Protocol_SendNACK(
    uint16_t sequence,
    uint8_t reason)
{
    return Protocol_SendPacket(
        CMD_NACK,
        sequence,
        &reason,
        1U
    );
}
