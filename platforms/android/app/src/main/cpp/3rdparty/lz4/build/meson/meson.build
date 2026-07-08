# #############################################################################
# Copyright (c) 2018-present    lzutao <taolzu(at)gmail.com>
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# #############################################################################

# This is a dummy meson file.
# The intention is that it can be easily moved to the root of the project
# (together with meson_options.txt) and packaged for wrapdb.

project(
  'lz4',
  'c',
  license: 'BSD-2-Clause-Patent AND GPL-2.0-or-later',
  default_options: [
    'buildtype=release',
    'warning_level=3'
  ],
  version: run_command(
    find_program('GetLz4LibraryVersion.py'),
    '../../lib/lz4.h',
    check: true
  ).stdout().strip(),
  meson_version: '>=0.58.0'
)

subdir('meson')
