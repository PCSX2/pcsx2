fs = import('fs')
fs.copyfile('camera.svg')

executable('camera2png', 'camera2png.c', dependencies: plutosvg_dep)
executable('svg2png', 'svg2png.c', dependencies: plutosvg_dep)
if freetype_dep.found()
    executable('emoji2png', 'emoji2png.c', dependencies: [plutosvg_dep, freetype_dep])
endif
