/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/bmpbuttn.h
// Purpose:     wxBitmapButton class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_BMPBUTTN_H_
#define _WX_OSX_BMPBUTTN_H_

#include "wx/button.h"

#define wxDEFAULT_BUTTON_MARGIN 4

class WXDLLIMPEXP_CORE wxBitmapButton : public wxBitmapButtonBase
{
public:
    wxBitmapButton()
        {
            SetMargins(wxDEFAULT_BUTTON_MARGIN, wxDEFAULT_BUTTON_MARGIN);
        }

    wxBitmapButton(wxWindow *parent, wxWindowID id, const wxBitmap& bitmap,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize, long style = wxBU_AUTODRAW,
                   const wxValidator& validator = wxDefaultValidator,
                   const wxString& name = wxButtonNameStr)
        {
            Create(parent, id, bitmap, pos, size, style, validator, name);
        }

    bool Create(wxWindow *parent, wxWindowID id, const wxBitmap& bitmap,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize, long style = wxBU_AUTODRAW,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxButtonNameStr);

protected:

    virtual wxSize DoGetBestSize() const;

    DECLARE_DYNAMIC_CLASS(wxBitmapButton)
};

#endif // _WX_OSX_BMPBUTTN_H_
