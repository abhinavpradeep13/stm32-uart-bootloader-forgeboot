
#include "update.h"
#include "protocol.h"
#include "flash.h"
#include "crc32.h"
#include "metadata.h"

/* Global firmware update context */
static UpdateContext update_context;

/* Track the most recently completed ABORT command. */
static uint8_t abort_packet_valid = 0U;
static uint16_t last_abort_sequence = 0U;


/* Convert four little-endian bytes into uint32_t */
static uint32_t Update_ReadUint32LE(const uint8_t *data)
{
    return ((uint32_t)data[0])
         | ((uint32_t)data[1] << 8U)
         | ((uint32_t)data[2] << 16U)
         | ((uint32_t)data[3] << 24U);
}


/* Initialize firmware update manager */
void Update_Init(void)
{
    update_context.state = UPDATE_STATE_IDLE;

    update_context.expected_size = 0U;
    update_context.expected_crc = 0U;

    update_context.received_size = 0U;
    update_context.flash_address = 0U;

    update_context.begin_sequence = 0U;

    update_context.expected_sequence = 0U;
    update_context.last_sequence = 0U;

    update_context.last_packet_valid = 0U;

    update_context.last_data_offset = 0U;
    update_context.last_data_crc = 0U;
    update_context.last_data_length = 0U;
}

/* Check whether firmware update completed successfully. */
uint8_t Update_IsComplete(void)
{
    return (update_context.state == UPDATE_STATE_COMPLETE);
}


/*
 * Process CMD_BEGIN.
 *
 * Payload:
 * Bytes 0-3: Firmware size
 * Bytes 4-7: Expected firmware CRC32
 */
static void Update_HandleBegin(const ProtocolPacket *packet)
{
    uint32_t image_size;
    uint32_t image_crc;
    FlashStatus flash_status;

    /* BEGIN packet must contain exactly 8 bytes. */
    if (packet->length != 8U)
    {
        Protocol_SendNACK(packet->sequence, NACK_INVALID_LENGTH);
        return;
    }

    /* Extract firmware information. */
    image_size = Update_ReadUint32LE(&packet->payload[0]);
    image_crc  = Update_ReadUint32LE(&packet->payload[4]);

    /* Handle retransmission of the original BEGIN packet. */
    if ((update_context.state == UPDATE_STATE_RECEIVING) &&
        (packet->sequence == update_context.begin_sequence))
    {
        if ((image_size == update_context.expected_size) &&
            (image_crc == update_context.expected_crc))
        {
            Protocol_SendACK(packet->sequence);
        }
        else
        {
            Protocol_SendNACK(packet->sequence, NACK_INVALID_SEQUENCE);
        }

        return;
    }

    /* A new update can only start from IDLE. */
    if (update_context.state != UPDATE_STATE_IDLE)
    {
        Protocol_SendNACK(packet->sequence, NACK_INVALID_STATE);
        return;
    }

    /* Validate firmware image size. */
    if ((image_size < 8U) ||
        (image_size > (FLASH_APP_END_ADDR - FLASH_APP_START_ADDR)))
    {
        Protocol_SendNACK(packet->sequence, NACK_INVALID_LENGTH);
        return;
    }

    /* Erase application Flash sectors. */
    flash_status = Flash_EraseApplication();

    if (flash_status != FLASH_STATUS_OK)
    {
        update_context.state = UPDATE_STATE_ERROR;
        Protocol_SendNACK(packet->sequence, NACK_FLASH_ERROR);
        return;
    }

    /* A new update invalidates the previous ABORT record. */
    abort_packet_valid = 0U;

    /* Initialize firmware update information. */
    update_context.expected_size = image_size;
    update_context.expected_crc = image_crc;
    update_context.received_size = 0U;
    update_context.flash_address = FLASH_APP_START_ADDR;

    update_context.begin_sequence = packet->sequence;

    /* Initialize packet tracking. */
    update_context.last_sequence = packet->sequence;
    update_context.last_packet_valid = 1U;

    update_context.last_data_offset = 0U;
    update_context.last_data_crc = 0U;
    update_context.last_data_length = 0U;

    update_context.expected_sequence =
        (uint16_t)(packet->sequence + 1U);

    /* Enter firmware reception state. */
    update_context.state = UPDATE_STATE_RECEIVING;

    Protocol_SendACK(packet->sequence);
}

