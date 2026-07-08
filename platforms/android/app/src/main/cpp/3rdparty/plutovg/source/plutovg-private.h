#ifndef PLUTOVG_PRIVATE_H
#define PLUTOVG_PRIVATE_H

#include "plutovg.h"

#if defined(_WIN32)

#include <windows.h>

typedef LONG plutovg_ref_count_t;

#define plutovg_init_reference(ob) ((ob)->ref_count = 1)
#define plutovg_increment_reference(ob) (void)(ob && InterlockedIncrement(&(ob)->ref_count))
#define plutovg_destroy_reference(ob) (ob && InterlockedDecrement(&(ob)->ref_count) == 0)
#define plutovg_get_reference_count(ob) ((ob) ? InterlockedCompareExchange((LONG*)&(ob)->ref_count, 0, 0) : 0)

#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)

#include <stdatomic.h>

typedef atomic_int plutovg_ref_count_t;

#define plutovg_init_reference(ob) atomic_init(&(ob)->ref_count, 1)
#define plutovg_increment_reference(ob) (void)(ob && atomic_fetch_add(&(ob)->ref_count, 1))
#define plutovg_destroy_reference(ob) (ob && atomic_fetch_sub(&(ob)->ref_count, 1) == 1)
#define plutovg_get_reference_count(ob) ((ob) ? atomic_load(&(ob)->ref_count) : 0)

#else

typedef int plutovg_ref_count_t;

#define plutovg_init_reference(ob) ((ob)->ref_count = 1)
#define plutovg_increment_reference(ob) (void)(ob && ++(ob)->ref_count)
#define plutovg_destroy_reference(ob) (ob && --(ob)->ref_count == 0)
#define plutovg_get_reference_count(ob) ((ob) ? (ob)->ref_count : 0)

#endif

struct plutovg_surface {
    plutovg_ref_count_t ref_count;
    int width;
    int height;
    int stride;
    unsigned char* data;
};

struct plutovg_path {
    plutovg_ref_count_t ref_count;
    int num_points;
    int num_contours;
    int num_curves;
    plutovg_point_t start_point;
    struct {
        plutovg_path_element_t* data;
        int size;
        int capacity;
    } elements;
};

typedef enum {
    PLUTOVG_PAINT_TYPE_COLOR,
    PLUTOVG_PAINT_TYPE_GRADIENT,
    PLUTOVG_PAINT_TYPE_TEXTURE
} plutovg_paint_type_t;

struct plutovg_paint {
    plutovg_ref_count_t ref_count;
    plutovg_paint_type_t type;
};

typedef struct {
    plutovg_paint_t base;
    plutovg_color_t color;
} plutovg_solid_paint_t;

typedef enum {
    PLUTOVG_GRADIENT_TYPE_LINEAR,
    PLUTOVG_GRADIENT_TYPE_RADIAL
} plutovg_gradient_type_t;

typedef struct {
    plutovg_paint_t base;
    plutovg_gradient_type_t type;
    plutovg_spread_method_t spread;
    plutovg_matrix_t matrix;
    plutovg_gradient_stop_t* stops;
    int nstops;
    float values[6];
} plutovg_gradient_paint_t;

typedef struct {
    plutovg_paint_t base;
    plutovg_texture_type_t type;
    float opacity;
    plutovg_matrix_t matrix;
    plutovg_surface_t* surface;
} plutovg_texture_paint_t;

typedef struct {
    int x;
    int len;
    int y;
    unsigned char coverage;
} plutovg_span_t;

typedef struct {
    struct {
        plutovg_span_t* data;
        int size;
        int capacity;
    } spans;

    int x;
    int y;
    int w;
    int h;
} plutovg_span_buffer_t;

typedef struct {
    float offset;
    struct {
        float* data;
        int size;
        int capacity;
    } array;
} plutovg_stroke_dash_t;

typedef struct {
    float width;
    plutovg_line_cap_t cap;
    plutovg_line_join_t join;
    float miter_limit;
} plutovg_stroke_style_t;

typedef struct {
    plutovg_stroke_style_t style;
    plutovg_stroke_dash_t dash;
} plutovg_stroke_data_t;

typedef struct plutovg_state {
    plutovg_paint_t* paint;
    plutovg_font_face_t* font_face;
    plutovg_color_t color;
    plutovg_matrix_t matrix;
    plutovg_stroke_data_t stroke;
    plutovg_span_buffer_t clip_spans;
    plutovg_fill_rule_t winding;
    plutovg_operator_t op;
    float font_size;
    float opacity;
    bool clipping;
    struct plutovg_state* next;
} plutovg_state_t;

struct plutovg_canvas {
    plutovg_ref_count_t ref_count;
    plutovg_surface_t* surface;
    plutovg_path_t* path;
    plutovg_state_t* state;
    plutovg_state_t* freed_state;
    plutovg_font_face_cache_t* face_cache;
    plutovg_rect_t clip_rect;
    plutovg_span_buffer_t clip_spans;
    plutovg_span_buffer_t fill_spans;
};

void plutovg_span_buffer_init(plutovg_span_buffer_t* span_buffer);
void plutovg_span_buffer_init_rect(plutovg_span_buffer_t* span_buffer, int x, int y, int width, int height);
void plutovg_span_buffer_reset(plutovg_span_buffer_t* span_buffer);
void plutovg_span_buffer_destroy(plutovg_span_buffer_t* span_buffer);
void plutovg_span_buffer_copy(plutovg_span_buffer_t* span_buffer, const plutovg_span_buffer_t* source);
bool plutovg_span_buffer_contains(const plutovg_span_buffer_t* span_buffer, float x, float y);
void plutovg_span_buffer_extents(plutovg_span_buffer_t* span_buffer, plutovg_rect_t* extents);
void plutovg_span_buffer_intersect(plutovg_span_buffer_t* span_buffer, const plutovg_span_buffer_t* a, const plutovg_span_buffer_t* b);

void plutovg_rasterize(plutovg_span_buffer_t* span_buffer, const plutovg_path_t* path, const plutovg_matrix_t* matrix, const plutovg_rect_t* clip_rect, const plutovg_stroke_data_t* stroke_data, plutovg_fill_rule_t winding);
void plutovg_blend(plutovg_canvas_t* canvas, const plutovg_span_buffer_t* span_buffer);
void plutovg_memfill32(unsigned int* dest, int length, unsigned int value);

#endif // PLUTOVG_PRIVATE_H
