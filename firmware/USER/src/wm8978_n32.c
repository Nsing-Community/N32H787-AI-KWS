// SPDX-License-Identifier: GPL-2.0-only
#include "wm8978_n32.h"
#include "n32h7xx.h"
#include "n32h7xx_cfg.h"
#include "n32h7xx_i2s.h"

#define WAIT_LIMIT 100000U
#define I2C_ERRORS (I2C_FLAG_NAKF | I2C_FLAG_BSER | I2C_FLAG_ABLO | I2C_FLAG_TMOUT)

volatile wm8978_n32_diagnostics_t g_wm8978_i2c_diag;

static bool owns_bus(void)
{
#if defined(CORE_CM7)
    return (RCC->M4RSTREL & RCC_M4RSTREL_EN) == 0U;
#else
    return true;
#endif
}

static wm8978_status_t wait_flag(uint32_t wanted)
{
    for (unsigned i = 0; i < WAIT_LIMIT; ++i) {
        uint32_t flags = I2C4->STSINT;
        if (flags & I2C_ERRORS) return WM8978_IO;
        if (flags & wanted) return WM8978_OK;
    }
    return WM8978_TIMEOUT;
}

static wm8978_status_t bus_write(void *context, uint8_t address, const uint8_t bytes[2])
{
    (void)context;
    if (!owns_bus() || __get_IPSR() != 0U) return WM8978_BUSY;
    if ((I2C4->CTRL1 & I2C_CTRL1_I2CEN) == 0U) return WM8978_STATE;
    /* BUSY is not a cross-core lock. owns_bus() enforces the project's owner. */
    if (I2C4->STSINT & I2C_FLAG_BUSY) return WM8978_BUSY;
    ++g_wm8978_i2c_diag.attempts;
    g_wm8978_i2c_diag.last_reg = bytes[0] >> 1U;
    I2C_ClrFlag(I2C4, I2C_ERRORS | I2C_FLAG_STOPF);
    /* WRE is software-settable: discard any byte left by an aborted transfer. */
    I2C4->STSINT = I2C_FLAG_WRE;
    I2C_EnableReload(I2C4, DISABLE);
    I2C_EnableAutomaticEnd(I2C4, ENABLE);
    I2C_ConfigSendAddress(I2C4, (uint32_t)address << 1U, I2C_DIRECTION_SEND);
    I2C_SetTransferByteNumber(I2C4, 2U);
    I2C_GenerateStart(I2C4, ENABLE);
    wm8978_status_t rc;
    for (unsigned i = 0; i < 2; ++i) {
        rc = wait_flag(I2C_FLAG_WRAVL);
        if (rc != WM8978_OK) goto fail;
        I2C_SendData(I2C4, bytes[i]);
    }
    rc = wait_flag(I2C_FLAG_STOPF);
    if (rc != WM8978_OK) goto fail;
    g_wm8978_i2c_diag.last_flags = I2C4->STSINT;
    ++g_wm8978_i2c_diag.acked;
    I2C_ClrFlag(I2C4, I2C_FLAG_STOPF);
    return WM8978_OK;
fail:
    g_wm8978_i2c_diag.last_flags = I2C4->STSINT;
    /* Never generate STOP after losing arbitration to another bus master. */
    if ((I2C4->STSINT & (I2C_FLAG_BUSY | I2C_FLAG_ABLO)) == I2C_FLAG_BUSY) {
        I2C_GenerateStop(I2C4, ENABLE);
        for (unsigned i = 0; i < WAIT_LIMIT; ++i)
            if (!(I2C4->STSINT & I2C_FLAG_BUSY)) break;
    }
    I2C_ClrFlag(I2C4, I2C_ERRORS | I2C_FLAG_STOPF);
    return rc;
}

static void delay_ms(void *context, uint32_t ms)
{
    (void)context;
    SysTick_Delayms(ms);
}

static void audio_pin(GPIO_Module *port, uint16_t pin, uint32_t af, uint32_t current)
{
    GPIO_InitType gpio;
    GPIO_InitStruct(&gpio);
    gpio.Pin = pin;
    /* Alternate function controls pin direction in I2S slave mode. */
    gpio.GPIO_Mode = GPIO_MODE_AF_PP;
    gpio.GPIO_Pull = GPIO_NO_PULL;
    gpio.GPIO_Slew_Rate = GPIO_SLEW_RATE_FAST;
    gpio.GPIO_Current = current;
    gpio.GPIO_Alternate = af;
    GPIO_InitPeripheral(port, &gpio);
}

