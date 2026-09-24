// SPDX-License-Identifier: GPL-2.0-only
/* Bare-metal adaptation of register defaults, PLL selection and bias sequencing
 * from Linux v6.12 sound/soc/codecs/wm8978.c (blob 40d22b36b7a96727ae365ca57d0440a42d7807a6).
 * Copyright (C) 2009-2010 Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 * Copyright (C) 2007 Carlos Munoz <carlos@kenati.com>
 * Copyright 2006-2009 Wolfson Microelectronics PLC.
 * Reference: https://github.com/torvalds/linux/blob/v6.12/sound/soc/codecs/wm8978.c
 */
#include "wm8978.h"
#include <string.h>

enum {
    RESET = 0, POWER1 = 1, POWER2 = 2, POWER3 = 3, INTERFACE = 4,
    CLOCK = 6, ADDITIONAL = 7, DAC = 10, DAC_L = 11, ADC_L = 15,
    PLL_N = 36, PLL_K1 = 37, PLL_K2 = 38, PLL_K3 = 39,
    INPUT = 44, PGA_L = 45, BOOST_L = 47, MIXER_L = 50,
    HP_L = 52, SPK_L = 54
};

static const uint16_t defaults[WM8978_REGISTER_COUNT] = {
    0x000, 0x000, 0x000, 0x000, 0x050, 0x000, 0x140, 0x000,
    0x000, 0x000, 0x000, 0x0ff, 0x0ff, 0x000, 0x100, 0x0ff,
    0x0ff, 0x000, 0x12c, 0x02c, 0x02c, 0x02c, 0x02c, 0x000,
    0x032, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,
    0x038, 0x00b, 0x032, 0x000, 0x008, 0x00c, 0x093, 0x0e9,
    0x000, 0x000, 0x000, 0x000, 0x033, 0x010, 0x010, 0x100,
    0x100, 0x002, 0x001, 0x001, 0x039, 0x039, 0x039, 0x039,
    0x001, 0x001
};

static bool valid_reg(uint8_t reg)
{
    return reg < WM8978_REGISTER_COUNT && reg != 17 && reg != 23 &&
           reg != 26 && reg != 31 && reg != 40 && reg != 42;
}

static int filter_rate(uint32_t rate)
{
    switch (rate) {
    case 8000: return 5;
    case 11025: return 4;
    case 16000: return 3;
    case 22050: return 2;
    case 32000: return 1;
    case 44100: case 48000: return 0;
    default: return -1;
    }
}

wm8978_status_t wm8978_calculate_pll(uint32_t mclk, uint32_t rate,
                                    wm8978_pll_t *pll)
{
    static const uint8_t num[] = {1, 3, 2, 3, 4, 6, 8, 12};
    static const uint8_t den[] = {1, 2, 1, 1, 1, 1, 1, 1};
    if (!pll || filter_rate(rate) < 0 || mclk < 3781000U || mclk > 32768000U)
        return WM8978_INVALID;
    for (unsigned i = 0; i < sizeof(num); ++i) {
        uint64_t target = (uint64_t)rate * 256U * num[i] * 4U / den[i];
        if (target < 3ULL * mclk || target >= 13ULL * mclk) continue;
        unsigned div2 = target / mclk < 6U;
        /* Keep odd input frequencies exact through the optional /2 stage.
         * Round the whole fixed-point ratio so K carry propagates into N. */
        uint64_t ratio = ((target << (24U + div2)) + mclk / 2U) / mclk;
        unsigned n = (unsigned)(ratio >> 24);
        if (n < 6U || n > 12U) continue;
        *pll = (wm8978_pll_t){
            .k = (uint32_t)(ratio & 0xffffffU),
            .pll_out_hz = (uint32_t)(target / 4U),
            .n = (uint8_t)n, .pre_div2 = (uint8_t)div2,
            .mclk_div_index = (uint8_t)i
        };
        return WM8978_OK;
    }
    return WM8978_INVALID;
}

