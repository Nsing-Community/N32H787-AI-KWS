/*
 * Copyright (c) 2025 Nations Technologies Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef M4_SHARED_H
#define M4_SHARED_H

#include <stdint.h>
#include <stddef.h>

#define M4_BOOT_ADDRESS 0x15080000UL
#define M4_SHARED_MAGIC 0x4d344951UL
#define M7_FLASH_ABI 0x4d374331UL
#define M7_BOOT_REQUEST_MAGIC 0x4d374254UL
#define M4_SHARED_WORDS 64U
#define INFERENCE_WIDTH 160U
#define INFERENCE_HEIGHT 120U
#define INFERENCE_FRAME_BYTES (INFERENCE_WIDTH * INFERENCE_HEIGHT)

/* Words in audio_diag[].  The audio probe that filled this array is retired;
   the field is kept because removing it would move every offset after it. */
#define AUDIO_DIAG_WORDS 40U

/* Legacy inter-core mailbox, retained for its OpenOCD ABI rather than for its
   contents.  The M4 core, the camera and person detection are gone, so the
   snapshot, test-pattern and audio-probe fields below are vestigial: they
   exist only so flash_pause/m7_flash_abi/boot_request keep the byte offsets
   that firmware/openocd/n32h7x_cmsisdap.tcl writes to by absolute address.
   Do not reorder, resize or delete fields here without updating that script
   and the static_asserts at the bottom of this file. */
typedef struct {
    uint32_t magic;
    uint32_t clock_hz;
    uint32_t ready;
    uint32_t heartbeat;
    uint32_t request;
    uint32_t response;
    uint32_t fault;
    uint32_t cfsr;
    uint32_t cpuid;
    uint32_t vtor;
    uint32_t input[M4_SHARED_WORDS];
    uint32_t output[M4_SHARED_WORDS];
    uint32_t boot_status;
    uint32_t boot_checks;
    uint32_t boot_errors;
    uint32_t person_init;
    uint32_t person_control; /* bit0 enabled, upper bits toggle generation */
    uint32_t snapshot_request; /* M7 owns request/released; M4 owns ready/data */
    uint32_t snapshot_ready;
    uint32_t snapshot_released;
    uint32_t snapshot_frame;
    uint32_t snapshot_tick;
    uint32_t copy_cycles;
    uint32_t result_version; /* odd while M7 writes, even when published */
    uint32_t result_control;
    uint32_t result_frame;
    uint32_t result_tick;
    uint32_t result_scores;
    uint32_t result_flags;
    uint32_t person_diag[15];
    uint32_t flash_pause;
    uint32_t flash_paused;
    uint32_t m7_flash_paused;
    uint32_t m7_flash_abi;
    uint32_t m7_clock_hz;
    uint32_t m7_cache_ccr;
    uint32_t result_copy_us;
    uint32_t result_preprocess_us;
    uint32_t result_invoke_us;
    uint32_t sdram_status; /* M7 startup self-test; 0=ready for audio buffer. */
    uint32_t audio_version; /* M7 writer seqlock; M4 reads for serial W. */
    uint32_t audio_diag[AUDIO_DIAG_WORDS];
    uint32_t audio_request; /* M4 request, M7 response transfers buffer ownership. */
    uint32_t audio_response;
    uint32_t audio_status;
    uint32_t audio_address;
    uint32_t audio_words;
    uint32_t audio_elapsed_us;
    uint32_t audio_overruns;
    uint32_t audio_crc32;
    uint32_t audio_live_request; /* M4 asks M7 to release I2S1 for live DMA. */
    uint32_t audio_live_ack;
    uint32_t boot_request; /* Debugger -> application request to re-enter USB boot. */
} m4_shared_t;

/* OpenOCD uses these two fixed offsets before reusing the AXI DMA workspace. */
#ifdef __cplusplus
static_assert(offsetof(m4_shared_t, flash_pause) == 680, "Flash handshake ABI");
static_assert(offsetof(m4_shared_t, flash_paused) == 684, "Flash handshake ABI");
static_assert(offsetof(m4_shared_t, m7_flash_paused) == 688, "M7 flash ABI");
static_assert(offsetof(m4_shared_t, m7_flash_abi) == 692, "M7 flash ABI");
static_assert(offsetof(m4_shared_t, boot_request) == 924, "Boot request ABI");
static_assert(sizeof(m4_shared_t) <= 1024, "Shared RAM overflow");
#else
_Static_assert(offsetof(m4_shared_t, flash_pause) == 680, "Flash handshake ABI");
_Static_assert(offsetof(m4_shared_t, flash_paused) == 684, "Flash handshake ABI");
_Static_assert(offsetof(m4_shared_t, m7_flash_paused) == 688, "M7 flash ABI");
_Static_assert(offsetof(m4_shared_t, m7_flash_abi) == 692, "M7 flash ABI");
_Static_assert(offsetof(m4_shared_t, boot_request) == 924, "Boot request ABI");
_Static_assert(sizeof(m4_shared_t) <= 1024, "Shared RAM overflow");
#endif

extern volatile m4_shared_t g_m4_shared;

#endif
