# #############################################################################
# Copyright (c) 2018-present        lzutao <taolzu(at)gmail.com>
# Copyright (c) 2022-present        Tristan Partin <tristan(at)partin.io>
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# #############################################################################

cc = meson.get_compiler('c')

fs = import('fs')
pkgconfig = import('pkgconfig')

lz4_source_root = '../../..'

add_project_arguments('-DXXH_NAMESPACE=LZ4_', language: 'c')

if get_option('debug')
  add_project_arguments(cc.get_supported_arguments(
      '-Wcast-qual',
      '-Wcast-align',
      '-Wshadow',
      '-Wswitch-enum',
      '-Wdeclaration-after-statement',
      '-Wstrict-prototypes',
      '-Wundef',
      '-Wpointer-arith',
      '-Wstrict-aliasing=1',
      '-DLZ4_DEBUG=@0@'.format(get_option('debug-level'))
    ),
    language: 'c'
  )
endif

compile_args = []

if not get_option('align-test')
  add_project_arguments('-DLZ4_ALIGN_TEST=0', language: 'c')
endif

if get_option('disable-memory-allocation')
  if get_option('default_library') != 'static'
    error('Memory allocation can only be disabled in static builds')
  endif

  add_project_arguments('-DLZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION')
  compile_args += '-DLZ4_STATIC_LINKING_ONLY_DISABLE_MEMORY_ALLOCATION'
endif

add_project_arguments(
  '-DLZ4_DISTANCE_MAX=@0@'.format(get_option('distance-max')),
  language: 'c'
)
compile_args += '-DLZ4_DISTANCE_MAX=@0@'.format(get_option('distance-max'))

if not get_option('fast-dec-loop').auto()
  add_project_arguments(
    '-DLZ4_FAST_DEC_LOOP=@0@'.format(
      get_option('fast-dec-loop').enabled() ? 1 : 0
    ),
    language: 'c'
  )
endif

if get_option('force-sw-bitcount')
  add_project_arguments('-DLZ4_FORCE_SW_BITCOUNT', language: 'c')
endif

if get_option('freestanding')
  add_project_arguments('-DLZ4_FREESTANDING=1', language: 'c')
  compile_args += '-DLZ4_FREESTANDING=1'
endif

if get_option('memory-usage') > 0
  add_project_arguments(
    '-DLZ4_MEMORY_USAGE=@0@'.format(get_option('memory-usage')),
    language: 'c'
  )
  compile_args += '-DLZ4_MEMORY_USAGE=@0@'.format(get_option('memory-usage'))
endif

if get_option('endianness-independent-output')
  if get_option('default_library') != 'static'
    error('Endianness-independent output can only be enabled in static builds')
  endif

  add_project_arguments('-DLZ4_STATIC_LINKING_ONLY_ENDIANNESS_INDEPENDENT_OUTPUT')
  compile_args += '-DLZ4_STATIC_LINKING_ONLY_ENDIANNESS_INDEPENDENT_OUTPUT'
endif

if get_option('unstable')
  add_project_arguments('-DLZ4_STATIC_LINKING_ONLY', language: 'c')
  compile_args += '-DLZ4_STATIC_LINKING_ONLY'
  if get_option('default_library') != 'static'
    add_project_arguments('-DLZ4_PUBLISH_STATIC_FUNCTIONS', language: 'c')
    compile_args += '-DLZ4_PUBLISH_STATIC_FUNCTIONS'

    add_project_arguments('-DLZ4F_PUBLISH_STATIC_FUNCTIONS', language: 'c')
    compile_args += '-DLZ4F_PUBLISH_STATIC_FUNCTIONS'
  endif
endif

if get_option('user-memory-functions')
  add_project_arguments('-DLZ4_USER_MEMORY_FUNCTIONS', language: 'c')
endif

run_env = environment()

subdir('lib')

if get_option('programs')
  subdir('programs')
else
  lz4 = disabler()
  lz4cat = disabler()
  unlz4 = disabler()
endif

if get_option('tests')
  subdir('tests')
endif

if get_option('contrib')
  subdir('contrib')
endif

if get_option('examples')
  subdir('examples')
endif

if get_option('ossfuzz')
  subdir('ossfuzz')
endif