static wm8978_status_t write_reg(wm8978_t *dev, uint8_t reg, uint16_t value)
{
    uint8_t bytes[2] = {(uint8_t)((reg << 1U) | (value >> 8U)), (uint8_t)value};
    wm8978_status_t rc = dev->bus.write(dev->bus.context, WM8978_I2C_ADDRESS, bytes);
    dev->last_error = rc;
    if (rc != WM8978_OK) {
        /* A timeout may occur after the codec latched the value. Do not use
         * stale cache entries for any further read/modify/write operation. */
        dev->shadow_valid = false;
        dev->initialized = false;
    } else {
        dev->shadow[reg] = value;
    }
    return rc;
}

static wm8978_status_t update(wm8978_t *dev, uint8_t reg, uint16_t mask, uint16_t value)
{
    uint16_t next = (dev->shadow[reg] & ~mask) | (value & mask);
    return next == dev->shadow[reg] ? WM8978_OK : write_reg(dev, reg, next);
}

#define TRY(call) do { wm8978_status_t rc_ = (call); if (rc_ != WM8978_OK) return rc_; } while (0)

static bool ready(const wm8978_t *dev)
{
    return dev && dev->initialized && dev->shadow_valid;
}

static wm8978_status_t stereo(wm8978_t *dev, uint8_t reg, uint16_t left, uint16_t right)
{
    /* Bit 8 latches both channels. Set it only on the second write. */
    TRY(write_reg(dev, reg, left));
    return write_reg(dev, reg + 1U, right | 0x100U);
}

wm8978_status_t wm8978_init(wm8978_t *dev, const wm8978_bus_t *bus,
                          const wm8978_config_t *config)
{
    wm8978_pll_t pll;
    unsigned width;
    if (!dev || !bus || !bus->write || !bus->delay_ms || !config)
        return WM8978_INVALID;
    switch (config->sample_bits) {
    case 16: width = 0; break;
    case 20: width = 1; break;
    case 24: width = 2; break;
    case 32: width = 3; break;
    default: return WM8978_INVALID;
    }
    TRY(wm8978_calculate_pll(config->mclk_hz, config->sample_rate, &pll));
    /* Allow callers to pass &dev->bus / &dev->config when recovering. */
    wm8978_bus_t saved_bus = *bus;
    wm8978_config_t saved_config = *config;
    memset(dev, 0, sizeof(*dev));
    dev->bus = saved_bus;
    dev->config = saved_config;
    dev->pll = pll;
    TRY(write_reg(dev, RESET, 0));
    memcpy(dev->shadow, defaults, sizeof(defaults));
    dev->shadow_valid = true;
    TRY(write_reg(dev, DAC, 0x40));
    TRY(stereo(dev, HP_L, 0x40, 0x40));
    TRY(stereo(dev, SPK_L, 0x40, 0x40));
    TRY(write_reg(dev, POWER1, 0x0f)); /* VMID 5k, bias and buffer on. */
    dev->bus.delay_ms(dev->bus.context, 100);
    TRY(write_reg(dev, POWER1, 0x0e)); /* Standby VMID 500k. */
    TRY(write_reg(dev, INTERFACE, 0x10U | (width << 5U)));
    TRY(write_reg(dev, ADDITIONAL, (uint16_t)filter_rate(saved_config.sample_rate) << 1U));
    TRY(write_reg(dev, CLOCK, 0)); /* Use MCLK while PLL is being programmed. */
    TRY(write_reg(dev, PLL_N, (pll.pre_div2 << 4U) | pll.n));
    TRY(write_reg(dev, PLL_K1, pll.k >> 18U));
    TRY(write_reg(dev, PLL_K2, (pll.k >> 9U) & 0x1ffU));
    TRY(write_reg(dev, PLL_K3, pll.k & 0x1ffU));
    TRY(update(dev, POWER1, 0x20, 0x20));
    dev->bus.delay_ms(dev->bus.context, 10); /* No readable PLL lock status. */
    TRY(write_reg(dev, CLOCK, 0x100U | (pll.mclk_div_index << 5U) |
                  (2U << 2U) | (saved_config.codec_master ? 1U : 0U)));
    dev->initialized = true;
    return WM8978_OK;
}

wm8978_status_t wm8978_get_cached(const wm8978_t *dev, uint8_t reg, uint16_t *value)
{
    if (!value || !valid_reg(reg) || reg == RESET) return WM8978_INVALID;
    if (!ready(dev)) return WM8978_STATE;
    *value = dev->shadow[reg];
    return WM8978_OK;
}

