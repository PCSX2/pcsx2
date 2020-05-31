///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/wince/checklst.h
// Purpose:     wxCheckListBox class - a listbox with checkable items
// Author:      Wlodzimierz ABX Skiba
// Modified by:
// Created:     30.10.2005
// Copyright:   (c) Wlodzimierz Skiba
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef   __CHECKLSTCE__H_
#define   __CHECKLSTCE__H_

class WXDLLIMPEXP_CORE wxCheckListBox : public wxCheckListBoxBase
{
public:
    // ctors
    wxCheckListBox();
    wxCheckListBox(wxWindow *parent, wxWindowID id,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   int nStrings = 0,
                   const wxString choices[] = NULL,
                   long style = 0,
                   const wxValidator& validator = wxDefaultValidator,
                   const wxString& name = wxListBoxNameStr);
    wxCheckListBox(wxWindow *parent, wxWindowID id,
                   const wxPoint& pos,
                   const wxSize& size,
                   const wxArrayString& choices,
                   long style = 0,
                   const wxValidator& validator = wxDefaultValidator,
                   const wxString& name = wxListBoxNameStr);
    virtual ~wxCheckListBox();

    bool Create(wxWindow *parent, wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                int n = 0, const wxString choices[] = NULL,
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

    // items may be checked
    virtual bool IsChecked(unsigned int uiIndex) const;
    virtual void Check(unsigned int uiIndex, bool bCheck = true);

    // public interface derived from wxListBox and lower classes
    virtual void DoClear();
    virtual void DoDeleteOneItem(unsigned int n);
    virtual unsigned int GetCount() const;
    virtual int GetSelection() const;
    virtual int GetSelections(wxArrayInt& aSelections) const;
    virtual wxString GetString(unsigned int n) const;
    virtual bool IsSelected(int n) const;
    virtual void SetString(unsigned int n, const wxString& s);

    // Implementation
    virtual bool MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM *result);
protected:

    void OnSize(wxSizeEvent& event);

    // protected interface derived from wxListBox and lower classes
    virtual int DoInsertItems(const wxArrayStringsAdapter& items,
                              unsigned int pos,
                              void **clientData, wxClientDataType type);

    virtual void* DoGetItemClientData(unsigned int n) const;
    virtual void DoSetItemClientData(unsigned int n, void* clientData);
    virtual void DoSetFirstItem(int n);
    virtual void DoSetSelection(int n, bool select);
    // convert our styles to Windows
    virtual WXDWORD MSWGetStyle(long style, WXDWORD *exstyle) const;

private:
    wxArrayPtrVoid m_itemsClientData;

    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxCheckListBox)
};

#endif    //_CHECKLSTCE_H
