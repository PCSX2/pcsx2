project('plutosvg', 'c',
    version: '0.0.7',
    license: 'MIT',
    meson_version: '>=0.64.0',
    default_options: ['c_std=c99']
)

plutosvg_deps = []
plutosvg_compile_args = []

plutovg_dep = dependency('plutovg',
    required: true,
    version: '>=1.0.0',
    fallback: ['plutovg', 'plutovg_dep']
)

plutosvg_deps += [plutovg_dep]

cc = meson.get_compiler('c')
math_dep = cc.find_library('m', required: false)
if math_dep.found()
    plutosvg_deps += [math_dep]
endif

freetype_dep = dependency('freetype2',
    required: get_option('freetype'),
    version: '>=2.12',
    allow_fallback: false
)

if freetype_dep.found() and cc.has_type('SVG_RendererHooks', dependencies: freetype_dep, prefix: '#include <freetype/otsvg.h>')
    plutosvg_deps += [freetype_dep]
    plutosvg_compile_args += ['-DPLUTOSVG_HAS_FREETYPE']
else
    freetype_dep = disabler()
endif

if get_option('default_library') == 'static'
    plutosvg_compile_args += ['-DPLUTOSVG_BUILD_STATIC']
endif

plutosvg_lib = library('plutosvg', 'source/plutosvg.c',
    include_directories: include_directories('source'),
    dependencies: plutosvg_deps,
    version: meson.project_version(),
    c_args: ['-DPLUTOSVG_BUILD'] + plutosvg_compile_args,
    cpp_args: ['-DPLUTOSVG_BUILD'] + plutosvg_compile_args,
    gnu_symbol_visibility: 'hidden',
    install: true
)

plutosvg_dep = declare_dependency(
    link_with: plutosvg_lib,
    dependencies: plutovg_dep,
    include_directories: include_directories('source'),
    compile_args: plutosvg_compile_args
)

meson.override_dependency('plutosvg', plutosvg_dep)
install_headers('source/plutosvg.h', 'source/plutosvg-ft.h', subdir: 'plutosvg')

if not get_option('examples').disabled()
    subdir('examples')
endif

pkgmod = import('pkgconfig')
pkgmod.generate(plutosvg_lib,
    name: 'PlutoSVG',
    description: 'Tiny SVG rendering library',
    filebase: 'plutosvg',
    subdirs: 'plutosvg'
)
