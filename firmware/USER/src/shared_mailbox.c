/*
 * Copyright (c) 2025 Nations Technologies Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "m4_shared.h"

/* The shared mailbox lives at a fixed 0x30000000 in AHB SRAM and is mapped
   Normal non-cacheable by m7_cache.c region 0, so a debugger (and, formerly,
   the M4) sees writes as soon as the __DMB() before them retires.

   Only the OpenOCD flash handshake uses this struct now.  It is defined here
   rather than in a header so the linker script can KEEP it in .m4_shared and
   the ABI stays pinned by the static_asserts in m4_shared.h. */
volatile m4_shared_t g_m4_shared
    __attribute__((section(".m4_shared"), aligned(32)));
