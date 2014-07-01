/////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/private.h
// Purpose:     Private declarations for wxMotif port
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_H_
#define _WX_PRIVATE_H_

#include "wx/defs.h"
#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include "wx/evtloop.h"

class WXDLLIMPEXP_FWD_CORE wxFont;
class WXDLLIMPEXP_FWD_CORE wxWindow;
class WXDLLIMPEXP_FWD_CORE wxSize;
class WXDLLIMPEXP_FWD_CORE wxBitmap;
class WXDLLIMPEXP_FWD_CORE wxColour;

#include "wx/x11/privx.h"

// Put any private declarations here: native Motif types may be used because
// this header is included after Xm/Xm.h

// ----------------------------------------------------------------------------
// convenience macros
// ----------------------------------------------------------------------------

#define wxCHECK_MOTIF_VERSION( major, minor ) \
  ( XmVersion >= (major) * 1000 + (minor) )

#define wxCHECK_LESSTIF_VERSION( major, minor ) \
  ( LesstifVersion >= (major) * 1000 + (minor) )

#define wxCHECK_LESSTIF() ( __WXLESSTIF__ )

// some compilers (e.g. Sun CC) give warnings when treating string literals as
// (non const) "char *" but many Motif functions take "char *" parameters which
// are really "const char *" so use this macro to suppress the warnings when we
// know it's ok
#define wxMOTIF_STR(x) const_cast<char *>(x)

// ----------------------------------------------------------------------------
// Miscellaneous functions
// ----------------------------------------------------------------------------

WXWidget wxCreateBorderWidget( WXWidget parent, long style );

// ----------------------------------------------------------------------------
// common callbacks
// ----------------------------------------------------------------------------

// All widgets should have this as their resize proc.
extern void wxWidgetResizeProc(Widget w, XConfigureEvent *event,
                               String args[], int *num_args);

// For repainting arbitrary windows
void wxUniversalRepaintProc(Widget w, XtPointer WXUNUSED(c_data),
                            XEvent *event, char *);

// ----------------------------------------------------------------------------
// we maintain a hash table which contains the mapping from Widget to wxWindow
// corresponding to the window for this widget
// ----------------------------------------------------------------------------

extern void wxDeleteWindowFromTable(Widget w);
extern wxWindow *wxGetWindowFromTable(Widget w);
extern bool wxAddWindowToTable(Widget w, wxWindow *win);

// ----------------------------------------------------------------------------
// wxBitmap related functions
// ----------------------------------------------------------------------------

// Creates a bitmap with transparent areas drawn in the given colour.
wxBitmap wxCreateMaskedBitmap(const wxBitmap& bitmap, const wxColour& colour);

// ----------------------------------------------------------------------------
// key events related functions
// ----------------------------------------------------------------------------

extern char wxFindMnemonic(const char* s);

extern char * wxFindAccelerator (const char *s);
extern XmString wxFindAcceleratorText (const char *s);

// ----------------------------------------------------------------------------
// TranslateXXXEvent() functions - translate Motif event to wxWindow one
// ----------------------------------------------------------------------------

extern bool wxTranslateMouseEvent(wxMouseEvent& wxevent, wxWindow *win,
                                  Widget widget, const XEvent *xevent);
extern bool wxTranslateKeyEvent(wxKeyEvent& wxevent, wxWindow *win,
                                Widget widget, const XEvent *xevent);

extern void wxDoChangeForegroundColour(WXWidget widget,
                                       wxColour& foregroundColour);
extern void wxDoChangeBackgroundColour(WXWidget widget,
                                       const wxColour& backgroundColour,
                                       bool changeArmColour = false);
extern void wxDoChangeFont(WXWidget widget, const wxFont& font);
extern void wxGetTextExtent(WXDisplay* display, const wxFont& font,
                            double scale,
                            const wxString& string, int* width, int* height,
                            int* ascent, int* descent);
extern void wxGetTextExtent(const wxWindow* window, const wxString& str,
                            int* width, int* height,
                            int* ascent, int* descent);

#define wxNO_COLORS   0x00
#define wxBACK_COLORS 0x01
#define wxFORE_COLORS 0x02

extern XColor itemColors[5] ;

#define wxBACK_INDEX 0
#define wxFORE_INDEX 1
#define wxSELE_INDEX 2
#define wxTOPS_INDEX 3
#define wxBOTS_INDEX 4

// ----------------------------------------------------------------------------
// XmString/wxString conversion utilities
// ----------------------------------------------------------------------------

wxString wxXmStringToString( const XmString& xmString );
XmString wxStringToXmString( const char* string );
inline XmString wxStringToXmString( const wxScopedCharBuffer& string )
    { return wxStringToXmString(string.data()); }
inline XmString wxStringToXmString( const wxString& string )
    { return wxStringToXmString((const char*)string.mb_str()); }

// XmString made easy to use in wxWidgets (and has an added benefit of
// cleaning up automatically)
class wxXmString
{
    void Init(const char *str)
    {
        m_string = XmStringCreateLtoR
                   (
                    const_cast<char *>(str),
                    const_cast<char *>(XmSTRING_DEFAULT_CHARSET)
                   );
    }

public:
    wxXmString(const char* str)
    {
        Init(str);
    }

    wxXmString(const wchar_t* str)
    {
        Init(wxConvLibc.cWC2MB(str));
    }

    wxXmString(const wxString& str)
    {
        Init(str.mb_str());
    }

    wxXmString(const wxCStrData& str)
    {
        Init(str);
    }

    // just to avoid calling XmStringFree()
    wxXmString(const XmString& string) { m_string = string; }

    ~wxXmString() { XmStringFree(m_string); }

    // semi-implicit conversion to XmString (shouldn't rely on implicit
    // conversion because many of Motif functions are macros)
    XmString operator()() const { return m_string; }

private:
    XmString m_string;
};

// ----------------------------------------------------------------------------
// Routines used in both wxTextCtrl/wxListBox and nativa wxComboBox
// (defined in src/motif/listbox.cpp or src/motif/textctrl.cpp
// ----------------------------------------------------------------------------

int wxDoFindStringInList( Widget listWidget, const wxString& str );
int wxDoGetSelectionInList( Widget listWidget );
wxString wxDoGetStringInList( Widget listWidget, int n );
wxSize wxDoGetListBoxBestSize( Widget listWidget, const wxWindow* window );

wxSize wxDoGetSingleTextCtrlBestSize( Widget textWidget,
                                      const wxWindow* window );

// ----------------------------------------------------------------------------
// event-related functions
// ----------------------------------------------------------------------------

// executes one main loop iteration (implemented in src/motif/evtloop.cpp)
// returns true if the loop should be exited
bool wxDoEventLoopIteration( wxGUIEventLoop& evtLoop );

// Consume all events until no more left
void wxFlushEvents(WXDisplay* display);

// ----------------------------------------------------------------------------
// macros to avoid casting WXFOO to Foo all the time
// ----------------------------------------------------------------------------

// argument is of type "wxWindow *"
#define GetWidget(w)    ((Widget)(w)->GetHandle())

// ----------------------------------------------------------------------------
// accessors for C modules
// ----------------------------------------------------------------------------

extern "C" XtAppContext wxGetAppContext();

#endif
// _WX_PRIVATE_H_
