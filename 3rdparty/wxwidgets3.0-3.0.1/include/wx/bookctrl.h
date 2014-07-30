///////////////////////////////////////////////////////////////////////////////
// Name:        wx/bookctrl.h
// Purpose:     wxBookCtrlBase: common base class for wxList/Tree/Notebook
// Author:      Vadim Zeitlin
// Modified by:
// Created:     19.08.03
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_BOOKCTRL_H_
#define _WX_BOOKCTRL_H_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/defs.h"

#if wxUSE_BOOKCTRL

#include "wx/control.h"
#include "wx/dynarray.h"
#include "wx/withimages.h"

WX_DEFINE_EXPORTED_ARRAY_PTR(wxWindow *, wxArrayPages);

class WXDLLIMPEXP_FWD_CORE wxImageList;
class WXDLLIMPEXP_FWD_CORE wxBookCtrlEvent;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// wxBookCtrl hit results
enum
{
    wxBK_HITTEST_NOWHERE = 1,   // not on tab
    wxBK_HITTEST_ONICON  = 2,   // on icon
    wxBK_HITTEST_ONLABEL = 4,   // on label
    wxBK_HITTEST_ONITEM  = wxBK_HITTEST_ONICON | wxBK_HITTEST_ONLABEL,
    wxBK_HITTEST_ONPAGE  = 8    // not on tab control, but over the selected page
};

// wxBookCtrl flags (common for wxNotebook, wxListbook, wxChoicebook, wxTreebook)
#define wxBK_DEFAULT          0x0000
#define wxBK_TOP              0x0010
#define wxBK_BOTTOM           0x0020
#define wxBK_LEFT             0x0040
#define wxBK_RIGHT            0x0080
#define wxBK_ALIGN_MASK       (wxBK_TOP | wxBK_BOTTOM | wxBK_LEFT | wxBK_RIGHT)

