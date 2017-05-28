///////////////////////////////////////////////////////////////////////////////
// Name:        wx/checkbox.h
// Purpose:     wxCheckBox class interface
// Author:      Vadim Zeitlin
// Modified by:
// Created:     07.09.00
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CHECKBOX_H_BASE_
#define _WX_CHECKBOX_H_BASE_

#include "wx/defs.h"

#if wxUSE_CHECKBOX

#include "wx/control.h"


/*
 * wxCheckBox style flags
 * (Using wxCHK_* because wxCB_* is used by wxComboBox).
 * Determine whether to use a 3-state or 2-state
 * checkbox. 3-state enables to differentiate
 * between 'unchecked', 'checked' and 'undetermined'.
 *
 * In addition to the styles here it is also possible to specify just 0 which
 * is treated the same as wxCHK_2STATE for compatibility (but using explicit
 * flag is preferred).
 */
#define wxCHK_2STATE           0x4000
#define wxCHK_3STATE           0x1000

/*
 * If this style is set the user can set the checkbox to the
 * undetermined state. If not set the undetermined set can only
 * be set programmatically.
 * This style can only be used with 3 state checkboxes.
 */
#define wxCHK_ALLOW_3RD_STATE_FOR_USER 0x2000

extern WXDLLIMPEXP_DATA_CORE(const char) wxCheckBoxNameStr[];

// ----------------------------------------------------------------------------
// wxCheckBox: a control which shows a label and a box which may be checked
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxCheckBoxBase : public wxControl
{
public:
    wxCheckBoxBase() { }

    // set/get the checked status of the listbox
    virtual void SetValue(bool value) = 0;
    virtual bool GetValue() const = 0;

    bool IsChecked() const
    {
        wxASSERT_MSG( !Is3State(), wxT("Calling IsChecked() doesn't make sense for")
            wxT(" a three state checkbox, Use Get3StateValue() instead") );

        return GetValue();
    }

    wxCheckBoxState Get3StateValue() const
    {
        wxCheckBoxState state = DoGet3StateValue();

        if ( state == wxCHK_UNDETERMINED && !Is3State() )
        {
            // Undetermined state with a 2-state checkbox??
            wxFAIL_MSG( wxT("DoGet3StateValue() says the 2-state checkbox is ")
                wxT("in an undetermined/third state") );

            state = wxCHK_UNCHECKED;
        }

        return state;
    }

    void Set3StateValue(wxCheckBoxState state)
    {
        if ( state == wxCHK_UNDETERMINED && !Is3State() )
        {
            wxFAIL_MSG(wxT("Setting a 2-state checkbox to undetermined state"));
            state = wxCHK_UNCHECKED;
        }

        DoSet3StateValue(state);
    }

    bool Is3State() const { return HasFlag(wxCHK_3STATE); }

    bool Is3rdStateAllowedForUser() const
    {
        return HasFlag(wxCHK_ALLOW_3RD_STATE_FOR_USER);
    }

    virtual bool HasTransparentBackground() wxOVERRIDE { return true; }

    // wxCheckBox-specific processing after processing the update event
    virtual void DoUpdateWindowUI(wxUpdateUIEvent& event) wxOVERRIDE
    {
        wxControl::DoUpdateWindowUI(event);

        if ( event.GetSetChecked() )
            SetValue(event.GetChecked());
    }

protected:
    // choose the default border for this window
    virtual wxBorder GetDefaultBorder() const wxOVERRIDE { return wxBORDER_NONE; }

    virtual void DoSet3StateValue(wxCheckBoxState WXUNUSED(state)) { wxFAIL; }

    virtual wxCheckBoxState DoGet3StateValue() const
    {
        wxFAIL;
        return wxCHK_UNCHECKED;
    }

    // Helper function to be called from derived classes Create()
    // implementations: it checks that the style doesn't contain any
    // incompatible bits and modifies it to be sane if it does.
    static void WXValidateStyle(long *stylePtr)
    {
        long& style = *stylePtr;

        if ( !(style & (wxCHK_2STATE | wxCHK_3STATE)) )
        {
            // For compatibility we use absence of style flags as wxCHK_2STATE
            // because wxCHK_2STATE used to have the value of 0 and some
            // existing code uses 0 instead of it. Moreover, some code even
            // uses some non-0 style, e.g. wxBORDER_XXX, but doesn't specify
            // neither wxCHK_2STATE nor wxCHK_3STATE -- to avoid breaking it,
            // assume (much more common) 2 state checkbox by default.
            style |= wxCHK_2STATE;
        }

        if ( style & wxCHK_3STATE )
        {
            if ( style & wxCHK_2STATE )
            {
                wxFAIL_MSG( "wxCHK_2STATE and wxCHK_3STATE can't be used "
                            "together" );
                style &= ~wxCHK_3STATE;
            }
        }
        else // No wxCHK_3STATE
        {
            if ( style & wxCHK_ALLOW_3RD_STATE_FOR_USER )
            {
                wxFAIL_MSG( "wxCHK_ALLOW_3RD_STATE_FOR_USER doesn't make sense "
                            "without wxCHK_3STATE" );
                style &= ~wxCHK_ALLOW_3RD_STATE_FOR_USER;
            }
        }
    }

private:
    wxDECLARE_NO_COPY_CLASS(wxCheckBoxBase);
};

// Most ports support 3 state checkboxes so define this by default.
#define wxHAS_3STATE_CHECKBOX

#if defined(__WXUNIVERSAL__)
    #include "wx/univ/checkbox.h"
#elif defined(__WXMSW__)
    #include "wx/msw/checkbox.h"
#elif defined(__WXMOTIF__)
    #include "wx/motif/checkbox.h"
#elif defined(__WXGTK20__)
    #include "wx/gtk/checkbox.h"
#elif defined(__WXGTK__)
    #undef wxHAS_3STATE_CHECKBOX
    #include "wx/gtk1/checkbox.h"
#elif defined(__WXMAC__)
    #include "wx/osx/checkbox.h"
#elif defined(__WXQT__)
    #include "wx/qt/checkbox.h"
#endif

#endif // wxUSE_CHECKBOX

#endif // _WX_CHECKBOX_H_BASE_
