#include "stm32f411xe.h"
#include "uart.h"

/*
 * Current clock configuration:
 * HCLK = 16 MHz
 * PCLK2 = 16 MHz
 *
 * Update this if the clock configuration changes.
 */
#define UART_CORE_CLOCK_HZ    16000000U

#define UART_SR_PE            (1U << 0)
#define UART_SR_FE            (1U << 1)
#define UART_SR_NE            (1U << 2)
#define UART_SR_ORE           (1U << 3)
#define UART_SR_RXNE          (1U << 5)
#define UART_SR_TXE           (1U << 7)

#define UART_SR_ERROR_MASK    (UART_SR_PE | UART_SR_FE | \
                               UART_SR_NE | UART_SR_ORE)


/*
 * Initialize DWT cycle counter.
 * Used for accurate timeout measurement.
 */
static void UART1_DWT_Init(void)
{
    /* Enable trace and debug blocks */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* Reset cycle counter */
    DWT->CYCCNT = 0U;

    /* Enable cycle counter */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}


void UART1_Init(void)
{
    /* Enable GPIOA and USART1 clocks */
    RCC->AHB1ENR |= (1U << 0);
    RCC->APB2ENR |= (1U << 4);

    /* PA9 and PA10: Alternate Function mode */
    GPIOA->MODER &= ~((3U << 18) | (3U << 20));
    GPIOA->MODER |=  ((2U << 18) | (2U << 20));

    /* Select AF7 for USART1 */
    GPIOA->AFR[1] &= ~((0xFU << 4) | (0xFU << 8));
    GPIOA->AFR[1] |=  ((7U << 4) | (7U << 8));

    /* Push-pull, high speed, pull-up */
    GPIOA->OTYPER &= ~((1U << 9) | (1U << 10));

    GPIOA->OSPEEDR |= ((3U << 18) | (3U << 20));

    GPIOA->PUPDR &= ~((3U << 18) | (3U << 20));
    GPIOA->PUPDR |=  ((1U << 18) | (1U << 20));

    /* Disable USART before configuration */
    USART1->CR1 = 0U;
    USART1->CR2 = 0U;
    USART1->CR3 = 0U;

    /* 115200 baud at PCLK2 = 16 MHz */
    USART1->BRR = 0x008B;

    /* Enable transmitter, receiver and USART */
    USART1->CR1 |= (1U << 3);
    USART1->CR1 |= (1U << 2);
    USART1->CR1 |= (1U << 13);

    /* Initialize timeout counter */
    UART1_DWT_Init();
}


/* Transmit one character */
void UART1_SendChar(char data)
{
    while (!(USART1->SR & UART_SR_TXE));

    USART1->DR = (uint8_t)data;
}


/* Transmit a string */
void UART1_SendString(const char *str)
{
    while (*str)
    {
        UART1_SendChar(*str++);
    }
}


/*
 * Original blocking receive function.
 * Retained for compatibility with existing code.
 */
char UART1_ReceiveChar(void)
{
    while (!(USART1->SR & UART_SR_RXNE));

    return (char)(USART1->DR & 0xFFU);
}


/*
 * Receive one byte with timeout.
 *
 * Parameters:
 * data       = Pointer to store received byte
 * timeout_ms = Maximum waiting time in milliseconds
 *
 * Returns:
 * UART_RX_OK
 * UART_RX_TIMEOUT
 * UART_RX_ERROR
 */
uint8_t UART1_ReceiveCharTimeout(
    uint8_t *data,
    uint32_t timeout_ms)
{
    uint32_t start_cycle;
    uint32_t timeout_cycles;
    uint32_t status;

    if (data == 0)
    {
        return UART_RX_ERROR;
    }

    if (timeout_ms == 0U)
    {
        return UART_RX_TIMEOUT;
    }

    /*
     * Calculate timeout in CPU cycles.
     * At 16 MHz, one millisecond = 16000 cycles.
     */
    timeout_cycles = (UART_CORE_CLOCK_HZ / 1000U) * timeout_ms;

    start_cycle = DWT->CYCCNT;

    while (1)
    {
        status = USART1->SR;

        /* Check UART communication errors */
        if ((status & UART_SR_ERROR_MASK) != 0U)
        {
            /* Reading DR clears the USART error flags */
            (void)USART1->DR;

            return UART_RX_ERROR;
        }

        /* Check whether a byte has arrived */
        if ((status & UART_SR_RXNE) != 0U)
        {
            *data = (uint8_t)(USART1->DR & 0xFFU);

            return UART_RX_OK;
        }

        /* Check timeout */
        if ((uint32_t)(DWT->CYCCNT - start_cycle)
            >= timeout_cycles)
        {
            return UART_RX_TIMEOUT;
        }
    }
}
