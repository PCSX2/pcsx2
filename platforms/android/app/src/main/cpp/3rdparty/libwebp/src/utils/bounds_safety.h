// Copyright 2025 Google LLC
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// Adds compatibility / portability macros to support usage of -fbounds-safety

#ifndef WEBP_UTILS_BOUNDS_SAFETY_H_
#define WEBP_UTILS_BOUNDS_SAFETY_H_

#ifdef WEBP_SUPPORT_FBOUNDS_SAFETY

#include <ptrcheck.h>
//
// There's some inherent complexity here due to the way -fbounds-safety works.
// Some annotations (notably __indexable and __bidi_indexable) change the ABI
// of the function or struct, so we don't want those annotations to silently
// disappear if they're expected.
//
// In ptrcheck.h provided by the compiler, ABI changing annotations do not
// "vanish" under any build configuration. This is intentional. Consider the
// following example:
//
// == Safe.h, where Safe.c is always compiled with -fbounds-safety ==
// Forward declare my_function, implemented in Safe.c
// void my_function(char *__bidi_indexable ptr);
//
// If we have a project that does not use -fbounds-safety, and we want to call
// my_function that was pre-built with -fbounds-safety, this annotation cannot
// vanish or there'll be an ABI mismatch, which may fail to compile or have
// worse behaviors at runtime.
//
// TODO: https://issues.webmproject.org/432511225 - In the future, we should
// have CMake append to a header file (like this one) that libwebp was built
// with -fbounds-safety, so that we know to never make annotations vanish.

// The annotations below are ABI breaking as they turn normal pointers into
// "wide" pointers. Breaking them down:
// * __indexable is akin to { ptr_curr, ptr_end }, and can only be
//   forward-indexed.
// * __bidi_indexable (bidirectional indexable) is
//   { ptr_begin, ptr_curr, ptr_end }
//   and can be both forward and backward indexed.
// See https://clang.llvm.org/docs/BoundsSafety.html for more comprehensive
// documentation
#define WEBP_INDEXABLE __indexable
#define WEBP_BIDI_INDEXABLE __bidi_indexable

#else  // WEBP_SUPPORT_FBOUNDS_SAFETY

#define WEBP_INDEXABLE
#define WEBP_BIDI_INDEXABLE

#endif  // WEBP_SUPPORT_FBOUNDS_SAFETY

#endif  // WEBP_UTILS_BOUNDS_SAFETY_H_
