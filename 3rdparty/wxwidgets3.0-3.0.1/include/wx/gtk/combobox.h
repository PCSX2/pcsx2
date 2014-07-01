/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/combobox.h
// Purpose:
// Author:      Robert Roebling
// Created:     01/02/97
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_COMBOBOX_H_
#define _WX_GTK_COMBOBOX_H_

#include "wx/choice.h"

typedef struct _GtkEntry GtkEntry;

//-----------------------------------------------------------------------------
// wxComboBox
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxComboBox : public wxChoice,
                                    public wxTextEntry
{
public:
    wxComboBox()
        : wxChoice(), wxTextEntry()
    {
        Init();
    }
    wxComboBox(wxWindow *parent,
               wxWindowID id,
               const wxString& value = wxEmptyString,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               int n = 0, const wxString choices[] = NULL,
               long style = 0,
               const wxValidator& validator = wxDefaultValidator,
               const wxString& name = wxComboBoxNameStr)
        : wxChoice(), wxTextEntry()
    {
        Init();
        Create(parent, id, value, pos, size, n, choices, style, validator, name);
    }

    wxComboBox(wxWindow *parent, wxWindowID id,
               const wxString& value,
               const wxPoint& pos,
               const wxSize& size,
               const wxArrayString& choices,
               long style = 0,
               const wxValidator& validator = wxDefaultValidator,
               const wxString& name = wxComboBoxNameStr)
        : wxChoice(), wxTextEntry()
    {
        Init();
        Create(parent, id, value, pos, size, choices, style, validator, name);
    }
    ~wxComboBox();

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

    // Set/GetSelection() from wxTextEntry and wxChoice

    virtual void SetSelection(int n) { wxChoice::SetSelection(n); }
    virtual void SetSelection(long from, long to)
                               { wxTextEntry::SetSelection(from, to); }

    virtual int GetSelection() const { return wxChoice::GetSelection(); }
    virtual void GetSelection(long *from, long *to) const
                               { return wxTextEntry::GetSelection(from, to); }

    virtual wxString GetStringSelection() const
    {
        return wxItemContainer::GetStringSelection();
    }

    virtual void SetString(unsigned int n, const wxString& string);

    virtual void Popup();
    virtual void Dismiss();

    virtual void Clear()
    {
        wxTextEntry::Clear();
        wxItemContainer::Clear();
    }

    // See wxComboBoxBase discussion of IsEmpty().
    bool IsListEmpty() const { return wxItemContainer::IsEmpty(); }
    bool IsTextEmpty() const { return wxTextEntry::IsEmpty(); }

    void OnChar( wxKeyEvent &event );

    virtual void SetValue(const wxString& value);

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

    virtual void GTKDisableEvents();
    virtual void GTKEnableEvents();
    GtkWidget* GetConnectWidget();

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

protected:
    // From wxWindowGTK:
    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const;

    // Widgets that use the style->base colour for the BG colour should
    // override this and return true.
    virtual bool UseGTKStyleBase() const { return true; }

    // Override in derived classes to create combo box widgets with
    // custom list stores.
    virtual void GTKCreateComboBoxWidget();

    virtual wxSize DoGetSizeFromTextSize(int xlen, int ylen = -1) const;

    virtual GtkEntry *GetEntry() const
        { return m_entry; }

    GtkEntry*   m_entry;

private:
    // From wxTextEntry:
    virtual wxWindow *GetEditableWindow() { return this; }
    virtual GtkEditable *GetEditable() const;
    virtual void EnableTextChangedEvents(bool enable);

    void Init();

    DECLARE_DYNAMIC_CLASS_NO_COPY(wxComboBox)
    DECLARE_EVENT_TABLE()
};

#endif // _WX_GTK_COMBOBOX_H_
