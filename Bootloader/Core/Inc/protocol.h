#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/* Packet configuration */
#define PROTOCOL_SYNC_1          0xA5U
#define PROTOCOL_SYNC_2          0x5AU

#define PROTOCOL_MAX_PAYLOAD     256U

/* Command identifiers */
#define CMD_BEGIN                0x01U
#define CMD_DATA                 0x02U
#define CMD_END                  0x03U
#define CMD_ABORT                0x04U

/* Response identifiers */
#define CMD_ACK                  0x80U
#define CMD_NACK                 0x81U

#define PROTOCOL_BYTE_TIMEOUT_MS    1000U

/* Packet reception status */
typedef enum
{
    PROTOCOL_OK = 0,
    PROTOCOL_ERROR,
    PROTOCOL_INVALID_LENGTH,
    PROTOCOL_CRC_ERROR,
    PROTOCOL_INVALID_ARGUMENT,
    PROTOCOL_TIMEOUT
} ProtocolStatus;

/* Received packet structure */
typedef struct
{
    uint8_t type;
    uint16_t sequence;
    uint16_t length;

    uint8_t payload[PROTOCOL_MAX_PAYLOAD];

} ProtocolPacket;

/* NACK reason codes */
#define NACK_INVALID_COMMAND      0x01U
#define NACK_INVALID_LENGTH       0x02U
#define NACK_CRC_ERROR            0x03U
#define NACK_INVALID_SEQUENCE     0x04U
#define NACK_FLASH_ERROR          0x05U
#define NACK_INVALID_STATE        0x06U

/* Packet transmission functions */
ProtocolStatus Protocol_SendPacket(
    uint8_t type,
    uint16_t sequence,
    const uint8_t *payload,
    uint16_t length
);

ProtocolStatus Protocol_SendACK(uint16_t sequence);

ProtocolStatus Protocol_SendNACK(
    uint16_t sequence,
    uint8_t reason
);

/* Receive and validate one complete packet */
ProtocolStatus Protocol_ReceivePacket(ProtocolPacket *packet);

#endif
