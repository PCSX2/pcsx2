///////////////////////////////////////////////////////////////////////////////
// Name:        wx/univ/checklst.h
// Purpose:     wxCheckListBox class for wxUniversal
// Author:      Vadim Zeitlin
// Modified by:
// Created:     12.09.00
// Copyright:   (c) Vadim Zeitlin
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIV_CHECKLST_H_
#define _WX_UNIV_CHECKLST_H_

// ----------------------------------------------------------------------------
// actions
// ----------------------------------------------------------------------------

#define wxACTION_CHECKLISTBOX_TOGGLE wxT("toggle")

// ----------------------------------------------------------------------------
// wxCheckListBox
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxCheckListBox : public wxCheckListBoxBase
{
public:
    // ctors
    wxCheckListBox() { Init(); }

    wxCheckListBox(wxWindow *parent,
                   wxWindowID id,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   int nStrings = 0,
                   const wxString choices[] = NULL,
                   long style = 0,
                   const wxValidator& validator = wxDefaultValidator,
                   const wxString& name = wxListBoxNameStr)
    {
        Init();

        Create(parent, id, pos, size, nStrings, choices, style, validator, name);
    }
    wxCheckListBox(wxWindow *parent,
                   wxWindowID id,
                   const wxPoint& pos,
                   const wxSize& size,
                   const wxArrayString& choices,
                   long style = 0,
                   const wxValidator& validator = wxDefaultValidator,
                   const wxString& name = wxListBoxNameStr);

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                int nStrings = 0,
                const wxString choices[] = (const wxString *) NULL,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxListBoxNameStr);
    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos,
                const wxSize& size,
                const wxArrayString& choices,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxListBoxNameStr);

    // implement check list box methods
    virtual bool IsChecked(unsigned int item) const wxOVERRIDE;
    virtual void Check(unsigned int item, bool check = true) wxOVERRIDE;

    // and input handling
    virtual bool PerformAction(const wxControlAction& action,
                               long numArg = -1l,
                               const wxString& strArg = wxEmptyString) wxOVERRIDE;

    static wxInputHandler *GetStdInputHandler(wxInputHandler *handlerDef);
    virtual wxInputHandler *DoGetStdInputHandler(wxInputHandler *handlerDef) wxOVERRIDE
    {
        return GetStdInputHandler(handlerDef);
    }

protected:
    // override all methods which add/delete items to update m_checks array as
    // well
    virtual void OnItemInserted(unsigned int pos) wxOVERRIDE;
    virtual void DoDeleteOneItem(unsigned int n) wxOVERRIDE;
    virtual void DoClear() wxOVERRIDE;

    // draw the check items instead of the usual ones
    virtual void DoDrawRange(wxControlRenderer *renderer,
                             int itemFirst, int itemLast) wxOVERRIDE;

    // take them also into account for size calculation
    virtual wxSize DoGetBestClientSize() const wxOVERRIDE;

    // common part of all ctors
    void Init();

private:
    // the array containing the checked status of the items
    wxArrayInt m_checks;

    wxDECLARE_DYNAMIC_CLASS(wxCheckListBox);
};

#endif // _WX_UNIV_CHECKLST_H_
