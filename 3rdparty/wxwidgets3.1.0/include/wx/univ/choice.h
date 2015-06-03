///////////////////////////////////////////////////////////////////////////////
// Name:        wx/univ/choice.h
// Purpose:     the universal choice
// Author:      Vadim Zeitlin
// Modified by:
// Created:     30.08.00
// Copyright:   (c) 2000 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIV_CHOICE_H_
#define _WX_UNIV_CHOICE_H_

#include "wx/combobox.h"

// VS: This is only a *temporary* implementation, real wxChoice should not
//     derive from wxComboBox and may have different l&f
class WXDLLIMPEXP_CORE wxChoice : public wxComboBox
{
public:
    wxChoice() {}
    wxChoice(wxWindow *parent, wxWindowID id,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize,
            int n = 0, const wxString choices[] = (const wxString *) NULL,
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
            const wxString& name = wxChoiceNameStr);

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

private:
    void OnComboBox(wxCommandEvent &event);

    wxDECLARE_EVENT_TABLE();
    wxDECLARE_DYNAMIC_CLASS(wxChoice);
};


#endif // _WX_UNIV_CHOICE_H_
