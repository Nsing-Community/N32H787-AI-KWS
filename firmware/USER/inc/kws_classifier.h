/*
 * Copyright (c) 2025 Nations Technologies Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KWS_CLASSIFIER_H
#define KWS_CLASSIFIER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DS-CNN-S keyword spotter, float32 TFLite model from
   models/DS_CNN_S.tflite.  The graph takes the raw (unquantised) MFCC
   coefficients kws_mfcc_compute() produces, reshapes them to a 49 x 10
   single-channel image, and emits a 12-way softmax. */

#define KWS_INPUT_FRAMES   49U
#define KWS_INPUT_COEFFS   10U
#define KWS_INPUT_FEATURES (KWS_INPUT_FRAMES * KWS_INPUT_COEFFS)
#define KWS_NUM_CLASSES    12U

/* Class names in the model's own output order, so index 0 is silence and
   index 1 is the catch-all unknown bucket.  The training script builds this
   list as [silence, unknown] + the ten target words. */
extern const char *const kws_class_labels[KWS_NUM_CLASSES];

/* Builds the interpreter and allocates the tensor arena.  Returns 1 on
   success.  Validation failures are reported through
   kws_classifier_init_error rather than aborting, so a debugger can read
   which check failed. */
int kws_classifier_init(void);

/* Runs one inference over KWS_INPUT_FEATURES coefficients laid out frame
   major (frame 0's ten coefficients, then frame 1's, ...), which is the
   order the reference implementation feeds its sliding window in.  Writes
   KWS_NUM_CLASSES softmax scores summing to 1.  Returns 1 on success. */
int kws_classifier_run(const float *features, float *scores);

/* 0 when init succeeded; 1 model version, 2 AllocateTensors, 3 null tensor,
   4 input type, 5 input shape, 6 output type/shape. */
extern volatile int kws_classifier_init_error;
extern volatile int kws_classifier_input_type;
extern volatile int kws_classifier_output_type;
/* Reported by arena_used_bytes(); compare against the arena size to see how
   much of the 128 KiB the graph actually needs. */
extern volatile uint32_t kws_arena_bytes;
extern volatile uint32_t kws_invoke_cycles;

#ifdef __cplusplus
}
#endif

#endif /* KWS_CLASSIFIER_H */
