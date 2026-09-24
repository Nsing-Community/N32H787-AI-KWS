# Copyright (c) 2025 Nations Technologies Inc.
# SPDX-License-Identifier: Apache-2.0

"""Execute the actual N32 transport functions with mocked peripheral events."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

MOCK = r'''
#include "wm8978_n32.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
/* The M7 builds the codec driver with CORE_CM7 defined, so owns_bus() reads
   RCC->M4RSTREL.  That check is a live guard, not a vestige of the dual-core
   build: this firmware holds the second core in reset and never releases it,
   but if that ever changed the codec would end up with two bus masters. */
#define CORE_CM7 1
#define RCC_M4RSTREL_EN 1U
#define I2C_CTRL1_I2CEN 1U
#define I2C_FLAG_BUSY 2U
#define I2C_FLAG_NAKF 4U
#define I2C_FLAG_BSER 8U
#define I2C_FLAG_ABLO 16U
#define I2C_FLAG_TMOUT 32U
#define I2C_FLAG_STOPF 64U
#define I2C_FLAG_WRAVL 128U
#define I2C_FLAG_WRE 256U
#define ENABLE 1
#define DISABLE 0
#define I2C_DIRECTION_SEND 0
static struct {uint32_t M4RSTREL;} rcc;
static struct {volatile uint32_t CTRL1, STSINT;} i2c;
#define RCC (&rcc)
#define I2C4 (&i2c)
static unsigned isr, sends, starts, stops, cleared, scenario;
static unsigned __get_IPSR(void) {return isr;}
static void I2C_ClrFlag(void *p, uint32_t flags)
{
    assert(p == I2C4); ++cleared; i2c.STSINT &= ~flags;
}
static void I2C_EnableReload(void *p, int enabled) {assert(p == I2C4 && !enabled);}
static void I2C_EnableAutomaticEnd(void *p, int enabled) {assert(p == I2C4 && enabled);}
static void I2C_ConfigSendAddress(void *p, uint32_t address, unsigned dir)
{
    assert(p == I2C4 && address == 0x34 && dir == 0);
}
static void I2C_SetTransferByteNumber(void *p, unsigned bytes) {assert(p == I2C4 && bytes == 2);}
static void I2C_GenerateStart(void *p, int enabled)
{
    assert(p == I2C4 && enabled); ++starts;
    assert(i2c.STSINT == I2C_FLAG_WRE);
    i2c.STSINT = I2C_FLAG_BUSY;
    if (scenario == 1) i2c.STSINT |= I2C_FLAG_NAKF;
    else if (scenario == 2) i2c.STSINT |= I2C_FLAG_ABLO;
    else if (scenario == 3) {} /* Timed out waiting for transmit-ready. */
    else i2c.STSINT |= I2C_FLAG_WRAVL;
}
static void I2C_SendData(void *p, uint8_t byte)
{
    assert(p == I2C4 && byte == (sends == 0 ? 0x19 : 0xff));
    if (++sends == 2) {
        i2c.STSINT = I2C_FLAG_STOPF;
        if (scenario == 4) i2c.STSINT |= I2C_FLAG_NAKF;
        if (scenario == 5) i2c.STSINT = I2C_FLAG_BUSY;
    }
}
static void I2C_GenerateStop(void *p, int enabled)
{
    assert(p == I2C4 && enabled); ++stops; i2c.STSINT = I2C_FLAG_STOPF;
}
'''

CASES = r'''
int main(int argc, char **argv)
{
    assert(argc == 2);
    scenario = (unsigned)atoi(argv[1]);
    i2c.CTRL1 = I2C_CTRL1_I2CEN;
    i2c.STSINT = I2C_FLAG_NAKF | I2C_FLAG_STOPF; /* Stale flags. */
    if (scenario == 6) rcc.M4RSTREL = 1;
    if (scenario == 7) isr = 1;
    if (scenario == 8) i2c.STSINT = I2C_FLAG_BUSY;
    if (scenario == 9) i2c.CTRL1 = 0;
    const uint8_t bytes[2] = {0x19, 0xff};
    wm8978_status_t rc = bus_write(NULL, 0x1a, bytes);
    if (scenario >= 6) {
        assert(rc == (scenario == 9 ? WM8978_STATE : WM8978_BUSY));
        assert(starts == 0 && sends == 0 && stops == 0 && cleared == 0);
    } else if (scenario == 0) {
        assert(rc == 0 && sends == 2 && starts == 1 && stops == 0);
        assert(!(i2c.STSINT & I2C_FLAG_STOPF));
    } else if (scenario == 1 || scenario == 2) {
        assert(rc == WM8978_IO && sends == 0 && starts == 1);
        assert(stops == (scenario == 1 ? 1U : 0U));
    } else if (scenario == 3 || scenario == 5) {
        assert(rc == WM8978_TIMEOUT && stops == 1);
        assert(sends == (scenario == 3 ? 0U : 2U));
    } else if (scenario == 4) {
        assert(rc == WM8978_IO && sends == 2 && stops == 0);
    }
    return 0;
}
'''


class Wm8978TransportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        folder = Path(cls.tmp.name)
        actual = (ROOT / "firmware/USER/src/wm8978_n32.c").read_text()
        transport = actual[actual.index("#define WAIT_LIMIT"):actual.index("static void delay_ms")]
        source = folder / "transport.c"
        source.write_text(MOCK + transport + CASES)
        cls.binary = folder / "transport"
        result = subprocess.run([
            "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=undefined", "-fno-sanitize-recover=all",
            "-I", str(ROOT / "firmware/USER/inc"), str(source), "-o", str(cls.binary),
        ], capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)

    def run_case(self, case):
        result = subprocess.run([str(self.binary), str(case)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_two_byte_transfer_and_stale_flags(self): self.run_case(0)
    def test_nack_aborts_transfer(self): self.run_case(1)
    def test_arbitration_loss_does_not_generate_stop(self): self.run_case(2)
    def test_transmit_timeout_is_bounded(self): self.run_case(3)
    def test_final_nack_is_not_mistaken_for_success(self): self.run_case(4)
    def test_stop_timeout_is_bounded(self): self.run_case(5)
    def test_bus_is_refused_when_the_second_core_owns_it(self): self.run_case(6)
    def test_isr_cannot_use_blocking_driver(self): self.run_case(7)
    def test_busy_bus_is_untouched(self): self.run_case(8)
    def test_disabled_bus_is_untouched(self): self.run_case(9)


class BusOwnerTests(unittest.TestCase):
    """main() has to claim the bus before anything touches the codec.

    The guard exercised above only protects the bus if the boot sequence
    actually clears the second core's reset-release bit, and nothing else in
    the firmware or the linker enforces that ordering.
    """

    @staticmethod
    def call_offset(text, name):
        """Offset of a top-level call, excluding mentions in comments.

        Comments here name the very functions being ordered, so a bare
        substring search would match prose and silently invert the assertion.
        """
        match = re.search(rf"^    {re.escape(name)}\(\);$", text, re.MULTILINE)
        assert match, f"no top-level call to {name}() in main.c"
        return match.start()

    def test_main_clears_the_second_core_release_bit_before_the_codec(self):
        main = (ROOT / "firmware/USER/src/main.c").read_text()
        release = main.index("RCC->M4RSTREL &= ~RCC_M4RSTREL_EN")
        self.assertLess(release, self.call_offset(main, "kws_audio_init"))
        self.assertNotIn("RCC_EnableCM4", main)

    def test_the_cached_mailbox_is_reinitialised_before_the_cache_goes_on(self):
        """A stale flash_pause would stop the board dead on the first poll.

        AHB SRAM is not cleared by reset, so the pause word the flasher left
        behind survives `reset run`; m7_flash_poll() acts on it by disabling
        both caches and spinning.  The application has to clear it before it
        starts polling.
        """
        main = (ROOT / "firmware/USER/src/main.c").read_text()
        for word in ("flash_pause", "flash_paused", "m7_flash_paused", "boot_request"):
            self.assertRegex(main, rf"g_m4_shared\.{word}\s*=\s*0U;")
        self.assertIn("g_m4_shared.magic = M4_SHARED_MAGIC;", main)
        self.assertLess(main.index("g_m4_shared.flash_pause"),
                        self.call_offset(main, "m7_cache_enable"))
        self.assertLess(self.call_offset(main, "m7_cache_enable"),
                        self.call_offset(main, "kws_audio_init"))
        self.assertLess(main.index("g_m4_shared.magic"),
                        self.call_offset(main, "kws_app_run"))


if __name__ == "__main__":
    unittest.main()
