[![Actions](https://github.com/sammycage/plutovg/actions/workflows/main.yml/badge.svg)](https://github.com/sammycage/plutovg/actions)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](https://github.com/sammycage/plutovg/blob/main/LICENSE)
[![Releases](https://img.shields.io/github/v/release/sammycage/plutovg)](https://github.com/sammycage/plutovg/releases)
[![CodeFactor](https://www.codefactor.io/repository/github/sammycage/plutovg/badge)](https://www.codefactor.io/repository/github/sammycage/plutovg)

# PlutoVG
PlutoVG is a standalone 2D vector graphics library in C.

## Features
- Path Filling, Stroking and Dashing
- Solid, Gradient and Texture Paints
- Fonts and Texts
- Clipping and Compositing
- Transformations
- Images

## Example
```c
#include <plutovg.h>

int main(void)
{
    const int width = 150;
    const int height = 150;

    const float center_x = width / 2.f;
    const float center_y = height / 2.f;
    const float face_radius = 70;
    const float mouth_radius = 50;
    const float eye_radius = 10;
    const float eye_offset_x = 25;
    const float eye_offset_y = 20;
    const float eye_x = center_x - eye_offset_x;
    const float eye_y = center_y - eye_offset_y;

    plutovg_surface_t* surface = plutovg_surface_create(width, height);
    plutovg_canvas_t* canvas = plutovg_canvas_create(surface);

    plutovg_canvas_save(canvas);
    plutovg_canvas_arc(canvas, center_x, center_y, face_radius, 0, PLUTOVG_TWO_PI, 0);
    plutovg_canvas_set_rgb(canvas, 1, 1, 0);
    plutovg_canvas_fill_preserve(canvas);
    plutovg_canvas_set_rgb(canvas, 0, 0, 0);
    plutovg_canvas_set_line_width(canvas, 5);
    plutovg_canvas_stroke(canvas);
    plutovg_canvas_restore(canvas);

    plutovg_canvas_save(canvas);
    plutovg_canvas_arc(canvas, eye_x, eye_y, eye_radius, 0, PLUTOVG_TWO_PI, 0);
    plutovg_canvas_arc(canvas, center_x + eye_offset_x, eye_y, eye_radius, 0, PLUTOVG_TWO_PI, 0);
    plutovg_canvas_set_rgb(canvas, 0, 0, 0);
    plutovg_canvas_fill(canvas);
    plutovg_canvas_restore(canvas);

    plutovg_canvas_save(canvas);
    plutovg_canvas_arc(canvas, center_x, center_y, mouth_radius, 0, PLUTOVG_PI, 0);
    plutovg_canvas_set_rgb(canvas, 0, 0, 0);
    plutovg_canvas_set_line_width(canvas, 5);
    plutovg_canvas_stroke(canvas);
    plutovg_canvas_restore(canvas);

    plutovg_surface_write_to_png(surface, "smiley.png");
    plutovg_canvas_destroy(canvas);
    plutovg_surface_destroy(surface);
    return 0;
}
```

output:

![smiley.png](smiley.png)

## Installation

Follow the steps below to install PlutoVG using either [Meson](https://mesonbuild.com/) or [CMake](https://cmake.org/).

### Using Meson

```bash
git clone https://github.com/sammycage/plutovg.git
cd plutovg
meson setup build
meson compile -C build
meson install -C build
```

### Using CMake

```bash
git clone https://github.com/sammycage/plutovg.git
cd plutovg
cmake -B build .
cmake --build build
cmake --install build
```

## Projects using PlutoVG
- [LunaSVG](https://github.com/sammycage/lunasvg)
- [PlutoSVG](https://github.com/sammycage/plutosvg)
