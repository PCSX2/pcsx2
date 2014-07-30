/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk1/listbox.h
// Purpose:     wxListBox class declaration
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __GTKLISTBOXH__
#define __GTKLISTBOXH__

#include "wx/list.h"

typedef struct _GtkList GtkList;

class WXDLLIMPEXP_FWD_BASE wxSortedArrayString;

//-----------------------------------------------------------------------------
// wxListBox
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxListBox : public wxListBoxBase
{
public:
    // ctors and such
    wxListBox();
    wxListBox( wxWindow *parent, wxWindowID id,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            int n = 0, const wxString choices[] = (const wxString *) NULL,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxListBoxNameStr )
    {
#if wxUSE_CHECKLISTBOX
        m_hasCheckBoxes = false;
#endif // wxUSE_CHECKLISTBOX
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
#if wxUSE_CHECKLISTBOX
        m_hasCheckBoxes = false;
#endif // wxUSE_CHECKLISTBOX
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

    // implement base class pure virtuals
    virtual void DoClear();
    virtual void DoDeleteOneItem(unsigned int n);

    virtual unsigned int GetCount() const;
    virtual wxString GetString(unsigned int n) const;
    virtual void SetString(unsigned int n, const wxString& s);
    virtual int FindString(const wxString& s, bool bCase = false) const;

    virtual bool IsSelected(int n) const;
    virtual void DoSetSelection(int n, bool select);
    virtual int GetSelection() const;
    virtual int GetSelections(wxArrayInt& aSelections) const;

    virtual int DoInsertItems(const wxArrayStringsAdapter& items,
                              unsigned int pos,
                              void **clientData, wxClientDataType type);

    virtual void DoSetFirstItem(int n);

    virtual void DoSetItemClientData(unsigned int n, void* clientData);
    virtual void* DoGetItemClientData(unsigned int n) const;

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    // implementation from now on

    void GtkAddItem( const wxString &item, int pos=-1 );
    int GtkGetIndex( GtkWidget *item ) const;
    GtkWidget *GetConnectWidget();
    bool IsOwnGtkWindow( GdkWindow *window );
    void DoApplyWidgetStyle(GtkRcStyle *style);
    void OnInternalIdle();

#if wxUSE_TOOLTIPS
    void ApplyToolTip( GtkTooltips *tips, const wxChar *tip );
#endif // wxUSE_TOOLTIPS

    GtkList   *m_list;
    wxList     m_clientList;

#if wxUSE_CHECKLISTBOX
    bool       m_hasCheckBoxes;
#endif // wxUSE_CHECKLISTBOX

    int        m_prevSelection;
    bool       m_blockEvent;

    virtual void FixUpMouseEvent(GtkWidget *widget, wxCoord& x, wxCoord& y);

protected:
    virtual wxSize DoGetBestSize() const;

    // return the string label for the given item
    wxString GetRealLabel(struct _GList *item) const;

    // Widgets that use the style->base colour for the BG colour should
    // override this and return true.
    virtual bool UseGTKStyleBase() const { return true; }

private:
    // this array is only used for controls with wxCB_SORT style, so only
    // allocate it if it's needed (hence using pointer)
    wxSortedArrayString *m_strings;

    DECLARE_DYNAMIC_CLASS(wxListBox)
};

#endif // __GTKLISTBOXH__
