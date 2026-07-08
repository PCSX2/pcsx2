/*
    lorem.c - lorem ipsum generator to stdout
    Copyright (C) Yann Collet 2024

    GPL v2 License

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    You can contact the author at :
   - LZ4 source repository : https://github.com/lz4/lz4
   - Public forum : https://groups.google.com/forum/#!forum/lz4c
*/

/* Implementation notes:
 * Generates a stream of Lorem ipsum paragraphs to stdout,
 * up to the requested size, which can be very large (> 4 GB).
 * Note that, beyond 1 paragraph, this generator produces
 * a different content than LOREM_genBuffer (even when using same seed).
 */

#include "platform.h"  /* Compiler options, SET_BINARY_MODE */
#include "loremOut.h"
#include "lorem.h"     /* LOREM_genBlock */
#include <stdio.h>
#include <assert.h>


#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define LOREM_BLOCKSIZE (2 << 10)
void LOREM_genOut(unsigned long long size, unsigned seed)
{
  char buff[LOREM_BLOCKSIZE] = {0};
  unsigned long long total = 0;
  size_t genBlockSize = (size_t)MIN(size, LOREM_BLOCKSIZE);

  /* init */
  SET_BINARY_MODE(stdout);

  /* Generate Ipsum text, one paragraph at a time */
  while (total < size) {
    size_t generated = LOREM_genBlock(buff, genBlockSize, seed++, total == 0, 0);
    assert(generated <= genBlockSize);
    total += generated;
    assert(total <= size);
    fwrite(buff, 1, generated, stdout); /* note: should check potential write error */
    if (size - total < genBlockSize)
      genBlockSize = (size_t)(size - total);
  }
  assert(total == size);
}
