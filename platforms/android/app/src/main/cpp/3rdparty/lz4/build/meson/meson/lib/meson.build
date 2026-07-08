# #############################################################################
# Copyright (c) 2018-present        lzutao <taolzu(at)gmail.com>
# Copyright (c) 2022-present        Tristan Partin <tristan(at)partin.io>
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# #############################################################################

lz4_source_root = '../../../..'

sources = files(
  lz4_source_root / 'lib/lz4.c',
  lz4_source_root / 'lib/lz4frame.c',
  lz4_source_root / 'lib/lz4hc.c',
  lz4_source_root / 'lib/xxhash.c'
)

if get_option('unstable')
  sources += files(lz4_source_root / 'lib/lz4file.c')
endif

c_args = []

if host_machine.system() == 'windows' and get_option('default_library') != 'static'
  c_args += '-DLZ4_DLL_EXPORT=1'
endif

liblz4 = library(
  'lz4',
  sources,
  c_args: c_args,
  install: true,
  version: meson.project_version(),
  gnu_symbol_visibility: 'hidden'
)

liblz4_dep = declare_dependency(
  link_with: liblz4,
  compile_args: compile_args,
  include_directories: include_directories(lz4_source_root / 'lib')
)

meson.override_dependency('liblz4', liblz4_dep)

if get_option('tests') or get_option('programs') or get_option('examples') or get_option('ossfuzz')
  if get_option('default_library') == 'shared'
    liblz4_internal = static_library(
      'lz4-internal',
      objects: liblz4.extract_all_objects(recursive: true),
      gnu_symbol_visibility: 'hidden'
    )
  elif get_option('default_library') == 'static'
    liblz4_internal = liblz4
  elif get_option('default_library') == 'both'
    liblz4_internal = liblz4.get_static_lib()
  endif

  liblz4_internal_dep = declare_dependency(
    link_with: liblz4_internal,
    compile_args: compile_args,
    include_directories: include_directories(lz4_source_root / 'lib')
  )
endif

pkgconfig.generate(
  liblz4,
  name: 'lz4',
  filebase: 'liblz4',
  description: 'extremely fast lossless compression algorithm library',
  version: meson.project_version(),
  url: 'http://www.lz4.org/'
)

install_headers(
  lz4_source_root / 'lib/lz4.h',
  lz4_source_root / 'lib/lz4hc.h',
  lz4_source_root / 'lib/lz4frame.h'
)

if get_option('default_library') != 'shared'
  install_headers(lz4_source_root / 'lib/lz4frame_static.h')
  if get_option('unstable')
    install_headers(lz4_source_root / 'lib/lz4file.h')
  endif
endif
