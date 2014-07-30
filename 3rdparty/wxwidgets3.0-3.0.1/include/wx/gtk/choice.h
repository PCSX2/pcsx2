/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/choice.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_CHOICE_H_
#define _WX_GTK_CHOICE_H_

class WXDLLIMPEXP_FWD_BASE wxSortedArrayString;
class WXDLLIMPEXP_FWD_BASE wxArrayString;

//-----------------------------------------------------------------------------
// wxChoice
//-----------------------------------------------------------------------------

class wxGtkCollatedArrayString;

class WXDLLIMPEXP_CORE wxChoice : public wxChoiceBase
{
public:
    wxChoice()
    {
        Init();
    }
    wxChoice( wxWindow *parent, wxWindowID id,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            int n = 0, const wxString choices[] = (const wxString *) NULL,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxChoiceNameStr )
    {
        Init();
        Create(parent, id, pos, size, n, choices, style, validator, name);
    }
    wxChoice( wxWindow *parent, wxWindowID id,
            const wxPoint& pos,
            const wxSize& size,
            const wxArrayString& choices,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxChoiceNameStr )
    {
        Init();
        Create(parent, id, pos, size, choices, style, validator, name);
    }
    virtual ~wxChoice();
    bool Create( wxWindow *parent, wxWindowID id,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            int n = 0, const wxString choices[] = NULL,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxChoiceNameStr );
    bool Create( wxWindow *parent, wxWindowID id,
            const wxPoint& pos,
            const wxSize& size,
            const wxArrayString& choices,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxChoiceNameStr );

    int GetSelection() const;
    void SetSelection(int n);

    virtual unsigned int GetCount() const;
    virtual int FindString(const wxString& s, bool bCase = false) const;
    virtual wxString GetString(unsigned int n) const;
    virtual void SetString(unsigned int n, const wxString& string);

    virtual void SetColumns(int n=1);
    virtual int GetColumns() const;

    virtual void GTKDisableEvents();
    virtual void GTKEnableEvents();

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

protected:
    // this array is only used for controls with wxCB_SORT style, so only
    // allocate it if it's needed (hence using pointer)
    wxGtkCollatedArrayString *m_strings;

    // contains the client data for the items
    wxArrayPtrVoid m_clientData;

    // index to GtkListStore cell which displays the item text
    int m_stringCellIndex;

    virtual wxSize DoGetBestSize() const;
    virtual wxSize DoGetSizeFromTextSize(int xlen, int ylen = -1) const;
    virtual int DoInsertItems(const wxArrayStringsAdapter& items,
                              unsigned int pos,
                              void **clientData, wxClientDataType type);
    virtual void DoSetItemClientData(unsigned int n, void* clientData);
    virtual void* DoGetItemClientData(unsigned int n) const;
    virtual void DoClear();
    virtual void DoDeleteOneItem(unsigned int n);

    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const;
    virtual void DoApplyWidgetStyle(GtkRcStyle *style);

    // in derived classes, implement this to insert list store entry
    // with all items default except text
    virtual void GTKInsertComboBoxTextItem( unsigned int n, const wxString& text );

private:
    void Init();

    DECLARE_DYNAMIC_CLASS(wxChoice)
};


#endif // _WX_GTK_CHOICE_H_
