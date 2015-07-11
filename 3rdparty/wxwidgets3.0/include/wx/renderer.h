///////////////////////////////////////////////////////////////////////////////
// Name:        wx/renderer.h
// Purpose:     wxRendererNative class declaration
// Author:      Vadim Zeitlin
// Modified by:
// Created:     20.07.2003
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

/*
   Renderers are used in wxWidgets for two similar but different things:
    (a) wxUniversal uses them to draw everything, i.e. all the control
    (b) all the native ports use them to draw generic controls only

   wxUniversal needs more functionality than what is included in the base class
   as it needs to draw stuff like scrollbars which are never going to be
   generic. So we put the bare minimum needed by the native ports here and the
   full wxRenderer class is declared in wx/univ/renderer.h and is only used by
   wxUniveral (although note that native ports can load wxRenderer objects from
   theme DLLs and use them as wxRendererNative ones, of course).
 */

#ifndef _WX_RENDERER_H_
#define _WX_RENDERER_H_

class WXDLLIMPEXP_FWD_CORE wxDC;
class WXDLLIMPEXP_FWD_CORE wxWindow;

#include "wx/gdicmn.h" // for wxPoint, wxSize
#include "wx/colour.h"
#include "wx/font.h"
#include "wx/bitmap.h"
#include "wx/string.h"

// some platforms have their own renderers, others use the generic one
#if defined(__WXMSW__) || ( defined(__WXMAC__) && wxOSX_USE_COCOA_OR_CARBON ) || defined(__WXGTK__)
    #define wxHAS_NATIVE_RENDERER
#else
    #undef wxHAS_NATIVE_RENDERER
#endif

// only MSW and OS X currently provides DrawTitleBarBitmap() method
#if defined(__WXMSW__) || (defined(__WXMAC__) && wxUSE_LIBPNG && wxUSE_IMAGE)
    #define wxHAS_DRAW_TITLE_BAR_BITMAP
#endif

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// control state flags used in wxRenderer and wxColourScheme
enum
{
    wxCONTROL_DISABLED   = 0x00000001,  // control is disabled
    wxCONTROL_FOCUSED    = 0x00000002,  // currently has keyboard focus
    wxCONTROL_PRESSED    = 0x00000004,  // (button) is pressed
    wxCONTROL_SPECIAL    = 0x00000008,  // control-specific bit:
    wxCONTROL_ISDEFAULT  = wxCONTROL_SPECIAL, // only for the buttons
    wxCONTROL_ISSUBMENU  = wxCONTROL_SPECIAL, // only for the menu items
    wxCONTROL_EXPANDED   = wxCONTROL_SPECIAL, // only for the tree items
    wxCONTROL_SIZEGRIP   = wxCONTROL_SPECIAL, // only for the status bar panes
    wxCONTROL_FLAT       = wxCONTROL_SPECIAL, // checkboxes only: flat border
    wxCONTROL_CURRENT    = 0x00000010,  // mouse is currently over the control
    wxCONTROL_SELECTED   = 0x00000020,  // selected item in e.g. listbox
    wxCONTROL_CHECKED    = 0x00000040,  // (check/radio button) is checked
    wxCONTROL_CHECKABLE  = 0x00000080,  // (menu) item can be checked
    wxCONTROL_UNDETERMINED = wxCONTROL_CHECKABLE, // (check) undetermined state

    wxCONTROL_FLAGS_MASK = 0x000000ff,

    // this is a pseudo flag not used directly by wxRenderer but rather by some
    // controls internally
    wxCONTROL_DIRTY      = 0x80000000
};

// title bar buttons supported by DrawTitleBarBitmap()
//
// NB: they have the same values as wxTOPLEVEL_BUTTON_XXX constants in
//     wx/univ/toplevel.h as they really represent the same things
enum wxTitleBarButton
{
    wxTITLEBAR_BUTTON_CLOSE    = 0x01000000,
    wxTITLEBAR_BUTTON_MAXIMIZE = 0x02000000,
    wxTITLEBAR_BUTTON_ICONIZE  = 0x04000000,
    wxTITLEBAR_BUTTON_RESTORE  = 0x08000000,
    wxTITLEBAR_BUTTON_HELP     = 0x10000000
};

