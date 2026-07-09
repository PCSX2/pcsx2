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

#ifndef PLUTOSVG_H
#define PLUTOSVG_H

#include <plutovg.h>

#if defined(PLUTOSVG_BUILD_STATIC)
#define PLUTOSVG_EXPORT
#define PLUTOSVG_IMPORT
#elif (defined(_WIN32) || defined(__CYGWIN__))
#define PLUTOSVG_EXPORT __declspec(dllexport)
#define PLUTOSVG_IMPORT __declspec(dllimport)
#elif defined(__GNUC__) && (__GNUC__ >= 4)
#define PLUTOSVG_EXPORT __attribute__((__visibility__("default")))
#define PLUTOSVG_IMPORT
#else
#define PLUTOSVG_EXPORT
#define PLUTOSVG_IMPORT
#endif

#ifdef PLUTOSVG_BUILD
#define PLUTOSVG_API PLUTOSVG_EXPORT
#else
#define PLUTOSVG_API PLUTOSVG_IMPORT
#endif

#define PLUTOSVG_VERSION_MAJOR 0
#define PLUTOSVG_VERSION_MINOR 0
#define PLUTOSVG_VERSION_MICRO 7

#define PLUTOSVG_VERSION PLUTOVG_VERSION_ENCODE(PLUTOSVG_VERSION_MAJOR, PLUTOSVG_VERSION_MINOR, PLUTOSVG_VERSION_MICRO)
#define PLUTOSVG_VERSION_STRING PLUTOVG_VERSION_STRINGIZE(PLUTOSVG_VERSION_MAJOR, PLUTOSVG_VERSION_MINOR, PLUTOSVG_VERSION_MICRO)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Returns the version number of PlutoSVG.
 *
 * @return The version number as an integer.
 */
PLUTOSVG_API int plutosvg_version(void);

/**
 * @brief Returns the version string of PlutoSVG.
 *
 * @return The version number as a null-terminated string.
 */
PLUTOSVG_API const char* plutosvg_version_string(void);

/**
 * @brief Represents an abstract SVG document handle.
 */
typedef struct plutosvg_document plutosvg_document_t;

/**
 * @brief Callback type for resolving CSS color variables in SVG documents.
 *
 * @param closure User-defined data passed to the callback.
 * @param name Name of the color variable.
 * @param length Length of the color variable name.
 * @param color Pointer to a `plutovg_color_t` object where the resolved color will be stored.
 * @return `true` if the color variable was successfully resolved; `false` otherwise.
 */
typedef bool (*plutosvg_palette_func_t)(void* closure, const char* name, int length, plutovg_color_t* color);

/**
 * @brief Loads an SVG document from a data buffer.
 *
 * @note The buffer pointed to by `data` must remain valid until the returned `plutosvg_document_t` object is destroyed.
 *
 * @param data Pointer to the SVG data buffer.
 * @param length Length of the data buffer.
 * @param width Container width used to resolve the intrinsic width, or `-1` if unspecified.
 * @param height Container height used to resolve the intrinsic height, or `-1` if unspecified.
 * @param destroy_func Custom function called when the document is destroyed.
 * @param closure User-defined data passed to the `destroy_func` callback.
 * @return Pointer to the loaded `plutosvg_document_t` object, or `NULL` if loading fails.
 */
PLUTOSVG_API plutosvg_document_t* plutosvg_document_load_from_data(const char* data, int length, float width, float height,
    plutovg_destroy_func_t destroy_func, void* closure);

/**
 * @brief Loads an SVG document from a file.
 *
 * @param filename Path to the SVG file.
 * @param width Container width used to resolve the intrinsic width, or `-1` if unspecified.
 * @param height Container height used to resolve the intrinsic height, or `-1` if unspecified.
 * @return Pointer to the loaded `plutosvg_document_t` object, or `NULL` if loading fails.
 */
