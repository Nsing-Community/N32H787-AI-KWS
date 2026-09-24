# Copyright (c) 2025 Nations Technologies Inc.
# SPDX-License-Identifier: Apache-2.0

"""Compile the real codec driver and exercise it against a failing I2C bus."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

HARNESS = r'''
#include "wm8978.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned calls, fail_at, delay_count;
static uint8_t regs[512], wire[512][2];
static uint16_t vals[512];
static uint32_t delays[16];
static wm8978_status_t fail_code = WM8978_IO;
static wm8978_status_t send(void *ctx, uint8_t address, const uint8_t data[2])
{
    assert(ctx == &calls && address == 0x1a);
    assert(calls < 512);
    regs[calls] = data[0] >> 1;
    wire[calls][0] = data[0]; wire[calls][1] = data[1];
    vals[calls] = ((data[0] & 1) << 8) | data[1];
    assert(regs[calls] < 58);
    assert(regs[calls] != 17 && regs[calls] != 23 && regs[calls] != 26 &&
           regs[calls] != 31 && regs[calls] != 40 && regs[calls] != 42);
    return ++calls == fail_at ? fail_code : WM8978_OK;
}
static void delay(void *ctx, uint32_t ms)
{
    assert(ctx == &calls && delay_count < 16);
    delays[delay_count++] = ms;
}
static const wm8978_bus_t bus = {&calls, send, delay};
static wm8978_config_t cfg = {9375000, 48000, 16, true};
static wm8978_t dev;

static void fresh(void)
{
    calls = fail_at = delay_count = 0;
    memset(&dev, 0, sizeof(dev));
    assert(wm8978_init(&dev, &bus, &cfg) == WM8978_OK);
}
static uint16_t cached(unsigned reg)
{
    uint16_t value;
    assert(wm8978_get_cached(&dev, reg, &value) == WM8978_OK);
    return value;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *which = argv[1];
    if (!strcmp(which, "pll")) {
        const uint32_t rates[] = {8000,11025,16000,22050,32000,44100,48000};
        const uint32_t clocks[] = {3781000,8000000,8000001,9375000,11289600,
                                  12000000,12288000,12500000,24000000,32768000};
        const double divs[] = {1,1.5,2,3,4,6,8,12};
        wm8978_pll_t pll;
        for (unsigned r = 0; r < 7; ++r) for (unsigned c = 0; c < 10; ++c) {
            assert(wm8978_calculate_pll(clocks[c], rates[r], &pll) == WM8978_OK);
            assert(pll.n >= 6 && pll.n <= 12 && pll.k < (1U<<24));
            double achieved = clocks[c] / (pll.pre_div2 ? 2.0 : 1.0) *
                (pll.n + pll.k / 16777216.0) / 4 / divs[pll.mclk_div_index] / 256;
            double error = achieved / rates[r] - 1;
            assert(error < 0.00000001 && error > -0.00000001);
        }
        assert(wm8978_calculate_pll(12000000, 48000, &pll) == 0);
        assert(pll.n == 8 && pll.pre_div2 == 1 && pll.k == 0x3126e9);
        assert(pll.mclk_div_index == 0 && pll.pll_out_hz == 12288000);
        assert(wm8978_calculate_pll(12000000, 44100, &pll) == 0);
        assert(pll.n == 7 && pll.pre_div2 == 1 && pll.k == 0x86c227);
        assert(wm8978_calculate_pll(9375000, 48000, &pll) == 0);
        printf("9.375MHz -> 48kHz: N=%u K=0x%06x prediv=%u MCLKDIV-index=%u\n",
               pll.n, pll.k, pll.pre_div2 ? 2 : 1, pll.mclk_div_index);
        memset(&pll, 0xa5, sizeof(pll));
        wm8978_pll_t before = pll;
        assert(wm8978_calculate_pll(0, 48000, &pll) == WM8978_INVALID);
        assert(wm8978_calculate_pll(UINT32_MAX, 48000, &pll) == WM8978_INVALID);
        assert(wm8978_calculate_pll(12000000, UINT32_MAX, &pll) == WM8978_INVALID);
        assert(wm8978_calculate_pll(12000000, 96000, &pll) == WM8978_INVALID);
        assert(wm8978_calculate_pll(12000000, 48000, NULL) == WM8978_INVALID);
        assert(memcmp(&before, &pll, sizeof(pll)) == 0);
        return 0;
    }
    fresh();
    if (!strcmp(which, "init")) {
        assert(regs[0] == 0 && vals[0] == 0);
        assert(cached(2) == 0 && cached(3) == 0);
        assert(cached(4) == 0x10 && (cached(6) & 0x11d) == 0x109);
        assert(cached(10) & 0x40);
        assert(cached(52) == 0x40 && cached(53) == 0x140);
        assert(cached(54) == 0x40 && cached(55) == 0x140);
        assert(cached(1) == 0x2e && delays[0] == 100 && delays[1] == 10);
        assert(cached(36) == ((dev.pll.pre_div2 << 4) | dev.pll.n));
        assert((((uint32_t)cached(37) << 18) | (cached(38) << 9) | cached(39)) == dev.pll.k);
    } else if (!strcmp(which, "volumes")) {
        unsigned i = calls;
        assert(wm8978_set_dac_volume(&dev, 128, 255) == 0);
        assert(wire[i][0] == 22 && wire[i][1] == 128);
        assert(wire[i+1][0] == 25 && wire[i+1][1] == 255);
        assert(cached(11) == 128 && cached(12) == 511);
        assert(wm8978_set_adc_volume(&dev, 0, 127) == 0);
        assert(cached(15) == 0 && cached(16) == 383);
        assert(wm8978_set_output_volume(&dev, WM8978_HEADPHONE, 10, 20, false) == 0);
        assert(cached(52) == 10 && cached(53) == 276);
        assert(wm8978_set_output_volume(&dev, WM8978_SPEAKER, 63, 0, true) == 0);
        assert(cached(54) == 127 && cached(55) == 320);
        assert(wm8978_set_dac_mute(&dev, false) == 0 && cached(10) == 0);
        i = calls;
        assert(wm8978_set_dac_mute(&dev, false) == 0 && calls == i);
        assert(wm8978_set_dac_mute(&dev, true) == 0 && cached(10) == 0x40);
    } else if (!strcmp(which, "paths")) {
        wm8978_paths_t p = {WM8978_INPUT_MIC_DIFFERENTIAL, true, true, false, 16, false};
        assert(wm8978_set_paths(&dev, &p) == 0);
        assert(cached(1) == 0x3d && cached(2) == 0x1bf && cached(3) == 0xf);
        assert(cached(44) == 0x33 && cached(45) == 16 && cached(46) == 272);
        assert(cached(47) == 0 && cached(48) == 0 && cached(50) == 1 && cached(51) == 1);
        assert(cached(10) & 0x40); /* Routing does not unmute DAC or outputs. */
        p = (wm8978_paths_t){WM8978_INPUT_LINE2, false, false, true, 63, true};
        assert(wm8978_set_paths(&dev, &p) == 0);
        assert(cached(44) == 0x44 && cached(2) == 0x3f && cached(3) == 0x6f);
        assert(cached(47) == 0x100 && cached(48) == 0x100);
        p = (wm8978_paths_t){0};
        assert(wm8978_set_paths(&dev, &p) == 0);
        assert(cached(1) == 0x2e && cached(2) == 0 && cached(3) == 0);
        assert(cached(45) & 0x40);
    } else if (!strcmp(which, "invalid")) {
        unsigned before = calls;
        uint16_t value;
        wm8978_config_t invalid = cfg;
        invalid.sample_bits = 8;
        assert(wm8978_init(&dev, &bus, &invalid) == WM8978_INVALID);
        invalid = cfg; invalid.sample_rate = 12345;
        assert(wm8978_init(&dev, &bus, &invalid) == WM8978_INVALID);
        assert(wm8978_init(NULL, &bus, &cfg) == WM8978_INVALID);
        assert(wm8978_init(&dev, NULL, &cfg) == WM8978_INVALID);
        assert(wm8978_set_output_volume(&dev, WM8978_HEADPHONE, 64, 0, false) == WM8978_INVALID);
        assert(wm8978_set_output_volume(&dev, (wm8978_output_t)2, 0, 0, false) == WM8978_INVALID);
        assert(wm8978_get_cached(&dev, 58, &value) == WM8978_INVALID);
        assert(wm8978_get_cached(&dev, 0, &value) == WM8978_INVALID);
        assert(wm8978_get_cached(&dev, 17, &value) == WM8978_INVALID);
        assert(wm8978_get_cached(&dev, 1, NULL) == WM8978_INVALID);
        assert(wm8978_set_paths(&dev, NULL) == WM8978_INVALID);
        wm8978_paths_t p = {.input = WM8978_INPUT_LINE2, .mic_bias = true};
        assert(wm8978_set_paths(&dev, &p) == WM8978_INVALID);
        assert(calls == before && dev.initialized);
        assert(wm8978_set_dac_mute(NULL, true) == WM8978_STATE);
    } else if (!strcmp(which, "fail_init")) {
        unsigned writes = calls;
        for (unsigned f = 1; f <= writes; ++f) {
            calls = delay_count = 0; fail_at = f;
            assert(wm8978_init(&dev, &bus, &cfg) == WM8978_IO);
            assert(calls == f && !dev.initialized && !dev.shadow_valid);
            assert(dev.last_error == WM8978_IO);
            assert(wm8978_set_dac_mute(&dev, false) == WM8978_STATE);
            uint16_t value;
            assert(wm8978_get_cached(&dev, 1, &value) == WM8978_STATE);
        }
        fresh();
    } else if (!strcmp(which, "fail_write")) {
        const wm8978_status_t errors[] = {WM8978_IO,WM8978_TIMEOUT,WM8978_BUSY};
        for (unsigned e = 0; e < 3; ++e) for (unsigned f = 1; f <= 2; ++f) {
            fresh(); fail_code = errors[e]; fail_at = calls + f;
            assert(wm8978_set_dac_volume(&dev, 7, 8) == errors[e]);
            assert(calls == fail_at && !dev.initialized && !dev.shadow_valid);
            assert(dev.last_error == errors[e]);
            assert(wm8978_set_adc_volume(&dev, 1, 2) == WM8978_STATE);
        }
    } else if (!strcmp(which, "fail_paths")) {
        wm8978_paths_t p = {WM8978_INPUT_LINE2, false, true, true, 32, true};
        unsigned before = calls;
        assert(wm8978_set_paths(&dev, &p) == 0);
        unsigned writes = calls - before;
        for (unsigned f = 1; f <= writes; ++f) {
            fresh(); fail_at = calls + f;
            assert(wm8978_set_paths(&dev, &p) == WM8978_IO);
            assert(calls == fail_at && !dev.initialized);
        }
    } else if (!strcmp(which, "shutdown")) {
        unsigned before = calls;
        assert(wm8978_power_down(&dev) == 0 && calls == before + 9);
        assert(!dev.initialized && !dev.shadow_valid);
        fresh(); before = calls; fail_at = before + 1;
        assert(wm8978_power_down(&dev) == WM8978_IO && calls == before + 9);
        assert(dev.last_error == WM8978_IO && !dev.initialized);
        assert(regs[calls-1] == 1 && vals[calls-1] == 0);
    } else if (!strcmp(which, "formats")) {
        const unsigned bits[] = {16,20,24,32};
        const unsigned rates[] = {8000,11025,16000,22050,32000,44100,48000};
        const unsigned filters[] = {5,4,3,2,1,0,0};
        for (unsigned b = 0; b < 4; ++b) for (unsigned r = 0; r < 7; ++r) {
            cfg.sample_bits = bits[b]; cfg.sample_rate = rates[r]; cfg.codec_master = (r & 1);
            fresh();
            assert(cached(4) == (0x10 | (b<<5)));
            assert(cached(7) == filters[r]*2);
            assert((cached(6) & 1) == (r & 1));
        }
    } else if (!strcmp(which, "recovery")) {
        fail_at = calls + 1;
        assert(wm8978_set_dac_mute(&dev, false) == WM8978_IO);
        calls = fail_at = delay_count = 0;
        assert(wm8978_init(&dev, &dev.bus, &dev.config) == 0);
        assert(dev.initialized && cached(10) == 0x40 && dev.config.mclk_hz == 9375000);
    } else { assert(!"unknown case"); }
    return 0;
}
'''


class Wm8978Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        folder = Path(cls.tmp.name)
        source = folder / "test.c"
        source.write_text(HARNESS)
        cls.binary = folder / "test"
        result = subprocess.run([
            "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=undefined", "-fno-sanitize-recover=all",
            "-I", str(ROOT / "firmware/USER/inc"), str(source),
            str(ROOT / "firmware/USER/src/wm8978.c"), "-o", str(cls.binary),
        ], capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)

    def run_case(self, case):
        result = subprocess.run([str(self.binary), case], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_pll_vectors_and_frequency_error(self): self.run_case("pll")
    def test_initialization_and_muted_defaults(self): self.run_case("init")
    def test_wire_format_and_stereo_volume_latch(self): self.run_case("volumes")
    def test_capture_playback_routing(self): self.run_case("paths")
    def test_invalid_arguments_do_not_write(self): self.run_case("invalid")
    def test_failure_at_every_initialization_write(self): self.run_case("fail_init")
    def test_failed_stereo_write_invalidates_cache(self): self.run_case("fail_write")
    def test_failure_at_every_routing_write(self): self.run_case("fail_paths")
    def test_shutdown_continues_after_failure(self): self.run_case("shutdown")
    def test_formats_rates_and_master_slave_modes(self): self.run_case("formats")
    def test_reinitialize_after_transport_failure(self): self.run_case("recovery")


if __name__ == "__main__":
    unittest.main()