// ----------------------------------------------------------------------------
// helper structs
// ----------------------------------------------------------------------------

// wxSplitterWindow parameters
struct WXDLLIMPEXP_CORE wxSplitterRenderParams
{
    // the only way to initialize this struct is by using this ctor
    wxSplitterRenderParams(wxCoord widthSash_, wxCoord border_, bool isSens_)
        : widthSash(widthSash_), border(border_), isHotSensitive(isSens_)
        {
        }

    // the width of the splitter sash
    const wxCoord widthSash;

    // the width of the border of the splitter window
    const wxCoord border;

    // true if the splitter changes its appearance when the mouse is over it
    const bool isHotSensitive;
};


// extra optional parameters for DrawHeaderButton
struct WXDLLIMPEXP_CORE wxHeaderButtonParams
{
    wxHeaderButtonParams()
        : m_labelAlignment(wxALIGN_LEFT)
    { }

    wxColour    m_arrowColour;
    wxColour    m_selectionColour;
    wxString    m_labelText;
    wxFont      m_labelFont;
    wxColour    m_labelColour;
    wxBitmap    m_labelBitmap;
    int         m_labelAlignment;
};

enum wxHeaderSortIconType
{
    wxHDR_SORT_ICON_NONE,        // Header button has no sort arrow
    wxHDR_SORT_ICON_UP,          // Header button an up sort arrow icon
    wxHDR_SORT_ICON_DOWN         // Header button a down sort arrow icon
};


// wxRendererNative interface version
struct WXDLLIMPEXP_CORE wxRendererVersion
{
    wxRendererVersion(int version_, int age_) : version(version_), age(age_) { }

    // default copy ctor, assignment operator and dtor are ok

    // the current version and age of wxRendererNative interface: different
    // versions are incompatible (in both ways) while the ages inside the same
    // version are upwards compatible, i.e. the version of the renderer must
    // match the version of the main program exactly while the age may be
    // highergreater or equal to it
    //
    // NB: don't forget to increment age after adding any new virtual function!
    enum
    {
        Current_Version = 1,
        Current_Age = 5
    };


    // check if the given version is compatible with the current one
    static bool IsCompatible(const wxRendererVersion& ver)
    {
        return ver.version == Current_Version && ver.age >= Current_Age;
    }

    const int version;
    const int age;
};