// ----------------------------------------------------------------------------
// wxBookCtrlBase
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxBookCtrlBase : public wxControl,
                                        public wxWithImages
{
public:
    // construction
    // ------------

    wxBookCtrlBase()
    {
        Init();
    }

    wxBookCtrlBase(wxWindow *parent,
                   wxWindowID winid,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   long style = 0,
                   const wxString& name = wxEmptyString)
    {
        Init();

        (void)Create(parent, winid, pos, size, style, name);
    }

    // quasi ctor
    bool Create(wxWindow *parent,
                wxWindowID winid,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxEmptyString);


    // accessors
    // ---------

    // get number of pages in the dialog
    virtual size_t GetPageCount() const { return m_pages.size(); }

    // get the panel which represents the given page
    virtual wxWindow *GetPage(size_t n) const { return m_pages[n]; }

    // get the current page or NULL if none
    wxWindow *GetCurrentPage() const
    {
        const int n = GetSelection();
        return n == wxNOT_FOUND ? NULL : GetPage(n);
    }

    // get the currently selected page or wxNOT_FOUND if none
    virtual int GetSelection() const { return m_selection; }

    // set/get the title of a page
    virtual bool SetPageText(size_t n, const wxString& strText) = 0;
    virtual wxString GetPageText(size_t n) const = 0;


    // image list stuff: each page may have an image associated with it (all
    // images belong to the same image list)
    // ---------------------------------------------------------------------

    // sets/returns item's image index in the current image list
    virtual int GetPageImage(size_t n) const = 0;
    virtual bool SetPageImage(size_t n, int imageId) = 0;


    // geometry
    // --------

    // resize the notebook so that all pages will have the specified size
    virtual void SetPageSize(const wxSize& size);

    // return the size of the area needed to accommodate the controller
    wxSize GetControllerSize() const;

    // calculate the size of the control from the size of its page
    //
    // by default this simply returns size enough to fit both the page and the
    // controller
    virtual wxSize CalcSizeFromPage(const wxSize& sizePage) const;

    // get/set size of area between book control area and page area
    unsigned int GetInternalBorder() const { return m_internalBorder; }
    void SetInternalBorder(unsigned int border) { m_internalBorder = border; }

    // Sets/gets the margin around the controller
    void SetControlMargin(int margin) { m_controlMargin = margin; }
    int GetControlMargin() const { return m_controlMargin; }

    // returns true if we have wxBK_TOP or wxBK_BOTTOM style
    bool IsVertical() const { return HasFlag(wxBK_BOTTOM | wxBK_TOP); }

    // set/get option to shrink to fit current page
    void SetFitToCurrentPage(bool fit) { m_fitToCurrentPage = fit; }
    bool GetFitToCurrentPage() const { return m_fitToCurrentPage; }

    // returns the sizer containing the control, if any
    wxSizer* GetControlSizer() const { return m_controlSizer; }


    // operations
    // ----------

    // remove one page from the control and delete it
    virtual bool DeletePage(size_t n);

    // remove one page from the notebook, without deleting it
    virtual bool RemovePage(size_t n)
    {
        DoInvalidateBestSize();
        return DoRemovePage(n) != NULL;
    }

    // remove all pages and delete them
    virtual bool DeleteAllPages()
    {
        m_selection = wxNOT_FOUND;
        DoInvalidateBestSize();
        WX_CLEAR_ARRAY(m_pages);
        return true;
    }

    // adds a new page to the control
    virtual bool AddPage(wxWindow *page,
                         const wxString& text,
                         bool bSelect = false,
                         int imageId = NO_IMAGE)
    {
        DoInvalidateBestSize();
        return InsertPage(GetPageCount(), page, text, bSelect, imageId);
    }

    // the same as AddPage(), but adds the page at the specified position
    virtual bool InsertPage(size_t n,
                            wxWindow *page,
                            const wxString& text,
                            bool bSelect = false,
                            int imageId = NO_IMAGE) = 0;

    // set the currently selected page, return the index of the previously
    // selected one (or wxNOT_FOUND on error)
    //
    // NB: this function will generate PAGE_CHANGING/ED events
    virtual int SetSelection(size_t n) = 0;

    // acts as SetSelection but does not generate events
    virtual int ChangeSelection(size_t n) = 0;


    // cycle thru the pages
    void AdvanceSelection(bool forward = true)
    {
        int nPage = GetNextPage(forward);
        if ( nPage != wxNOT_FOUND )
        {
            // cast is safe because of the check above
            SetSelection((size_t)nPage);
        }
    }

    // return the index of the given page or wxNOT_FOUND
    int FindPage(const wxWindow* page) const;

    // hit test: returns which page is hit and, optionally, where (icon, label)
    virtual int HitTest(const wxPoint& WXUNUSED(pt),
                        long * WXUNUSED(flags) = NULL) const
    {
        return wxNOT_FOUND;
    }


    // we do have multiple pages
    virtual bool HasMultiplePages() const { return true; }

    // we don't want focus for ourselves
    virtual bool AcceptsFocus() const { return false; }

    // returns true if the platform should explicitly apply a theme border
    virtual bool CanApplyThemeBorder() const { return false; }

protected:
    // flags for DoSetSelection()
    enum
    {
        SetSelection_SendEvent = 1
    };

    // choose the default border for this window
    virtual wxBorder GetDefaultBorder() const { return wxBORDER_NONE; }

    // After the insertion of the page in the method InsertPage, calling this
    // method sets the selection to the given page or the first one if there is
    // still no selection. The "selection changed" event is sent only if
    // bSelect is true, so when it is false, no event is sent even if the
    // selection changed from wxNOT_FOUND to 0 when inserting the first page.
    //
    // Returns true if the selection was set to the specified page (explicitly
    // because of bSelect == true or implicitly because it's the first page) or
    // false otherwise.
    bool DoSetSelectionAfterInsertion(size_t n, bool bSelect);

    // Update the selection after removing the page at the given index,
    // typically called from the derived class overridden DoRemovePage().
    void DoSetSelectionAfterRemoval(size_t n);

    // set the selection to the given page, sending the events (which can
    // possibly prevent the page change from taking place) if SendEvent flag is
    // included
    virtual int DoSetSelection(size_t nPage, int flags = 0);

    // if the derived class uses DoSetSelection() for implementing
    // [Set|Change]Selection, it must override UpdateSelectedPage(),
    // CreatePageChangingEvent() and MakeChangedEvent(), but as it might not
    // use it, these functions are not pure virtual

    // called to notify the control about a new current page
    virtual void UpdateSelectedPage(size_t WXUNUSED(newsel))
        { wxFAIL_MSG(wxT("Override this function!")); }

    // create a new "page changing" event
    virtual wxBookCtrlEvent* CreatePageChangingEvent() const
        { wxFAIL_MSG(wxT("Override this function!")); return NULL; }

    // modify the event created by CreatePageChangingEvent() to "page changed"
    // event, usually by just calling SetEventType() on it
    virtual void MakeChangedEvent(wxBookCtrlEvent& WXUNUSED(event))
        { wxFAIL_MSG(wxT("Override this function!")); }


    // The derived class also may override the following method, also called
    // from DoSetSelection(), to show/hide pages differently.
    virtual void DoShowPage(wxWindow* page, bool show) { page->Show(show); }


    // Should we accept NULL page pointers in Add/InsertPage()?
    //
    // Default is no but derived classes may override it if they can treat NULL
    // pages in some sensible way (e.g. wxTreebook overrides this to allow
    // having nodes without any associated page)
    virtual bool AllowNullPage() const { return false; }

    // Remove the page and return a pointer to it.
    //
    // It also needs to update the current selection if necessary, i.e. if the
    // page being removed comes before the selected one and the helper method
    // DoSetSelectionAfterRemoval() can be used for this.
    virtual wxWindow *DoRemovePage(size_t page) = 0;

    // our best size is the size which fits all our pages
    virtual wxSize DoGetBestSize() const;

    // helper: get the next page wrapping if we reached the end
    int GetNextPage(bool forward) const;

    // Lay out controls
    virtual void DoSize();

    // This method also invalidates the size of the controller and should be
    // called instead of just InvalidateBestSize() whenever pages are added or
    // removed as this also affects the controller
    void DoInvalidateBestSize();

#if wxUSE_HELP
    // Show the help for the corresponding page
    void OnHelp(wxHelpEvent& event);
#endif // wxUSE_HELP


    // the array of all pages of this control
    wxArrayPages m_pages;

    // get the page area
    virtual wxRect GetPageRect() const;

    // event handlers
    void OnSize(wxSizeEvent& event);

    // controller buddy if available, NULL otherwise (usually for native book controls like wxNotebook)
    wxControl *m_bookctrl;

    // Whether to shrink to fit current page
    bool m_fitToCurrentPage;

    // the sizer containing the choice control
    wxSizer *m_controlSizer;

    // the margin around the choice control
    int m_controlMargin;

    // The currently selected page (in range 0..m_pages.size()-1 inclusive) or
    // wxNOT_FOUND if none (this can normally only be the case for an empty
    // control without any pages).
    int m_selection;

private:

    // common part of all ctors
    void Init();

    // internal border
    unsigned int m_internalBorder;

    DECLARE_ABSTRACT_CLASS(wxBookCtrlBase)
    wxDECLARE_NO_COPY_CLASS(wxBookCtrlBase);

    DECLARE_EVENT_TABLE()
};

