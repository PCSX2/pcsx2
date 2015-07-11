/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/cairo.cpp
// Purpose:     Cairo library
// Author:      Anthony Betaudeau
// Created:     2007-08-25
// Copyright:   (c) Anthony Bretaudeau
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////


// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_CAIRO

// keep cairo.h from defining dllimport as we're defining the symbols inside
// the wx dll in order to load them dynamically.
#define cairo_public 

#include <cairo.h>
#include "wx/dynlib.h"

#ifdef __WXMSW__
#include "wx/msw/wrapwin.h"
#endif

#ifdef __WXMAC__
#include "wx/osx/private.h"
#include <cairo-quartz.h>
#endif

#ifndef WX_PRECOMP
    #include "wx/module.h"
    #include "wx/log.h"
#endif

#define wxCAIRO_METHOD_TYPE(name) \
    wxCairo##name##_t
    
#define wxCAIRO_STATIC_METHOD_DEFINE(rettype, name, args, argnames, defret) \
   static wxCAIRO_METHOD_TYPE(name) name;

#define wxCAIRO_STATIC_VOIDMETHOD_DEFINE(name, args, argnames) \
    wxCAIRO_STATIC_METHOD_DEFINE(void, name, args, argnames, NULL)

#define wxFOR_ALL_CAIRO_VOIDMETHODS(m) \
    m( cairo_append_path, \
        (cairo_t *cr, const cairo_path_t *path), (cr, path) ) \
    m( cairo_arc, \
        (cairo_t *cr, double xc, double yc, double radius, double angle1, double angle2), (cr, xc, yc, radius, angle1, angle2) ) \
    m( cairo_arc_negative, \
        (cairo_t *cr, double xc, double yc, double radius, double angle1, double angle2), (cr, xc, yc, radius, angle1, angle2) ) \
    m( cairo_clip, \
        (cairo_t *cr), (cr) ) \
    m( cairo_close_path, \
        (cairo_t *cr), (cr) ) \
    m( cairo_curve_to, \
        (cairo_t *cr, double x1, double y1, double x2, double y2, double x3, double y3), (cr, x1, y1, x2, y2, x3, y3) ) \
    m( cairo_destroy, \
        (cairo_t *cr), (cr) ) \
    m( cairo_fill, \
        (cairo_t *cr), (cr) ) \
    m( cairo_fill_preserve, \
        (cairo_t *cr), (cr) ) \
    m( cairo_font_extents, \
        (cairo_t *cr, cairo_font_extents_t *extents), (cr, extents) ) \
    m( cairo_font_face_destroy, \
        (cairo_font_face_t *font_face), (font_face) ) \
    m( cairo_get_current_point, \
        (cairo_t *cr, double *x, double *y), (cr, x, y) ) \
    m( cairo_get_matrix, \
        (cairo_t *cr, cairo_matrix_t *matrix), (cr, matrix) ) \
    m( cairo_line_to, \
        (cairo_t *cr, double x, double y), (cr, x, y) ) \
    m( cairo_matrix_init, \
        (cairo_matrix_t *matrix, double xx, double yx, double xy, double yy, double x0, double y0), (matrix, xx, yx, xy, yy, x0, y0) ) \
    m( cairo_matrix_multiply, \
        (cairo_matrix_t *result, const cairo_matrix_t *a, const cairo_matrix_t *b), (result, a, b) ) \
    m( cairo_matrix_rotate, \
        (cairo_matrix_t *matrix, double radians), (matrix, radians) ) \
    m( cairo_matrix_scale, \
        (cairo_matrix_t *matrix, double sx, double sy), (matrix, sx, sy) ) \
    m( cairo_matrix_transform_distance, \
        (const cairo_matrix_t *matrix, double *dx, double *dy), (matrix, dx, dy) ) \
    m( cairo_matrix_transform_point, \
        (const cairo_matrix_t *matrix, double *x, double *y), (matrix, x, y) ) \
    m( cairo_matrix_translate, \
        (cairo_matrix_t *matrix, double tx, double ty), (matrix, tx, ty) ) \
    m( cairo_move_to, \
        (cairo_t *cr, double x, double y), (cr, x, y) ) \
    m( cairo_new_path, \
        (cairo_t *cr), (cr) ) \
    m( cairo_paint, \
        (cairo_t *cr), (cr) ) \
    m( cairo_paint_with_alpha, \
        (cairo_t *cr, double alpha), (cr, alpha) ) \
    m( cairo_path_destroy, \
        (cairo_path_t *path), (path) ) \
    m( cairo_pattern_add_color_stop_rgba, \
        (cairo_pattern_t *pattern, double offset, double red, double green, double blue, double alpha), (pattern, offset, red, green, blue, alpha) ) \
    m( cairo_pattern_destroy, \
        (cairo_pattern_t *pattern), (pattern) ) \
    m( cairo_pattern_set_extend, \
        (cairo_pattern_t *pattern, cairo_extend_t extend), (pattern, extend) ) \
    m( cairo_pattern_set_filter, \
        (cairo_pattern_t *pattern, cairo_filter_t filter), (pattern, filter) ) \
    m( cairo_pop_group_to_source, \
        (cairo_t *cr), (cr) ) \
    m( cairo_push_group, \
        (cairo_t *cr), (cr) ) \
    m( cairo_rectangle, \
        (cairo_t *cr, double x, double y, double width, double height), (cr, x, y, width, height) ) \
    m( cairo_reset_clip, \
        (cairo_t *cr), (cr) ) \
    m( cairo_restore, \
        (cairo_t *cr), (cr) ) \
    m( cairo_rotate, \
        (cairo_t *cr, double angle), (cr, angle) ) \
    m( cairo_save, \
        (cairo_t *cr), (cr) ) \
    m( cairo_scale, \
        (cairo_t *cr, double sx, double sy), (cr, sx, sy) ) \
    m( cairo_select_font_face, \
        (cairo_t *cr, const char *family, cairo_font_slant_t slant, cairo_font_weight_t weight), (cr, family, slant, weight) ) \
    m( cairo_set_antialias, \
        (cairo_t *cr, cairo_antialias_t antialias), (cr, antialias) ) \
    m( cairo_set_dash, \
        (cairo_t *cr, const double *dashes, int num_dashes, double offset), (cr, dashes, num_dashes, offset) ) \
    m( cairo_set_fill_rule, \
        (cairo_t *cr, cairo_fill_rule_t fill_rule), (cr, fill_rule) ) \
    m( cairo_set_font_face, \
        (cairo_t *cr, cairo_font_face_t *font_face), (cr, font_face) ) \
    m( cairo_set_font_size, \
        (cairo_t *cr, double size), (cr, size) ) \
    m( cairo_set_line_cap, \
        (cairo_t *cr, cairo_line_cap_t line_cap), (cr, line_cap) ) \
    m( cairo_set_line_join, \
        (cairo_t *cr, cairo_line_join_t line_join), (cr, line_join) ) \
    m( cairo_set_line_width, \
        (cairo_t *cr, double width), (cr, width) ) \
    m( cairo_set_matrix, \
        (cairo_t *cr, const cairo_matrix_t *matrix), (cr, matrix) ) \
    m( cairo_set_operator, \
        (cairo_t *cr, cairo_operator_t op), (cr, op) ) \
    m( cairo_set_source, \
        (cairo_t *cr, cairo_pattern_t *source), (cr, source) ) \
    m( cairo_set_source_rgba, \
        (cairo_t *cr, double red, double green, double blue, double alpha), (cr, red, green, blue, alpha) ) \
    m( cairo_show_text, \
        (cairo_t *cr, const char *utf8), (cr, utf8) ) \
    m( cairo_stroke, \
        (cairo_t *cr), (cr) ) \
    m( cairo_stroke_extents, \
        (cairo_t *cr, double *x1, double *y1, double *x2, double *y2), (cr, x1, y1, x2, y2) ) \
    m( cairo_stroke_preserve, \
        (cairo_t *cr), (cr) ) \
    m( cairo_surface_destroy, \
        (cairo_surface_t *surface), (surface) ) \
    m( cairo_text_extents, \
        (cairo_t *cr, const char *utf8, cairo_text_extents_t *extents), (cr, utf8, extents) ) \
    m( cairo_transform, \
        (cairo_t *cr, const cairo_matrix_t *matrix), (cr, matrix) ) \
    m( cairo_translate, \
        (cairo_t *cr, double tx, double ty), (cr, tx, ty) ) \