// ----------------------------------------------------------------------------
// wxRendererNative: abstracts drawing methods needed by the native controls
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxRendererNative
{
public:
    // drawing functions
    // -----------------

    // draw the header control button (used by wxListCtrl) Returns optimal
    // width for the label contents.
    virtual int  DrawHeaderButton(wxWindow *win,
                                  wxDC& dc,
                                  const wxRect& rect,
                                  int flags = 0,
                                  wxHeaderSortIconType sortArrow = wxHDR_SORT_ICON_NONE,
                                  wxHeaderButtonParams* params=NULL) = 0;


    // Draw the contents of a header control button (label, sort arrows, etc.)
    // Normally only called by DrawHeaderButton.
    virtual int  DrawHeaderButtonContents(wxWindow *win,
                                          wxDC& dc,
                                          const wxRect& rect,
                                          int flags = 0,
                                          wxHeaderSortIconType sortArrow = wxHDR_SORT_ICON_NONE,
                                          wxHeaderButtonParams* params=NULL) = 0;

    // Returns the default height of a header button, either a fixed platform
    // height if available, or a generic height based on the window's font.
    virtual int GetHeaderButtonHeight(wxWindow *win) = 0;

    // Returns the margin on left and right sides of header button's label
    virtual int GetHeaderButtonMargin(wxWindow *win) = 0;


    // draw the expanded/collapsed icon for a tree control item
    virtual void DrawTreeItemButton(wxWindow *win,
                                    wxDC& dc,
                                    const wxRect& rect,
                                    int flags = 0) = 0;

    // draw the border for sash window: this border must be such that the sash
    // drawn by DrawSash() blends into it well
    virtual void DrawSplitterBorder(wxWindow *win,
                                    wxDC& dc,
                                    const wxRect& rect,
                                    int flags = 0) = 0;

    // draw a (vertical) sash
    virtual void DrawSplitterSash(wxWindow *win,
                                  wxDC& dc,
                                  const wxSize& size,
                                  wxCoord position,
                                  wxOrientation orient,
                                  int flags = 0) = 0;

    // draw a combobox dropdown button
    //
    // flags may use wxCONTROL_PRESSED and wxCONTROL_CURRENT
    virtual void DrawComboBoxDropButton(wxWindow *win,
                                        wxDC& dc,
                                        const wxRect& rect,
                                        int flags = 0) = 0;

    // draw a dropdown arrow
    //
    // flags may use wxCONTROL_PRESSED and wxCONTROL_CURRENT
    virtual void DrawDropArrow(wxWindow *win,
                               wxDC& dc,
                               const wxRect& rect,
                               int flags = 0) = 0;

    // draw check button
    //
    // flags may use wxCONTROL_CHECKED, wxCONTROL_UNDETERMINED and wxCONTROL_CURRENT
    virtual void DrawCheckBox(wxWindow *win,
                              wxDC& dc,
                              const wxRect& rect,
                              int flags = 0) = 0;

    // Returns the default size of a check box.
    virtual wxSize GetCheckBoxSize(wxWindow *win) = 0;

    // draw blank button
    //
    // flags may use wxCONTROL_PRESSED, wxCONTROL_CURRENT and wxCONTROL_ISDEFAULT
    virtual void DrawPushButton(wxWindow *win,
                                wxDC& dc,
                                const wxRect& rect,
                                int flags = 0) = 0;

    // draw rectangle indicating that an item in e.g. a list control
    // has been selected or focused
    //
    // flags may use
    // wxCONTROL_SELECTED (item is selected, e.g. draw background)
    // wxCONTROL_CURRENT (item is the current item, e.g. dotted border)
    // wxCONTROL_FOCUSED (the whole control has focus, e.g. blue background vs. grey otherwise)
    virtual void DrawItemSelectionRect(wxWindow *win,
                                       wxDC& dc,
                                       const wxRect& rect,
                                       int flags = 0) = 0;

    // draw the focus rectangle around the label contained in the given rect
    //
    // only wxCONTROL_SELECTED makes sense in flags here
    virtual void DrawFocusRect(wxWindow* win,
                               wxDC& dc,
                               const wxRect& rect,
                               int flags = 0) = 0;

    // Draw a native wxChoice
    virtual void DrawChoice(wxWindow* win,
                            wxDC& dc,
                            const wxRect& rect,
                            int flags = 0) = 0;

    // Draw a native wxComboBox
    virtual void DrawComboBox(wxWindow* win,
                              wxDC& dc,
                              const wxRect& rect,
                              int flags = 0) = 0;

    // Draw a native wxTextCtrl frame
    virtual void DrawTextCtrl(wxWindow* win,
                              wxDC& dc,
                              const wxRect& rect,
                              int flags = 0) = 0;

    // Draw a native wxRadioButton bitmap
    virtual void DrawRadioBitmap(wxWindow* win,
                                 wxDC& dc,
                                 const wxRect& rect,
                                 int flags = 0) = 0;

#ifdef wxHAS_DRAW_TITLE_BAR_BITMAP
    // Draw one of the standard title bar buttons
    //
    // This is currently implemented only for MSW and OS X (for the close
    // button only) because there is no way to render standard title bar
    // buttons under the other platforms, the best can be done is to use normal
    // (only) images which wxArtProvider provides for wxART_HELP and
    // wxART_CLOSE (but not any other title bar buttons)
    //
    // NB: make sure PNG handler is enabled if using this function under OS X
    virtual void DrawTitleBarBitmap(wxWindow *win,
                                    wxDC& dc,
                                    const wxRect& rect,
                                    wxTitleBarButton button,
                                    int flags = 0) = 0;
#endif // wxHAS_DRAW_TITLE_BAR_BITMAP


    // geometry functions
    // ------------------

    // get the splitter parameters: the x field of the returned point is the
    // sash width and the y field is the border width
    virtual wxSplitterRenderParams GetSplitterParams(const wxWindow *win) = 0;


    // pseudo constructors
    // -------------------

    // return the currently used renderer
    static wxRendererNative& Get();

    // return the generic implementation of the renderer
    static wxRendererNative& GetGeneric();

    // return the default (native) implementation for this platform
    static wxRendererNative& GetDefault();


    // changing the global renderer
    // ----------------------------

#if wxUSE_DYNLIB_CLASS
    // load the renderer from the specified DLL, the returned pointer must be
    // deleted by caller if not NULL when it is not used any more
    static wxRendererNative *Load(const wxString& name);
#endif // wxUSE_DYNLIB_CLASS

    // set the renderer to use, passing NULL reverts to using the default
    // renderer
    //
    // return the previous renderer used with Set() or NULL if none
    static wxRendererNative *Set(wxRendererNative *renderer);


    // miscellaneous stuff
    // -------------------

    // this function is used for version checking: Load() refuses to load any
    // DLLs implementing an older or incompatible version; it should be
    // implemented simply by returning wxRendererVersion::Current_XXX values
    virtual wxRendererVersion GetVersion() const = 0;

    // virtual dtor for any base class
    virtual ~wxRendererNative();
};

