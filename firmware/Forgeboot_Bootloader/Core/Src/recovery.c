#include "recovery.h"
#include "stm32f411xe.h"

/*
 * Recovery button:
 * PB12
 * Active LOW
 * Internal pull-up enabled
 */

void RecoveryButton_Init(void)
{
    /* Enable GPIOB peripheral clock */
    RCC->AHB1ENR |= (1U << 1);

    /* Configure PB12 as input */
    GPIOB->MODER &= ~(3U << 24);

    /* Enable internal pull-up for PB12 */
    GPIOB->PUPDR &= ~(3U << 24);
    GPIOB->PUPDR |=  (1U << 24);
}


/*
 * Returns:
 * 1 = Button pressed
 * 0 = Button released
 */
uint8_t RecoveryButton_IsPressed(void)
{
    if ((GPIOB->IDR & (1U << 12)) == 0U)
    {
        return 1U;
    }

    return 0U;
}
