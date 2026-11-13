
#include "stm32f411xe.h"
#include "flash.h"

/* Flash controller register bits */

#define FLASH_SR_ERROR_MASK      (0x000001F2U)
#define FLASH_SR_CLEAR_MASK      (0x000001F3U)

#define FLASH_CR_SNB_MASK        (0xFU << 3)
#define FLASH_CR_PSIZE_MASK      (3U << 8)

/* 32-bit programming: voltage range 2.7 V to 3.6 V */
#define FLASH_PSIZE_WORD         (2U << 8)

/* Software polling timeout */
#define FLASH_TIMEOUT_LOOPS     10000000U


/* -------------------------------------------------------
 * Internal function: wait for Flash controller
 * ------------------------------------------------------- */
static FlashStatus Flash_WaitForReady(void)
{
    uint32_t timeout = FLASH_TIMEOUT_LOOPS;

    while (FLASH->SR & FLASH_SR_BSY)
    {
        if (timeout == 0U)
        {
            return FLASH_STATUS_TIMEOUT;
        }

        timeout--;
    }

    return FLASH_STATUS_OK;
}


/* -------------------------------------------------------
 * Internal function: unlock Flash controller
 * ------------------------------------------------------- */
static void Flash_Unlock(void)
{
    if (FLASH->CR & FLASH_CR_LOCK)
    {
        FLASH->KEYR = 0x45670123U;
        FLASH->KEYR = 0xCDEF89ABU;
    }
}


/* -------------------------------------------------------
 * Internal function: lock Flash controller
 * ------------------------------------------------------- */
static void Flash_Lock(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}


/* -------------------------------------------------------
 * Internal function: clear Flash status flags
 * ------------------------------------------------------- */
static void Flash_ClearFlags(void)
{
    FLASH->SR = FLASH_SR_CLEAR_MASK;
}


/* -------------------------------------------------------
 * Internal function: validate writable address
 *
 * Sector 0 is deliberately excluded.
 * ------------------------------------------------------- */
static uint8_t Flash_IsWritableAddress(uint32_t address)
{
    if (address < FLASH_METADATA_START_ADDR)
    {
        return 0U;
    }

    if (address >= FLASH_APP_END_ADDR)
    {
        return 0U;
    }

    return 1U;
}


/* -------------------------------------------------------
 * Internal function: erase one sector
 * ------------------------------------------------------- */
static FlashStatus Flash_EraseSector(uint32_t sector)
{
    FlashStatus status;
    uint32_t primask;

    /* Only sectors 1 through 7 are permitted */
    if ((sector < 1U) || (sector > 7U))
    {
        return FLASH_STATUS_INVALID_ADDRESS;
    }

    status = Flash_WaitForReady();

    if (status != FLASH_STATUS_OK)
    {
        return status;
    }

    /* Preserve the previous interrupt state */
    primask = __get_PRIMASK();
    __disable_irq();

    Flash_Unlock();
    Flash_ClearFlags();

    /* Configure sector erase and 32-bit parallelism */
    FLASH->CR &= ~(FLASH_CR_SNB_MASK | FLASH_CR_PSIZE_MASK);

    FLASH->CR |= FLASH_CR_SER;
    FLASH->CR |= (sector << 3);
    FLASH->CR |= FLASH_PSIZE_WORD;

    /* Start erase operation */
    FLASH->CR |= FLASH_CR_STRT;

    status = Flash_WaitForReady();

    /* Wait until the Flash controller is no longer busy. */
    if (status != FLASH_STATUS_OK)
    {
        while (FLASH->SR & FLASH_SR_BSY)
        {
            /* Wait for the ongoing operation to finish. */
        }
    }
    /* Disable sector erase mode */
    FLASH->CR &= ~FLASH_CR_SER;

    if (status == FLASH_STATUS_OK)
    {
        if (FLASH->SR & FLASH_SR_ERROR_MASK)
        {
            status = FLASH_STATUS_ERROR;
        }
    }

    Flash_ClearFlags();
    Flash_Lock();

    /* Restore previous interrupt state */
    if ((primask & 1U) == 0U)
    {
        __enable_irq();
    }

    return status;
}


/* -------------------------------------------------------
 * Public function: erase metadata sector 1
 * ------------------------------------------------------- */
FlashStatus Flash_EraseMetadata(void)
{
    return Flash_EraseSector(1U);
}


/* -------------------------------------------------------
 * Public function: erase application sectors 2 to 7
 * ------------------------------------------------------- */
FlashStatus Flash_EraseApplication(void)
{
    FlashStatus status;

    for (uint32_t sector = 2U; sector <= 7U; sector++)
    {
        status = Flash_EraseSector(sector);

        if (status != FLASH_STATUS_OK)
        {
            return status;
        }
    }

    return FLASH_STATUS_OK;
}


/* -------------------------------------------------------
 * Public function: program one 32-bit word
 * ------------------------------------------------------- */
FlashStatus Flash_ProgramWord(uint32_t address, uint32_t data)
{
    FlashStatus status;
    uint32_t primask;

    /* Validate address */
    if (!Flash_IsWritableAddress(address))
    {
        return FLASH_STATUS_INVALID_ADDRESS;
    }

    /* Flash word programming requires 4-byte alignment */
    if ((address & 3U) != 0U)
    {
        return FLASH_STATUS_INVALID_ADDRESS;
    }

    status = Flash_WaitForReady();

    if (status != FLASH_STATUS_OK)
    {
        return status;
    }

    /*
     * STM32 Flash programming changes bits from 1 to 0.
     * Require the target word to be erased first.
     */
    if (*(volatile uint32_t *)address != 0xFFFFFFFFU)
    {
        return FLASH_STATUS_NOT_ERASED;
    }

    /* Preserve previous interrupt state */
    primask = __get_PRIMASK();
    __disable_irq();

    Flash_Unlock();
    Flash_ClearFlags();

    /* Select 32-bit programming */
    FLASH->CR &= ~FLASH_CR_PSIZE_MASK;
    FLASH->CR |= FLASH_PSIZE_WORD;

    /* Enable programming */
    FLASH->CR |= FLASH_CR_PG;

    /* Write data to Flash */
    *(volatile uint32_t *)address = data;

    status = Flash_WaitForReady();

    /* Wait until the Flash controller is no longer busy. */
    if (status != FLASH_STATUS_OK)
    {
        while (FLASH->SR & FLASH_SR_BSY)
        {
            /* Wait for the ongoing operation to finish. */
        }
    }

    /* Disable programming mode */
    FLASH->CR &= ~FLASH_CR_PG;

    if (status == FLASH_STATUS_OK)
    {
        if (FLASH->SR & FLASH_SR_ERROR_MASK)
        {
            status = FLASH_STATUS_ERROR;
        }
    }

    Flash_ClearFlags();
    Flash_Lock();

    /* Restore previous interrupt state */
    if ((primask & 1U) == 0U)
    {
        __enable_irq();
    }

    if (status != FLASH_STATUS_OK)
    {
        return status;
    }

    /* Verify programmed word */
    if (*(volatile uint32_t *)address != data)
    {
        return FLASH_STATUS_ERROR;
    }

    return FLASH_STATUS_OK;
}


/* -------------------------------------------------------
 * Public function: read one 32-bit word
 * ------------------------------------------------------- */
uint32_t Flash_ReadWord(uint32_t address)
{
    if ((address < FLASH_METADATA_START_ADDR) ||
        (address >= FLASH_APP_END_ADDR) ||
        ((address & 3U) != 0U))
    {
        return 0xFFFFFFFFU;
    }

    return *(volatile uint32_t *)address;
}