#ifdef __WXMAC__
#define wxCAIRO_PLATFORM_METHODS(m) \
    m( cairo_font_face_t*, cairo_quartz_font_face_create_for_cgfont, \
        (CGFontRef font), (font), NULL ) \
    m( cairo_surface_t*, cairo_quartz_surface_create_for_cg_context, \
        (CGContextRef cgContext, unsigned int width, unsigned int height), (cgContext, width, height), NULL )
#elif defined(__WXMSW__)
#define wxCAIRO_PLATFORM_METHODS(m) \
    m( cairo_surface_t*, cairo_win32_surface_create, \
        (HDC hdc), (hdc), NULL ) \
    m( cairo_surface_t*, cairo_win32_printing_surface_create, \
        (HDC hdc), (hdc), NULL )
#else
#define wxCAIRO_PLATFORM_METHODS(m) 
#endif

#define wxFOR_ALL_CAIRO_METHODS(m) \
    m( cairo_path_t*, cairo_copy_path, \
        (cairo_t *cr), (cr), NULL ) \
    m( cairo_t*, cairo_create, \
        (cairo_surface_t *target), (target), NULL) \
    m( cairo_surface_t*, cairo_get_target, \
        (cairo_t *cr), (cr), NULL) \
    m( cairo_surface_t*, cairo_image_surface_create, \
        (cairo_format_t format, int width, int height), (format, width, height), NULL ) \
    m( cairo_surface_t*, cairo_image_surface_create_for_data, \
        (unsigned char *data, cairo_format_t format, int width, int height, int stride), (data, format, width, height, stride), NULL) \
    m( cairo_bool_t, cairo_in_fill, \
        (cairo_t *cr, double x, double y), (cr, x, y), false ) \
    m( cairo_status_t, cairo_matrix_invert, \
        (cairo_matrix_t *matrix), (matrix), NULL) \
    m( cairo_pattern_t*, cairo_pattern_create_for_surface, \
        (cairo_surface_t *surface), (surface), NULL) \
    m( cairo_pattern_t*, cairo_pattern_create_linear, \
        (double x0, double y0, double x1, double y1), (x0, y0, x1, y1), NULL) \
    m( cairo_pattern_t*, cairo_pattern_create_radial, \
        (double cx0, double cy0, double radius0, double cx1, double cy1, double radius1), (cx0, cy0, radius0, cx1, cy1, radius1), NULL) \
    m( cairo_status_t, cairo_pattern_status, \
        (cairo_pattern_t *pattern), (pattern), 4) \
    m( cairo_t*, cairo_reference, \
        (cairo_t *cr), (cr), NULL ) \
    m( cairo_surface_t*, cairo_surface_create_similar, \
        (cairo_surface_t *other, cairo_content_t content, int width, int height), (other, content, width, height), NULL) \
    m( int, cairo_format_stride_for_width, \
       (cairo_format_t format, int width), (format, width), 0)  \
    m( int, cairo_version, \
       (), (), 0)  \
    m( int, cairo_image_surface_get_width, \
       (cairo_surface_t *surface), (surface), 0) \
    m( int, cairo_image_surface_get_height, \
       (cairo_surface_t *surface), (surface), 0) \
    m( int, cairo_image_surface_get_stride, \
       (cairo_surface_t *surface), (surface), 0) \
    m( unsigned char *, cairo_image_surface_get_data, \
       (cairo_surface_t *surface), (surface), NULL) \
    m( cairo_format_t, cairo_image_surface_get_format, \
       (cairo_surface_t *surface), (surface), CAIRO_FORMAT_INVALID) \
    m( cairo_surface_type_t, cairo_surface_get_type, \
       (cairo_surface_t *surface), (surface), -1) \
    wxCAIRO_PLATFORM_METHODS(m)

    
