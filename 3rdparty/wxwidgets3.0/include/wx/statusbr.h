/////////////////////////////////////////////////////////////////////////////
// Name:        wx/statusbr.h
// Purpose:     wxStatusBar class interface
// Author:      Vadim Zeitlin
// Modified by:
// Created:     05.02.00
// Copyright:   (c) Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_STATUSBR_H_BASE_
#define _WX_STATUSBR_H_BASE_

#include "wx/defs.h"

#if wxUSE_STATUSBAR

#include "wx/control.h"
#include "wx/list.h"
#include "wx/dynarray.h"

extern WXDLLIMPEXP_DATA_CORE(const char) wxStatusBarNameStr[];

// ----------------------------------------------------------------------------
// wxStatusBar constants
// ----------------------------------------------------------------------------

// wxStatusBar styles
#define wxSTB_SIZEGRIP         0x0010
#define wxSTB_SHOW_TIPS        0x0020

#define wxSTB_ELLIPSIZE_START   0x0040
#define wxSTB_ELLIPSIZE_MIDDLE  0x0080
#define wxSTB_ELLIPSIZE_END     0x0100

#define wxSTB_DEFAULT_STYLE    (wxSTB_SIZEGRIP|wxSTB_ELLIPSIZE_END|wxSTB_SHOW_TIPS|wxFULL_REPAINT_ON_RESIZE)


// old compat style name:
#define wxST_SIZEGRIP    wxSTB_SIZEGRIP


// style flags for wxStatusBar fields
#define wxSB_NORMAL    0x0000
#define wxSB_FLAT      0x0001
#define wxSB_RAISED    0x0002
#define wxSB_SUNKEN    0x0003

// ----------------------------------------------------------------------------
// wxStatusBarPane: an helper for wxStatusBar
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxStatusBarPane
{
public:
    wxStatusBarPane(int style = wxSB_NORMAL, int width = 0)
        : m_nStyle(style), m_nWidth(width)
        { m_bEllipsized = false; }

    int GetWidth() const { return m_nWidth; }
    int GetStyle() const { return m_nStyle; }
    wxString GetText() const { return m_text; }


    // implementation-only from now on
    // -------------------------------

    bool IsEllipsized() const
        { return m_bEllipsized; }
    void SetIsEllipsized(bool isEllipsized) { m_bEllipsized = isEllipsized; }

    void SetWidth(int width) { m_nWidth = width; }
    void SetStyle(int style) { m_nStyle = style; }

    // set text, return true if it changed or false if it was already set to
    // this value
    bool SetText(const wxString& text);

    // save the existing text on top of our stack and make the new text
    // current; return true if the text really changed
    bool PushText(const wxString& text);

    // restore the message saved by the last call to Push() (unless it was
    // changed by an intervening call to SetText()) and return true if we
    // really restored anything
    bool PopText();

private:
    int m_nStyle;
    int m_nWidth;     // may be negative, indicating a variable-width field
    wxString m_text;

    // the array used to keep the previous values of this pane after a
    // PushStatusText() call, its top element is the value to restore after the
    // next PopStatusText() call while the currently shown value is always in
    // m_text
    wxArrayString m_arrStack;

    // is the currently shown value shown with ellipsis in the status bar?
    bool m_bEllipsized;
};

WX_DECLARE_EXPORTED_OBJARRAY(wxStatusBarPane, wxStatusBarPaneArray);

