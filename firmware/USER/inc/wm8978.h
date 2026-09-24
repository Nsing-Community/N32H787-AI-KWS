/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef WM8978_H
#define WM8978_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WM8978_I2C_ADDRESS 0x1AU /* Unshifted, 7-bit address. */
#define WM8978_REGISTER_COUNT 58U

typedef enum {
    WM8978_OK = 0,
    WM8978_INVALID = -1,
    WM8978_IO = -2,
    WM8978_TIMEOUT = -3,
    WM8978_BUSY = -4,
    WM8978_STATE = -5
} wm8978_status_t;

/* Synchronous, non-reentrant API. Caller owns/serializes the control bus.
 * A write sends exactly two bytes and must wait for STOP/ACK completion.
 * Do not call from an ISR: initialization includes the VMID charging delay. */
typedef struct {
    void *context;
    wm8978_status_t (*write)(void *context, uint8_t address,
                             const uint8_t bytes[2]);
    void (*delay_ms)(void *context, uint32_t ms);
} wm8978_bus_t;

typedef struct {
    uint32_t mclk_hz; /* Actual clock at the codec pin, not the MCU kernel. */
    uint32_t sample_rate;
    uint8_t sample_bits; /* 16, 20, 24 or 32; always two 32-bit I2S slots. */
    bool codec_master; /* true: WM8978 supplies BCLK=64fs and LRC=fs. */
} wm8978_config_t;

typedef struct {
    uint32_t k; /* 24-bit fraction. */
    uint32_t pll_out_hz;
    uint8_t n;
    uint8_t pre_div2;
    uint8_t mclk_div_index;
} wm8978_pll_t;

typedef enum {
    WM8978_INPUT_NONE,
    WM8978_INPUT_MIC_DIFFERENTIAL,
    WM8978_INPUT_LINE2
} wm8978_input_t;

typedef enum { WM8978_HEADPHONE, WM8978_SPEAKER } wm8978_output_t;

typedef struct {
    wm8978_input_t input;
    bool mic_bias;
    bool headphone;
    bool speaker;
    uint8_t pga_gain; /* 0..63: -12 dB + 0.75 dB/step. */
    bool boost_20db;
} wm8978_paths_t;

typedef struct {
    wm8978_bus_t bus;
    wm8978_config_t config;
    wm8978_pll_t pll;
    uint16_t shadow[WM8978_REGISTER_COUNT];
    bool shadow_valid;
    bool initialized;
    wm8978_status_t last_error;
} wm8978_t;

wm8978_status_t wm8978_calculate_pll(uint32_t mclk_hz, uint32_t sample_rate,
                                    wm8978_pll_t *pll);
/* MCLK must already be running. Resets codec; starts muted, all paths off.
 * Reinitialize after any transport failure or external codec reset/power loss.
 * In slave mode the caller must provide synchronous BCLK/LRC. */
wm8978_status_t wm8978_init(wm8978_t *dev, const wm8978_bus_t *bus,
                          const wm8978_config_t *config);
wm8978_status_t wm8978_power_down(wm8978_t *dev);
/* Software shadow only: the 2-wire control interface has no register reads. */
wm8978_status_t wm8978_get_cached(const wm8978_t *dev, uint8_t reg,
                                uint16_t *value);
/* Changing paths mutes the DAC. Explicitly unmute after PCM is available. */
wm8978_status_t wm8978_set_paths(wm8978_t *dev, const wm8978_paths_t *paths);
wm8978_status_t wm8978_set_dac_mute(wm8978_t *dev, bool mute);
/* DAC/ADC digital levels: 0=mute, 1..255=-127..0 dB (0.5 dB steps). */
wm8978_status_t wm8978_set_dac_volume(wm8978_t *dev, uint8_t left, uint8_t right);
wm8978_status_t wm8978_set_adc_volume(wm8978_t *dev, uint8_t left, uint8_t right);
/* Output levels 0..63: -57..+6 dB. Output mute is independent of DAC mute. */
wm8978_status_t wm8978_set_output_volume(wm8978_t *dev, wm8978_output_t output,
                                       uint8_t left, uint8_t right, bool mute);

#ifdef __cplusplus
}
#endif
#endif
