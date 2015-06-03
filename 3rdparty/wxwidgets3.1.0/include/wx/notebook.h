///////////////////////////////////////////////////////////////////////////////
// Name:        wx/notebook.h
// Purpose:     wxNotebook interface
// Author:      Vadim Zeitlin
// Modified by:
// Created:     01.02.01
// Copyright:   (c) 1996-2000 Vadim Zeitlin
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_NOTEBOOK_H_BASE_
#define _WX_NOTEBOOK_H_BASE_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/defs.h"

#if wxUSE_NOTEBOOK

#include "wx/bookctrl.h"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// wxNotebook hit results, use wxBK_HITTEST so other book controls can share them
// if wxUSE_NOTEBOOK is disabled
enum
{
    wxNB_HITTEST_NOWHERE = wxBK_HITTEST_NOWHERE,
    wxNB_HITTEST_ONICON  = wxBK_HITTEST_ONICON,
    wxNB_HITTEST_ONLABEL = wxBK_HITTEST_ONLABEL,
    wxNB_HITTEST_ONITEM  = wxBK_HITTEST_ONITEM,
    wxNB_HITTEST_ONPAGE  = wxBK_HITTEST_ONPAGE
};

// wxNotebook flags

// use common book wxBK_* flags for describing alignment
#define wxNB_DEFAULT          wxBK_DEFAULT
#define wxNB_TOP              wxBK_TOP
#define wxNB_BOTTOM           wxBK_BOTTOM
#define wxNB_LEFT             wxBK_LEFT
#define wxNB_RIGHT            wxBK_RIGHT

#define wxNB_FIXEDWIDTH       0x0100
#define wxNB_MULTILINE        0x0200
#define wxNB_NOPAGETHEME      0x0400
#define wxNB_FLAT             0x0800


typedef wxWindow wxNotebookPage;  // so far, any window can be a page

extern WXDLLIMPEXP_DATA_CORE(const char) wxNotebookNameStr[];

#if wxUSE_EXTENDED_RTTI

// ----------------------------------------------------------------------------
// XTI accessor
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxNotebookPageInfo : public wxObject
{
public:
    wxNotebookPageInfo() { m_page = NULL; m_imageId = -1; m_selected = false; }
    virtual ~wxNotebookPageInfo() { }
    
    bool Create(wxNotebookPage *page,
                const wxString& text,
                bool selected,
                int imageId)
    {
        m_page = page;
        m_text = text;
        m_selected = selected;
        m_imageId = imageId;
        return true;
    }
    
    wxNotebookPage* GetPage() const { return m_page; }
    wxString GetText() const { return m_text; }
    bool GetSelected() const { return m_selected; }
    int GetImageId() const { return m_imageId; }
    
private:
    wxNotebookPage *m_page;
    wxString m_text;
    bool m_selected;
    int m_imageId;
    
    wxDECLARE_DYNAMIC_CLASS(wxNotebookPageInfo);
};

WX_DECLARE_EXPORTED_LIST(wxNotebookPageInfo, wxNotebookPageInfoList );

#endif

// ----------------------------------------------------------------------------
// wxNotebookBase: define wxNotebook interface
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxNotebookBase : public wxBookCtrlBase
{
public:
    // ctors
    // -----

    wxNotebookBase() { }

    // wxNotebook-specific additions to wxBookCtrlBase interface
    // ---------------------------------------------------------

    // get the number of rows for a control with wxNB_MULTILINE style (not all
    // versions support it - they will always return 1 then)
    virtual int GetRowCount() const { return 1; }

    // set the padding between tabs (in pixels)
    virtual void SetPadding(const wxSize& padding) = 0;

    // set the size of the tabs for wxNB_FIXEDWIDTH controls
    virtual void SetTabSize(const wxSize& sz) = 0;



    // implement some base class functions
    virtual wxSize CalcSizeFromPage(const wxSize& sizePage) const wxOVERRIDE;

    // On platforms that support it, get the theme page background colour, else invalid colour
    virtual wxColour GetThemeBackgroundColour() const { return wxNullColour; }


    // send wxEVT_NOTEBOOK_PAGE_CHANGING/ED events

    // returns false if the change to nPage is vetoed by the program
    bool SendPageChangingEvent(int nPage);

    // sends the event about page change from old to new (or GetSelection() if
    // new is wxNOT_FOUND)
    void SendPageChangedEvent(int nPageOld, int nPageNew = wxNOT_FOUND);

    // wxBookCtrlBase overrides this method to return false but we do need
    // focus because we have tabs
    virtual bool AcceptsFocus() const wxOVERRIDE { return wxControl::AcceptsFocus(); }

#if wxUSE_EXTENDED_RTTI    
    // XTI accessors
    virtual void AddPageInfo( wxNotebookPageInfo* info );
    virtual const wxNotebookPageInfoList& GetPageInfos() const;
#endif
        
protected:
#if wxUSE_EXTENDED_RTTI    
    wxNotebookPageInfoList m_pageInfos;
#endif    
    wxDECLARE_NO_COPY_CLASS(wxNotebookBase);
};

// ----------------------------------------------------------------------------
// notebook event class and related stuff
// ----------------------------------------------------------------------------

// wxNotebookEvent is obsolete and defined for compatibility only (notice that
// we use #define and not typedef to also keep compatibility with the existing
// code which forward declares it)
#define wxNotebookEvent wxBookCtrlEvent
typedef wxBookCtrlEventFunction wxNotebookEventFunction;
#define wxNotebookEventHandler(func) wxBookCtrlEventHandler(func)

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_NOTEBOOK_PAGE_CHANGED, wxBookCtrlEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_NOTEBOOK_PAGE_CHANGING, wxBookCtrlEvent );

#define EVT_NOTEBOOK_PAGE_CHANGED(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_NOTEBOOK_PAGE_CHANGED, winid, wxBookCtrlEventHandler(fn))

#define EVT_NOTEBOOK_PAGE_CHANGING(winid, fn) \
    wx__DECLARE_EVT1(wxEVT_NOTEBOOK_PAGE_CHANGING, winid, wxBookCtrlEventHandler(fn))

// ----------------------------------------------------------------------------
// wxNotebook class itself
// ----------------------------------------------------------------------------

#if defined(__WXUNIVERSAL__)
    #include "wx/univ/notebook.h"
#elif defined(__WXMSW__)
    #include  "wx/msw/notebook.h"
#elif defined(__WXMOTIF__)
    #include  "wx/generic/notebook.h"
#elif defined(__WXGTK20__)
    #include  "wx/gtk/notebook.h"
#elif defined(__WXGTK__)
    #include  "wx/gtk1/notebook.h"
#elif defined(__WXMAC__)
    #include  "wx/osx/notebook.h"
#elif defined(__WXQT__)
    #include "wx/qt/notebook.h"
#endif

// old wxEVT_COMMAND_* constants
#define wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED    wxEVT_NOTEBOOK_PAGE_CHANGED
#define wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGING   wxEVT_NOTEBOOK_PAGE_CHANGING

#endif // wxUSE_NOTEBOOK

#endif
    // _WX_NOTEBOOK_H_BASE_
