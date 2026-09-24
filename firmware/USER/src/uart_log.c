/*
 * Copyright (c) 2025 Nations Technologies Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "uart_log.h"

#include "main.h"

void kws_uart_init(void)
{
    /* USART1 is already configured and enabled by USART_Configuration().  Read
       the data register once to clear any stale byte left by the flasher, then
       take the error flags down so the first transmit does not inherit one. */
    volatile uint32_t discard = USART1->DAT;
    discard = USART1->STS;
    (void)discard;
}

void kws_uart_putc(char c)
{
    /* TXDE means the transmit data register has room for the next byte. */
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXDE) == RESET) {}
    USART_SendData(USART1, (uint8_t)c);
}

void kws_uart_puts(const char *s)
{
    while (*s != '\0') kws_uart_putc(*s++);
}

void kws_uart_put_u32(uint32_t value)
{
    char digits[10];
    unsigned n = 0;
    if (value == 0U) {
        kws_uart_putc('0');
        return;
    }
    while (value != 0U && n < sizeof(digits)) {
        digits[n++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (n != 0U) kws_uart_putc(digits[--n]);
}

void kws_uart_drain(void)
{
    /* TXC means the shift register and the transmit buffer are both empty. */
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXC) == RESET) {}
}
