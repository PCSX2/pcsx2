///////////////////////////////////////////////////////////////////////////////
// Name:        wx/checklst.h
// Purpose:     wxCheckListBox class interface
// Author:      Vadim Zeitlin
// Modified by:
// Created:     12.09.00
// Copyright:   (c) Vadim Zeitlin
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CHECKLST_H_BASE_
#define _WX_CHECKLST_H_BASE_

#include "wx/defs.h"

#if wxUSE_CHECKLISTBOX

#include "wx/listbox.h"

// ----------------------------------------------------------------------------
// wxCheckListBox: a listbox whose items may be checked
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxCheckListBoxBase : public
                                              #ifdef __WXWINCE__
                                                  // keep virtuals synchronised
                                                  wxListBoxBase
                                              #else
                                                  wxListBox
                                              #endif
{
public:
    wxCheckListBoxBase() { }

    // check list box specific methods
    virtual bool IsChecked(unsigned int item) const = 0;
    virtual void Check(unsigned int item, bool check = true) = 0;

    virtual unsigned int GetCheckedItems(wxArrayInt& checkedItems) const;

    wxDECLARE_NO_COPY_CLASS(wxCheckListBoxBase);
};

#if defined(__WXUNIVERSAL__)
    #include "wx/univ/checklst.h"
#elif defined(__WXWINCE__)
    #include "wx/msw/wince/checklst.h"
#elif defined(__WXMSW__)
    #include "wx/msw/checklst.h"
#elif defined(__WXMOTIF__)
    #include "wx/motif/checklst.h"
#elif defined(__WXGTK20__)
    #include "wx/gtk/checklst.h"
#elif defined(__WXGTK__)
    #include "wx/gtk1/checklst.h"
#elif defined(__WXMAC__)
    #include "wx/osx/checklst.h"
#elif defined(__WXCOCOA__)
    #include "wx/cocoa/checklst.h"
#elif defined(__WXPM__)
    #include "wx/os2/checklst.h"
#endif

#endif // wxUSE_CHECKLISTBOX

#endif
    // _WX_CHECKLST_H_BASE_