static uint8_t Update_ProgramData(uint32_t offset,
                                  const uint8_t *data,
                                  uint32_t length)
{
    uint32_t i;
    uint32_t j;
    uint32_t word;
    uint32_t address;

    for (i = 0U; i < length; i += 4U)
    {
        /* Start with erased Flash value.
           This also pads the final partial word with 0xFF. */
        word = 0xFFFFFFFFU;

        for (j = 0U; (j < 4U) && ((i + j) < length); j++)
        {
            word &= ~(0xFFU << (j * 8U));
            word |= ((uint32_t)data[i + j] << (j * 8U));
        }

        address = FLASH_APP_START_ADDR + offset + i;

        if (Flash_ProgramWord(address, word) != FLASH_STATUS_OK)
        {
            return 0U;
        }
    }

    return 1U;
}

/*
 * Process CMD_DATA.
 *
 * Payload:
 * Bytes 0-3: Firmware offset
 * Bytes 4 onwards: Firmware data
 */
static void Update_HandleData(const ProtocolPacket *packet)
{
    uint32_t offset;
    uint32_t data_length;
    uint32_t data_crc;

    /* DATA packet must contain offset + at least one data byte. */
    if (packet->length <= UPDATE_DATA_OFFSET_SIZE)
    {
        Protocol_SendNACK(packet->sequence, NACK_INVALID_LENGTH);
        return;
    }

    if (update_context.state != UPDATE_STATE_RECEIVING)
    {
        Protocol_SendNACK(packet->sequence, NACK_INVALID_STATE);
        return;
    }

    offset = Update_ReadUint32LE(&packet->payload[0]);
    data_length = packet->length - UPDATE_DATA_OFFSET_SIZE;

    data_crc = CRC32_Calculate(
        &packet->payload[UPDATE_DATA_OFFSET_SIZE],
        data_length
    );

    /* Handle retransmission of the immediately previous DATA packet. */
    if ((update_context.last_packet_valid != 0U) &&
        (packet->sequence == update_context.last_sequence))
    {
        if ((offset == update_context.last_data_offset) &&
            (data_length == update_context.last_data_length) &&
            (data_crc == update_context.last_data_crc))
        {
            Protocol_SendACK(packet->sequence);
        }
        else
        {
            Protocol_SendNACK(packet->sequence, NACK_INVALID_SEQUENCE);
        }

        return;
    }

    /* Verify sequence number. */
    if (packet->sequence != update_context.expected_sequence)
    {
        Protocol_SendNACK(packet->sequence, NACK_INVALID_SEQUENCE);
        return;
    }

    /* Verify firmware offset. */
    if (offset != update_context.received_size)
    {
        Protocol_SendNACK(packet->sequence, NACK_INVALID_SEQUENCE);
        return;
    }

    /* Verify word alignment. */
    if ((offset % 4U) != 0U)
    {
        Protocol_SendNACK(packet->sequence, NACK_INVALID_LENGTH);
        return;
    }

    /* Ensure the packet does not exceed the remaining image size. */
    if (data_length >
        (update_context.expected_size - update_context.received_size))
    {
        Protocol_SendNACK(packet->sequence, NACK_INVALID_LENGTH);
        return;
    }

    /*
     * Every non-final packet must contain a multiple of four bytes.
     * The final packet may contain 1–3 additional bytes.
     */
    if ((data_length % 4U != 0U) &&
        (data_length !=
         (update_context.expected_size - update_context.received_size)))
    {
        Protocol_SendNACK(packet->sequence, NACK_INVALID_LENGTH);
        return;
    }

    /* Program the firmware data into application Flash. */
    if (Update_ProgramData(
            offset,
            &packet->payload[UPDATE_DATA_OFFSET_SIZE],
            data_length) == 0U)
    {
        update_context.state = UPDATE_STATE_ERROR;

        Protocol_SendNACK(packet->sequence, NACK_FLASH_ERROR);
        return;
    }

    /* Update context only after successful Flash programming. */
    update_context.received_size += data_length;

    update_context.last_sequence = packet->sequence;
    update_context.last_packet_valid = 1U;

    update_context.last_data_offset = offset;
    update_context.last_data_length = (uint16_t)data_length;
    update_context.last_data_crc = data_crc;

    update_context.expected_sequence =
        (uint16_t)(packet->sequence + 1U);

    /* Confirm successful reception and programming. */
    Protocol_SendACK(packet->sequence);
}

