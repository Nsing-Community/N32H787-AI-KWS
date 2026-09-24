/*
 * Copyright (c) 2025 Nations Technologies Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "main.h"
#include "m4_shared.h"
#include "m7_cache.h"
#include "kws_app.h"
#include "kws_audio.h"
#include "kws_classifier.h"
#include "kws_mfcc.h"
#include "uart_log.h"

/* ---- PI8 bring-up indicator ---------------------------------------------

   PI8 is the run heartbeat, but the application only starts toggling it from
   kws_app_run(), which is the last thing main does.  Everything ahead of that
   -- the clock tree, the codec's I2C bus, TFLM's arena, the MPU and D-cache,
   the I2S/DMA ring -- can hang, and every one of those hangs looks identical
   from the outside: a dark board and nothing on the serial port.

   main therefore reports its own progress on PI8 before it hands over.  PI8 is
   active low, so lighting it means driving the pin low:

     never lights        the reset vector, Reset_Handler or SystemInit did not
                         finish, so main was never entered.  That points at the
                         image base (0x15006000), the TCM partition, the
                         bootloader's jump, or the DTCM model copy that
                         startup_n32h78x_cm7_gcc.s retries silently.
     solid on            main was entered and stopped inside the chip-level
                         setup, i.e. PWR_Configuration() or RCC_Configuration().
                         Both spin on hardware ready flags and the PLL1A lock
                         wait in RCC_Configuration() has no timeout.
     N pulses, then dark N bring-up stages completed (map at boot_stage()); the
                         stage after the last pulse is the one that hung.
     slow 500 ms toggle  the application is running.

   Pulses are 60 ms on / 140 ms off and are counted from power-on, so the number
   of blinks is the progress.  They add ~1.4 s to startup; set KWS_BOOT_PROGRESS
   to 0 to drop that once the board is known good. */
#define KWS_BOOT_PROGRESS 1

#define BOOT_PULSE_ON_MS  60U
#define BOOT_PULSE_GAP_MS 140U

static void boot_led(int on)
{
    if (on) GPIO_ResetBits(GPIOI, GPIO_PIN_8);
    else    GPIO_SetBits(GPIOI, GPIO_PIN_8);
}

/* Claim PI8 as the earliest liveness signal main can produce.  This
   deliberately repeats the PI8 lines of GPIO_Configuration(): the GPIOI clock
   gate is an AHB5 register and the AHB runs from HSI out of reset, so it works
   before RCC_Configuration().  Keep the two in step. */
static void boot_led_init(void)
{
    GPIO_InitType pi8;
    RCC_EnableAHB5PeriphClk2(RCC_AHB5_PERIPHEN_M7_GPIOI, ENABLE);
    GPIO_InitStruct(&pi8);
    pi8.GPIO_Mode      = GPIO_MODE_OUTPUT_PP;
    pi8.GPIO_Pull      = GPIO_PULL_UP;
    pi8.GPIO_Slew_Rate = GPIO_SLEW_RATE_SLOW;
    pi8.GPIO_Current   = GPIO_5VTOL_DC_1mA;
    pi8.GPIO_Alternate = GPIO_NO_AF;
    pi8.Pin            = GPIO_PIN_8;
    GPIO_InitPeripheral(GPIOI, &pi8);
    boot_led(1);
}

#if KWS_BOOT_PROGRESS
/* One pulse per completed stage.  Called in order and blocking, so the total
   count since power-on is the last stage that finished:

     1  clock tree, pin muxing, 1 ms SysTick and the DWT counter
     2  SDRAM_Configuration()          spare SDRAM bank
     3  kws_uart_init()                USART1 configured and enabled
     4  kws_mfcc_init()                FFT twiddle tables
     5  kws_classifier_init()          TFLM arena, interpreter, ops
     6  m7_cache_enable()              MPU regions and the D-cache
     7  kws_audio_init()               WM8978 over I2C4, I2S1, DMA1 ring

   Seven pulses followed by the slow toggle means all of bring-up passed and
   kws_app_run() took over. */
