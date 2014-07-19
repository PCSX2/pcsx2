/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/combobox.h
// Purpose:     wxComboBox class
// Author:      Ryan Norton
// Modified by:
// Created:     2005/02/16
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_COMBOBOX_H__
#define __WX_COCOA_COMBOBOX_H__

//Begin NSComboBox.h

#include "wx/hashmap.h"
#include "wx/cocoa/ObjcAssociate.h"

#include "wx/textctrl.h"

DECLARE_WXCOCOA_OBJC_CLASS(NSComboBox);

WX_DECLARE_OBJC_HASHMAP(NSComboBox);
class wxCocoaNSComboBox
{
    WX_DECLARE_OBJC_INTERFACE_HASHMAP(NSComboBox)
public:
    void AssociateNSComboBox(WX_NSComboBox cocoaNSComboBox);
    void DisassociateNSComboBox(WX_NSComboBox cocoaNSComboBox);

    virtual void doWxEvent(int nEvent) = 0;
    virtual ~wxCocoaNSComboBox() { }
};

//begin combobox.h

#include "wx/dynarray.h"

// ========================================================================
// wxComboBox
// ========================================================================
class WXDLLIMPEXP_CORE wxComboBox : public wxControl, public wxComboBoxBase, protected wxCocoaNSComboBox, protected wxCocoaNSTextField
{
    DECLARE_DYNAMIC_CLASS(wxComboBox)
    DECLARE_EVENT_TABLE()
    WX_DECLARE_COCOA_OWNER(NSComboBox,NSTextField,NSView)
    WX_DECLARE_COCOA_OWNER(NSTextField,NSControl,NSView)
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    wxComboBox() { }
    wxComboBox(wxWindow *parent, wxWindowID winid,
            const wxString& value = wxEmptyString,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            int n = 0, const wxString choices[] = NULL,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxComboBoxNameStr)
    {
        Create(parent, winid, value, pos, size, n, choices, style, validator, name);
    }
    wxComboBox(wxWindow *parent, wxWindowID winid,
            const wxString& value,
            const wxPoint& pos,
            const wxSize& size,
            const wxArrayString& choices,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxComboBoxNameStr)
    {
        Create(parent, winid, value, pos, size, choices, style,
               validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID winid,
            const wxString& value = wxEmptyString,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            int n = 0, const wxString choices[] = NULL,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxComboBoxNameStr);
    bool Create(wxWindow *parent, wxWindowID winid,
            const wxString& value,
            const wxPoint& pos,
            const wxSize& size,
            const wxArrayString& choices,
            long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxComboBoxNameStr);
    virtual ~wxComboBox();

// ------------------------------------------------------------------------
// Cocoa callbacks
// ------------------------------------------------------------------------
protected:
    wxArrayPtrVoid m_Datas;
    virtual void doWxEvent(int nEvent);

    virtual void Cocoa_didChangeText()
    {}
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    void Clear() // HACK
    {   wxComboBoxBase::Clear(); }

    // wxCombobox methods
    virtual void SetSelection(int pos);
    // Overlapping methods
    virtual wxString GetStringSelection();
    // wxItemContainer
    virtual void DoClear();
    virtual void DoDeleteOneItem(unsigned int n);
    virtual unsigned int GetCount() const;
    virtual wxString GetString(unsigned int) const;
    virtual void SetString(unsigned int pos, const wxString&);
    virtual int FindString(const wxString& s, bool bCase = false) const;
    virtual int GetSelection() const;
    virtual int DoInsertItems(const wxArrayStringsAdapter& items,
                              unsigned int pos,
                              void **clientData, wxClientDataType type);
    virtual void DoSetItemClientData(unsigned int, void*);
    virtual void* DoGetItemClientData(unsigned int) const;
    virtual bool IsSorted() const { return HasFlag(wxCB_SORT); }

// ------------------------------------------------------------------------
// wxTextEntryBase virtual implementations:
// ------------------------------------------------------------------------
    // FIXME: This needs to be moved to some sort of common code.
    virtual void WriteText(const wxString&);
    virtual wxString GetValue() const;
    virtual void Remove(long, long);
    virtual void Cut();
    virtual void Copy();
    virtual void Paste();
    virtual void Undo();
    virtual void Redo();
    virtual bool CanUndo() const;
    virtual bool CanRedo() const;
    virtual void SetInsertionPoint(long pos);
    virtual long GetInsertionPoint() const;
    virtual wxTextPos GetLastPosition() const;
    virtual void SetSelection(long from, long to);
    virtual void GetSelection(long *from, long *to) const;
    virtual bool IsEditable() const;
    virtual void SetEditable(bool editable);

private:
    // implement wxTextEntry pure virtual method
    virtual wxWindow *GetEditableWindow() { return this; }
};

#endif // __WX_COCOA_COMBOBOX_H__
