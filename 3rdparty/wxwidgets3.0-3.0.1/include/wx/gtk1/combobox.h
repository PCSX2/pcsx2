/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk1/combobox.h
// Purpose:
// Author:      Robert Roebling
// Created:     01/02/97
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __GTKCOMBOBOXH__
#define __GTKCOMBOBOXH__

#include "wx/defs.h"

#if wxUSE_COMBOBOX

#include "wx/object.h"

//-----------------------------------------------------------------------------
// classes
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxComboBox;

//-----------------------------------------------------------------------------
// global data
//-----------------------------------------------------------------------------

extern WXDLLIMPEXP_DATA_CORE(const char) wxComboBoxNameStr[];
extern WXDLLIMPEXP_BASE const wxChar* wxEmptyString;

//-----------------------------------------------------------------------------
// wxComboBox
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxComboBox :
    public wxWindowWithItems<wxControl, wxComboBoxBase>
{
public:
    inline wxComboBox() {}
    inline wxComboBox(wxWindow *parent, wxWindowID id,
           const wxString& value = wxEmptyString,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           int n = 0, const wxString choices[] = (const wxString *) NULL,
           long style = 0,
           const wxValidator& validator = wxDefaultValidator,
           const wxString& name = wxComboBoxNameStr)
    {
        Create(parent, id, value, pos, size, n, choices, style, validator, name);
    }
    inline wxComboBox(wxWindow *parent, wxWindowID id,
           const wxString& value,
           const wxPoint& pos,
           const wxSize& size,
           const wxArrayString& choices,
           long style = 0,
           const wxValidator& validator = wxDefaultValidator,
           const wxString& name = wxComboBoxNameStr)
    {
        Create(parent, id, value, pos, size, choices, style, validator, name);
    }

    virtual ~wxComboBox();

    bool Create(wxWindow *parent, wxWindowID id,
           const wxString& value = wxEmptyString,
           const wxPoint& pos = wxDefaultPosition,
           const wxSize& size = wxDefaultSize,
           int n = 0, const wxString choices[] = (const wxString *) NULL,
           long style = 0,
           const wxValidator& validator = wxDefaultValidator,
           const wxString& name = wxComboBoxNameStr);
    bool Create(wxWindow *parent, wxWindowID id,
           const wxString& value,
           const wxPoint& pos,
           const wxSize& size,
           const wxArrayString& choices,
           long style = 0,
           const wxValidator& validator = wxDefaultValidator,
           const wxString& name = wxComboBoxNameStr);

    void DoClear();
    void DoDeleteOneItem(unsigned int n);

    virtual int FindString(const wxString& s, bool bCase = false) const;
    int GetSelection() const;
    int GetCurrentSelection() const;
    virtual wxString GetString(unsigned int n) const;
    wxString GetStringSelection() const;
    virtual unsigned int GetCount() const;
    virtual void SetSelection(int n);
    virtual void SetString(unsigned int n, const wxString &text);

    wxString GetValue() const { return DoGetValue(); }
    void SetValue(const wxString& value);
    void WriteText(const wxString& value);

    void Copy();
    void Cut();
    void Paste();
    bool CanCopy() const;
    bool CanCut() const;
    bool CanPaste() const;
    void SetInsertionPoint( long pos );
    void SetInsertionPointEnd() { SetInsertionPoint( -1 ); }
    long GetInsertionPoint() const;
    virtual wxTextPos GetLastPosition() const;
    void Remove(long from, long to) { Replace(from, to, wxEmptyString); }
    void Replace( long from, long to, const wxString& value );
    void SetSelection( long from, long to );
    void GetSelection( long* from, long* to ) const;
    void SetEditable( bool editable );
    void Undo() ;
    void Redo() ;
    bool CanUndo() const;
    bool CanRedo() const;
    void SelectAll();
    bool IsEditable() const ;
    bool HasSelection() const ;

    // implementation

    virtual void SetFocus();

    void OnSize( wxSizeEvent &event );
    void OnChar( wxKeyEvent &event );

    // Standard event handling
    void OnCut(wxCommandEvent& event);
    void OnCopy(wxCommandEvent& event);
    void OnPaste(wxCommandEvent& event);
    void OnUndo(wxCommandEvent& event);
    void OnRedo(wxCommandEvent& event);
    void OnDelete(wxCommandEvent& event);
    void OnSelectAll(wxCommandEvent& event);

    void OnUpdateCut(wxUpdateUIEvent& event);
    void OnUpdateCopy(wxUpdateUIEvent& event);
    void OnUpdatePaste(wxUpdateUIEvent& event);
    void OnUpdateUndo(wxUpdateUIEvent& event);
    void OnUpdateRedo(wxUpdateUIEvent& event);
    void OnUpdateDelete(wxUpdateUIEvent& event);
    void OnUpdateSelectAll(wxUpdateUIEvent& event);

    bool     m_ignoreNextUpdate:1;
    wxList   m_clientDataList;
    wxList   m_clientObjectList;
    int      m_prevSelection;

    void DisableEvents();
    void EnableEvents();
    GtkWidget* GetConnectWidget();
    bool IsOwnGtkWindow( GdkWindow *window );
    void DoApplyWidgetStyle(GtkRcStyle *style);

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

protected:
    virtual int DoInsertItems(const wxArrayStringsAdapter& items,
                              unsigned int pos,
                              void **clientData, wxClientDataType type);

    virtual void DoSetItemClientData(unsigned int n, void* clientData);
    virtual void* DoGetItemClientData(unsigned int n) const;

    virtual wxSize DoGetBestSize() const;

    // implement wxTextEntry pure virtual methods
    virtual wxString DoGetValue() const;
    virtual wxWindow *GetEditableWindow() { return this; }

    // Widgets that use the style->base colour for the BG colour should
    // override this and return true.
    virtual bool UseGTKStyleBase() const { return true; }

private:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxComboBox)
    DECLARE_EVENT_TABLE()
};

#endif

#endif

  // __GTKCOMBOBOXH__
