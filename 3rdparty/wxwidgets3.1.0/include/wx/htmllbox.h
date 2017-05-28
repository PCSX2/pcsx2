///////////////////////////////////////////////////////////////////////////////
// Name:        wx/htmllbox.h
// Purpose:     wxHtmlListBox is a listbox whose items are wxHtmlCells
// Author:      Vadim Zeitlin
// Modified by:
// Created:     31.05.03
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_HTMLLBOX_H_
#define _WX_HTMLLBOX_H_

#include "wx/vlbox.h"               // base class
#include "wx/html/htmlwin.h"
#include "wx/ctrlsub.h"

#if wxUSE_FILESYSTEM
    #include "wx/filesys.h"
#endif // wxUSE_FILESYSTEM

class WXDLLIMPEXP_FWD_HTML wxHtmlCell;
class WXDLLIMPEXP_FWD_HTML wxHtmlWinParser;
class WXDLLIMPEXP_FWD_HTML wxHtmlListBoxCache;
class WXDLLIMPEXP_FWD_HTML wxHtmlListBoxStyle;

extern WXDLLIMPEXP_DATA_HTML(const char) wxHtmlListBoxNameStr[];
extern WXDLLIMPEXP_DATA_HTML(const char) wxSimpleHtmlListBoxNameStr[];