// ----------------------------------------------------------------------------
// wxBookCtrlEvent: page changing events generated by book classes
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxBookCtrlEvent : public wxNotifyEvent
{
public:
    wxBookCtrlEvent(wxEventType commandType = wxEVT_NULL, int winid = 0,
                        int nSel = wxNOT_FOUND, int nOldSel = wxNOT_FOUND)
        : wxNotifyEvent(commandType, winid)
    {
        m_nSel = nSel;
        m_nOldSel = nOldSel;
    }

    wxBookCtrlEvent(const wxBookCtrlEvent& event)
        : wxNotifyEvent(event)
    {
        m_nSel = event.m_nSel;
        m_nOldSel = event.m_nOldSel;
    }

    virtual wxEvent *Clone() const { return new wxBookCtrlEvent(*this); }

    // accessors
        // the currently selected page (wxNOT_FOUND if none)
    int GetSelection() const { return m_nSel; }
    void SetSelection(int nSel) { m_nSel = nSel; }
        // the page that was selected before the change (wxNOT_FOUND if none)
    int GetOldSelection() const { return m_nOldSel; }
    void SetOldSelection(int nOldSel) { m_nOldSel = nOldSel; }

private:
    int m_nSel,     // currently selected page
        m_nOldSel;  // previously selected page

    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxBookCtrlEvent)
};

typedef void (wxEvtHandler::*wxBookCtrlEventFunction)(wxBookCtrlEvent&);

#define wxBookCtrlEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxBookCtrlEventFunction, func)

// obsolete name, defined for compatibility only
#define wxBookCtrlBaseEvent wxBookCtrlEvent

// make a default book control for given platform
#if wxUSE_NOTEBOOK
    // dedicated to majority of desktops
    #include "wx/notebook.h"
    #define wxBookCtrl                             wxNotebook
    #define wxEVT_BOOKCTRL_PAGE_CHANGED            wxEVT_NOTEBOOK_PAGE_CHANGED
    #define wxEVT_BOOKCTRL_PAGE_CHANGING           wxEVT_NOTEBOOK_PAGE_CHANGING
    #define EVT_BOOKCTRL_PAGE_CHANGED(id, fn)      EVT_NOTEBOOK_PAGE_CHANGED(id, fn)
    #define EVT_BOOKCTRL_PAGE_CHANGING(id, fn)     EVT_NOTEBOOK_PAGE_CHANGING(id, fn)
#else
    // dedicated to Smartphones
    #include "wx/choicebk.h"
    #define wxBookCtrl                             wxChoicebook
    #define wxEVT_BOOKCTRL_PAGE_CHANGED            wxEVT_CHOICEBOOK_PAGE_CHANGED
    #define wxEVT_BOOKCTRL_PAGE_CHANGING           wxEVT_CHOICEBOOK_PAGE_CHANGING
    #define EVT_BOOKCTRL_PAGE_CHANGED(id, fn)      EVT_CHOICEBOOK_PAGE_CHANGED(id, fn)
    #define EVT_BOOKCTRL_PAGE_CHANGING(id, fn)     EVT_CHOICEBOOK_PAGE_CHANGING(id, fn)
#endif

// old wxEVT_COMMAND_* constants
#define wxEVT_COMMAND_BOOKCTRL_PAGE_CHANGED    wxEVT_BOOKCTRL_PAGE_CHANGED
#define wxEVT_COMMAND_BOOKCTRL_PAGE_CHANGING   wxEVT_BOOKCTRL_PAGE_CHANGING

#if WXWIN_COMPATIBILITY_2_6
    #define wxBC_TOP                               wxBK_TOP
    #define wxBC_BOTTOM                            wxBK_BOTTOM
    #define wxBC_LEFT                              wxBK_LEFT
    #define wxBC_RIGHT                             wxBK_RIGHT
    #define wxBC_DEFAULT                           wxBK_DEFAULT
#endif

#endif // wxUSE_BOOKCTRL

#endif // _WX_BOOKCTRL_H_
