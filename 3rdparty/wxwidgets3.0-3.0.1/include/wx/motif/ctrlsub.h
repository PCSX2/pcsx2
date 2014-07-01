///////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/ctrlsub.h
// Purpose:     common functionality of wxItemContainer-derived controls
// Author:      Vadim Zeitlin
// Created:     2007-07-25
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MOTIF_CTRLSUB_H_
#define _WX_MOTIF_CTRLSUB_H_

#include "wx/dynarray.h"
#include "wx/generic/ctrlsub.h"

// ----------------------------------------------------------------------------
// wxControlWithItems
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxControlWithItems : public wxControlWithItemsGeneric
{
public:
    wxControlWithItems() { }

protected:
    // Motif functions inserting items in the control interpret positions
    // differently from wx: they're 1-based and 0 means to append
    unsigned int GetMotifPosition(unsigned int pos) const
    {
        return pos == GetCount() ? 0 : pos + 1;
    }

private:
    DECLARE_ABSTRACT_CLASS(wxControlWithItems)
    wxDECLARE_NO_COPY_CLASS(wxControlWithItems);
};

#endif // _WX_MOTIF_CTRLSUB_H_