// ----------------------------------------------------------------------------
// wxHtmlListBox
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_HTML wxHtmlListBox : public wxVListBox,
                                       public wxHtmlWindowInterface,
                                       public wxHtmlWindowMouseHelper
{
    wxDECLARE_ABSTRACT_CLASS(wxHtmlListBox);
public:
    // constructors and such
    // ---------------------

    // default constructor, you must call Create() later
    wxHtmlListBox();

    // normal constructor which calls Create() internally
    wxHtmlListBox(wxWindow *parent,
                  wxWindowID id = wxID_ANY,
                  const wxPoint& pos = wxDefaultPosition,
                  const wxSize& size = wxDefaultSize,
                  long style = 0,
                  const wxString& name = wxHtmlListBoxNameStr);

    // really creates the control and sets the initial number of items in it
    // (which may be changed later with SetItemCount())
    //
    // the only special style which may be specified here is wxLB_MULTIPLE
    //
    // returns true on success or false if the control couldn't be created
    bool Create(wxWindow *parent,
                wxWindowID id = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxHtmlListBoxNameStr);

    // destructor cleans up whatever resources we use
    virtual ~wxHtmlListBox();

    // override some base class virtuals
    virtual void RefreshRow(size_t line) wxOVERRIDE;
    virtual void RefreshRows(size_t from, size_t to) wxOVERRIDE;
    virtual void RefreshAll() wxOVERRIDE;
    virtual void SetItemCount(size_t count) wxOVERRIDE;

#if wxUSE_FILESYSTEM
    // retrieve the file system used by the wxHtmlWinParser: if you use
    // relative paths in your HTML, you should use its ChangePathTo() method
    wxFileSystem& GetFileSystem() { return m_filesystem; }
    const wxFileSystem& GetFileSystem() const { return m_filesystem; }
#endif // wxUSE_FILESYSTEM

    virtual void OnInternalIdle() wxOVERRIDE;

protected:
    // this method must be implemented in the derived class and should return
    // the body (i.e. without <html>) of the HTML for the given item
    virtual wxString OnGetItem(size_t n) const = 0;

    // this function may be overridden to decorate HTML returned by OnGetItem()
    virtual wxString OnGetItemMarkup(size_t n) const;


    // this method allows to customize the selection appearance: it may be used
    // to specify the colour of the text which normally has the given colour
    // colFg when it is inside the selection
    //
    // by default, the original colour is not used at all and all text has the
    // same (default for this system) colour inside selection
    virtual wxColour GetSelectedTextColour(const wxColour& colFg) const;

    // this is the same as GetSelectedTextColour() but allows to customize the
    // background colour -- this is even more rarely used as you can change it
    // globally using SetSelectionBackground()
    virtual wxColour GetSelectedTextBgColour(const wxColour& colBg) const;


    // we implement both of these functions in terms of OnGetItem(), they are
    // not supposed to be overridden by our descendants
    virtual void OnDrawItem(wxDC& dc, const wxRect& rect, size_t n) const wxOVERRIDE;
    virtual wxCoord OnMeasureItem(size_t n) const wxOVERRIDE;

    // override this one to draw custom background for selected items correctly
    virtual void OnDrawBackground(wxDC& dc, const wxRect& rect, size_t n) const wxOVERRIDE;

    // this method may be overridden to handle clicking on a link in the
    // listbox (by default, clicks on links are simply ignored)
    virtual void OnLinkClicked(size_t n, const wxHtmlLinkInfo& link);

    // event handlers
    void OnSize(wxSizeEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnLeftDown(wxMouseEvent& event);


    // common part of all ctors
    void Init();

    // ensure that the given item is cached
    void CacheItem(size_t n) const;

private:
    // wxHtmlWindowInterface methods:
    virtual void SetHTMLWindowTitle(const wxString& title) wxOVERRIDE;
    virtual void OnHTMLLinkClicked(const wxHtmlLinkInfo& link) wxOVERRIDE;
    virtual wxHtmlOpeningStatus OnHTMLOpeningURL(wxHtmlURLType type,
                                                 const wxString& url,
                                                 wxString *redirect) const wxOVERRIDE;
    virtual wxPoint HTMLCoordsToWindow(wxHtmlCell *cell,
                                       const wxPoint& pos) const wxOVERRIDE;
    virtual wxWindow* GetHTMLWindow() wxOVERRIDE;
    virtual wxColour GetHTMLBackgroundColour() const wxOVERRIDE;
    virtual void SetHTMLBackgroundColour(const wxColour& clr) wxOVERRIDE;
    virtual void SetHTMLBackgroundImage(const wxBitmap& bmpBg) wxOVERRIDE;
    virtual void SetHTMLStatusText(const wxString& text) wxOVERRIDE;
    virtual wxCursor GetHTMLCursor(HTMLCursor type) const wxOVERRIDE;

    // returns index of item that contains given HTML cell
    size_t GetItemForCell(const wxHtmlCell *cell) const;

    // return physical coordinates of root wxHtmlCell of n-th item
    wxPoint GetRootCellCoords(size_t n) const;

    // Converts physical coordinates stored in @a pos into coordinates
    // relative to the root cell of the item under mouse cursor, if any. If no
    // cell is found under the cursor, returns false.  Otherwise stores the new
    // coordinates back into @a pos and pointer to the cell under cursor into
    // @a cell and returns true.
    bool PhysicalCoordsToCell(wxPoint& pos, wxHtmlCell*& cell) const;

    // The opposite of PhysicalCoordsToCell: converts coordinates relative to
    // given cell to physical coordinates in the window
    wxPoint CellCoordsToPhysical(const wxPoint& pos, wxHtmlCell *cell) const;

private:
    // this class caches the pre-parsed HTML to speed up display
    wxHtmlListBoxCache *m_cache;

    // HTML parser we use
    wxHtmlWinParser *m_htmlParser;

#if wxUSE_FILESYSTEM
    // file system used by m_htmlParser
    wxFileSystem m_filesystem;
#endif // wxUSE_FILESYSTEM

    // rendering style for the parser which allows us to customize our colours
    wxHtmlListBoxStyle *m_htmlRendStyle;


    // it calls our GetSelectedTextColour() and GetSelectedTextBgColour()
    friend class wxHtmlListBoxStyle;
    friend class wxHtmlListBoxWinInterface;


    wxDECLARE_EVENT_TABLE();
    wxDECLARE_NO_COPY_CLASS(wxHtmlListBox);
};


// ----------------------------------------------------------------------------
// wxSimpleHtmlListBox
// ----------------------------------------------------------------------------

#define wxHLB_DEFAULT_STYLE     wxBORDER_SUNKEN
#define wxHLB_MULTIPLE          wxLB_MULTIPLE

class WXDLLIMPEXP_HTML wxSimpleHtmlListBox :
    public wxWindowWithItems<wxHtmlListBox, wxItemContainer>
{
    wxDECLARE_ABSTRACT_CLASS(wxSimpleHtmlListBox);
public:
    // wxListbox-compatible constructors
    // ---------------------------------

    wxSimpleHtmlListBox() { }

    wxSimpleHtmlListBox(wxWindow *parent,
                        wxWindowID id,
                        const wxPoint& pos = wxDefaultPosition,
                        const wxSize& size = wxDefaultSize,
                        int n = 0, const wxString choices[] = NULL,
                        long style = wxHLB_DEFAULT_STYLE,
                        const wxValidator& validator = wxDefaultValidator,
                        const wxString& name = wxSimpleHtmlListBoxNameStr)
    {
        Create(parent, id, pos, size, n, choices, style, validator, name);
    }

    wxSimpleHtmlListBox(wxWindow *parent,
                        wxWindowID id,
                        const wxPoint& pos,
                        const wxSize& size,
                        const wxArrayString& choices,
                        long style = wxHLB_DEFAULT_STYLE,
                        const wxValidator& validator = wxDefaultValidator,
                        const wxString& name = wxSimpleHtmlListBoxNameStr)
    {
        Create(parent, id, pos, size, choices, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                int n = 0, const wxString choices[] = NULL,
                long style = wxHLB_DEFAULT_STYLE,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxSimpleHtmlListBoxNameStr);
    bool Create(wxWindow *parent, wxWindowID id,
                const wxPoint& pos,
                const wxSize& size,
                const wxArrayString& choices,
                long style = wxHLB_DEFAULT_STYLE,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxSimpleHtmlListBoxNameStr);

    virtual ~wxSimpleHtmlListBox();

    // these must be overloaded otherwise the compiler will complain
    // about  wxItemContainerImmutable::[G|S]etSelection being pure virtuals...
    void SetSelection(int n) wxOVERRIDE
        { wxVListBox::SetSelection(n); }
    int GetSelection() const wxOVERRIDE
        { return wxVListBox::GetSelection(); }


    // accessing strings
    // -----------------

    virtual unsigned int GetCount() const wxOVERRIDE
        { return m_items.GetCount(); }

    virtual wxString GetString(unsigned int n) const wxOVERRIDE;

    // override default unoptimized wxItemContainer::GetStrings() function
    wxArrayString GetStrings() const
        { return m_items; }

    virtual void SetString(unsigned int n, const wxString& s) wxOVERRIDE;

    // resolve ambiguity between wxItemContainer and wxVListBox versions
    void Clear();

protected:
    virtual int DoInsertItems(const wxArrayStringsAdapter & items,
                              unsigned int pos,
                              void **clientData, wxClientDataType type) wxOVERRIDE;

    virtual void DoSetItemClientData(unsigned int n, void *clientData) wxOVERRIDE
        { m_HTMLclientData[n] = clientData; }

    virtual void *DoGetItemClientData(unsigned int n) const wxOVERRIDE
        { return m_HTMLclientData[n]; }

    // wxItemContainer methods
    virtual void DoClear() wxOVERRIDE;
    virtual void DoDeleteOneItem(unsigned int n) wxOVERRIDE;

    // calls wxHtmlListBox::SetItemCount() and RefreshAll()
    void UpdateCount();

    // override these functions just to change their visibility: users of
    // wxSimpleHtmlListBox shouldn't be allowed to call them directly!
    virtual void SetItemCount(size_t count) wxOVERRIDE
        { wxHtmlListBox::SetItemCount(count); }
    virtual void SetRowCount(size_t count)
        { wxHtmlListBox::SetRowCount(count); }

    virtual wxString OnGetItem(size_t n) const wxOVERRIDE
        { return m_items[n]; }

    virtual void InitEvent(wxCommandEvent& event, int n) wxOVERRIDE
        {
            // we're not a virtual control and we can include the string
            // of the item which was clicked:
            event.SetString(m_items[n]);
            wxVListBox::InitEvent(event, n);
        }

    wxArrayString   m_items;
    wxArrayPtrVoid  m_HTMLclientData;

    // Note: For the benefit of old compilers (like gcc-2.8) this should
    // not be named m_clientdata as that clashes with the name of an
    // anonymous struct member in wxEvtHandler, which we derive from.

    wxDECLARE_NO_COPY_CLASS(wxSimpleHtmlListBox);
};

#endif // _WX_HTMLLBOX_H_

