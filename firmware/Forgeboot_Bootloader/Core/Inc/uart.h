#ifndef UART_H
#define UART_H

#include <stdint.h>

#define UART_RX_OK          0U
#define UART_RX_TIMEOUT     1U
#define UART_RX_ERROR       2U

void UART1_Init(void);

void UART1_SendChar(char data);
void UART1_SendString(const char *str);

char UART1_ReceiveChar(void);

/* New timeout-aware receive function */
uint8_t UART1_ReceiveCharTimeout(
    uint8_t *data,
    uint32_t timeout_ms
);

#endif
