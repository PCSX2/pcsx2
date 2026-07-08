/*
    lorem.h - lorem ipsum generator
    Copyright (c) Yann Collet. All rights reserved.

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


#include <stddef.h>   /* size_t */

/*
 * LOREM_genBuffer():
 * Generate @size bytes of compressible data using lorem ipsum generator
 * into provided @buffer.
 */
void LOREM_genBuffer(void* buffer, size_t size, unsigned seed);

/*
 * LOREM_genBlock():
 * Similar to LOREM_genBuffer, with additional controls :
 * - @first : generate the first sentence
 * - @fill : fill the entire @buffer,
 *           if ==0: generate one paragraph at most.
 * @return : nb of bytes generated into @buffer.
 */
size_t LOREM_genBlock(void* buffer, size_t size,
                      unsigned seed,
                      int first, int fill);
