/*
 * Copyright (c) 2025 Nations Technologies Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KWS_MFCC_H
#define KWS_MFCC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MFCC front end for the DS-CNN-S keyword spotter.

   This is a port of Arm's reference implementation from ML-KWS-for-MCU
   (Deployment/Source/MFCC/mfcc.cpp), which is in turn documented as "MFCC
   feature extraction to match with TensorFlow MFCC Op".  The numbers below
   are the ones the pretrained model was trained with; see
   docs/KWS_DEPLOYMENT.md for the line-by-line comparison against the
   TensorFlow kernels. */

#define KWS_MFCC_SAMP_FREQ   16000U  /* Hz */
#define KWS_MFCC_FRAME_LEN   640U    /* 40 ms window */
#define KWS_MFCC_FRAME_SHIFT 320U    /* 20 ms stride */
#define KWS_MFCC_FFT_LEN     1024U   /* 640 rounded up to a power of two */
#define KWS_MFCC_NUM_BINS    40U     /* mel filters, 20 Hz .. 4000 Hz */
#define KWS_MFCC_NUM_COEFFS  10U     /* DCT-II coefficients kept */
#define KWS_MFCC_MEL_LOW_HZ  20U
#define KWS_MFCC_MEL_HIGH_HZ 4000U

/* Fills the window, mel filterbank and DCT tables.  No hardware access; call
   before kws_mfcc_compute(). */
void kws_mfcc_init(void);

/* Computes one 10-coefficient MFCC frame from KWS_MFCC_FRAME_LEN samples of
   signed 16-bit PCM.  The output is the raw float feature the model expects --
   the reference implementation's q7 conversion belongs to the quantised
   models and is deliberately not applied here. */
void kws_mfcc_compute(const int16_t *frame, float out[KWS_MFCC_NUM_COEFFS]);

/* Non-zero if kws_mfcc_init() rejected its own tables.  Only the sparse
   filterbank span can fail, and only if the constants above are edited. */
extern volatile uint32_t kws_mfcc_init_error;

#ifdef __cplusplus
}
#endif

#endif /* KWS_MFCC_H */
