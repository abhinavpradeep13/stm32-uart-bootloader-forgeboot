
#include "crc32.h"

#define CRC32_POLYNOMIAL    0xEDB88320U
#define CRC32_INITIAL       0xFFFFFFFFU
#define CRC32_FINAL_XOR     0xFFFFFFFFU

uint32_t CRC32_Calculate(const uint8_t *data, uint32_t length)
{
    uint32_t crc = CRC32_INITIAL;

    for (uint32_t i = 0; i < length; i++)
    {
        crc ^= data[i];

        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 1U)
            {
                crc = (crc >> 1) ^ CRC32_POLYNOMIAL;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc ^ CRC32_FINAL_XOR;
}
