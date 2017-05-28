/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/dcscreen.h
// Purpose:     wxScreenDC class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_DCSCREEN_H_
#define _WX_MSW_DCSCREEN_H_

#include "wx/dcscreen.h"
#include "wx/msw/dc.h"

class WXDLLIMPEXP_CORE wxScreenDCImpl : public wxMSWDCImpl
{
public:
    // Create a DC representing the whole screen
    wxScreenDCImpl( wxScreenDC *owner );

    virtual void DoGetSize(int *w, int *h) const
    {
        GetDeviceSize(w, h);
    }

    wxDECLARE_CLASS(wxScreenDCImpl);
    wxDECLARE_NO_COPY_CLASS(wxScreenDCImpl);
};

#endif // _WX_MSW_DCSCREEN_H_

