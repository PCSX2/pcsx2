/*
 * wrpng-8.c
 *
 * Copyright (C) 2026, D. R. Commander.
 * For conditions of distribution and use, see the accompanying README.ijg
 * file.
 *
 * This file is a wrapper for compiling wrpng.c to support 2 to 8 bits of
 * data precision.  wrpng.c should not be compiled directly.
 */

#define BITS_IN_JSAMPLE  8

#include "../wrpng.c"
