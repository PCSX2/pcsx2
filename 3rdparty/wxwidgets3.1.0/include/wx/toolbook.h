///////////////////////////////////////////////////////////////////////////////
// Name:        wx/toolbook.h
// Purpose:     wxToolbook: wxToolBar and wxNotebook combination
// Author:      Julian Smart
// Modified by:
// Created:     2006-01-29
// Copyright:   (c) 2006 Julian Smart
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TOOLBOOK_H_
#define _WX_TOOLBOOK_H_

#include "wx/defs.h"

#if wxUSE_TOOLBOOK

#include "wx/bookctrl.h"
#include "wx/containr.h"

class WXDLLIMPEXP_FWD_CORE wxToolBarBase;
class WXDLLIMPEXP_FWD_CORE wxCommandEvent;

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_TOOLBOOK_PAGE_CHANGED,  wxBookCtrlEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_TOOLBOOK_PAGE_CHANGING, wxBookCtrlEvent );


// Use wxButtonToolBar
#define wxTBK_BUTTONBAR            0x0100

// Use wxTB_HORZ_LAYOUT style for the controlling toolbar
#define wxTBK_HORZ_LAYOUT          0x8000

// deprecated synonym, don't use
#if WXWIN_COMPATIBILITY_2_8
    #define wxBK_BUTTONBAR wxTBK_BUTTONBAR
#endif

// ----------------------------------------------------------------------------
// wxToolbook
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxToolbook : public wxNavigationEnabled<wxBookCtrlBase>
{
public:
    wxToolbook()
    {
        Init();
    }

    wxToolbook(wxWindow *parent,
               wxWindowID id,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = 0,
               const wxString& name = wxEmptyString)
    {
        Init();

        (void)Create(parent, id, pos, size, style, name);
    }

    // quasi ctor
    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxEmptyString);


    // implement base class virtuals
    virtual bool SetPageText(size_t n, const wxString& strText) wxOVERRIDE;
    virtual wxString GetPageText(size_t n) const wxOVERRIDE;
    virtual int GetPageImage(size_t n) const wxOVERRIDE;
    virtual bool SetPageImage(size_t n, int imageId) wxOVERRIDE;
    virtual bool InsertPage(size_t n,
                            wxWindow *page,
                            const wxString& text,
                            bool bSelect = false,
                            int imageId = NO_IMAGE) wxOVERRIDE;
    virtual int SetSelection(size_t n) wxOVERRIDE { return DoSetSelection(n, SetSelection_SendEvent); }
    virtual int ChangeSelection(size_t n) wxOVERRIDE { return DoSetSelection(n); }
    virtual void SetImageList(wxImageList *imageList) wxOVERRIDE;

    virtual bool DeleteAllPages() wxOVERRIDE;
    virtual int HitTest(const wxPoint& pt, long *flags = NULL) const wxOVERRIDE;


    // methods which are not part of base wxBookctrl API

    // get the underlying toolbar
    wxToolBarBase* GetToolBar() const { return (wxToolBarBase*)m_bookctrl; }

    // must be called in OnIdle or by application to realize the toolbar and
    // select the initial page.
    void Realize();

protected:
    virtual wxWindow *DoRemovePage(size_t page) wxOVERRIDE;

    // event handlers
    void OnToolSelected(wxCommandEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnIdle(wxIdleEvent& event);

    void UpdateSelectedPage(size_t newsel) wxOVERRIDE;

    wxBookCtrlEvent* CreatePageChangingEvent() const wxOVERRIDE;
    void MakeChangedEvent(wxBookCtrlEvent &event) wxOVERRIDE;

    // whether the toolbar needs to be realized
    bool m_needsRealizing;

    // maximum bitmap size
    wxSize m_maxBitmapSize;

private:
    // common part of all constructors
    void Init();

    wxDECLARE_EVENT_TABLE();
    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxToolbook);
};

// ----------------------------------------------------------------------------
// listbook event class and related stuff
// ----------------------------------------------------------------------------

// wxToolbookEvent is obsolete and defined for compatibility only
#define wxToolbookEvent wxBookCtrlEvent
typedef wxBookCtrlEventFunction wxToolbookEventFunction;
#define wxToolbookEventHandler(func) wxBookCtrlEventHandler(func)


#define EVT_TOOLBOOK_PAGE_CHANGED(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_TOOLBOOK_PAGE_CHANGED, winid, wxBookCtrlEventHandler(fn))

#define EVT_TOOLBOOK_PAGE_CHANGING(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_TOOLBOOK_PAGE_CHANGING, winid, wxBookCtrlEventHandler(fn))

// old wxEVT_COMMAND_* constants
#define wxEVT_COMMAND_TOOLBOOK_PAGE_CHANGED    wxEVT_TOOLBOOK_PAGE_CHANGED
#define wxEVT_COMMAND_TOOLBOOK_PAGE_CHANGING   wxEVT_TOOLBOOK_PAGE_CHANGING

#endif // wxUSE_TOOLBOOK

#endif // _WX_TOOLBOOK_H_
