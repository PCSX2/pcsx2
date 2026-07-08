/*
  LZ4conf.h - compile-time parameters
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
  - LZ4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/

#ifndef LZ4CONF_H_32432
#define LZ4CONF_H_32432


/* Default compression level.
 * Can be overridden by environment variable LZ4_CLEVEL.
 * Is overridden at runtime by command -# */
#ifndef LZ4_CLEVEL_DEFAULT
# define LZ4_CLEVEL_DEFAULT 1
#endif

/* Determines if multithreading is enabled or not
 * Default: enabled on Windows, disabled on other platforms */
#ifndef LZ4IO_MULTITHREAD
# ifdef _WIN32
    /* Windows supports Completion Ports */
#   define LZ4IO_MULTITHREAD 1
# else
    /* Requires <pthread> support.
     * Can't be reliably and portably tested at source code level
     * so must be set a build level */
#   define LZ4IO_MULTITHREAD 0
# endif
#endif

/* Determines default nb of threads for compression
 * Default value is 0, which means "auto" :
 * nb of threads is determined from detected local cpu.
 * Can be overridden by Environment Variable LZ4_NBWORKERS.
 * Can be overridden at runtime using -T# command */
#ifndef LZ4_NBWORKERS_DEFAULT
# define LZ4_NBWORKERS_DEFAULT 0
#endif

/* Maximum nb of compression threads selectable at runtime */
#ifndef LZ4_NBWORKERS_MAX
# define LZ4_NBWORKERS_MAX 200
#endif

/* Determines default lz4 block size when none provided.
 * Default value is 7, which represents 4 MB.
 * Can be overridden at runtime using -B# command */
#ifndef LZ4_BLOCKSIZEID_DEFAULT
# define LZ4_BLOCKSIZEID_DEFAULT 7
#endif


#endif  /* LZ4CONF_H_32432 */
