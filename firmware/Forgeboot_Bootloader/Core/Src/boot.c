#include "boot.h"
#include "metadata.h"
#include "crc32.h"
#include "stm32f411xe.h"

/* STM32F411CEU6 SRAM boundaries */
#define SRAM_START_ADDR     0x20000000U
#define SRAM_END_ADDR       0x20020000U

/*
 * Checks whether the application has:
 * 1. Valid metadata
 * 2. Valid image size
 * 3. Matching CRC32
 * 4. Valid initial stack pointer
 * 5. Valid reset handler address
 */
uint8_t Boot_IsApplicationValid(void)
{
    FirmwareMetadata metadata;

    uint32_t stack_pointer;
    uint32_t reset_handler;
    uint32_t reset_address;
    uint32_t calculated_crc;
    uint32_t image_end;

    /* Step 1: Read and validate metadata */
    if (Metadata_Read(&metadata) == 0U)
    {
        return 0U;
    }

    /* Step 2: Validate image size */
    if (metadata.image_size < 8U)
    {
        return 0U;
    }

    if (metadata.image_size > (APP_END_ADDR - APP_START_ADDR))
    {
        return 0U;
    }

    image_end = APP_START_ADDR + metadata.image_size;

    /* Step 3: Calculate CRC32 of the application */
    calculated_crc = CRC32_Calculate(
        (const uint8_t *)APP_START_ADDR,
        metadata.image_size
    );

    if (calculated_crc != metadata.image_crc)
    {
        return 0U;
    }

    /* Step 4: Read the application's vector table */
    stack_pointer = *(volatile uint32_t *)APP_START_ADDR;
    reset_handler = *(volatile uint32_t *)(APP_START_ADDR + 4U);

    /* Step 5: Validate initial stack pointer */
    if ((stack_pointer <= SRAM_START_ADDR) ||
        (stack_pointer > SRAM_END_ADDR))
    {
        return 0U;
    }

    /* Stack pointer must be 8-byte aligned */
    if ((stack_pointer & 0x7U) != 0U)
    {
        return 0U;
    }

    /* Step 6: Validate reset handler */
    if ((reset_handler & 0x1U) == 0U)
    {
        return 0U;
    }

    /* Remove the Thumb-state bit before checking the address */
    reset_address = reset_handler & ~0x1U;

    if ((reset_address < APP_START_ADDR) ||
        (reset_address >= image_end))
    {
        return 0U;
    }

    return 1U;
}


/*
 * Executes the application.
 *
 * The application must pass Boot_IsApplicationValid()
 * before execution.
 */
__attribute__((naked, noreturn))
static void Boot_ExecuteApplication(uint32_t app_msp,
                                    uint32_t app_reset)
{
	__asm volatile
	(
	    "msr msp, r0       \n"
	    "movs r2, #0       \n"
	    "msr control, r2   \n"
	    "isb               \n"
	    "bx r1             \n"
	);
}

/*
 * Transfers control from the bootloader to the application.
 */
void Boot_JumpToApplication(void)
{
    uint32_t app_msp;
    uint32_t app_reset;

    /* Never jump to an invalid application */
    if (Boot_IsApplicationValid() == 0U)
    {
        return;
    }

    /* Wait until the final UART transmission is complete. */
    while ((USART1->SR & (1U << 6)) == 0U)
    {
    }

    /* Stop the DWT cycle counter. */
    DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;

    /* Read application vector table */
    app_msp = *(volatile uint32_t *)APP_START_ADDR;
    app_reset = *(volatile uint32_t *)(APP_START_ADDR + 4U);

    /* Disable interrupts during handover */
    __disable_irq();

    /* Stop SysTick */
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    /* Clear pending SysTick and PendSV exceptions. */
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk |
                SCB_ICSR_PENDSVCLR_Msk;

    /* Disable USART1 */
    USART1->CR1 = 0U;
    USART1->CR2 = 0U;
    USART1->CR3 = 0U;

    /* Disable and clear pending NVIC interrupts */
    for (uint32_t i = 0U; i < 3U; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    /* Point interrupt vectors to the application */
    SCB->VTOR = APP_START_ADDR;

    __DSB();
    __ISB();

    /* Re-enable global interrupts */
    __enable_irq();

    /* Set MSP and branch to application's reset handler */
    Boot_ExecuteApplication(app_msp, app_reset);
}