wm8978_status_t wm8978_set_dac_mute(wm8978_t *dev, bool mute)
{
    if (!ready(dev)) return WM8978_STATE;
    return update(dev, DAC, 0x40, mute ? 0x40 : 0);
}

wm8978_status_t wm8978_set_dac_volume(wm8978_t *dev, uint8_t left, uint8_t right)
{
    if (!ready(dev)) return WM8978_STATE;
    return stereo(dev, DAC_L, left, right);
}

wm8978_status_t wm8978_set_adc_volume(wm8978_t *dev, uint8_t left, uint8_t right)
{
    if (!ready(dev)) return WM8978_STATE;
    return stereo(dev, ADC_L, left, right);
}

wm8978_status_t wm8978_set_output_volume(wm8978_t *dev, wm8978_output_t output,
                                       uint8_t left, uint8_t right, bool mute)
{
    if ((output != WM8978_HEADPHONE && output != WM8978_SPEAKER) || left > 63 || right > 63)
        return WM8978_INVALID;
    if (!ready(dev)) return WM8978_STATE;
    return stereo(dev, output == WM8978_HEADPHONE ? HP_L : SPK_L,
                  left | (mute ? 0x40U : 0U), right | (mute ? 0x40U : 0U));
}

wm8978_status_t wm8978_set_paths(wm8978_t *dev, const wm8978_paths_t *paths)
{
    if (!paths || paths->input < WM8978_INPUT_NONE || paths->input > WM8978_INPUT_LINE2 ||
        paths->pga_gain > 63 || (paths->mic_bias && paths->input != WM8978_INPUT_MIC_DIFFERENTIAL))
        return WM8978_INVALID;
    if (!ready(dev)) return WM8978_STATE;
    bool capture = paths->input != WM8978_INPUT_NONE;
    bool playback = paths->headphone || paths->speaker;
    TRY(wm8978_set_dac_mute(dev, true));
    TRY(write_reg(dev, POWER2, 0));
    TRY(write_reg(dev, POWER3, 0));
    TRY(update(dev, POWER1, 0x13, (capture || playback ? 1U : 2U) |
               (paths->mic_bias ? 0x10U : 0U)));
    TRY(update(dev, INPUT, 0x77, paths->input == WM8978_INPUT_MIC_DIFFERENTIAL ? 0x33 :
               paths->input == WM8978_INPUT_LINE2 ? 0x44 : 0));
    uint16_t pga = paths->pga_gain | (capture ? 0U : 0x40U);
    TRY(stereo(dev, PGA_L, pga, pga));
    TRY(write_reg(dev, BOOST_L, paths->boost_20db ? 0x100 : 0));
    TRY(write_reg(dev, BOOST_L + 1U, paths->boost_20db ? 0x100 : 0));
    TRY(write_reg(dev, MIXER_L, playback ? 1 : 0));
    TRY(write_reg(dev, MIXER_L + 1U, playback ? 1 : 0));
    TRY(write_reg(dev, POWER2, (capture ? 0x3fU : 0U) | (paths->headphone ? 0x180U : 0U)));
    return write_reg(dev, POWER3, (playback ? 0x0fU : 0U) | (paths->speaker ? 0x60U : 0U));
}

wm8978_status_t wm8978_power_down(wm8978_t *dev)
{
    if (!dev || !dev->bus.write) return WM8978_STATE;
    static const uint8_t regs[] = {DAC, HP_L, HP_L + 1, SPK_L, SPK_L + 1,
                                  POWER2, POWER3, CLOCK, POWER1};
    static const uint16_t values[] = {0x40, 0x40, 0x140, 0x40, 0x140, 0, 0, 0, 0};
    wm8978_status_t first_error = WM8978_OK;
    /* Try every power-off write even after an earlier transfer failed. */
    for (unsigned i = 0; i < sizeof(regs); ++i) {
        wm8978_status_t rc = write_reg(dev, regs[i], values[i]);
        if (first_error == WM8978_OK && rc != WM8978_OK) first_error = rc;
    }
    dev->last_error = first_error;
    dev->initialized = false;
    dev->shadow_valid = false;
    return first_error;
}