static void Update_HandleEnd(const ProtocolPacket *packet)
{
    uint32_t calculated_crc;
    uint32_t i;

    /* END packet must have an empty payload. */
    if (packet->length != 0U)
    {
        Protocol_SendNACK(
            packet->sequence,
            NACK_INVALID_LENGTH
        );

        return;
    }
    /* Handle retransmission after successful completion. */
    if ((update_context.state == UPDATE_STATE_COMPLETE) &&
        (packet->sequence == update_context.last_sequence))
    {
        Protocol_SendACK(packet->sequence);
        return;
    }

    /* An update must be in the receiving state. */
    if (update_context.state != UPDATE_STATE_RECEIVING)
    {
        Protocol_SendNACK(packet->sequence, NACK_INVALID_STATE);
        return;
    }

    /* Verify packet sequence. */
    if (packet->sequence != update_context.expected_sequence)
    {
        Protocol_SendNACK(packet->sequence, NACK_INVALID_SEQUENCE);
        return;
    }
    /* Verify that the complete firmware image was received. */
    if (update_context.received_size != update_context.expected_size)
    {
        Protocol_SendNACK(packet->sequence, NACK_INVALID_LENGTH);
        return;
    }

    update_context.state = UPDATE_STATE_VERIFYING;

    /* Calculate CRC32 directly from application Flash. */
    calculated_crc = 0xFFFFFFFFU;

    for (i = 0U; i < update_context.expected_size; i++)
    {
        uint32_t address = FLASH_APP_START_ADDR + i;
        uint32_t word_address = address & ~3U;
        uint32_t word = Flash_ReadWord(word_address);
        uint8_t byte = (uint8_t)(word >> ((address & 3U) * 8U));

        calculated_crc ^= byte;

        for (uint8_t bit = 0U; bit < 8U; bit++)
        {
            if ((calculated_crc & 1U) != 0U)
            {
                calculated_crc =
                    (calculated_crc >> 1U) ^ 0xEDB88320U;
            }
            else
            {
                calculated_crc >>= 1U;
            }
        }
    }

    calculated_crc ^= 0xFFFFFFFFU;

    /* Reject corrupted firmware. */
    if (calculated_crc != update_context.expected_crc)
    {
        update_context.state = UPDATE_STATE_ERROR;
        Protocol_SendNACK(packet->sequence, NACK_CRC_ERROR);
        return;
    }

    update_context.state = UPDATE_STATE_COMMITTING;

    /* Commit metadata only after successful CRC verification. */
    if (Metadata_Write(update_context.expected_size,
                       calculated_crc) == 0U)
    {
        update_context.state = UPDATE_STATE_ERROR;
        Protocol_SendNACK(packet->sequence, NACK_FLASH_ERROR);
        return;
    }

    update_context.last_sequence = packet->sequence;

    update_context.state = UPDATE_STATE_COMPLETE;

    Protocol_SendACK(packet->sequence);
}

static void Update_HandleAbort(const ProtocolPacket *packet)
{
    /* ABORT must not contain a payload. */
    if (packet->length != 0U)
    {
        Protocol_SendNACK(packet->sequence, NACK_INVALID_LENGTH);
        return;
    }

    /* Handle retransmission of the previous ABORT command. */
    if ((abort_packet_valid != 0U) &&
        (packet->sequence == last_abort_sequence))
    {
        Protocol_SendACK(packet->sequence);
        return;
    }

    /* ABORT is valid only during an active or failed update. */
    if ((update_context.state != UPDATE_STATE_RECEIVING) &&
        (update_context.state != UPDATE_STATE_ERROR))
    {
        Protocol_SendNACK(packet->sequence, NACK_INVALID_STATE);
        return;
    }

    /* Remember the ABORT packet before resetting the context. */
    last_abort_sequence = packet->sequence;
    abort_packet_valid = 1U;

    /* Reset the update manager. */
    Update_Init();

    /* Acknowledge the ABORT command. */
    Protocol_SendACK(packet->sequence);
}
/*
 * Process CMD_PING.
 *
 * Used by the host to verify that ForgeBoot
 * is actively responding in recovery mode.
 *
 * PING does not modify the update state.
 */
static void Update_HandlePing(const ProtocolPacket *packet)
{
    /* PING must contain an empty payload. */
    if (packet->length != 0U)
    {
        Protocol_SendNACK(packet->sequence, NACK_INVALID_LENGTH);
        return;
    }

    /* Confirm that the bootloader is responsive. */
    Protocol_SendACK(packet->sequence);
}
/*
 * Receive and process one protocol packet.
 */
void Update_Process(void)
{
    ProtocolPacket packet;
    ProtocolStatus status;

    status = Protocol_ReceivePacket(&packet);

    if (status != PROTOCOL_OK)
    {
        return;
    }

    switch (packet.type)
    {
    	case CMD_PING:

    		Update_HandlePing(&packet);
    		break;

    	case CMD_BEGIN:

            Update_HandleBegin(&packet);
            break;

        case CMD_DATA:

            Update_HandleData(&packet);
            break;

        case CMD_END:

            Update_HandleEnd(&packet);
            break;

        case CMD_ABORT:

            Update_HandleAbort(&packet);
            break;

        default:

            Protocol_SendNACK(
                packet.sequence,
                NACK_INVALID_COMMAND
            );
            break;
    }
}