PLUTOSVG_API plutosvg_document_t* plutosvg_document_load_from_file(const char* filename, float width, float height);

/**
 * @brief Renders an SVG document or a specific element onto a canvas.
 *
 * @param document Pointer to the SVG document.
 * @param id ID of the SVG element to render, or `NULL` to render the entire document.
 * @param canvas Canvas onto which the SVG element or document will be rendered.
 * @param current_color Color used to resolve CSS `currentColor` values.
 * @param palette_func Callback function for resolving CSS color variables.
 * @param closure User-defined data passed to the `palette_func` callback.
 * @return `true` if rendering was successful; `false` otherwise.
 */
PLUTOSVG_API bool plutosvg_document_render(const plutosvg_document_t* document, const char* id, plutovg_canvas_t* canvas,
    const plutovg_color_t* current_color, plutosvg_palette_func_t palette_func, void* closure);

/**
 * @brief Renders an SVG document or a specific element to a surface.
 *
 * @param document Pointer to the SVG document.
 * @param id ID of the SVG element to render, or `NULL` to render the entire document.
 * @param width Expected width of the surface, or `-1` if unspecified.
 * @param height Expected height of the surface, or `-1` if unspecified.
 * @param current_color Color used to resolve CSS `currentColor` values.
 * @param palette_func Callback function for resolving CSS color variables.
 * @param closure User-defined data passed to the `palette_func` callback.
 * @return Pointer to the rendered `plutovg_surface_t` object, or `NULL` if rendering fails.
 */
PLUTOSVG_API plutovg_surface_t* plutosvg_document_render_to_surface(const plutosvg_document_t* document, const char* id, int width, int height,
    const plutovg_color_t* current_color, plutosvg_palette_func_t palette_func, void* closure);

/**
 * @brief Returns the intrinsic width of the SVG document.
 *
 * @param document Pointer to the SVG document.
 * @return The intrinsic width of the SVG document.
 */
PLUTOSVG_API float plutosvg_document_get_width(const plutosvg_document_t* document);

/**
 * @brief Returns the intrinsic height of the SVG document.
 *
 * @param document Pointer to the SVG document.
 * @return The intrinsic height of the SVG document.
 */
PLUTOSVG_API float plutosvg_document_get_height(const plutosvg_document_t* document);

/**
 * @brief Retrieves the bounding box of a specific element or the entire SVG document.
 *
 * Calculates and retrieves the extents of an element identified by `id`, or the whole document if `id` is `NULL`.
 *
 * @param document Pointer to the SVG document.
 * @param id ID of the element whose extents to retrieve, or `NULL` to retrieve the extents of the entire document.
 * @param extents Pointer to a `plutovg_rect_t` object where the extents will be stored.
 * @return `true` if the extents were successfully retrieved; `false` otherwise.
 */
PLUTOSVG_API bool plutosvg_document_extents(const plutosvg_document_t* document, const char* id, plutovg_rect_t* extents);

/**
 * @brief Destroys an SVG document and frees its resources.
 *
 * @param document Pointer to a `plutosvg_document_t` object to be destroyed. If `NULL`, the function does nothing.
 */
PLUTOSVG_API void plutosvg_document_destroy(plutosvg_document_t* document);

/**
 * @brief Retrieves PlutoSVG hooks for integrating with FreeType's SVG module.
 *
 * @note If you want to manage FreeType integration independently from PlutoSVG,
 * include <plutosvg-ft.h> and use `plutosvg_ft_hooks` directly instead of this function.
 *
 * Provides hooks that allow FreeType to use PlutoSVG for rendering SVG graphics in fonts.
 *
 * @return Pointer to the structure containing PlutoSVG hooks for FreeType's SVG module, or `NULL` if FreeType integration is not enabled.
 */
PLUTOSVG_API const void* plutosvg_ft_svg_hooks(void);

#ifdef __cplusplus
}
#endif

#endif // PLUTOSVG_H
