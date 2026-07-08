/*
 * Copyright (c) 2020-2026 Samuel Ugochukwu <sammycageagle@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

/**
 * @brief FreeType hooks for rendering SVG glyphs with PlutoSVG.
 *
 * This file implements the integration layer between FreeType’s SVG module (typically
 * named "ot-svg") and PlutoSVG. It defines the necessary functions to initialize and
 * free the SVG rendering state, render an SVG glyph into a glyph slot, load (and cache)
 * SVG documents, and pre-configure glyph slots with appropriate metrics and transforms.
 *
 * These functions are aggregated into the `plutosvg_ft_hooks` structure.
 *
 * Usage example:
 * @code
 * #include <plutosvg-ft.h>
 *
 * #include <ft2build.h>
 * #include FT_FREETYPE_H
 * #include FT_MODULE_H
 *
 * int main(void)
 * {
 *     FT_Library library;
 *     if (FT_Init_FreeType(&library))
 *         return -1;
 *
 *     if (FT_Property_Set(library, "ot-svg", "svg-hooks", &plutosvg_ft_hooks))
 *         return -1;
 *
 *     // ... your code ...
 *
 *     FT_Done_FreeType(library);
 *     return 0;
 * }
 * @endcode
 */

#ifndef PLUTOSVG_FT_H
#define PLUTOSVG_FT_H

#include "plutosvg.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OTSVG_H
#include FT_COLOR_H

typedef struct {
    plutosvg_document_t* document;
    const FT_Byte* data;
    FT_ULong length;
} plutosvg_ft_document_entry_t;

#define PLUTOSVG_FT_MAX_DOCS 16

typedef struct {
    plutosvg_document_t* document;
    plutovg_matrix_t matrix;
    plutovg_rect_t extents;
    plutosvg_ft_document_entry_t entries[PLUTOSVG_FT_MAX_DOCS];
    FT_ULong num_entries;
} plutosvg_ft_state_t;

static FT_Error plutosvg_ft_init(FT_Pointer* ft_state)
{
    plutosvg_ft_state_t* state = (plutosvg_ft_state_t*)malloc(sizeof(plutosvg_ft_state_t));
    memset(state, 0, sizeof(plutosvg_ft_state_t));
    *ft_state = state;
    return FT_Err_Ok;
}

static void plutosvg_ft_free(FT_Pointer* ft_state)
{
    plutosvg_ft_state_t* state = (plutosvg_ft_state_t*)(*ft_state);
    for(FT_ULong i = 0; i < state->num_entries; ++i)
        plutosvg_document_destroy(state->entries[i].document);
    free(state);
}

#define PLUTOSVG_FT_PALETTE_INDEX 0

static bool plutosvg_ft_palette_func(void* closure, const char* name, int length, plutovg_color_t* color)
{
    FT_Face ft_face = (FT_Face)(closure);
    if(length < 5 || strncmp(name, "color", 5) != 0)
        return false;
    FT_Palette_Data ft_palette_data;
    if(FT_Palette_Data_Get(ft_face, &ft_palette_data))
        return false;
    FT_Color* ft_palette = NULL;
    if(FT_Palette_Select(ft_face, PLUTOSVG_FT_PALETTE_INDEX, &ft_palette)) {
        return false;
    }

    FT_Int index = 0;
    for(int i = 5; i < length; ++i) {
        const char ch = name[i];
        if(ch < '0' || ch > '9')
            return false;
        index = index * 10 + ch - '0';
    }

    if(index >= ft_palette_data.num_palette_entries)
        return false;
    FT_Color* ft_color = ft_palette + index;
    color->r = ft_color->red / 255.f;
    color->g = ft_color->green / 255.f;
    color->b = ft_color->blue / 255.f;
    color->a = ft_color->alpha / 255.f;
    return true;
}

static FT_Error plutosvg_ft_render(FT_GlyphSlot ft_slot, FT_Pointer* ft_state)
{
    plutosvg_ft_state_t* state = (plutosvg_ft_state_t*)(*ft_state);
    if(state->document == NULL)
        return FT_Err_Invalid_SVG_Document;
    plutovg_surface_t* surface = plutovg_surface_create_for_data(ft_slot->bitmap.buffer, ft_slot->bitmap.width, ft_slot->bitmap.rows, ft_slot->bitmap.pitch);
    plutovg_canvas_t* canvas = plutovg_canvas_create(surface);

    FT_SVG_Document ft_document = (FT_SVG_Document)ft_slot->other;
    FT_UShort start_glyph_id = ft_document->start_glyph_id;
    FT_UShort end_glyph_id = ft_document->end_glyph_id;

    char buffer[64];
    char* id = NULL;
    if(start_glyph_id < end_glyph_id) {
        snprintf(buffer, sizeof(buffer), "glyph%u", ft_slot->glyph_index);
        id = buffer;
    }

    plutovg_canvas_translate(canvas, -state->extents.x, -state->extents.y);
    plutovg_canvas_transform(canvas, &state->matrix);
    plutosvg_document_render(state->document, id, canvas, NULL, plutosvg_ft_palette_func, ft_slot->face);

    ft_slot->bitmap.pixel_mode = FT_PIXEL_MODE_BGRA;
    ft_slot->bitmap.num_grays = 256;
    ft_slot->format = FT_GLYPH_FORMAT_BITMAP;

    plutovg_canvas_destroy(canvas);
    plutovg_surface_destroy(surface);
    state->document = NULL;
    return FT_Err_Ok;
}

