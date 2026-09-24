# Copyright (c) 2025 Nations Technologies Inc.
# SPDX-License-Identifier: Apache-2.0

"""The PB3 indicator that follows a keyword detection.

kws_app.c's LED code is three lines of GPIO writes around one comparison, and
the comparison is the part worth testing: the deadline is a millisecond tick
that wraps every 49 days, and the pin is active low, so both "stuck on" and
"stuck off" are one sign error away.

The two functions are sliced out of the firmware and run against a GPIO mock
that records every write, so what is asserted is the pin traffic the hardware
would see -- including that an idle LED is not rewritten on every round.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
APP = ROOT / "firmware/USER/src/kws_app.c"

MOCK = r'''
#include <assert.h>
#include <stdint.h>

/* Device selectors.  Only the mock compares them, so the values are arbitrary
   except that they must not collide. */
static int gpiob;
#define GPIOB (&gpiob)
#define GPIO_PIN_3 0x0008U
#define GPIO_RESET_TAG 0x10000U
#define GPIO_SET_TAG   0x20000U

/* Every pin write, tagged with the operation.  Modelling the pin as lit/dark
   instead of as two calls would let an inverted polarity pass: the LED is
   active low, so lit is ResetBits and dark is SetBits. */
#define LOG_WORDS 64
static uint32_t log_[LOG_WORDS];
static unsigned log_len;

static void note(uint32_t word)
{
    assert(log_len < LOG_WORDS);
    log_[log_len++] = word;
}

static unsigned count_tag(uint32_t tag)
{
    unsigned n = 0U;
    for (unsigned i = 0; i < log_len; ++i) {
        if ((log_[i] & 0xffff0000U) == tag) ++n;
    }
    return n;
}

static unsigned count_lit(void)  { return count_tag(GPIO_RESET_TAG); }
static unsigned count_dark(void) { return count_tag(GPIO_SET_TAG); }

static void reset_log(void) { log_len = 0U; }

static void GPIO_ResetBits(void *port, uint16_t pin)
{
    assert(port == GPIOB && pin == GPIO_PIN_3);
    note(GPIO_RESET_TAG | pin);
}

static void GPIO_SetBits(void *port, uint16_t pin)
{
    assert(port == GPIOB && pin == GPIO_PIN_3);
    note(GPIO_SET_TAG | pin);
}

volatile uint32_t mwTick;
'''

CASES = r'''
/* Off and ready before each case, as at boot, when GPIO_Configuration() has
   driven every indicator high. */
static void arm(void)
{
    g_led_lit = 0;
    reset_log();
}

/* A detection lights the LED by driving the pin low, and does nothing else. */
static void case_a_detection_lights_the_led(void)
{
    arm();
    mwTick = 1234U;
    led_flash();
    assert(g_led_lit == 1);
    assert(count_lit() == 1U && count_dark() == 0U);
}

/* The whole interval, sampled every millisecond: not one millisecond early. */
static void case_it_stays_lit_for_the_whole_interval(void)
{
    arm();
    mwTick = 1000U;
    led_flash();
    for (uint32_t t = 0U; t < KWS_LED_MS; ++t) {
        mwTick = 1000U + t;
        led_service();
    }
    assert(g_led_lit == 1 && count_dark() == 0U);

    mwTick = 1000U + KWS_LED_MS;
    led_service();
    assert(g_led_lit == 0 && count_dark() == 1U);
    /* And the expiry is not re-issued on the rounds that follow it. */
    led_service();
    mwTick += 60U;
    led_service();
    assert(count_dark() == 1U);
}

/* A detected word arriving while the LED is still lit restarts the interval
   rather than being swallowed by it. */
static void case_a_second_detection_restarts_the_interval(void)
{
    arm();
    mwTick = 500U;
    led_flash();
    mwTick = 1200U;
    led_service();
    assert(g_led_lit == 1 && count_dark() == 0U);

    mwTick = 1300U;
    led_flash();
    assert(count_lit() == 2U && count_dark() == 0U);

    mwTick = 2000U;            /* 700 ms into the second interval */
    led_service();
    assert(g_led_lit == 1 && count_dark() == 0U);
    mwTick = 2300U;
    led_service();
    assert(g_led_lit == 0 && count_dark() == 1U);
}

/* mwTick is a free-running millisecond counter, so the comparison has to be
   the signed difference.  Written as `mwTick >= deadline` the LED would latch
   on at the wrap and never go dark again. */
static void case_the_tick_wrapping_does_not_strand_the_led(void)
{
    arm();
    mwTick = 0xfffffff0U;      /* 16 ms before mwTick rolls over */
    led_flash();
    assert(g_led_lit == 1);

    mwTick = 0xfffffff5U;      /* 5 ms in, deadline is past the wrap */
    led_service();
    assert(g_led_lit == 1 && count_dark() == 0U);

    mwTick = 0x00000010U;      /* 32 ms in: the difference is still negative */
    led_service();
    assert(g_led_lit == 1 && count_dark() == 0U);

    mwTick = 0x00000400U;      /* 1040 ms in */
    led_service();
    assert(g_led_lit == 0 && count_dark() == 1U);
}

/* The expiry check runs once per round, ~60 ms apart, so an idle LED must
   leave the pin alone entirely.  Without the g_led_lit guard this would drive
   PB3 high sixteen times a second for as long as the board is powered. */
static void case_an_idle_led_is_never_rewritten(void)
{
    arm();
    for (uint32_t round = 0U; round < 100U; ++round) {
        mwTick = round * 60U;
        led_service();
    }
    assert(log_len == 0U);
}
'''


def slice_functions(source):
    """The LED helpers, from led_flash() up to whatever follows led_service()."""
    start = source.index("static void led_flash(void)")
    end = source.index("static void banner(void)")
    return source[start:end]


class KwsLedTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        folder = Path(cls.tmp.name)
        actual = APP.read_text()

        led = slice_functions(actual)
        assert led.count("static void ") == 2, "extraction of the LED helpers looks wrong"
        assert led.rstrip().endswith("}"), "extraction of the LED helpers looks wrong"

        # The interval is sliced out of the firmware too, so shortening it
        # below one round is caught by the harness rather than by eye.
        match = re.search(r"#define KWS_LED_MS\s+(\d+)U", actual)
        assert match, "KWS_LED_MS is no longer a plain millisecond count"

        source = folder / "kws_led_harness.c"
        source.write_text(
            MOCK
            + "\nstatic uint32_t g_led_deadline;\n"
            + "static int g_led_lit;\n"
            + f"#define KWS_LED_MS {match.group(1)}U\n"
            + led + "\n"
            + CASES
            + "int main(void) {\n"
            + "  case_a_detection_lights_the_led();\n"
            + "  case_it_stays_lit_for_the_whole_interval();\n"
            + "  case_a_second_detection_restarts_the_interval();\n"
            + "  case_the_tick_wrapping_does_not_strand_the_led();\n"
            + "  case_an_idle_led_is_never_rewritten();\n"
            + "  return 0;\n}\n"
        )
        cls.binary = folder / "kws_led_harness"
        result = subprocess.run([
            "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=undefined", "-fno-sanitize-recover=all",
            str(source), "-o", str(cls.binary),
        ], capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)

    def test_timing_and_polarity(self):
        result = subprocess.run([str(self.binary)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_the_led_is_pb3_and_active_low(self):
        """The pin and its polarity, as the board wires them.

        GPIO_Configuration() comments PB3/PF10/PI8 as active-low and drives all
        three high before switching them to outputs.  Matching that here keeps
        the two files from disagreeing about what "off" means.
        """
        source = APP.read_text()
        lit = slice_functions(source).split("static void led_service", 1)[0]
        self.assertIn("GPIO_ResetBits(GPIOB, GPIO_PIN_3)", lit)
        self.assertIn("GPIO_SetBits(GPIOB, GPIO_PIN_3)",
                      slice_functions(source).split("static void led_service", 1)[1])

        cfg = (ROOT / "firmware/USER/src/n32h7xx_cfg.c").read_text()
        self.assertIn("GPIO_SetBits(GPIOB, GPIO_PIN_3);", cfg)

    def test_it_fires_where_the_line_is_printed(self):
        """Same condition as the serial report: past the threshold, past the
        debounce, and never for silence or the unknown bucket."""
        source = APP.read_text()
        after_debounce = source.split("if (g_last_reported == (int)best) return;", 1)[1]
        self.assertIn("led_flash();", after_debounce.split("kws_uart_puts", 1)[0])
        self.assertEqual(source.count("led_flash();"), 1,
                         "the LED is driven from somewhere other than the report")

    def test_the_main_loop_services_the_expiry(self):
        source = APP.read_text()
        loop = source.split("void kws_app_run(void)", 1)[1]
        self.assertIn("led_service();", loop)
        # Before round_once(), so a round that blocks on the audio read cannot
        # delay the expiry past the round it was already late by.
        self.assertLess(loop.index("led_service();"), loop.index("round_once();"))

    def test_the_round_cadence_barely_quantises_the_flash(self):
        """1 s measured by a 60 ms round is what the eye sees as one second."""
        source = APP.read_text()
        interval = int(re.search(r"#define KWS_LED_MS\s+(\d+)U", source).group(1))
        frames = int(re.search(r"#define KWS_AUDIO_BLOCK_FRAMES\s+(\d+)U",
                               (ROOT / "firmware/USER/inc/kws_audio.h").read_text()
                               ).group(1))
        sample_rate = int(re.search(r"#define KWS_MFCC_SAMP_FREQ\s+(\d+)U",
                                    (ROOT / "firmware/USER/inc/kws_mfcc.h").read_text()
                                    ).group(1))
        round_ms = frames * 1000.0 / sample_rate
        self.assertLess(round_ms / interval, 0.1,
                        "one round is a tenth of the flash or more")


if __name__ == "__main__":
    unittest.main()
