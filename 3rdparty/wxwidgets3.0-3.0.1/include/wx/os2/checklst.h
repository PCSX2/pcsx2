///////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/checklst.h
// Purpose:     wxCheckListBox class - a listbox with checkable items
//              Note: this is an optional class.
// Author:      David Webster
// Modified by:
// Created:     10/13/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CHECKLST_H_
#define _WX_CHECKLST_H_

#include <stddef.h>

#include "wx/defs.h"

class wxOwnerDrawn; // so the compiler knows, it is a class.

class WXDLLIMPEXP_CORE wxCheckListBox : public wxCheckListBoxBase
{
public:
    //
    // Ctors
    //
    wxCheckListBox();
    wxCheckListBox( wxWindow*          pParent
                   ,wxWindowID         vId
                   ,const wxPoint&     rPos = wxDefaultPosition
                   ,const wxSize&      vSize = wxDefaultSize
                   ,int                nStrings = 0
                   ,const wxString     asChoices[] = NULL
                   ,long               lStyle = 0
                   ,const wxValidator& rValidator = wxDefaultValidator
                   ,const wxString&    rsName = wxListBoxNameStr
                  );
    wxCheckListBox( wxWindow*            pParent
                   ,wxWindowID           vId
                   ,const wxPoint&       rPos
                   ,const wxSize&        vSize
                   ,const wxArrayString& asChoices
                   ,long                 lStyle = 0
                   ,const wxValidator&   rValidator = wxDefaultValidator
                   ,const wxString&      rsName = wxListBoxNameStr
                  );

    //
    // Override base class virtuals
    //
    virtual void Delete(unsigned int n);

    virtual bool SetFont(const wxFont &rFont);

    //
    // Items may be checked
    //
    bool IsChecked(unsigned int uiIndex) const;
    void Check(unsigned int uiIndex, bool bCheck = true);

    //
    // Accessors
    //
    size_t GetItemHeight(void) const { return m_nItemHeight; }

protected:
    //
    // We create our items ourselves and they have non-standard size,
    // so we need to override these functions
    //
    virtual wxOwnerDrawn* CreateItem(size_t n);
    virtual long          OS2OnMeasure(WXMEASUREITEMSTRUCT* pItem);

    //
    // Pressing space or clicking the check box toggles the item
    //
    void OnChar(wxKeyEvent& rEvent);
    void OnLeftClick(wxMouseEvent& rEvent);

private:
    size_t m_nItemHeight;  // height of checklistbox items (the same for all)

    DECLARE_DYNAMIC_CLASS(wxCheckListBox)
    DECLARE_EVENT_TABLE()
}; // end of CLASS wxCheckListBox

#endif
   // _WX_CHECKLST_H_
