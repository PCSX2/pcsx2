/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/button.h
// Purpose:     wxButton class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_BUTTON_H_
#define _WX_OSX_BUTTON_H_

#include "wx/control.h"
#include "wx/gdicmn.h"

// Pushbutton
class WXDLLIMPEXP_CORE wxButton : public wxButtonBase
{
public:
    wxButton() {}
    wxButton(wxWindow *parent,
             wxWindowID id,
             const wxString& label = wxEmptyString,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = 0,
             const wxValidator& validator = wxDefaultValidator,
             const wxString& name = wxButtonNameStr)
    {
        Create(parent, id, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& label = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxButtonNameStr);

    virtual void SetLabel(const wxString& label);
    virtual wxWindow *SetDefault();
    virtual void Command(wxCommandEvent& event);

    // osx specific event handling common for all osx-ports

    virtual bool        OSXHandleClicked( double timestampsec );

#if wxOSX_USE_COCOA
    void OSXUpdateAfterLabelChange(const wxString& label);
#endif

protected:
    DECLARE_DYNAMIC_CLASS(wxButton)
};

// OS X specific class, not part of public wx API
class WXDLLIMPEXP_CORE wxDisclosureTriangle : public wxControl
{
public:
    wxDisclosureTriangle(wxWindow *parent,
             wxWindowID id,
             const wxString& label = wxEmptyString,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = wxBORDER_NONE,
             const wxValidator& validator = wxDefaultValidator,
             const wxString& name = wxButtonNameStr)
    {
        Create(parent, id, label, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& label = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxBORDER_NONE,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxButtonNameStr);

    void SetOpen( bool open );
    bool IsOpen() const;

    // osx specific event handling common for all osx-ports

    virtual bool        OSXHandleClicked( double timestampsec );

protected:
    virtual wxSize DoGetBestSize() const ;
};

#endif // _WX_OSX_BUTTON_H_
