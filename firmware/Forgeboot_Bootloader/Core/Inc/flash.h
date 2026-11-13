
#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>

/* STM32F411CEU6 Flash memory allocation */
#define FLASH_METADATA_START_ADDR  0x08004000U
#define FLASH_METADATA_END_ADDR    0x08008000U

#define FLASH_APP_START_ADDR       0x08008000U
#define FLASH_APP_END_ADDR         0x08080000U

typedef enum
{
    FLASH_STATUS_OK = 0,
    FLASH_STATUS_ERROR,
    FLASH_STATUS_TIMEOUT,
    FLASH_STATUS_INVALID_ADDRESS,
    FLASH_STATUS_NOT_ERASED
} FlashStatus;

/* Erase metadata sector 1 only */
FlashStatus Flash_EraseMetadata(void);

/* Erase application sectors 2 through 7 */
FlashStatus Flash_EraseApplication(void);

/*
 * Program one aligned 32-bit word.
 * Permitted address range: metadata and application sectors.
 * Sector 0 is protected.
 */
FlashStatus Flash_ProgramWord(uint32_t address, uint32_t data);

/* Read one 32-bit word from metadata or application Flash */
uint32_t Flash_ReadWord(uint32_t address);

#endif
