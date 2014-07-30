/////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/combobox.h
// Purpose:     wxComboBox class
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COMBOBOX_H_
#define _WX_COMBOBOX_H_

#include "wx/choice.h"
#include "wx/textentry.h"

// Combobox item
class WXDLLIMPEXP_CORE wxComboBox : public wxChoice,
                               public wxTextEntry
{
public:
    wxComboBox() { m_inSetSelection = false; }
    virtual ~wxComboBox();

    inline wxComboBox(wxWindow *parent, wxWindowID id,
        const wxString& value = wxEmptyString,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        int n = 0, const wxString choices[] = NULL,
        long style = 0,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxComboBoxNameStr)
    {
        m_inSetSelection = false;
        Create(parent, id, value, pos, size, n, choices,
               style, validator, name);
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
        m_inSetSelection = false;
        Create(parent, id, value, pos, size, choices,
               style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
        const wxString& value = wxEmptyString,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        int n = 0, const wxString choices[] = NULL,
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

    // See wxComboBoxBase discussion of IsEmpty().
    bool IsListEmpty() const { return wxItemContainer::IsEmpty(); }
    bool IsTextEmpty() const { return wxTextEntry::IsEmpty(); }

    // resolve ambiguities among virtual functions inherited from both base
    // classes
    virtual void Clear();
    virtual wxString GetValue() const { return wxTextEntry::GetValue(); }
    virtual void SetValue(const wxString& value);
    virtual wxString GetStringSelection() const
        { return wxChoice::GetStringSelection(); }

    virtual void SetSelection(long from, long to)
        { wxTextEntry::SetSelection(from, to); }
    virtual void GetSelection(long *from, long *to) const
        { wxTextEntry::GetSelection(from, to); }


    // implementation of wxControlWithItems
    virtual int DoInsertItems(const wxArrayStringsAdapter& items,
                              unsigned int pos,
                              void **clientData, wxClientDataType type);
    virtual void DoDeleteOneItem(unsigned int n);
    virtual int GetSelection() const ;
    virtual void SetSelection(int n);
    virtual int FindString(const wxString& s, bool bCase = false) const;
    virtual wxString GetString(unsigned int n) const ;
    virtual void SetString(unsigned int n, const wxString& s);

    // Implementation
    virtual void ChangeFont(bool keepOriginalSize = true);
    virtual void ChangeBackgroundColour();
    virtual void ChangeForegroundColour();
    WXWidget GetTopWidget() const { return m_mainWidget; }
    WXWidget GetMainWidget() const { return m_mainWidget; }

   //Copied from wxComboBoxBase because for wxMOTIF wxComboBox does not inherit from it.
    virtual void Popup() { wxFAIL_MSG( wxT("Not implemented") ); }
    virtual void Dismiss() { wxFAIL_MSG( wxT("Not implemented") ); }

protected:
    virtual wxSize DoGetBestSize() const;
    virtual void DoSetSize(int x, int y,
                           int width, int height,
                           int sizeFlags = wxSIZE_AUTO);

    // implement wxTextEntry pure virtual methods
    virtual wxWindow *GetEditableWindow() { return this; }
    virtual WXWidget GetTextWidget() const;

private:
    // only implemented for native combo box
    void AdjustDropDownListSize();

    // implementation detail, should really be private
public:
    bool m_inSetSelection;

    DECLARE_DYNAMIC_CLASS(wxComboBox)
};

#endif // _WX_COMBOBOX_H_
