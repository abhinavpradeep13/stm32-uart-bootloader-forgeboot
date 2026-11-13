
#ifndef METADATA_H
#define METADATA_H

#include <stdint.h>

#define METADATA_ADDRESS    0x08004000U
#define METADATA_MAGIC      0x46424731U
#define METADATA_VERSION    1U
#define METADATA_COMMIT     0xC04D17EDU

typedef struct
{
    uint32_t image_size;
    uint32_t image_crc;
} FirmwareMetadata;

uint8_t Metadata_Write(uint32_t image_size, uint32_t image_crc);
uint8_t Metadata_Read(FirmwareMetadata *metadata);

#endif
