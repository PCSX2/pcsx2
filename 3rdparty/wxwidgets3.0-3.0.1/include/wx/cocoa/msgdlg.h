/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/msgdlg.h
// Purpose:     wxMessageDialog class
// Author:      Gareth Simpson
// Created:     2007-10-29
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COCOA_MSGDLG_H_
#define _WX_COCOA_MSGDLG_H_

#include "wx/msgdlg.h"

DECLARE_WXCOCOA_OBJC_CLASS(NSAlert);

#ifndef wxUSE_COCOA_NATIVE_MSGDLG
// trunk: Always use Cocoa dialog
// 2.8: Only use Cocoa dialog if ABI incompatible features is on
// Build both on both branches (there was no wxCocoaMessageDialog class so it's not an ABI issue)
    #if 1/* wxUSE_ABI_INCOMPATIBLE_FEATURES */
        #define wxUSE_COCOA_NATIVE_MSGDLG 1
    #else
        #define wxUSE_COCOA_NATIVE_MSGDLG 0
    #endif
#endif

#if wxUSE_COCOA_NATIVE_MSGDLG
    #define wxMessageDialog wxCocoaMessageDialog
#else
    #include "wx/generic/msgdlgg.h"

    #define wxMessageDialog wxGenericMessageDialog
#endif

// ----------------------------------------------------------------------------
// wxCocoaMessageDialog
// ----------------------------------------------------------------------------


class WXDLLIMPEXP_CORE wxCocoaMessageDialog
    : public wxMessageDialogWithCustomLabels
{
public:
    wxCocoaMessageDialog(wxWindow *parent,
                    const wxString& message,
                    const wxString& caption = wxMessageBoxCaptionStr,
                    long style = wxOK|wxCENTRE,
                    const wxPoint& pos = wxDefaultPosition);

    virtual int ShowModal();

protected:
    // not supported for message dialog
    virtual void DoSetSize(int WXUNUSED(x), int WXUNUSED(y),
                           int WXUNUSED(width), int WXUNUSED(height),
                           int WXUNUSED(sizeFlags) = wxSIZE_AUTO) {}

    // override wxMessageDialogWithCustomLabels method to get rid of
    // accelerators in the custom label strings
    //
    // VZ: I have no idea _why_ do we do this but the old version did and
    //     I didn't want to change the existing behaviour
    virtual void DoSetCustomLabel(wxString& var, const ButtonLabel& label);

    DECLARE_DYNAMIC_CLASS(wxCocoaMessageDialog)
    wxDECLARE_NO_COPY_CLASS(wxCocoaMessageDialog);
};

#endif // _WX_MSGDLG_H_

