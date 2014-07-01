/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/dcscreen.h
// Purpose:     wxScreenDC class
// Author:      David Webster
// Modified by:
// Created:     10/14/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OS2_DCSCREEN_H_
#define _WX_OS2_DCSCREEN_H_

#include "wx/dcscreen.h"
#include "wx/os2/dc.h"

class WXDLLIMPEXP_CORE wxScreenDCImpl: public wxPMDCImpl
{
 public:
    // Create a DC representing the whole screen
    wxScreenDCImpl( wxScreenDC *owner );

    virtual void DoGetSize( int* pnWidth
                           ,int* pnHeight
                          ) const;

private:
    DECLARE_CLASS(wxScreenDCImpl)
    wxDECLARE_NO_COPY_CLASS(wxScreenDCImpl);
}; // end of CLASS wxScreenDC

#endif
    // _WX_DCSCREEN_H_
