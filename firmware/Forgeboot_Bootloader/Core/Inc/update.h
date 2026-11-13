
#ifndef UPDATE_H
#define UPDATE_H

#include <stdint.h>
#include "protocol.h"

#define UPDATE_DATA_OFFSET_SIZE    4U
#define UPDATE_DATA_MAX_SIZE       (PROTOCOL_MAX_PAYLOAD - UPDATE_DATA_OFFSET_SIZE)
/* Firmware update states */

typedef enum
{
    UPDATE_STATE_IDLE = 0,
    UPDATE_STATE_RECEIVING,
    UPDATE_STATE_VERIFYING,
    UPDATE_STATE_COMMITTING,
    UPDATE_STATE_COMPLETE,
    UPDATE_STATE_ERROR
} UpdateState;


/* Firmware update context */

typedef struct
{
    UpdateState state;

    uint32_t expected_size;
    uint32_t expected_crc;

    uint32_t received_size;
    uint32_t flash_address;

    uint16_t begin_sequence;

    uint16_t expected_sequence;
    uint16_t last_sequence;

    uint8_t last_packet_valid;

    uint32_t last_data_offset;
    uint32_t last_data_crc;
    uint16_t last_data_length;

} UpdateContext;

uint8_t Update_IsComplete(void);

/* Initialize firmware update manager */

void Update_Init(void);

/* Process one incoming protocol packet */

void Update_Process(void);

#endif
