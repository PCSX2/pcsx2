/////////////////////////////////////////////////////////////////////////////
// Name:        wx/tglbtn.h
// Purpose:     This dummy header includes the proper header file for the
//              system we're compiling under.
// Author:      John Norris, minor changes by Axel Schlueter
// Modified by:
// Created:     08.02.01
// Copyright:   (c) 2000 Johnny C. Norris II
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TOGGLEBUTTON_H_BASE_
#define _WX_TOGGLEBUTTON_H_BASE_

#include "wx/defs.h"

#if wxUSE_TOGGLEBTN

#include "wx/event.h"
#include "wx/anybutton.h"     // base class

extern WXDLLIMPEXP_DATA_CORE(const char) wxCheckBoxNameStr[];

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_CORE, wxEVT_TOGGLEBUTTON, wxCommandEvent );

// ----------------------------------------------------------------------------
// wxToggleButtonBase
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxToggleButtonBase : public wxAnyButton
{
public:
    wxToggleButtonBase() { }

    // Get/set the value
    virtual void SetValue(bool state) = 0;
    virtual bool GetValue() const = 0;

    void UpdateWindowUI(long flags)
    {
        wxControl::UpdateWindowUI(flags);

        if ( !IsShown() )
            return;

        wxWindow *tlw = wxGetTopLevelParent( this );
        if (tlw && wxPendingDelete.Member( tlw ))
           return;

        wxUpdateUIEvent event( GetId() );
        event.SetEventObject(this);

        if (GetEventHandler()->ProcessEvent(event) )
        {
            if ( event.GetSetChecked() )
                SetValue( event.GetChecked() );
        }
    }

    // Buttons on MSW can look bad if they are not native colours, because
    // then they become owner-drawn and not theme-drawn.  Disable it here
    // in wxToggleButtonBase to make it consistent.
    virtual bool ShouldInheritColours() const { return false; }

protected:
    // choose the default border for this window
    virtual wxBorder GetDefaultBorder() const { return wxBORDER_NONE; }

    wxDECLARE_NO_COPY_CLASS(wxToggleButtonBase);
};


#define EVT_TOGGLEBUTTON(id, fn) \
    wx__DECLARE_EVT1(wxEVT_TOGGLEBUTTON, id, wxCommandEventHandler(fn))

#if defined(__WXUNIVERSAL__)
    #include "wx/univ/tglbtn.h"
#elif defined(__WXMSW__)
    #include "wx/msw/tglbtn.h"
    #define wxHAS_BITMAPTOGGLEBUTTON
#elif defined(__WXGTK20__)
    #include "wx/gtk/tglbtn.h"
    #define wxHAS_BITMAPTOGGLEBUTTON
#elif defined(__WXGTK__)
    #include "wx/gtk1/tglbtn.h"
# elif defined(__WXMOTIF__)
    #include "wx/motif/tglbtn.h"
#elif defined(__WXMAC__)
    #include "wx/osx/tglbtn.h"
    #define wxHAS_BITMAPTOGGLEBUTTON
#elif defined(__WXPM__)
    #include "wx/os2/tglbtn.h"
#endif

// old wxEVT_COMMAND_* constants
#define wxEVT_COMMAND_TOGGLEBUTTON_CLICKED   wxEVT_TOGGLEBUTTON

#endif // wxUSE_TOGGLEBTN

#endif // _WX_TOGGLEBUTTON_H_BASE_

