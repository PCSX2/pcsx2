/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/choice.h
// Purpose:     wxChoice class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CHOICE_H_
#define _WX_CHOICE_H_

#include "wx/control.h"

#include  "wx/dynarray.h"
#include  "wx/arrstr.h"

WX_DEFINE_ARRAY( char * , wxChoiceDataArray ) ;

// Choice item
class WXDLLIMPEXP_CORE wxChoice: public wxChoiceBase
{
    DECLARE_DYNAMIC_CLASS(wxChoice)

public:
    wxChoice()
        : m_strings(), m_datas()
        {}

    virtual ~wxChoice() ;

    wxChoice(wxWindow *parent, wxWindowID id,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             int n = 0, const wxString choices[] = NULL,
             long style = 0,
             const wxValidator& validator = wxDefaultValidator,
             const wxString& name = wxChoiceNameStr)
    {
        Create(parent, id, pos, size, n, choices, style, validator, name);
    }
    wxChoice(wxWindow *parent, wxWindowID id,
             const wxPoint& pos,
             const wxSize& size,
             const wxArrayString& choices,
             long style = 0,
             const wxValidator& validator = wxDefaultValidator,
             const wxString& name = wxChoiceNameStr)
    {
        Create(parent, id, pos, size, choices, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                int n = 0, const wxString choices[] = NULL,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxChoiceNameStr);
    bool Create(wxWindow *parent, wxWindowID id,
                const wxPoint& pos,
                const wxSize& size,
                const wxArrayString& choices,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxChoiceNameStr);

    virtual unsigned int GetCount() const ;
    virtual int GetSelection() const ;
    virtual void SetSelection(int n);

    virtual int FindString(const wxString& s, bool bCase = false) const;
    virtual wxString GetString(unsigned int n) const ;
    virtual void SetString(unsigned int pos, const wxString& s);
    // osx specific event handling common for all osx-ports

    virtual bool        OSXHandleClicked( double timestampsec );

protected:
    virtual void DoDeleteOneItem(unsigned int n);
    virtual void DoClear();

    virtual wxSize DoGetBestSize() const ;
    virtual int DoInsertItems(const wxArrayStringsAdapter& items,
                              unsigned int pos,
                              void **clientData, wxClientDataType type);

    virtual void DoSetItemClientData(unsigned int n, void* clientData);
    virtual void* DoGetItemClientData(unsigned int n) const;

    wxArrayString m_strings;
    wxChoiceDataArray m_datas ;
    wxMenu*    m_popUpMenu ;

private:
    // This should be called when the number of items in the control changes.
    void DoAfterItemCountChange();
};

#endif
    // _WX_CHOICE_H_
