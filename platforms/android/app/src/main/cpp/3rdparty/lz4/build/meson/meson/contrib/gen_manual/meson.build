# #############################################################################
# Copyright (c) 2018-present        lzutao <taolzu(at)gmail.com>
# Copyright (c) 2022-present        Tristan Partin <tristan(at)partin.io>
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# #############################################################################

lz4_source_root = '../../../../..'

add_languages('cpp', native: true)

sources = files(
  lz4_source_root / 'contrib/gen_manual/gen_manual.cpp'
)

gen_manual = executable(
  'gen_manual',
  sources,
  native: true,
  install: false
)

manual_pages = ['lz4', 'lz4frame']

foreach mp : manual_pages
  custom_target(
    '@0@_manual.html'.format(mp),
    build_by_default: true,
    input: lz4_source_root / 'lib/@0@.h'.format(mp),
    output: '@0@_manual.html'.format(mp),
    command: [
      gen_manual,
      meson.project_version(),
      '@INPUT@',
      '@OUTPUT@',
    ],
    install: false
  )
endforeach
