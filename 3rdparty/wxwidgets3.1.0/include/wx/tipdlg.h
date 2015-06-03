///////////////////////////////////////////////////////////////////////////////
// Name:        wx/tipdlg.h
// Purpose:     declaration of wxTipDialog
// Author:      Vadim Zeitlin
// Modified by:
// Created:     28.06.99
// Copyright:   (c) Vadim Zeitlin
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TIPDLG_H_
#define _WX_TIPDLG_H_

// ----------------------------------------------------------------------------
// headers which we must include here
// ----------------------------------------------------------------------------

#include "wx/defs.h"

#if wxUSE_STARTUP_TIPS

#include "wx/textfile.h"

// ----------------------------------------------------------------------------
// wxTipProvider - a class which is used by wxTipDialog to get the text of the
// tips
// ----------------------------------------------------------------------------

// the abstract base class: it provides the tips, i.e. implements the GetTip()
// function which returns the new tip each time it's called. To support this,
// wxTipProvider evidently needs some internal state which is the tip "index"
// and which should be saved/restored by the program to not always show one and
// the same tip (of course, you may use random starting position as well...)
class WXDLLIMPEXP_ADV wxTipProvider
{
public:
    wxTipProvider(size_t currentTip) { m_currentTip = currentTip; }

    // get the current tip and update the internal state to return the next tip
    // when called for the next time
    virtual wxString GetTip() = 0;

    // get the current tip "index" (or whatever allows the tip provider to know
    // from where to start the next time)
    size_t GetCurrentTip() const { return m_currentTip; }

    // virtual dtor for the base class
    virtual ~wxTipProvider() { }


#if WXWIN_COMPATIBILITY_3_0
    wxDEPRECATED_MSG("this method does nothing, simply don't call it")
    wxString PreprocessTip(const wxString& tip) { return tip; }
#endif

protected:
    size_t m_currentTip;
};

// a function which returns an implementation of wxTipProvider using the
// specified text file as the source of tips (each line is a tip).
//
// NB: the caller is responsible for deleting the pointer!
#if wxUSE_TEXTFILE
WXDLLIMPEXP_ADV wxTipProvider *wxCreateFileTipProvider(const wxString& filename,
                                                       size_t currentTip);
#endif // wxUSE_TEXTFILE

// ----------------------------------------------------------------------------
// wxTipDialog
// ----------------------------------------------------------------------------

// A dialog which shows a "tip" - a short and helpful messages describing to
// the user some program characteristic. Many programs show the tips at
// startup, so the dialog has "Show tips on startup" checkbox which allows to
// the user to disable this (however, it's the program which should show, or
// not, the dialog on startup depending on its value, not this class).
//
// The function returns true if this checkbox is checked, false otherwise.
WXDLLIMPEXP_ADV bool wxShowTip(wxWindow *parent,
                               wxTipProvider *tipProvider,
                               bool showAtStartup = true);

#endif // wxUSE_STARTUP_TIPS

#endif // _WX_TIPDLG_H_
