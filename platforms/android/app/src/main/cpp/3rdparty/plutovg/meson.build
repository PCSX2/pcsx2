project('plutovg', 'c',
    version: '1.3.2',
    license: 'MIT',
    meson_version: '>=1.3.0',
    default_options: ['c_std=gnu11']
)

plutovg_deps = []
plutovg_compile_args = []

cc = meson.get_compiler('c')

plutovg_c_args = ['-DPLUTOVG_BUILD']
plutovg_c_args += cc.get_supported_arguments(
    '-Wno-sign-compare',
    '-Wno-unused-function'
)

if get_option('font-face-cache-load').disabled()
    plutovg_c_args += ['-DPLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD']
endif

if get_option('default_library') == 'static'
    plutovg_c_args += ['-DPLUTOVG_BUILD_STATIC']
    plutovg_compile_args += ['-DPLUTOVG_BUILD_STATIC']
endif

math_dep = cc.find_library('m', required: false)
if math_dep.found()
    plutovg_deps += [math_dep]
endif

threads_dep = dependency('threads', required: false)
if threads_dep.found()
    plutovg_deps += [threads_dep]
endif
stdthreads_dep = cc.find_library('stdthreads', required: false)
if stdthreads_dep.found()
    plutovg_deps += [stdthreads_dep]
endif
if cc.check_header('threads.h')
    plutovg_c_args += ['-DHAVE_THREADS_H']
endif

plutovg_sources = [
    'source/plutovg-blend.c',
    'source/plutovg-canvas.c',
    'source/plutovg-font.c',
    'source/plutovg-matrix.c',
    'source/plutovg-paint.c',
    'source/plutovg-path.c',
    'source/plutovg-rasterize.c',
    'source/plutovg-surface.c',
    'source/plutovg-ft-math.c',
    'source/plutovg-ft-raster.c',
    'source/plutovg-ft-stroker.c'
]

plutovg_lib = library('plutovg', plutovg_sources,
    include_directories: include_directories('include', 'source'),
    dependencies: plutovg_deps,
    version: meson.project_version(),
    c_args: plutovg_c_args,
    gnu_symbol_visibility: 'hidden',
    install: true
)

plutovg_dep = declare_dependency(
    link_with: plutovg_lib,
    include_directories: include_directories('include'),
    compile_args: plutovg_compile_args
)

meson.override_dependency('plutovg', plutovg_dep)
install_headers('include/plutovg.h', subdir: 'plutovg')

if not get_option('examples').disabled()
    subdir('examples')
endif

pkgmod = import('pkgconfig')
pkgmod.generate(plutovg_lib,
    name: 'PlutoVG',
    description: 'Tiny 2D vector graphics library',
    filebase: 'plutovg',
    subdirs: 'plutovg'
)
