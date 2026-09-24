/*
 * Copyright (c) 2025 Nations Technologies Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UART_LOG_H
#define UART_LOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Polled USART1 output for the keyword log.

   Deliberately not libc printf: the application links with
   --specs=nosys.specs and defines no _write()/fputc(), so stdio output would
   be discarded silently.  These helpers talk to the peripheral directly.
   USART1 itself (PA9/PA10, 921600 8N1) is configured by USART_Configuration()
   in n32h7xx_cfg.c; kws_uart_init() only clears the transmitter state. */

void kws_uart_init(void);
void kws_uart_putc(char c);
void kws_uart_puts(const char *s);
/* Decimal, no padding. */
void kws_uart_put_u32(uint32_t value);
/* Blocks until the shift register has emptied, so a line survives a reset. */
void kws_uart_drain(void);

#ifdef __cplusplus
}
#endif

#endif /* UART_LOG_H */
