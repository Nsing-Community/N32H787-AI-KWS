/*
 * Copyright (c) 2025 Nations Technologies Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstddef>

/* Built by the Makefile from models/DS_CNN_S.tflite.  The array is placed in
   .kws_model, which the linker script locates in DTCM with a Flash load copy;
   the reset code moves it before TFLM's constructors run. */
extern const unsigned char g_kws_model_data[];
extern const std::size_t g_kws_model_data_len;
