
#include "stm32f411xe.h"

/* Initialize onboard LED on PC13 */
static void GPIO_Init(void)
{
    /* Enable GPIOC clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;

    /* PC13: General-purpose output mode */
    GPIOC->MODER &= ~(3U << (13U * 2U));
    GPIOC->MODER |=  (1U << (13U * 2U));

    /* Push-pull output */
    GPIOC->OTYPER &= ~(1U << 13U);

    /* Low speed */
    GPIOC->OSPEEDR &= ~(3U << (13U * 2U));

    /* Initially turn LED OFF (active-low) */
    GPIOC->ODR |= (1U << 13U);
}

/* Simple software delay */
static void Delay(volatile uint32_t count)
{
    while (count--);
}

int main(void)
{
    GPIO_Init();

    while (1)
    {
        /* Toggle onboard LED */
        GPIOC->ODR ^= (1U << 13U);

        Delay(400000U);
    }
}
