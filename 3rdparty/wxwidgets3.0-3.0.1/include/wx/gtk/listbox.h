/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/listbox.h
// Purpose:     wxListBox class declaration
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_LISTBOX_H_
#define _WX_GTK_LISTBOX_H_

struct _wxTreeEntry;
struct _GtkTreeIter;

//-----------------------------------------------------------------------------
// wxListBox
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxListBox : public wxListBoxBase
{
public:
    // ctors and such
    wxListBox()
    {
        Init();
    }
    wxListBox( wxWindow *parent, wxWindowID id,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            int n = 0, const wxString choices[] = (const wxString *) NULL,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxListBoxNameStr )
    {
        Init();
        Create(parent, id, pos, size, n, choices, style, validator, name);
    }
    wxListBox( wxWindow *parent, wxWindowID id,
            const wxPoint& pos,
            const wxSize& size,
            const wxArrayString& choices,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxListBoxNameStr )
    {
        Init();
        Create(parent, id, pos, size, choices, style, validator, name);
    }
    virtual ~wxListBox();

    bool Create(wxWindow *parent, wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                int n = 0, const wxString choices[] = (const wxString *) NULL,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxListBoxNameStr);
    bool Create(wxWindow *parent, wxWindowID id,
                const wxPoint& pos,
                const wxSize& size,
                const wxArrayString& choices,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxListBoxNameStr);

    virtual unsigned int GetCount() const;
    virtual wxString GetString(unsigned int n) const;
    virtual void SetString(unsigned int n, const wxString& s);
    virtual int FindString(const wxString& s, bool bCase = false) const;

    virtual bool IsSelected(int n) const;
    virtual int GetSelection() const;
    virtual int GetSelections(wxArrayInt& aSelections) const;

    virtual void EnsureVisible(int n);

    virtual void Update();

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    // implementation from now on

    virtual GtkWidget *GetConnectWidget();

    struct _GtkTreeView   *m_treeview;
    struct _GtkListStore  *m_liststore;

#if wxUSE_CHECKLISTBOX
    bool       m_hasCheckBoxes;
#endif // wxUSE_CHECKLISTBOX

    struct _wxTreeEntry* GTKGetEntry(unsigned pos) const;

    void GTKDisableEvents();
    void GTKEnableEvents();

    void GTKOnSelectionChanged();
    void GTKOnActivated(int item);

protected:
    virtual void DoClear();
    virtual void DoDeleteOneItem(unsigned int n);
    virtual wxSize DoGetBestSize() const;
    virtual void DoApplyWidgetStyle(GtkRcStyle *style);
    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const;

    virtual void DoSetSelection(int n, bool select);

    virtual int DoInsertItems(const wxArrayStringsAdapter& items,
                              unsigned int pos,
                              void **clientData, wxClientDataType type);
    virtual int DoInsertOneItem(const wxString& item, unsigned int pos);

    virtual void DoSetFirstItem(int n);
    virtual void DoSetItemClientData(unsigned int n, void* clientData);
    virtual void* DoGetItemClientData(unsigned int n) const;
    virtual int DoListHitTest(const wxPoint& point) const;

    // get the iterator for the given index, returns false if invalid
    bool GTKGetIteratorFor(unsigned pos, _GtkTreeIter *iter) const;

    // get the index for the given iterator, return wxNOT_FOUND on failure
    int GTKGetIndexFor(_GtkTreeIter& iter) const;

    // common part of DoSetFirstItem() and EnsureVisible()
    void DoScrollToCell(int n, float alignY, float alignX);

private:
    void Init(); //common construction

    DECLARE_DYNAMIC_CLASS(wxListBox)
};

#endif // _WX_GTK_LISTBOX_H_
