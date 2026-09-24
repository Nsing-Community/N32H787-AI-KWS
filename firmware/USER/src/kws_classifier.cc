/*
 * Copyright (c) 2025 Nations Technologies Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kws_classifier.h"

#include "kws_model_data.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace {

/* Peak live activation is two 25x5x64 float32 tensors (the depthwise block's
   input and output) at 32 KB each, plus TFLM's per-op scratch and allocator
   bookkeeping.  128 KiB leaves comfortable headroom; kws_arena_bytes reports
   the real figure so this can be tightened once measured on hardware. */
constexpr size_t kTensorArenaBytes = 128 * 1024;

/* Both of these live in DTCM, which the startup code zeroes before
   __libc_init_array, so the C++ members are constructed into zeroed memory.
   DTCM is never cached, so the interpreter needs no cache maintenance either
   before or after a debugger halts the core mid-inference. */
alignas(16) __attribute__((section(".kws_arena"))) uint8_t tensor_arena[kTensorArenaBytes];

/* DWT cycle counter, enabled by CPU_DELAY_INTI() in main().  Read as a raw
   address rather than through the vendor macros so this translation unit
   needs no C device header -- the same accessor person_detector.cc used. */
uint32_t Cycles() { return *reinterpret_cast<volatile uint32_t *>(0xE0001004UL); }

class Classifier {
 public:
  Classifier()
      : model_(tflite::GetModel(g_kws_model_data)),
        interpreter_(model_, resolver_, tensor_arena, sizeof(tensor_arena)) {
    /* Exactly the six operators this graph uses.  Parsing the flatbuffer
       gives: RESHAPE, then five DEPTHWISE_CONV_2D (the first is a 10x4 conv
       over a single input channel, which the converter expresses as a
       depthwise op with depth_multiplier 64) interleaved with four CONV_2D
       pointwise ops, an AVERAGE_POOL_2D, a FULLY_CONNECTED and a SOFTMAX.
       Adding more would only cost Flash -- but note the archive is a
       CMSIS-NN build and this model is float32, so these resolve to the
       generic reference kernels, not the optimised int8 ones. */
    resolver_.AddReshape();
    resolver_.AddDepthwiseConv2D();
    resolver_.AddConv2D();
    resolver_.AddAveragePool2D();
    resolver_.AddFullyConnected();
    resolver_.AddSoftmax();
  }

  bool Init() {
    init_error_ = 0;
    if (model_ == nullptr || model_->version() != TFLITE_SCHEMA_VERSION) {
      init_error_ = 1;
      return false;
    }
    if (interpreter_.AllocateTensors() != kTfLiteOk) {
      init_error_ = 2;
      return false;
    }
    kws_arena_bytes = interpreter_.arena_used_bytes();
    input_ = interpreter_.input(0);
    output_ = interpreter_.output(0);
    if (input_ == nullptr || output_ == nullptr) {
      init_error_ = 3;
      return false;
    }
    /* A quantised model here would be a silent 4x-capacity mistake: every
       score would be garbage but the interpreter would still run. */
    if (input_->type != kTfLiteFloat32) {
      init_error_ = 4;
      return false;
    }
    if (input_->dims->size != 2 || input_->dims->data[0] != 1 ||
        input_->dims->data[1] != (int)KWS_INPUT_FEATURES) {
      init_error_ = 5;
      return false;
    }
    if (output_->type != kTfLiteFloat32 || output_->dims->size != 2 ||
        output_->dims->data[0] != 1 ||
        output_->dims->data[1] != (int)KWS_NUM_CLASSES) {
      init_error_ = 6;
      return false;
    }
    return true;
  }

  int InitError() const { return init_error_; }
  int InputType() const { return input_ ? input_->type : -1; }
  int OutputType() const { return output_ ? output_->type : -1; }

  bool Run(const float *features, float *scores) {
    if (input_ == nullptr || output_ == nullptr) return false;
    const uint32_t start = Cycles();
    for (size_t i = 0; i < KWS_INPUT_FEATURES; ++i) {
      input_->data.f[i] = features[i];
    }
    if (interpreter_.Invoke() != kTfLiteOk) return false;
    kws_invoke_cycles = Cycles() - start;
    for (size_t i = 0; i < KWS_NUM_CLASSES; ++i) {
      scores[i] = output_->data.f[i];
    }
    return true;
  }

 private:
  const tflite::Model *model_;
  tflite::MicroMutableOpResolver<6> resolver_;
  tflite::MicroInterpreter interpreter_;
  TfLiteTensor *input_ = nullptr;
  TfLiteTensor *output_ = nullptr;
  int init_error_ = 0;
};

alignas(16) __attribute__((section(".kws_state"))) Classifier g_classifier;
bool g_initialized = false;

}  // namespace

extern "C" {
const char *const kws_class_labels[KWS_NUM_CLASSES] = {
    "silence", "unknown", "yes", "no",   "up",   "down",
    "left",    "right",   "on",  "off",  "stop", "go"};
volatile int kws_classifier_init_error = -1;
volatile int kws_classifier_input_type = -1;
volatile int kws_classifier_output_type = -1;
volatile uint32_t kws_arena_bytes = 0;
volatile uint32_t kws_invoke_cycles = 0;
}

extern "C" int kws_classifier_init(void) {
  g_initialized = g_classifier.Init();
  kws_classifier_init_error = g_initialized ? 0 : g_classifier.InitError();
  kws_classifier_input_type = g_classifier.InputType();
  kws_classifier_output_type = g_classifier.OutputType();
  return g_initialized ? 1 : 0;
}

extern "C" int kws_classifier_run(const float *features, float *scores) {
  if (!g_initialized || features == nullptr || scores == nullptr) return 0;
  return g_classifier.Run(features, scores) ? 1 : 0;
}