#if wxUSE_PANGO
#define wxFOR_ALL_PANGO_CAIRO_VOIDMETHODS(m) \
    m( pango_cairo_update_layout, \
        (cairo_t *cr, PangoLayout *layout), (cr, layout) ) \
    m( pango_cairo_show_layout, \
        (cairo_t *cr, PangoLayout *layout), (cr, layout) )
#endif

#define wxCAIRO_DECLARE_TYPE(rettype, name, args, argnames, defret) \
   typedef rettype (*wxCAIRO_METHOD_TYPE(name)) args ; \
   wxCAIRO_METHOD_TYPE(name) wxDL_METHOD_NAME(name);

#define wxCAIRO_DECLARE_VOIDTYPE(name, args, argnames) \
   wxCAIRO_DECLARE_TYPE(void, name, args, argnames, NULL)
   
wxFOR_ALL_CAIRO_VOIDMETHODS(wxCAIRO_DECLARE_VOIDTYPE)
wxFOR_ALL_CAIRO_METHODS(wxCAIRO_DECLARE_TYPE)


class wxCairo
{
public:
    static bool Initialize();

    // for internal use only
    static void CleanUp();

private:
    // the single wxCairo instance or NULL
    static wxCairo *ms_lib;

    wxCairo();
    ~wxCairo();

    bool IsOk();

    wxDynamicLibrary m_libCairo;
    wxDynamicLibrary m_libPangoCairo;

    // true if we successfully loaded the libraries and can use them
    //
    // note that this field must have this name as it's used by wxDL_XXX macros
    bool m_ok;

public:

    wxFOR_ALL_CAIRO_VOIDMETHODS(wxCAIRO_STATIC_VOIDMETHOD_DEFINE)
    wxFOR_ALL_CAIRO_METHODS(wxCAIRO_STATIC_METHOD_DEFINE)
#if wxUSE_PANGO // untested, uncomment to test compilation.
    //wxFOR_ALL_PANGO_METHODS(wxDL_STATIC_METHOD_DEFINE)
#endif

