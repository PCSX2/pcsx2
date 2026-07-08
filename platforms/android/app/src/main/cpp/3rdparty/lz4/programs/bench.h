/*
    bench.h - Demo program to benchmark open-source compression algorithm
    Copyright (c) Yann Collet. All rights reserved.

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
    - LZ4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/
#ifndef BENCH_H_125623623633
#define BENCH_H_125623623633

#include <stddef.h>

/* BMK_benchFiles() :
 * Benchmark all files provided through array @fileNamesTable.
 * All files must be valid, otherwise benchmark fails.
 * Roundtrip measurements are done for each file individually, but
 * unless BMK_setBenchSeparately() is set, all results are agglomerated.
 * The method benchmarks all compression levels from @cLevelStart to @cLevelLast,
 * both inclusive, providing one result per compression level.
 * If @cLevelLast <= @cLevelStart, BMK_benchFiles() benchmarks @cLevelStart only.
 * @dictFileName is optional, it's possible to provide NULL.
 * When provided, compression and decompression use the specified file as dictionary.
 * Only one dictionary can be provided, in which case it's applied to all benchmarked files.
**/
int BMK_benchFiles(const char** fileNamesTable, unsigned nbFiles,
                   int cLevelStart, int cLevelLast,
                   const char* dictFileName);

/* Set Parameters */
void BMK_setNbSeconds(unsigned nbSeconds);  /* minimum benchmark duration, in seconds, for both compression and decompression */
void BMK_setBlockSize(size_t blockSize);    /* Internally cut input file(s) into independent blocks of specified size */
void BMK_setNotificationLevel(unsigned level);  /* Influence verbosity level */
void BMK_setBenchSeparately(int separate);  /* When providing multiple files, output one result per file */
void BMK_setDecodeOnlyMode(int set);        /* v1.9.4+: set benchmark mode to decode only */
void BMK_skipChecksums(int skip);           /* v1.9.4+: only useful for DecodeOnlyMode; do not calculate checksum when present, to save CPU time */

void BMK_setAdditionalParam(int additionalParam); /* hidden param, influence output format, for python parsing */

#endif   /* BENCH_H_125623623633 */
