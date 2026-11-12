
#include "metadata.h"
#include "flash.h"

/*
 * Metadata sector layout:
 *
 * 0x08004000 : Magic number
 * 0x08004004 : Metadata version
 * 0x08004008 : Firmware image size
 * 0x0800400C : Firmware image CRC32
 * 0x08004010 : Commit marker
 *
 * The commit marker is programmed LAST.
 */

#define META_MAGIC_ADDR       (METADATA_ADDRESS + 0x00U)
#define META_VERSION_ADDR     (METADATA_ADDRESS + 0x04U)
#define META_SIZE_ADDR        (METADATA_ADDRESS + 0x08U)
#define META_CRC_ADDR         (METADATA_ADDRESS + 0x0CU)
#define META_COMMIT_ADDR      (METADATA_ADDRESS + 0x10U)

/* Maximum application image size */
#define MAX_IMAGE_SIZE        (FLASH_APP_END_ADDR - FLASH_APP_START_ADDR)


/* -------------------------------------------------------
 * Write firmware metadata
 *
 * Returns:
 * 1 = Success
 * 0 = Failure
 * ------------------------------------------------------- */
uint8_t Metadata_Write(uint32_t image_size, uint32_t image_crc)
{
    FlashStatus status;

    /* Validate image size before erasing metadata */
    if ((image_size == 0U) || (image_size > MAX_IMAGE_SIZE))
    {
        return 0U;
    }

    /* Erase metadata sector 1 */
    status = Flash_EraseMetadata();

    if (status != FLASH_STATUS_OK)
    {
        return 0U;
    }

    /* Write metadata magic number */
    status = Flash_ProgramWord(META_MAGIC_ADDR, METADATA_MAGIC);

    if (status != FLASH_STATUS_OK)
    {
        return 0U;
    }

    /* Write metadata version */
    status = Flash_ProgramWord(META_VERSION_ADDR, METADATA_VERSION);

    if (status != FLASH_STATUS_OK)
    {
        return 0U;
    }

    /* Write firmware image size */
    status = Flash_ProgramWord(META_SIZE_ADDR, image_size);

    if (status != FLASH_STATUS_OK)
    {
        return 0U;
    }

    /* Write firmware CRC32 */
    status = Flash_ProgramWord(META_CRC_ADDR, image_crc);

    if (status != FLASH_STATUS_OK)
    {
        return 0U;
    }

    /*
     * Commit marker is written LAST.
     * This indicates that all preceding metadata
     * fields were successfully programmed.
     */
    status = Flash_ProgramWord(META_COMMIT_ADDR, METADATA_COMMIT);

    if (status != FLASH_STATUS_OK)
    {
        return 0U;
    }

    /* Verify the committed metadata. */
    {
        FirmwareMetadata verify_metadata;

        if (Metadata_Read(&verify_metadata) == 0U)
        {
            return 0U;
        }

        if ((verify_metadata.image_size != image_size) ||
            (verify_metadata.image_crc != image_crc))
        {
            return 0U;
        }
    }

    return 1U;
}


/* -------------------------------------------------------
 * Read and validate firmware metadata
 *
 * Returns:
 * 1 = Valid metadata
 * 0 = Invalid metadata
 * ------------------------------------------------------- */
uint8_t Metadata_Read(FirmwareMetadata *metadata)
{
    uint32_t magic;
    uint32_t version;
    uint32_t commit;
    uint32_t image_size;
    uint32_t image_crc;

    /* Check pointer */
    if (metadata == 0)
    {
        return 0U;
    }

    /* Read metadata fields */
    magic      = Flash_ReadWord(META_MAGIC_ADDR);
    version    = Flash_ReadWord(META_VERSION_ADDR);
    image_size = Flash_ReadWord(META_SIZE_ADDR);
    image_crc  = Flash_ReadWord(META_CRC_ADDR);
    commit     = Flash_ReadWord(META_COMMIT_ADDR);

    /* Validate magic number */
    if (magic != METADATA_MAGIC)
    {
        return 0U;
    }

    /* Validate metadata version */
    if (version != METADATA_VERSION)
    {
        return 0U;
    }

    /* Validate commit marker */
    if (commit != METADATA_COMMIT)
    {
        return 0U;
    }

    /* Validate image size */
    if ((image_size == 0U) || (image_size > MAX_IMAGE_SIZE))
    {
        return 0U;
    }

    /* Return validated metadata */
    metadata->image_size = image_size;
    metadata->image_crc = image_crc;

    return 1U;
}