static void boot_stage(void)
{
    boot_led(1);
    SysTick_Delayms(BOOT_PULSE_ON_MS);
    boot_led(0);
    SysTick_Delayms(BOOT_PULSE_GAP_MS);
}
#else
static void boot_stage(void) { }
#endif

int main(void)
{
#ifndef APP_FLASH_BASE
#define APP_FLASH_BASE 0x15000000UL
#endif
    SCB->VTOR = APP_FLASH_BASE;
    __DSB();
    __ISB();

    boot_led_init();

    /* Chip-level setup.  PWR_Configuration() brings up the GRAPHICS sub power
       domain that the removed DVP2 sat in; nothing in this design reads it,
       but it is a proven part of the boot sequence and skipping it would be
       an unverifiable hardware change, so it stays.  It is called first
       because the vendor boot sequence does, not because KWS needs it. */
    const bool power_ok = PWR_Configuration();
    RCC_Configuration();
    GPIO_Configuration();
    /* GPIO_Configuration() drives PI8 high before selecting output mode, which
       is the dark state.  Re-light it: reaching this line means the chip-level
       setup completed. */
    boot_led(1);
    DMA_Configuration();
    I2C_Configuration();
    USART_Configuration();
    SysTick_Config(600000U);   /* 1 ms at the 600 MHz core clock */
    NVIC_Configuration();
    CPU_DELAY_INTI();          /* enables the DWT cycle counter */

    boot_stage();              /* 1 */

    /* Hold the second core in reset.  Nothing here starts it, but
       wm8978_n32_init() and SDRAM_Configuration() both refuse to run unless
       RCC_M4RSTREL_EN is clear, and m4_start() -- which used to be the only
       code that ever touched this bit -- is gone.  Making it explicit means a
       future change cannot silently reintroduce a second bus master. */
    RCC->M4RSTREL &= ~RCC_M4RSTREL_EN;
    __DSB();

    /* Initialise the spare SDRAM bank while the D-cache is still off. */
    SDRAM_Configuration();
    boot_stage();              /* 2 */

    kws_uart_init();
    if (!power_ok) {
        /* Non-fatal: this design never touches the domain, but a failure here
           means the chip is not in the state the rest of the sequence assumes,
           so it is worth a line on the port. */
        kws_uart_puts("kws: WARNING GRAPHICS power domain not ready\r\n");
        kws_uart_drain();
    }
    boot_stage();              /* 3 */
    kws_mfcc_init();
    boot_stage();              /* 4 */

    kws_classifier_init();
    boot_stage();              /* 5 */

    /* AHB SRAM survives the flasher's `reset run`, so the mailbox can still
       hold a pause request from the previous session.  If flash_pause were
       left set, the first m7_flash_poll() would disable both caches and spin
       forever -- a board that looks dead one flash after a firmware change.
       m4_start() used to clear the whole struct at boot; with it retired,
       clear the control words here, before anything that could poll them.

       The handshake word OpenOCD gates flashing on (n32h7x_quiesce_cached_m7)
       needs a writer for the same reason: m4_start() was the only one, and
       without it every flash target stops working as soon as the D-cache is
       on.  The other half of that gate, m7_flash_abi at 0x300002b4, is
       published by m7_cache_enable() below, so OpenOCD waits for both
       regardless of the order these two are written in. */
    g_m4_shared.flash_pause = 0U;
    g_m4_shared.flash_paused = 0U;
    g_m4_shared.m7_flash_paused = 0U;
    g_m4_shared.boot_request = 0U;
    __DMB();
    g_m4_shared.magic = M4_SHARED_MAGIC;
    __DMB();

    /* From here the AXI SRAM ring at 0x24000000 is mapped non-cacheable, so
       the audio DMA buffer must not be touched before this call.  It must
       also follow wm8978_n32_init()'s I2S1 slave-receiver setup, which
       kws_audio_init() performs. */
    m7_cache_enable();
    boot_stage();              /* 6 */

    kws_audio_init();
    boot_stage();              /* 7 */

    /* Never returns.  From here PI8 becomes the 500 ms run heartbeat. */
    kws_app_run();
    return 0;
}
