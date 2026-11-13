#include "stm32f411xe.h"

#include "uart.h"
#include "recovery.h"
#include "boot.h"
#include "update.h"

/*
 * Enter firmware recovery mode.
 * Wait for firmware update packets over UART.
 */
static void Boot_EnterRecoveryMode(void)
{
    UART1_SendString("\r\n");
    UART1_SendString("[FORGEBOOT] Recovery Mode Active\r\n");
    UART1_SendString("[FORGEBOOT] Waiting for firmware update...\r\n");

    /* Initialize the firmware update manager. */
    Update_Init();

    /* Continuously process incoming firmware packets. */
    while (1)
    {
        Update_Process();

        /* Automatically launch after successful firmware update. */
        if (Update_IsComplete() != 0U)
        {
            UART1_SendString(
                "[FORGEBOOT] Firmware update successful.\r\n"
            );

            UART1_SendString(
                "[FORGEBOOT] Starting application...\r\n"
            );

            Boot_JumpToApplication();

            /* If the jump fails, remain in recovery mode. */
            UART1_SendString(
                "[FORGEBOOT] Application jump failed.\r\n"
            );

            Update_Init();
        }
    }
}

int main(void)
{
    /* Initialize communication interface */
    UART1_Init();

    /* Initialize active-low recovery button on PB12 */
    RecoveryButton_Init();

    /* Display bootloader banner */
    UART1_SendString("\r\n");
    UART1_SendString("================================\r\n");
    UART1_SendString("       FORGEBOOT v0.2\r\n");
    UART1_SendString("       STM32F411CEU6\r\n");
    UART1_SendString("       Bootloader Started\r\n");
    UART1_SendString("================================\r\n");

    /* Check whether recovery button is pressed */
    if (RecoveryButton_IsPressed() != 0U)
    {
        UART1_SendString("[FORGEBOOT] Recovery button detected.\r\n");

        Boot_EnterRecoveryMode();
    }

    /* Validate the installed application */
    UART1_SendString("[FORGEBOOT] Checking application...\r\n");

    if (Boot_IsApplicationValid() != 0U)
    {
        UART1_SendString("[FORGEBOOT] Application is valid.\r\n");
        UART1_SendString("[FORGEBOOT] Starting application...\r\n");

        /* Transfer control to the application */
        Boot_JumpToApplication();

        /*
         * Normally this function never returns.
         * If it does, enter recovery mode.
         */
        UART1_SendString("[FORGEBOOT] Application jump failed.\r\n");
    }
    else
    {
        UART1_SendString("[FORGEBOOT] No valid application found.\r\n");
    }

    /* Invalid application or failed jump */
    Boot_EnterRecoveryMode();

    return 0;
}
