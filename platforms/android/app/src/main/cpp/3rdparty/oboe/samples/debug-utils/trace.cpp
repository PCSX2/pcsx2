/*
 * Copyright 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <dlfcn.h>
#include "logging_macros.h"
#include <cstdio>
#include "trace.h"

static const int TRACE_MAX_SECTION_NAME_LENGTH = 100;

// Tracing functions
static void *(*ATrace_beginSection)(const char *sectionName);

static void *(*ATrace_endSection)(void);

static bool *(*ATrace_isEnabled)(void);

typedef void *(*fp_ATrace_beginSection)(const char *sectionName);

typedef void *(*fp_ATrace_endSection)(void);

typedef bool *(*fp_ATrace_isEnabled)(void);

bool Trace::is_enabled_ = false;
bool Trace::has_error_been_shown_ = false;

void Trace::beginSection(const char *fmt, ...) {

  if (is_enabled_) {
    static char buff[TRACE_MAX_SECTION_NAME_LENGTH];
    va_list args;
    va_start(args, fmt);
    vsprintf(buff, fmt, args);
    va_end(args);
    ATrace_beginSection(buff);
  } else if (!has_error_been_shown_) {
    LOGE("Tracing is either not initialized (call Trace::initialize()) "
         "or not supported on this device");
    has_error_been_shown_ = true;
  }
}

void Trace::endSection() {

  if (is_enabled_) {
    ATrace_endSection();
  }
}

void Trace::initialize() {

  // Using dlsym allows us to use tracing on API 21+ without needing android/trace.h which wasn't
  // published until API 23
  void *lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
  if (lib == nullptr) {
    LOGE("Could not open libandroid.so to dynamically load tracing symbols");
  } else {
    ATrace_beginSection =
        reinterpret_cast<fp_ATrace_beginSection >(
            dlsym(lib, "ATrace_beginSection"));
    ATrace_endSection =
        reinterpret_cast<fp_ATrace_endSection >(
            dlsym(lib, "ATrace_endSection"));
    ATrace_isEnabled =
        reinterpret_cast<fp_ATrace_isEnabled >(
            dlsym(lib, "ATrace_isEnabled"));

    if (ATrace_isEnabled != nullptr && ATrace_isEnabled()) {
      is_enabled_ = true;
    }
  }
}
