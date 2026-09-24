/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef WM8978_N32_H
#define WM8978_N32_H
#include "wm8978.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t attempts;
    uint32_t acked;
    uint32_t last_flags;
    uint32_t last_reg;
} wm8978_n32_diagnostics_t;
extern volatile wm8978_n32_diagnostics_t g_wm8978_i2c_diag;

/* Opt-in board adapter; nothing runs until this is called.
 * Requires existing RCC/GPIO/I2C_Configuration and a working SysTick delay.
 * M7 may use it ONLY before releasing M4; afterwards I2C4 belongs to M4.
 * On M4, call from the camera/control loop, never concurrently or from an ISR.
 * Pins from the generated CM7 reference:
 *   PD12 AF10 / PD13 AF8: existing shared I2C4 SCL/SDA (not reinitialized).
 *   PA3 AF6: I2S4 MCK. PA5 AF6 / PG10 AF9: I2S1 CK/WS inputs.
 *   PG9 AF6: I2S1 SD input. PD7 AF5: I2S1_EXT SD output.
 * Uses SYSBUS/8 -> I2S4 /4 -> MCLK (9.375 MHz at SYSBUS=300 MHz).
 * Codec PLL generates audio clocks; PLL3/SDRAM clocks are never changed.
 * I2S1/EXT are configured but left disabled for a later PCM/DMA start.
 * Codec starts muted with capture/output paths off. MCLK stays running.
 * Supported sample widths: 16, 24, 32, all with 32-bit slots.
 * I2S1..4 must be unused; I2S1/2 and I2S3/4 share their kernel dividers.
 * MCLK continuity and I2S1 PCM reception verified on N32H787; absolute MCLK
 * frequency above is calculated, not an oscilloscope measurement. */
wm8978_status_t wm8978_n32_init(wm8978_t *dev, uint32_t rate, uint8_t bits);

#ifdef __cplusplus
}
#endif
#endif