    wxDECLARE_NO_COPY_CLASS(wxCairo);
};

#define wxINIT_CAIRO_VOIDFUNC(name, params, args) \
    wxCAIRO_METHOD_TYPE(name) wxCairo::name = NULL;

#define wxINIT_CAIRO_FUNC(rettype, name, params, args, defret) \
    wxCAIRO_METHOD_TYPE(name) wxCairo::name = NULL;

wxFOR_ALL_CAIRO_VOIDMETHODS(wxINIT_CAIRO_VOIDFUNC)
wxFOR_ALL_CAIRO_METHODS(wxINIT_CAIRO_FUNC)

#undef wxINIT_CAIRO_FUNC

wxCairo *wxCairo::ms_lib = NULL;

//----------------------------------------------------------------------------
// wxCairoLibrary
//----------------------------------------------------------------------------

wxCairo::wxCairo()
{
    wxLogNull log;

#ifdef __WXMSW__
    wxString cairoDllStr("libcairo-2.dll");
#else
    wxString cairoDllStr("libcairo.so.2");
#endif
    m_libCairo.Load(cairoDllStr);
    m_ok = m_libCairo.IsLoaded();
    if ( !m_ok )
        return;

#if wxUSE_PANGO
    m_libPangoCairo.Load("libpangocairo-1.0.so.0");
    m_ok = m_libPangoCairo.IsLoaded();
    if ( !m_ok )
    {
        m_libCairo.Unload();
        return;
    }
#endif

    
#define wxDO_LOAD_FUNC(name, nameStr)                                     \
    name = (wxCAIRO_METHOD_TYPE(name))m_libCairo.RawGetSymbol(nameStr);      \
    if ( !name )                                                          \
        return;

#define wxLOAD_CAIRO_VOIDFUNC(name, params, args)                         \
    wxDO_LOAD_FUNC(name, wxSTRINGIZE_T(name))

#define wxLOAD_CAIRO_FUNC(rettype, name, params, args, defret)            \
    wxDO_LOAD_FUNC(name, wxSTRINGIZE_T(name))

wxFOR_ALL_CAIRO_VOIDMETHODS(wxLOAD_CAIRO_VOIDFUNC)
wxFOR_ALL_CAIRO_METHODS(wxLOAD_CAIRO_FUNC)

#undef wxLOAD_CAIRO_FUNC

    m_ok = true;
}

wxCairo::~wxCairo()
{
}

/* static */ bool wxCairo::Initialize()
{
    if ( !ms_lib )
    {
        ms_lib = new wxCairo();
        if ( !ms_lib->IsOk() )
        {
            delete ms_lib;
            ms_lib = NULL;
        }
    }

    return ms_lib != NULL;
}

/* static */ void wxCairo::CleanUp()
{
    if (ms_lib)
    {
        delete ms_lib;
        ms_lib = NULL;
    }
}

bool wxCairo::IsOk()
{
    return m_ok;
}

// ============================================================================
// implementation of the functions themselves
// ============================================================================

bool wxCairoInit()
{
    return wxCairo::Initialize();
}

extern "C"
{

#define wxIMPL_CAIRO_FUNC(rettype, name, params, args, defret)                \
    rettype name params                                                               \
    {                                                                         \
        wxASSERT_MSG(wxCairo::Initialize(), "Cairo not initialized");  \
        return wxCairo::name args;                                     \
    }

#define wxIMPL_CAIRO_VOIDFUNC(name, params, args) \
    wxIMPL_CAIRO_FUNC(void, name, params, args, NULL)

// we currently link directly to Cairo on GTK since it is usually available there,
// so don't use our cairo_xyz wrapper functions until the decision is made to
// always load Cairo dynamically there.
#ifndef __WXGTK__
wxFOR_ALL_CAIRO_VOIDMETHODS(wxIMPL_CAIRO_VOIDFUNC)
wxFOR_ALL_CAIRO_METHODS(wxIMPL_CAIRO_FUNC)
#endif

} // extern "C"

//----------------------------------------------------------------------------
// wxCairoModule
//----------------------------------------------------------------------------

class wxCairoModule : public wxModule
{
public:
    wxCairoModule() { }
    virtual bool OnInit();
    virtual void OnExit();

private:
    DECLARE_DYNAMIC_CLASS(wxCairoModule)
};

bool wxCairoModule::OnInit()
{
    return true;
}

void wxCairoModule::OnExit()
{
    wxCairo::CleanUp();
}

IMPLEMENT_DYNAMIC_CLASS(wxCairoModule, wxModule)

#endif // wxUSE_CAIRO