// ----------------------------------------------------------------------------
// wxStatusBar: a window near the bottom of the frame used for status info
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxStatusBarBase : public wxControl
{
public:
    wxStatusBarBase();

    virtual ~wxStatusBarBase();

    // field count
    // -----------

    // set the number of fields and call SetStatusWidths(widths) if widths are
    // given
    virtual void SetFieldsCount(int number = 1, const int *widths = NULL);
    int GetFieldsCount() const { return (int)m_panes.GetCount(); }

    // field text
    // ----------

    // just change or get the currently shown text
    void SetStatusText(const wxString& text, int number = 0);
    wxString GetStatusText(int number = 0) const;

    // change the currently shown text to the new one and save the current
    // value to be restored by the next call to PopStatusText()
    void PushStatusText(const wxString& text, int number = 0);
    void PopStatusText(int number = 0);

    // fields widths
    // -------------

    // set status field widths as absolute numbers: positive widths mean that
    // the field has the specified absolute width, negative widths are
    // interpreted as the sizer options, i.e. the extra space (total space
    // minus the sum of fixed width fields) is divided between the fields with
    // negative width according to the abs value of the width (field with width
    // -2 grows twice as much as one with width -1 &c)
    virtual void SetStatusWidths(int n, const int widths[]);

    int GetStatusWidth(int n) const
        { return m_panes[n].GetWidth(); }

    // field styles
    // ------------

    // Set the field border style to one of wxSB_XXX values.
    virtual void SetStatusStyles(int n, const int styles[]);

    int GetStatusStyle(int n) const
        { return m_panes[n].GetStyle(); }

    // geometry
    // --------

    // Get the position and size of the field's internal bounding rectangle
    virtual bool GetFieldRect(int i, wxRect& rect) const = 0;

    // sets the minimal vertical size of the status bar
    virtual void SetMinHeight(int height) = 0;

    // get the dimensions of the horizontal and vertical borders
    virtual int GetBorderX() const = 0;
    virtual int GetBorderY() const = 0;

    wxSize GetBorders() const
        { return wxSize(GetBorderX(), GetBorderY()); }

    // miscellaneous
    // -------------

    const wxStatusBarPane& GetField(int n) const
        { return m_panes[n]; }

    // wxWindow overrides:

    // don't want status bars to accept the focus at all
    virtual bool AcceptsFocus() const { return false; }

    // the client size of a toplevel window doesn't include the status bar
    virtual bool CanBeOutsideClientArea() const { return true; }

protected:
    // called after the status bar pane text changed and should update its
    // display
    virtual void DoUpdateStatusText(int number) = 0;


    // wxWindow overrides:

#if wxUSE_TOOLTIPS
   virtual void DoSetToolTip( wxToolTip *tip )
        {
            wxASSERT_MSG(!HasFlag(wxSTB_SHOW_TIPS),
                         "Do not set tooltip(s) manually when using wxSTB_SHOW_TIPS!");
            wxWindow::DoSetToolTip(tip);
        }
#endif // wxUSE_TOOLTIPS
    virtual wxBorder GetDefaultBorder() const { return wxBORDER_NONE; }


    // internal helpers & data:

    // calculate the real field widths for the given total available size
    wxArrayInt CalculateAbsWidths(wxCoord widthTotal) const;

    // should be called to remember if the pane text is currently being show
    // ellipsized or not
    void SetEllipsizedFlag(int n, bool isEllipsized);


    // the array with the pane infos:
    wxStatusBarPaneArray m_panes;

    // if true overrides the width info of the wxStatusBarPanes
    bool m_bSameWidthForAllPanes;

    wxDECLARE_NO_COPY_CLASS(wxStatusBarBase);
};

// ----------------------------------------------------------------------------
// include the actual wxStatusBar class declaration
// ----------------------------------------------------------------------------

#if defined(__WXUNIVERSAL__)
    #define wxStatusBarUniv wxStatusBar
    #include "wx/univ/statusbr.h"
#elif defined(__WXMSW__) && wxUSE_NATIVE_STATUSBAR
    #include "wx/msw/statusbar.h"
#elif defined(__WXMAC__)
    #define wxStatusBarMac wxStatusBar
    #include "wx/generic/statusbr.h"
    #include "wx/osx/statusbr.h"
#else
    #define wxStatusBarGeneric wxStatusBar
    #include "wx/generic/statusbr.h"
#endif

#endif // wxUSE_STATUSBAR

#endif
    // _WX_STATUSBR_H_BASE_
