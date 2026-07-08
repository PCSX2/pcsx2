/*
 * Copyright © 2017 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#ifndef CUBEB_ASSERT
#define CUBEB_ASSERT

#include <stdio.h>
#include <stdlib.h>

/**
 * This allow using an external release assert method. This file should only
 * export a function or macro called XASSERT that aborts the program.
 */

#define XASSERT(expr)                                                          \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "%s:%d - fatal error: %s\n", __FILE__, __LINE__, #expr); \
      abort();                                                                 \
    }                                                                          \
  } while (0)

#endif