static plutosvg_document_t* plutosvg_ft_document_load(plutosvg_ft_state_t* state, const FT_Byte* data, FT_ULong length, FT_UShort units_per_EM)
{
    for(FT_ULong i = 0; i < state->num_entries; ++i) {
        if(data == state->entries[i].data && length == state->entries[i].length) {
            plutosvg_ft_document_entry_t entry = state->entries[i];
            memmove(&state->entries[1], &state->entries[0], i * sizeof(plutosvg_ft_document_entry_t));
            state->entries[0] = entry;
            return entry.document;
        }
    }

    plutosvg_document_t* document = plutosvg_document_load_from_data((const char*)data, length, units_per_EM, units_per_EM, NULL, NULL);
    if(document == NULL)
        return NULL;
    if(state->num_entries == PLUTOSVG_FT_MAX_DOCS) {
        state->num_entries--;
        plutosvg_document_destroy(state->entries[state->num_entries].document);
    }

    memmove(&state->entries[1], &state->entries[0], state->num_entries * sizeof(plutosvg_ft_document_entry_t));
    state->entries[0].document = document;
    state->entries[0].data = data;
    state->entries[0].length = length;
    state->num_entries++;
    return document;
}

static FT_Error plutosvg_ft_preset_slot(FT_GlyphSlot ft_slot, FT_Bool ft_cache, FT_Pointer* ft_state)
{
    plutosvg_ft_state_t* state = (plutosvg_ft_state_t*)(*ft_state);

    FT_SVG_Document ft_document = (FT_SVG_Document)ft_slot->other;
    FT_Size_Metrics ft_metrics = ft_document->metrics;

    FT_UShort start_glyph_id = ft_document->start_glyph_id;
    FT_UShort end_glyph_id = ft_document->end_glyph_id;

    plutosvg_document_t* document = plutosvg_ft_document_load(state, ft_document->svg_document, ft_document->svg_document_length, ft_document->units_per_EM);
    if(document == NULL) {
        return FT_Err_Invalid_SVG_Document;
    }

    float document_width = plutosvg_document_get_width(document);
    float document_height = plutosvg_document_get_height(document);

    plutovg_matrix_t transform = {
         (float)ft_document->transform.xx / (1 << 16),
        -(float)ft_document->transform.xy / (1 << 16),
        -(float)ft_document->transform.yx / (1 << 16),
         (float)ft_document->transform.yy / (1 << 16),
         (float)ft_document->delta.x / 64 * document_width / ft_metrics.x_ppem,
        -(float)ft_document->delta.y / 64 * document_height / ft_metrics.y_ppem
    };

    float x_svg_to_out = ft_metrics.x_ppem / document_width;
    float y_svg_to_out = ft_metrics.y_ppem / document_height;

    plutovg_matrix_t matrix;
    plutovg_matrix_init_scale(&matrix, x_svg_to_out, y_svg_to_out);
    plutovg_matrix_multiply(&matrix, &transform, &matrix);

    char buffer[64];
    char* id = NULL;
    if(start_glyph_id < end_glyph_id) {
        snprintf(buffer, sizeof(buffer), "glyph%u", ft_slot->glyph_index);
        id = buffer;
    }

    plutovg_rect_t extents;
    if(!plutosvg_document_extents(document, id, &extents)) {
        return FT_Err_Invalid_SVG_Document;
    }

    plutovg_matrix_map_rect(&matrix, &extents, &extents);
    ft_slot->bitmap_left = (FT_Int)extents.x;
    ft_slot->bitmap_top = (FT_Int)-extents.y;

    ft_slot->bitmap.rows = (unsigned int)ceilf(extents.h);
    ft_slot->bitmap.width = (unsigned int)ceilf(extents.w);
    ft_slot->bitmap.pitch = (int)ft_slot->bitmap.width * 4;
    ft_slot->bitmap.pixel_mode = FT_PIXEL_MODE_BGRA;

    float metrics_width = extents.w;
    float metrics_height = extents.h;

    float horiBearingX = extents.x;
    float horiBearingY = -extents.y;

    float vertBearingX = ft_slot->metrics.horiBearingX / 64.f - ft_slot->metrics.horiAdvance / 64.f / 2;
    float vertBearingY = (ft_slot->metrics.vertAdvance / 64.f - ft_slot->metrics.height / 64.f) / 2;

    ft_slot->metrics.width = (FT_Pos)roundf(metrics_width * 64);
    ft_slot->metrics.height = (FT_Pos)roundf(metrics_height * 64);

    ft_slot->metrics.horiBearingX = (FT_Pos)(horiBearingX * 64);
    ft_slot->metrics.horiBearingY = (FT_Pos)(horiBearingY * 64);
    ft_slot->metrics.vertBearingX = (FT_Pos)(vertBearingX * 64);
    ft_slot->metrics.vertBearingY = (FT_Pos)(vertBearingY * 64);
    if(ft_slot->metrics.vertAdvance == 0)
        ft_slot->metrics.vertAdvance = (FT_Pos)(metrics_height * 1.2f * 64);
    if(ft_cache) {
        state->document = document;
        state->extents = extents;
        state->matrix = matrix;
    }

    return FT_Err_Ok;
}

/**
 * @brief FreeType SVG renderer hooks.
 *
 * This structure is passed to FreeType via FT_Property_Set to delegate SVG glyph
 * rendering to PlutoSVG.
 */
static SVG_RendererHooks plutosvg_ft_hooks = {
    (SVG_Lib_Init_Func)plutosvg_ft_init,
    (SVG_Lib_Free_Func)plutosvg_ft_free,
    (SVG_Lib_Render_Func)plutosvg_ft_render,
    (SVG_Lib_Preset_Slot_Func)plutosvg_ft_preset_slot
};

#endif // PLUTOSVG_FT_H
