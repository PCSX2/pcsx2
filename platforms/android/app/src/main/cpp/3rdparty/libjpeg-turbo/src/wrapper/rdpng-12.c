/*
 * rdpng-12.c
 *
 * Copyright (C) 2026, D. R. Commander.
 * For conditions of distribution and use, see the accompanying README.ijg
 * file.
 *
 * This file is a wrapper for compiling rdpng.c to support 9 to 12 bits of
 * data precision.  rdpng.c should not be compiled directly.
 */

#define BITS_IN_JSAMPLE  12

#include "../rdpng.c"