// ----------------------------------------------------------------------------
// wxDelegateRendererNative: allows reuse of renderers code
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDelegateRendererNative : public wxRendererNative
{
public:
    wxDelegateRendererNative()
        : m_rendererNative(GetGeneric()) { }

    wxDelegateRendererNative(wxRendererNative& rendererNative)
        : m_rendererNative(rendererNative) { }


    virtual int  DrawHeaderButton(wxWindow *win,
                                  wxDC& dc,
                                  const wxRect& rect,
                                  int flags = 0,
                                  wxHeaderSortIconType sortArrow = wxHDR_SORT_ICON_NONE,
                                  wxHeaderButtonParams* params = NULL)
        { return m_rendererNative.DrawHeaderButton(win, dc, rect, flags, sortArrow, params); }

    virtual int  DrawHeaderButtonContents(wxWindow *win,
                                          wxDC& dc,
                                          const wxRect& rect,
                                          int flags = 0,
                                          wxHeaderSortIconType sortArrow = wxHDR_SORT_ICON_NONE,
                                          wxHeaderButtonParams* params = NULL)
        { return m_rendererNative.DrawHeaderButtonContents(win, dc, rect, flags, sortArrow, params); }

    virtual int GetHeaderButtonHeight(wxWindow *win)
        { return m_rendererNative.GetHeaderButtonHeight(win); }

    virtual int GetHeaderButtonMargin(wxWindow *win)
        { return m_rendererNative.GetHeaderButtonMargin(win); }

    virtual void DrawTreeItemButton(wxWindow *win,
                                    wxDC& dc,
                                    const wxRect& rect,
                                    int flags = 0)
        { m_rendererNative.DrawTreeItemButton(win, dc, rect, flags); }

    virtual void DrawSplitterBorder(wxWindow *win,
                                    wxDC& dc,
                                    const wxRect& rect,
                                    int flags = 0)
        { m_rendererNative.DrawSplitterBorder(win, dc, rect, flags); }

    virtual void DrawSplitterSash(wxWindow *win,
                                  wxDC& dc,
                                  const wxSize& size,
                                  wxCoord position,
                                  wxOrientation orient,
                                  int flags = 0)
        { m_rendererNative.DrawSplitterSash(win, dc, size,
                                            position, orient, flags); }

    virtual void DrawComboBoxDropButton(wxWindow *win,
                                        wxDC& dc,
                                        const wxRect& rect,
                                        int flags = 0)
        { m_rendererNative.DrawComboBoxDropButton(win, dc, rect, flags); }

    virtual void DrawDropArrow(wxWindow *win,
                               wxDC& dc,
                               const wxRect& rect,
                               int flags = 0)
        { m_rendererNative.DrawDropArrow(win, dc, rect, flags); }

    virtual void DrawCheckBox(wxWindow *win,
                              wxDC& dc,
                              const wxRect& rect,
                              int flags = 0)
        { m_rendererNative.DrawCheckBox( win, dc, rect, flags ); }

    virtual wxSize GetCheckBoxSize(wxWindow *win)
        { return m_rendererNative.GetCheckBoxSize(win); }

    virtual void DrawPushButton(wxWindow *win,
                                wxDC& dc,
                                const wxRect& rect,
                                int flags = 0)
        { m_rendererNative.DrawPushButton( win, dc, rect, flags ); }

    virtual void DrawItemSelectionRect(wxWindow *win,
                                       wxDC& dc,
                                       const wxRect& rect,
                                       int flags = 0)
        { m_rendererNative.DrawItemSelectionRect( win, dc, rect, flags ); }

    virtual void DrawFocusRect(wxWindow* win,
                               wxDC& dc,
                               const wxRect& rect,
                               int flags = 0)
        { m_rendererNative.DrawFocusRect( win, dc, rect, flags ); }

    virtual void DrawChoice(wxWindow* win,
                            wxDC& dc,
                            const wxRect& rect,
                            int flags = 0)
        { m_rendererNative.DrawChoice( win, dc, rect, flags); }

    virtual void DrawComboBox(wxWindow* win,
                              wxDC& dc,
                              const wxRect& rect,
                              int flags = 0)
        { m_rendererNative.DrawComboBox( win, dc, rect, flags); }

    virtual void DrawTextCtrl(wxWindow* win,
                              wxDC& dc,
                              const wxRect& rect,
                              int flags = 0)
        { m_rendererNative.DrawTextCtrl( win, dc, rect, flags); }

    virtual void DrawRadioBitmap(wxWindow* win,
                                 wxDC& dc,
                                 const wxRect& rect,
                                 int flags = 0)
        { m_rendererNative.DrawRadioBitmap(win, dc, rect, flags); }

#ifdef wxHAS_DRAW_TITLE_BAR_BITMAP
    virtual void DrawTitleBarBitmap(wxWindow *win,
                                    wxDC& dc,
                                    const wxRect& rect,
                                    wxTitleBarButton button,
                                    int flags = 0)
        { m_rendererNative.DrawTitleBarBitmap(win, dc, rect, button, flags); }
#endif // wxHAS_DRAW_TITLE_BAR_BITMAP

    virtual wxSplitterRenderParams GetSplitterParams(const wxWindow *win)
        { return m_rendererNative.GetSplitterParams(win); }

    virtual wxRendererVersion GetVersion() const
        { return m_rendererNative.GetVersion(); }

protected:
    wxRendererNative& m_rendererNative;

    wxDECLARE_NO_COPY_CLASS(wxDelegateRendererNative);
};

// ----------------------------------------------------------------------------
// inline functions implementation
// ----------------------------------------------------------------------------

#ifndef wxHAS_NATIVE_RENDERER

// default native renderer is the generic one then
/* static */ inline
wxRendererNative& wxRendererNative::GetDefault()
{
    return GetGeneric();
}

#endif // !wxHAS_NATIVE_RENDERER

#endif // _WX_RENDERER_H_
