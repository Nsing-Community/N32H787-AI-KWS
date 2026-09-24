# Copyright (c) 2025 Nations Technologies Inc.
# SPDX-License-Identifier: Apache-2.0

"""Exercise the real DMA ISR: event selectors differ from channel bit masks."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class StreamIRQTests(unittest.TestCase):
    def test_block_and_error_events(self):
        source = (ROOT / "firmware/USER/src/kws_audio.c").read_text()
        handler = source.split("void DMA1_Channel0_IRQHandler", 1)[1].split("static void stop_dma", 1)[0]
        mock = r'''
#include <assert.h>
#include <stdint.h>
#define DMA1 1
#define DMA_CHANNEL_0 0
#define DMA_CH_EVENT_BLOCK_TRANSFER_COMPLETE 2U
#define DMA_CH_EVENT_ERROR 16U
/* These similarly named macros are channel bit masks, NOT API selectors. */
#define DMA_EVENT_BLOCK_TRANSFER_COMPLETE 1U
#define DMA_EVENT_ERROR 1U
#define __DMB() ((void)0)
/* Only the fields the ISR touches are modelled; the real struct's ring and
   link list are never read from here. */
typedef struct { volatile uint32_t produced, dma_error; } kws_audio_state_t;
static kws_audio_state_t st;
static uint32_t pending, cleared;
static int DMA_GetChannelIntErrStatus(int d, int c)
{assert(d == 1 && c == 0); return !!(pending & 16);}
static int DMA_GetChannelIntBlockStatus(int d, int c)
{assert(d == 1 && c == 0); return !!(pending & 2);}
static void DMA_ClearChannelEventStatus(int d, int c, uint32_t events)
{assert(d == 1 && c == 0); cleared |= events; pending &= ~events;}
'''
        cases = r'''
int main(void) {
    DMA1_Channel0_IRQHandler(); assert(st.produced == 0 && cleared == 0);
    pending = 2; DMA1_Channel0_IRQHandler();
    assert(st.produced == 1 && pending == 0 && cleared == 2 && !st.dma_error);
    DMA1_Channel0_IRQHandler(); assert(st.produced == 1);
    pending = 18; DMA1_Channel0_IRQHandler();
    assert(st.produced == 2 && st.dma_error == 1 && pending == 0 && cleared == 18);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "irq.c"
            path.write_text(mock + "void DMA1_Channel0_IRQHandler" + handler + cases)
            binary = Path(tmp) / "irq"
            subprocess.run(["cc", "-Wall", "-Wextra", "-Werror", str(path), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
