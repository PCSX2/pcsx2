///////////////////////////////////////////////////////////////////////////////
// Name:        wx/univ/combobox.h
// Purpose:     the universal combobox
// Author:      Vadim Zeitlin
// Modified by:
// Created:     30.08.00
// Copyright:   (c) 2000 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////


#ifndef _WX_UNIV_COMBOBOX_H_
#define _WX_UNIV_COMBOBOX_H_

#include "wx/combo.h"

class WXDLLIMPEXP_FWD_CORE wxListBox;

// ----------------------------------------------------------------------------
// NB: some actions supported by this control are in wx/generic/combo.h
// ----------------------------------------------------------------------------

// choose the next/prev/specified (by numArg) item
#define wxACTION_COMBOBOX_SELECT_NEXT wxT("next")
#define wxACTION_COMBOBOX_SELECT_PREV wxT("prev")
#define wxACTION_COMBOBOX_SELECT      wxT("select")


// ----------------------------------------------------------------------------
// wxComboBox: a combination of text control and a listbox
// ----------------------------------------------------------------------------

// NB: Normally we'd like wxComboBox to inherit from wxComboBoxBase, but here
//     we can't really do that since both wxComboBoxBase and wxComboCtrl inherit
//     from wxTextCtrl.
class WXDLLIMPEXP_CORE wxComboBox :
    public wxWindowWithItems<wxComboCtrl, wxItemContainer>
{
public:
    // ctors and such
    wxComboBox() { Init(); }

    wxComboBox(wxWindow *parent,
               wxWindowID id,
               const wxString& value = wxEmptyString,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               int n = 0,
               const wxString choices[] = (const wxString *) NULL,
               long style = 0,
               const wxValidator& validator = wxDefaultValidator,
               const wxString& name = wxComboBoxNameStr)
    {
        Init();

        (void)Create(parent, id, value, pos, size, n, choices,
                     style, validator, name);
    }
    wxComboBox(wxWindow *parent,
               wxWindowID id,
               const wxString& value,
               const wxPoint& pos,
               const wxSize& size,
               const wxArrayString& choices,
               long style = 0,
               const wxValidator& validator = wxDefaultValidator,
               const wxString& name = wxComboBoxNameStr);

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& value = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                int n = 0,
                const wxString choices[] = (const wxString *) NULL,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxComboBoxNameStr);
    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& value,
                const wxPoint& pos,
                const wxSize& size,
                const wxArrayString& choices,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxComboBoxNameStr);

    virtual ~wxComboBox();

    // the wxUniversal-specific methods
    // --------------------------------

    // implement the combobox interface

    // wxTextCtrl methods
    virtual wxString GetValue() const { return DoGetValue(); }
    virtual void SetValue(const wxString& value);
    virtual void WriteText(const wxString& value);
    virtual void Copy();
    virtual void Cut();
    virtual void Paste();
    virtual void SetInsertionPoint(long pos);
    virtual void SetInsertionPointEnd();
    virtual long GetInsertionPoint() const;
    virtual wxTextPos GetLastPosition() const;
    virtual void Replace(long from, long to, const wxString& value);
    virtual void Remove(long from, long to);
    virtual void SetSelection(long from, long to);
    virtual void GetSelection(long *from, long *to) const;
    virtual void SetEditable(bool editable);
    virtual bool IsEditable() const;

    virtual void Undo();
    virtual void Redo();
    virtual void SelectAll();

    virtual bool CanCopy() const;
    virtual bool CanCut() const;
    virtual bool CanPaste() const;
    virtual bool CanUndo() const;
    virtual bool CanRedo() const;

    // override these methods to disambiguate between two base classes versions
    virtual void Clear()
    {
        wxComboCtrl::Clear();
        wxItemContainer::Clear();
    }

    // See wxComboBoxBase discussion of IsEmpty().
    bool IsListEmpty() const { return wxItemContainer::IsEmpty(); }
    bool IsTextEmpty() const { return wxTextEntry::IsEmpty(); }

    // wxControlWithItems methods
    virtual void DoClear();
    virtual void DoDeleteOneItem(unsigned int n);
    virtual unsigned int GetCount() const;
    virtual wxString GetString(unsigned int n) const;
    virtual void SetString(unsigned int n, const wxString& s);
    virtual int FindString(const wxString& s, bool bCase = false) const;
    virtual void SetSelection(int n);
    virtual int GetSelection() const;
    virtual wxString GetStringSelection() const;

    // we have our own input handler and our own actions
    // (but wxComboCtrl already handled Popup/Dismiss)
    /*
    virtual bool PerformAction(const wxControlAction& action,
                               long numArg = 0l,
                               const wxString& strArg = wxEmptyString);
    */

    static wxInputHandler *GetStdInputHandler(wxInputHandler *handlerDef);
    virtual wxInputHandler *DoGetStdInputHandler(wxInputHandler *handlerDef)
    {
        return GetStdInputHandler(handlerDef);
    }

    // we delegate our client data handling to wxListBox which we use for the
    // items, so override this and other methods dealing with the client data
    virtual wxClientDataType GetClientDataType() const;
    virtual void SetClientDataType(wxClientDataType clientDataItemsType);

protected:
    virtual wxString DoGetValue() const;

    virtual int DoInsertItems(const wxArrayStringsAdapter& items,
                              unsigned int pos,
                              void **clientData, wxClientDataType type);

    virtual void DoSetItemClientData(unsigned int n, void* clientData);
    virtual void* DoGetItemClientData(unsigned int n) const;


    // common part of all ctors
    void Init();

    // get the associated listbox
    wxListBox *GetLBox() const { return m_lbox; }

private:
    // implement wxTextEntry pure virtual method
    virtual wxWindow *GetEditableWindow() { return this; }

    // the popup listbox
    wxListBox *m_lbox;

    //DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxComboBox)
};

#endif // _WX_UNIV_COMBOBOX_H_
