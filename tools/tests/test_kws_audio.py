# Copyright (c) 2025 Nations Technologies Inc.
# SPDX-License-Identifier: Apache-2.0

"""Exercise the real capture code with the vendor peripherals stubbed out.

Two parts of kws_audio.c cannot be checked by reading them and cannot be
debugged on the board without a scope: the WS-edge arming in start_dma(),
where the receiver is enabled on an exact edge with interrupts off and the
timeout paths have to restore them, and the descriptor ring, where pNext has
to close on itself and block 0 has to be written first.

The state typedef, the extracted functions and the geometry constants are all
taken from the firmware sources, so a change there reaches this test rather
than leaving it passing against a stale copy.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

MOCK = r'''
#include "kws_audio.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

/* Device selectors.  Only the mock compares them, so the values are arbitrary
   except that they must not collide. */
#define DMA1 (&dma1)
#define DMAMUX1_ID (&dmamux1)
#define DMA_CHANNEL_0 0
#define DMAMUX_CHANNEL_0 0
#define DMAMUX1_REQUEST_I2S1_RX 0xB2U
#define I2S1 (&i2s1)
#define I2S_DMA_RX 1
#define GPIOG (&gpiog)
#define GPIO_PIN_10 0x0400U
#define ENABLE 1
#define DISABLE 0
#define DMA_CH_STS_OK 0
#define DMA_CH_EVENT_BLOCK_TRANSFER_COMPLETE 2U
#define DMA_CH_EVENT_ERROR 16U
#define DMA_CH_TRANSFER_WIDTH_16 1U
#define DMA_CH_ADDRESS_COUNT_MODE_NO_CHANGE 0U
#define DMA_CH_ADDRESS_COUNT_MODE_INCREMENT 1U
#define DMA_CH_BURST_LENGTH_1 0U
#define DMA_CH_TRANSFER_FLOW_P2M_DMA 3U
#define DMA_CH_TRANSFER_TYPE_MULTI_BLOCK_SRCADR_LINKED_DSTADR_LINKED 3U
#define DMA_CH_SRC_HANDSHAKING_HARDWARE 1U
#define DMA_CH_HARDWARE_HANDSHAKING_IF_0 0U
#define DMA_CH_PRIORITY_7 7U
#define DMA1_Channel0_IRQn 90
#define RCC_AHB1_PERIPHEN_M7_DMA1 0x1U
#define RCC_AHB1_PERIPHEN_M7_DMAMUX1 0x2U
#define RCC_AHB1_PERIPHEN_M4_DMA1 0x4U
#define RCC_AHB1_PERIPHEN_M4_DMAMUX1 0x8U

/* The two descriptor structs are mirrored from the vendor header field for
   field, including the bitfields, because the hardware reads those bits: the
   interesting failure is a control value that fits the struct but lands in the
   wrong bits, or a 64-bit initialiser truncated when it is copied into the
   descriptor's low word.

   The address fields stay uint32_t, as the vendor declares them, even though
   this harness runs on a 64-bit host: the firmware's own `(uint32_t)&x` casts
   are then exercised rather than sidestepped, and the address assertions below
   mask to match.  That needs -Wno-pointer-to-int-cast, which is an artifact of
   pointer sizes differing and not something the target ever sees. */
typedef struct __mock_link_item
{
    uint32_t SrcAddr;
    uint32_t DstAddr;
    struct __mock_link_item *pNext;
    union {
        uint32_t ChCtrlL32;
        struct {
            uint32_t IntEn: 1;
            uint32_t DstTfrWidth: 3;
            uint32_t SrcTfrWidth: 3;
            uint32_t DstAddrCountMode: 2;
            uint32_t SrcAddrCountMode: 2;
            uint32_t DstBurstLen: 3;
            uint32_t SrcBurstLen: 3;
            uint32_t SrcGatherEn: 1;
            uint32_t DstScatterEn: 1;
            uint32_t : 1;
            uint32_t TfrTypeFlowCtrl: 3;
            uint32_t : 4;
            uint32_t DstLinkedListEn: 1;
            uint32_t SrcLinkedListEn: 1;
            uint32_t : 3;
        };
    };
    union {
        uint32_t ChCtrlH32;
        struct {
            uint32_t BlkTfrSize: 12;
            uint32_t DoneFlag: 1;
            uint32_t : 19;
        };
    };
} DMA_LinkListItemType;

typedef struct {
    union {
        uint64_t ChCtrl;
        struct {
            uint64_t IntEn: 1;
            uint64_t DstTfrWidth: 3;
            uint64_t SrcTfrWidth: 3;
            uint64_t DstAddrCountMode: 2;
            uint64_t SrcAddrCountMode: 2;
            uint64_t DstBurstLen: 3;
            uint64_t SrcBurstLen: 3;
            uint64_t SrcGatherEn: 1;
            uint64_t DstScatterEn: 1;
            uint64_t : 1;
            uint64_t TfrTypeFlowCtrl: 3;
            uint64_t DstMasterSelect: 2;
            uint64_t SrcMasterSelect: 2;
            uint64_t : 5;
            uint64_t BlkTfrSize: 12;
            uint64_t : 20;
        };
    };
    uint32_t SrcAddr;
    uint32_t DstAddr;
    DMA_LinkListItemType *pLinkListItem;
    uint32_t SrcGatherCtrl;
    uint32_t DstScatterCtrl;
    uint32_t TfrType;
    uint32_t ChannelPriority;
    uint32_t SrcHandshaking;
    uint32_t SrcHsInterface;
    uint32_t SrcHsInterfacePol;
    uint32_t DstHandshaking;
    uint32_t DstHsInterface;
    uint32_t DstHsInterfacePol;
} DMA_ChInitType;

static struct { uint32_t I2SCFGR, I2SPR, DR; } i2s1;
static struct { int pad; } dma1, dmamux1, gpiog;

/* Event log: several assertions are about the order of calls, not the set. */
#define MAX_EVENTS 64
static char events[MAX_EVENTS][20];
static unsigned event_count;
static void ev(const char *name)
{
    assert(event_count < MAX_EVENTS);
    strncpy(events[event_count++], name, sizeof(events[0]) - 1);
}
static int seen(const char *name)
{
    for (unsigned i = 0; i < event_count; ++i) if (!strcmp(events[i], name)) return 1;
    return 0;
}
static int index_of(const char *name)
{
    for (unsigned i = 0; i < event_count; ++i) if (!strcmp(events[i], name)) return (int)i;
    return -1;
}

/* Simulated core state.  The firmware's bounded waits poll the cycle counter
   and give up when enough of it has passed, so every read has to advance it:
   a plain variable would leave those loops spinning forever here, and a test
   that never returns is worse than one that fails. */
static uint32_t dwt_counter;
static uint32_t dwt_step = 1;
static uint32_t dwt_advance(void) { dwt_counter += dwt_step; return dwt_counter; }
static uint32_t primask_saved, irq_disable_count;
#define DWT_CYCCNT (dwt_advance())
#define SystemCoreClock 600000000U
static uint32_t __get_PRIMASK(void) { return primask_saved; }
static void __set_PRIMASK(uint32_t v) { primask_saved = v; ev("set_primask"); }
static void __disable_irq(void) { ++irq_disable_count; ev("disable_irq"); }
static void __DMB(void) {}
static void __DSB(void) {}

/* Scripted WS level.  The last entry repeats once the script is exhausted, so
   a "stuck" test needs one entry and a "clean edge" test needs two. */
static int ws_values[8] = {0, 1};
static unsigned ws_count = 2, ws_index;
static int GPIO_ReadInputDataBit(void *port, uint32_t pin)
{
    assert(port == GPIOG && pin == GPIO_PIN_10);
    ++ws_index;
    if (ws_index <= 8) ev("read_ws");
    return ws_values[ws_index - 1 < ws_count ? ws_index - 1 : ws_count - 1];
}

static void RCC_EnableAHB1PeriphClk1(uint32_t mask, int en) { assert(!en == 0 && mask != 0U); }
static void RCC_EnableAHB1PeriphClk3(uint32_t mask, int en) { (void)mask; (void)en; }
static void DMA_ControllerCmd(void *d, int en) { assert(d == DMA1); (void)en; ev("dma_controller"); }
static void DMA_ChannelCmd(void *d, int ch, int en)
{
    assert(d == DMA1 && ch == DMA_CHANNEL_0);
    ev(en ? "channel_on" : "channel_off");
}
static void DMA_ChannelEventCmd(void *d, int ch, uint32_t e, int en)
{
    assert(d == DMA1 && ch == DMA_CHANNEL_0 && e != 0U);
    ev(en ? "events_on" : "events_off");
}
static void DMA_ClearChannelEventStatus(void *d, int ch, uint32_t e)
{
    assert(d == DMA1 && ch == DMA_CHANNEL_0 && e != 0U);
    ev("clear_status");
}
static void NVIC_DisableIRQ(int irqn) { assert(irqn == 90); ev("nvic_off"); }
static void NVIC_ClearPendingIRQ(int irqn) { assert(irqn == 90); ev("nvic_clear"); }
static void NVIC_EnableIRQ(int irqn) { assert(irqn == 90); ev("nvic_on"); }
static void NVIC_SetPriority(int irqn, uint32_t p) { assert(irqn == 90 && p == 2U); }
static void I2S_Enable(void *p, int en) { assert(p == I2S1); ev(en ? "i2s_on" : "i2s_off"); }
static void I2S_EnableDma(void *p, int dir, int en)
{
    assert(p == I2S1 && dir == I2S_DMA_RX);
    ev(en ? "i2s_dma_on" : "i2s_dma_off");
}
static void I2S_Reset(void *p) { assert(p == I2S1); ev("i2s_reset"); }
static void DMAMUX_SetRequestID(void *m, int ch, uint32_t req)
{
    assert(m == DMAMUX1_ID && ch == DMAMUX_CHANNEL_0 && req == DMAMUX1_REQUEST_I2S1_RX);
    ev("dmamux");
}
/* The vendor initialiser fills the control union with the sane defaults that
   start_dma() then overrides field by field.  Seeding the mirrored bitfields
   rather than an opaque word is what lets the assertions below tell a control
   value that was copied into the descriptor from one that was dropped. */
static void DMA_ChannelStructInit(DMA_ChInitType *init)
{
    memset(init, 0, sizeof(*init));
    init->ChCtrl = 0x1ULL;               /* IntEn, and nothing else */
    init->TfrTypeFlowCtrl = DMA_CH_TRANSFER_FLOW_P2M_DMA;
    ev("init_defaults");
}
static DMA_ChInitType *last_init;
static DMA_LinkListItemType *last_links;
static int DMA_ChannelInit(void *d, DMA_ChInitType *init, int ch)
{
    assert(d == DMA1 && ch == DMA_CHANNEL_0);
    last_init = init;
    /* The list deliberately starts at item 1: DstAddr already points at block
       0, so the engine writes block 0 first and then walks the ring in order.
       Item 0 must therefore still exist and be reachable, and the ring is
       checked for that in case_descriptors_form_a_ring(). */
    last_links = init->pLinkListItem - 1;
    assert(init->BlkTfrSize == KWS_AUDIO_BLOCK_WORDS);
    assert(KWS_AUDIO_BLOCK_WORDS <= 4095U);   /* BlkTfrSize is a 12-bit field */
    for (unsigned i = 0; i < KWS_AUDIO_BLOCKS; ++i) {
        /* The firmware narrows the peripheral address to 32 bits; on this
           host that is a truncation, so compare the same way it stores. */
        assert(last_links[i].SrcAddr == (uint32_t)(uintptr_t)&I2S1->DR);
        assert(last_links[i].SrcLinkedListEn == 1U && last_links[i].DstLinkedListEn == 1U);
        assert(last_links[i].BlkTfrSize == KWS_AUDIO_BLOCK_WORDS);
        /* Everything start_dma() configured on the initialiser has to survive
           the copy into the descriptor's 32-bit control word. */
        assert(last_links[i].SrcTfrWidth == DMA_CH_TRANSFER_WIDTH_16);
        assert(last_links[i].DstTfrWidth == DMA_CH_TRANSFER_WIDTH_16);
        assert(last_links[i].DstAddrCountMode == DMA_CH_ADDRESS_COUNT_MODE_INCREMENT);
        assert(last_links[i].SrcAddrCountMode == DMA_CH_ADDRESS_COUNT_MODE_NO_CHANGE);
        assert(last_links[i].SrcBurstLen == DMA_CH_BURST_LENGTH_1);
        assert(last_links[i].DstBurstLen == DMA_CH_BURST_LENGTH_1);
        assert(last_links[i].TfrTypeFlowCtrl == DMA_CH_TRANSFER_FLOW_P2M_DMA);
    }
    ev("channel_init");
    return DMA_CH_STS_OK;
}
'''

CASES = r'''
static void reset_log(void)
{
    event_count = 0U; ws_index = 0U; dwt_counter = 0U; dwt_step = 1U;
    primask_saved = 0x11U; irq_disable_count = 0U;
    /* The default script is a clean falling-then-rising edge; the cases that
       need a stuck line override it after calling this. */
    ws_values[0] = 0; ws_values[1] = 1; ws_count = 2;
    i2s1.I2SCFGR = 0x0F0U; i2s1.I2SPR = 0x1234U;
    memset((void *)&st, 0, sizeof(st));
}

/* Success: WS is already low, then rises, and the receiver is enabled on it. */
static void case_arms_on_rising_edge(void)
{
    reset_log();
    ws_values[0] = 0; ws_values[1] = 1; ws_count = 2;
    assert(start_dma() == 0U);
    assert(seen("i2s_on"));
    /* Enabling the receiver is the last thing before interrupts come back. */
    assert(index_of("i2s_on") == (int)event_count - 2);
    assert(strcmp(events[event_count - 1], "set_primask") == 0);
    assert(primask_saved == 0x11U);
    /* The codec's I2S configuration has to survive I2S_Reset(). */
    assert(i2s1.I2SCFGR == 0x0F0U && i2s1.I2SPR == 0x1234U);
    /* The one-block discard is armed so a partially written block 0 is not
       handed to the frontend. */
    assert(st.discard == 1U && st.produced == 0U && st.consumed == 0U);
}

/* WS never falls: the wait is bounded and interrupts are never masked. */
static void case_ws_stuck_high_times_out(void)
{
    reset_log();
    ws_values[0] = 1; ws_count = 1;
    dwt_step = SystemCoreClock / 50U;
    assert(start_dma() == 4U);
    assert(!seen("i2s_on"));
    assert(irq_disable_count == 0U);
    assert(!seen("set_primask"));
}

/* WS falls but never rises: the critical section is entered, so its exit must
   still restore the interrupt state. */
static void case_ws_stuck_low_restores_interrupts(void)
{
    reset_log();
    ws_values[0] = 0; ws_count = 1;
    dwt_step = SystemCoreClock / 50U;
    assert(start_dma() == 4U);
    assert(irq_disable_count == 1U);
    assert(seen("set_primask") && primask_saved == 0x11U);
    assert(!seen("i2s_on"));
}

/* The ring: every descriptor points at the next, and the last closes the
   circle.  A tail of 0 would stop the engine after one pass. */
static void case_descriptors_form_a_ring(void)
{
    reset_log();
    assert(start_dma() == 0U);
    for (unsigned i = 0; i < KWS_AUDIO_BLOCKS; ++i) {
        assert(last_links[i].pNext == &last_links[(i + 1U) % KWS_AUDIO_BLOCKS]);
        assert(last_links[i].DstAddr == (uint32_t)(uintptr_t)st.ring[i]);
    }
    assert(last_links[KWS_AUDIO_BLOCKS - 1U].pNext == &last_links[0]);
    assert(last_init->pLinkListItem == &last_links[1]);
    assert(last_init->DstAddr == (uint32_t)(uintptr_t)st.ring[0]);
}

/* stop_dma's ordering is what makes a restart safe: the DMA request is
   silenced before the channel is disabled, and the pending flags are cleared
   last so a masked event cannot fire at the next enable. */
static void case_stop_orders_quiet_then_clear(void)
{
    reset_log();
    start_dma();
    const int off = index_of("i2s_dma_off");
    const int chan = index_of("channel_off");
    const int mask = index_of("events_off");
    const int clear = index_of("clear_status");
    assert(off >= 0 && chan >= 0 && mask >= 0 && clear >= 0);
    assert(off < chan && chan < mask && mask < clear);
}

/* The consumer honours the discard, then takes the next whole block. */
static void case_read_discards_the_first_block(void)
{
    reset_log();
    static int16_t dst[KWS_AUDIO_BLOCK_FRAMES];
    st.discard = 1U;
    st.produced = 1U; st.consumed = 0U;
    assert(kws_audio_read(dst, 1U) == 0U);         /* still one short */
    assert(st.consumed == 0U && st.discard == 1U);
    st.produced = 2U;
    assert(kws_audio_read(dst, 1U) == KWS_AUDIO_BLOCK_FRAMES);
    assert(st.consumed == 2U && st.discard == 0U && st.lost == 0U);
}

/* A consumer that fell behind must not be handed audio from hundreds of
   milliseconds ago as though it were contiguous: the ring drops the oldest
   blocks, reports the gap, and resumes from the oldest block it kept. */
static void case_read_resyncs_after_an_overrun(void)
{
    reset_log();
    static int16_t dst[KWS_AUDIO_BLOCK_FRAMES];
    st.discard = 0U;
    /* The ring holds 8; 13 are outstanding, so 6 are unreachable. */
    st.produced = KWS_AUDIO_BLOCKS + 5U; st.consumed = 0U;
    for (unsigned b = 0; b < KWS_AUDIO_BLOCKS; ++b)
        for (unsigned i = 0; i < KWS_AUDIO_BLOCK_FRAMES; ++i)
            st.ring[b][i * 2U] = (int16_t)b;
    assert(kws_audio_read(dst, 1U) == KWS_AUDIO_BLOCK_FRAMES);
    assert(st.lost == 6U && st.discontinuity == 1U);
    /* Resuming at block 6: the six oldest were dropped, and the read advances
       the consumer past the block it just took. */
    assert(dst[0] == 6 && dst[KWS_AUDIO_BLOCK_FRAMES - 1U] == 6);
    assert(st.consumed == 7U);
    assert(kws_audio_take_discontinuity() == 1);
    assert(kws_audio_take_discontinuity() == 0);
}

/* Left channel only: the codec's differential front end duplicates the signal
   across both slots, and reading the wrong one would shift every frame. */
static void case_read_takes_the_left_channel(void)
{
    reset_log();
    static int16_t dst[KWS_AUDIO_BLOCK_FRAMES];
    st.discard = 0U;
    st.produced = 1U; st.consumed = 0U;
    for (unsigned i = 0; i < KWS_AUDIO_BLOCK_FRAMES; ++i) {
        st.ring[0][i * 2U] = (int16_t)i;
        st.ring[0][i * 2U + 1U] = -1;
    }
    assert(kws_audio_read(dst, 1U) == KWS_AUDIO_BLOCK_FRAMES);
    for (unsigned i = 0; i < KWS_AUDIO_BLOCK_FRAMES; ++i) assert(dst[i] == (int16_t)i);
}

/* A DMA error stops the wait and is surfaced as a discontinuity, so the caller
   rebuilds its window rather than splicing across the failure. */
static void case_read_reports_a_dma_error(void)
{
    reset_log();
    static int16_t dst[KWS_AUDIO_BLOCK_FRAMES];
    st.discard = 0U;
    st.dma_error = 1U;
    assert(kws_audio_read(dst, 1U) == 0U);
    assert(kws_audio_dma_error == 1U && st.discontinuity == 1U);
}

/* A dry stream times out rather than blocking forever. */
static void case_read_times_out(void)
{
    reset_log();
    static int16_t dst[KWS_AUDIO_BLOCK_FRAMES];
    st.discard = 0U;
    dwt_step = SystemCoreClock / 1000U;   /* 1 ms per poll */
    assert(kws_audio_read(dst, 5U) == 0U);
    assert(st.consumed == 0U);
}
'''


class KwsAudioTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        folder = Path(cls.tmp.name)
        actual = (ROOT / "firmware/USER/src/kws_audio.c").read_text()

        # The state struct is sliced out of the firmware so the mock cannot
        # drift from the fields the ISR and the consumer actually touch.
        state = actual[actual.index("typedef struct {"):actual.index("} kws_audio_state_t;")
                 + len("} kws_audio_state_t;")] + ";"
        stop = "static void stop_dma(void)" + actual.split("static void stop_dma(void)", 1)[1] \
            .split("/* Arms the receiver", 1)[0]
        events = actual[actual.index("#define EVENTS"):]
        events = events[:events.index("\n")]
        arm = "static uint32_t start_dma(void)" + \
            actual.split("static uint32_t start_dma(void)", 1)[1].split("uint32_t kws_audio_init", 1)[0]
        consume = "static uint32_t blocks_ready(uint32_t *first)" + \
            actual.split("static uint32_t blocks_ready(uint32_t *first)", 1)[1] \
            .split("int kws_audio_take_discontinuity", 1)[0]
        # The tail of the file, so the flag-clearing contract is the real one.
        take = actual[actual.index("int kws_audio_take_discontinuity"):]

        for label, chunk in (("stop_dma", stop), ("start_dma", arm),
                             ("consumer", consume), ("discontinuity", take)):
            self_contained = chunk.count("{") > 0 and chunk.rstrip().endswith("}")
            assert self_contained, f"extraction of {label} from kws_audio.c looks wrong"

        source = folder / "kws_audio_harness.c"
        source.write_text(
            MOCK
            + state + "\n"
            + "static kws_audio_state_t st;\n"
            + "volatile uint32_t kws_audio_lost_blocks, kws_audio_dma_error;\n"
            + events + "\n"
            + stop + "\n" + arm + "\n" + consume + "\n" + take + "\n"
            + CASES
            + "int main(void) {\n"
            + "  case_arms_on_rising_edge();\n"
            + "  case_ws_stuck_high_times_out();\n"
            + "  case_ws_stuck_low_restores_interrupts();\n"

            + "  case_descriptors_form_a_ring();\n"
            + "  case_stop_orders_quiet_then_clear();\n"
            + "  case_read_discards_the_first_block();\n"
            + "  case_read_resyncs_after_an_overrun();\n"
            + "  case_read_takes_the_left_channel();\n"
            + "  case_read_reports_a_dma_error();\n"
            + "  case_read_times_out();\n"
            + "  return 0;\n}\n"
        )
        cls.binary = folder / "kws_audio_harness"
        result = subprocess.run([
            "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=undefined", "-fno-sanitize-recover=all",
            # The firmware narrows 32-bit-target addresses with (uint32_t)
            # casts; on a 64-bit host those truncate and warn.  See the
            # comment on DMA_LinkListItemType.
            "-Wno-pointer-to-int-cast",
            "-I", str(ROOT / "firmware/USER/inc"), str(source), "-o", str(cls.binary),
        ], capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)

    def test_arming_ring_and_consumer(self):
        result = subprocess.run([str(self.binary)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_the_ws_timeout_outlasts_several_ws_periods(self):
        """A wait shorter than one WS period would reject a healthy codec.

        I2S1 is a slave to the codec's free-running bit clock, so the WS wait
        is the only thing standing between a good signal and a spurious
        "codec not clocking" failure -- and its length is a bare divisor that
        nothing else in the firmware depends on.
        """
        source = (ROOT / "firmware/USER/src/kws_audio.c").read_text()
        match = re.search(r"const uint32_t timeout = SystemCoreClock / (\d+)U;", source)
        self.assertIsNotNone(match, "the WS wait is no longer a simple divisor")
        seconds = 1.0 / int(match.group(1))
        # WS toggles once per stereo sample, so its period is one sample time.
        periods = seconds * 16000
        self.assertGreaterEqual(periods, 8, "too short: a healthy codec could time out")
        self.assertLess(seconds, 0.1, "too long: a dead codec delays startup a tenth of a second")

    def test_the_isr_still_only_counts_and_clears(self):
        """The ISR must not grow work: it runs on the audio deadline.

        Everything expensive -- overrun recovery, restarts, cache maintenance
        -- belongs to the consumer, so a fault in the handler cannot cascade.
        """
        source = (ROOT / "firmware/USER/src/kws_audio.c").read_text()
        handler = source.split("void DMA1_Channel0_IRQHandler", 1)[1].split("static void stop_dma", 1)[0]
        for forbidden in ("I2S_", "DMA_ChannelCmd", "DMA_ChannelInit", "wm8978", "memcpy", "memset"):
            self.assertNotIn(forbidden, handler)
        self.assertIn("++st.produced;", handler)
        self.assertIn("__DMB();", handler)


if __name__ == "__main__":
    unittest.main()
