/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/dcscreen.h
// Purpose:     wxScreenDC class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DCSCREEN_H_
#define _WX_DCSCREEN_H_

#include "wx/dcclient.h"
#include "wx/osx/dcclient.h"

class WXDLLIMPEXP_CORE wxScreenDCImpl: public wxWindowDCImpl
{
public:
    wxScreenDCImpl( wxDC *owner );
    virtual ~wxScreenDCImpl();

    virtual wxBitmap DoGetAsBitmap(const wxRect *subrect) const;
private:
    void* m_overlayWindow;

private:
    DECLARE_CLASS(wxScreenDCImpl)
    wxDECLARE_NO_COPY_CLASS(wxScreenDCImpl);
};

#endif
    // _WX_DCSCREEN_H_

