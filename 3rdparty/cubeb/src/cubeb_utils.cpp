/*
 * Copyright © 2018 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#include "cubeb_utils.h"

size_t
cubeb_sample_size(cubeb_sample_format format)
{
  switch (format) {
  case CUBEB_SAMPLE_S16LE:
  case CUBEB_SAMPLE_S16BE:
    return sizeof(int16_t);
  case CUBEB_SAMPLE_FLOAT32LE:
  case CUBEB_SAMPLE_FLOAT32BE:
    return sizeof(float);
  default:
    // should never happen as all cases are handled above.
    assert(false);
    return 0;
  }
}
