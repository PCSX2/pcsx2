/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk1/choice.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __GTKCHOICEH__
#define __GTKCHOICEH__

class WXDLLIMPEXP_FWD_BASE wxSortedArrayString;
class WXDLLIMPEXP_FWD_BASE wxArrayString;

//-----------------------------------------------------------------------------
// wxChoice
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxChoice : public wxChoiceBase
{
public:
    wxChoice();
    wxChoice( wxWindow *parent, wxWindowID id,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            int n = 0, const wxString choices[] = (const wxString *) NULL,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxChoiceNameStr )
    {
        m_strings = NULL;

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
        m_strings = NULL;

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

    // implement base class pure virtuals
    void DoDeleteOneItem(unsigned int n);
    void DoClear();

    int GetSelection() const;
    virtual void SetSelection(int n);

    virtual unsigned int GetCount() const;
    virtual int FindString(const wxString& s, bool bCase = false) const;
    virtual wxString GetString(unsigned int n) const;
    virtual void SetString(unsigned int n, const wxString& string);

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

protected:
    wxList m_clientList;    // contains the client data for the items

    void DoApplyWidgetStyle(GtkRcStyle *style);
    virtual int DoInsertItems(const wxArrayStringsAdapter& items,
                              unsigned int pos,
                              void **clientData, wxClientDataType type);

    virtual void DoSetItemClientData(unsigned int n, void* clientData);
    virtual void* DoGetItemClientData(unsigned int n) const;

    virtual wxSize DoGetBestSize() const;

    virtual bool IsOwnGtkWindow( GdkWindow *window );

private:
    // DoInsertItems() helper
    int GtkAddHelper(GtkWidget *menu, unsigned int pos, const wxString& item);

    // this array is only used for controls with wxCB_SORT style, so only
    // allocate it if it's needed (hence using pointer)
    wxSortedArrayString *m_strings;

public:
    // this circumvents a GTK+ 2.0 bug so that the selection is
    // invalidated properly
    int m_selection_hack;

private:
    DECLARE_DYNAMIC_CLASS(wxChoice)
};


#endif // __GTKCHOICEH__
