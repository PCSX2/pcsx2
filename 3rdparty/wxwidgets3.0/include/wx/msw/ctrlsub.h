///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/ctrlsub.h
// Purpose:     common functionality of wxItemContainer-derived controls
// Author:      Vadim Zeitlin
// Created:     2007-07-25
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_CTRLSUB_H_
#define _WX_MSW_CTRLSUB_H_

// ----------------------------------------------------------------------------
// wxControlWithItems
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxControlWithItems : public wxControlWithItemsBase
{
public:
    wxControlWithItems() { }

protected:
    // preallocate memory for inserting the given new items into the control
    // using the wm message (normally either LB_INITSTORAGE or CB_INITSTORAGE)
    void MSWAllocStorage(const wxArrayStringsAdapter& items, unsigned wm);

    // insert or append a string to the controls using the given message
    // (one of {CB,LB}_{ADD,INSERT}STRING, pos must be 0 when appending)
    int MSWInsertOrAppendItem(unsigned pos, const wxString& item, unsigned wm);

    // normally the control containing the items is this window itself but if
    // the derived control is composed of several windows, this method can be
    // overridden to return the real list/combobox control
    virtual WXHWND MSWGetItemsHWND() const { return GetHWND(); }

private:
    DECLARE_ABSTRACT_CLASS(wxControlWithItems)
    wxDECLARE_NO_COPY_CLASS(wxControlWithItems);
};

#endif // _WX_MSW_CTRLSUB_H_