wm8978_status_t wm8978_n32_init(wm8978_t *dev, uint32_t rate, uint8_t bits)
{
    if (!dev || (bits != 16 && bits != 24 && bits != 32)) return WM8978_INVALID;
    if (!owns_bus() || __get_IPSR() != 0U) return WM8978_BUSY;
    if (!(I2C4->CTRL1 & I2C_CTRL1_I2CEN)) return WM8978_STATE;
    g_wm8978_i2c_diag = (wm8978_n32_diagnostics_t){0};
    RCC_ClocksTypeDef clocks;
    RCC_GetClocksFreqValue(&clocks);
    if (clocks.SysBusDivClkFreq % 32U != 0U) return WM8978_INVALID;
    wm8978_config_t config = {
        .mclk_hz = clocks.SysBusDivClkFreq / 32U,
        .sample_rate = rate, .sample_bits = bits, .codec_master = true
    };
    wm8978_pll_t pll;
    wm8978_status_t rc = wm8978_calculate_pll(config.mclk_hz, rate, &pll);
    if (rc != WM8978_OK) return rc;
#if defined(CORE_CM7)
    RCC_EnableAHB5PeriphClk1(RCC_AHB5_PERIPHEN_M7_GPIOA | RCC_AHB5_PERIPHEN_M7_GPIOD |
                            RCC_AHB5_PERIPHEN_M7_GPIOG, ENABLE);
    RCC_EnableAPB2PeriphClk2(RCC_APB2_PERIPHEN_M7_I2S1, ENABLE);
    RCC_EnableAPB1PeriphClk4(RCC_APB1_PERIPHEN_M7_I2S4, ENABLE);
#else
    RCC_EnableAHB5PeriphClk1(RCC_AHB5_PERIPHEN_M4_GPIOA | RCC_AHB5_PERIPHEN_M4_GPIOD |
                            RCC_AHB5_PERIPHEN_M4_GPIOG, ENABLE);
    RCC_EnableAPB2PeriphClk2(RCC_APB2_PERIPHEN_M4_I2S1, ENABLE);
    RCC_EnableAPB1PeriphClk4(RCC_APB1_PERIPHEN_M4_I2S4, ENABLE);
#endif
    if ((I2S1->I2SCFGR | I2S4->I2SCFGR) & SPI_I2SCFG_I2SEN) return WM8978_BUSY;
    RCC_ConfigI2S1_2_KerSysDivider(RCC_I2SKERCLK_SYSBUSDIV8);
    RCC_ConfigI2S1KerClkSource(RCC_I2SKERCLK_SRC_SYSBUSDIV);
    RCC_ConfigI2S3_4_KerSysDivider(RCC_I2SKERCLK_SYSBUSDIV8);
    RCC_ConfigI2S4KerClkSource(RCC_I2SKERCLK_SRC_SYSBUSDIV);

    I2S_InitType i2s;
    I2S_InitStruct(&i2s);
    /* TX stalls MCLK when its data register is empty on this device. RX
     * keeps the clock running without dummy TX DMA. Its unpinned SD input
     * may overrun; those ignored samples do not stop MCLK (verified on board). */
    i2s.I2sMode = I2S_MODE_MASTER_RX;
    i2s.Standard = I2S_STD_PHILLIPS;
    i2s.DataFormat = I2S_DATA_FMT_16BITS;
    i2s.MCLKEnable = I2S_MCLK_ENABLE;
    /* DEFAULT sets LDIV=2, ODD=0: MCLK=kernel/(2*2), not 256*rate. */
    i2s.AudioFrequency = I2S_AUDIO_FREQ_DEFAULT;
    i2s.ClkSrcFrequency = clocks.SysBusDivClkFreq / 8U;
    i2s.CLKPOL = I2S_CLKPOL_LOW;
    I2S_Init(I2S4, &i2s);
    audio_pin(GPIOA, GPIO_PIN_3, GPIO_AF6, GPIO_DC_2mA);
    I2S_Enable(I2S4, ENABLE);

    wm8978_bus_t bus = {.context = NULL, .write = bus_write, .delay_ms = delay_ms};
    rc = wm8978_init(dev, &bus, &config);
    if (rc != WM8978_OK) {
        wm8978_power_down(dev);
        I2S_Enable(I2S4, DISABLE);
        dev->last_error = rc;
        return rc;
    }

    /* Codec owns BCLK/LRC. Main I2S1 receives PG9, extension sends PD7.
     * PCM/DMA setup must complete before enabling either data interface. */
    i2s.I2sMode = I2S_MODE_SlAVE_RX;
    i2s.MCLKEnable = I2S_MCLK_DISABLE;
    i2s.DataFormat = bits == 16 ? I2S_DATA_FMT_16BITS_EXTENDED :
                    bits == 24 ? I2S_DATA_FMT_24BITS : I2S_DATA_FMT_32BITS;
    I2S_Init(I2S1, &i2s);
    i2s.I2sMode = I2S_MODE_SlAVE_TX;
    I2S_EXTInit(I2S1_EXT, &i2s);
    audio_pin(GPIOA, GPIO_PIN_5, GPIO_AF6, GPIO_HS_IO_DC_1mA);
    audio_pin(GPIOG, GPIO_PIN_10, GPIO_AF9, GPIO_DC_2mA);
    audio_pin(GPIOG, GPIO_PIN_9, GPIO_AF6, GPIO_DC_2mA);
    audio_pin(GPIOD, GPIO_PIN_7, GPIO_AF5, GPIO_DC_2mA);
    return WM8978_OK;
}
