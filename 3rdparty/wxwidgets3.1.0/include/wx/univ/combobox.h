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
    virtual wxString GetValue() const wxOVERRIDE { return DoGetValue(); }
    virtual void SetValue(const wxString& value) wxOVERRIDE;
    virtual void WriteText(const wxString& value) wxOVERRIDE;
    virtual void Copy() wxOVERRIDE;
    virtual void Cut() wxOVERRIDE;
    virtual void Paste() wxOVERRIDE;
    virtual void SetInsertionPoint(long pos) wxOVERRIDE;
    virtual void SetInsertionPointEnd() wxOVERRIDE;
    virtual long GetInsertionPoint() const wxOVERRIDE;
    virtual wxTextPos GetLastPosition() const wxOVERRIDE;
    virtual void Replace(long from, long to, const wxString& value) wxOVERRIDE;
    virtual void Remove(long from, long to) wxOVERRIDE;
    virtual void SetSelection(long from, long to) wxOVERRIDE;
    virtual void GetSelection(long *from, long *to) const wxOVERRIDE;
    virtual void SetEditable(bool editable) wxOVERRIDE;
    virtual bool IsEditable() const wxOVERRIDE;

    virtual void Undo() wxOVERRIDE;
    virtual void Redo() wxOVERRIDE;
    virtual void SelectAll() wxOVERRIDE;

    virtual bool CanCopy() const wxOVERRIDE;
    virtual bool CanCut() const wxOVERRIDE;
    virtual bool CanPaste() const wxOVERRIDE;
    virtual bool CanUndo() const wxOVERRIDE;
    virtual bool CanRedo() const wxOVERRIDE;

    // override these methods to disambiguate between two base classes versions
    virtual void Clear() wxOVERRIDE
    {
        wxItemContainer::Clear();
    }

    // See wxComboBoxBase discussion of IsEmpty().
    bool IsListEmpty() const { return wxItemContainer::IsEmpty(); }
    bool IsTextEmpty() const { return wxTextEntry::IsEmpty(); }

    // wxControlWithItems methods
    virtual void DoClear() wxOVERRIDE;
    virtual void DoDeleteOneItem(unsigned int n) wxOVERRIDE;
    virtual unsigned int GetCount() const wxOVERRIDE;
    virtual wxString GetString(unsigned int n) const wxOVERRIDE;
    virtual void SetString(unsigned int n, const wxString& s) wxOVERRIDE;
    virtual int FindString(const wxString& s, bool bCase = false) const wxOVERRIDE;
    virtual void SetSelection(int n) wxOVERRIDE;
    virtual int GetSelection() const wxOVERRIDE;
    virtual wxString GetStringSelection() const wxOVERRIDE;

    // we have our own input handler and our own actions
    // (but wxComboCtrl already handled Popup/Dismiss)
    /*
    virtual bool PerformAction(const wxControlAction& action,
                               long numArg = 0l,
                               const wxString& strArg = wxEmptyString);
    */

    static wxInputHandler *GetStdInputHandler(wxInputHandler *handlerDef);
    virtual wxInputHandler *DoGetStdInputHandler(wxInputHandler *handlerDef) wxOVERRIDE
    {
        return GetStdInputHandler(handlerDef);
    }

    // we delegate our client data handling to wxListBox which we use for the
    // items, so override this and other methods dealing with the client data
    virtual wxClientDataType GetClientDataType() const wxOVERRIDE;
    virtual void SetClientDataType(wxClientDataType clientDataItemsType) wxOVERRIDE;

protected:
    virtual int DoInsertItems(const wxArrayStringsAdapter& items,
                              unsigned int pos,
                              void **clientData, wxClientDataType type) wxOVERRIDE;

    virtual void DoSetItemClientData(unsigned int n, void* clientData) wxOVERRIDE;
    virtual void* DoGetItemClientData(unsigned int n) const wxOVERRIDE;


    // common part of all ctors
    void Init();

    // get the associated listbox
    wxListBox *GetLBox() const { return m_lbox; }

private:
    // implement wxTextEntry pure virtual method
    virtual wxWindow *GetEditableWindow() wxOVERRIDE { return this; }

    // the popup listbox
    wxListBox *m_lbox;

    //wxDECLARE_EVENT_TABLE();
    wxDECLARE_DYNAMIC_CLASS(wxComboBox);
};

#endif // _WX_UNIV_COMBOBOX_H_
