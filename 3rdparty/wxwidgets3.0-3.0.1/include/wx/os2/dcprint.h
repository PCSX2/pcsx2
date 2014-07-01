/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/dcprint.h
// Purpose:     wxPrinterDC class
// Author:      David Webster
// Modified by:
// Created:     09/12/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DCPRINT_H_
#define _WX_DCPRINT_H_

#if wxUSE_PRINTING_ARCHITECTURE

#include "wx/dc.h"
#include "wx/cmndata.h"
#include "wx/os2/dc.h"

class WXDLLIMPEXP_CORE wxPrinterDCImpl: public wxPMDCImpl
{
 public:
    // Create a printer DC

    // Create from print data
    wxPrinterDCImpl( wxPrinterDC *owner, const wxPrintData& rData );
    wxPrinterDCImpl( wxPrinterDC *owner, WXHDC hTheDC);

    // override some base class virtuals
    virtual bool StartDoc(const wxString& rsMessage);
    virtual void EndDoc(void);
    virtual void StartPage(void);
    virtual void EndPage(void);

    virtual wxRect GetPaperRect() const;

protected:
    virtual void DoDrawBitmap( const wxBitmap& rBmp
                              ,wxCoord         vX
                              ,wxCoord         vY
                              ,bool            bUseMask = FALSE
                             );
    virtual bool DoBlit( wxCoord vXdest
                        ,wxCoord vYdest
                        ,wxCoord vWidth
                        ,wxCoord vHeight
                        ,wxDC*   pSource
                        ,wxCoord vXsrc
                        ,wxCoord vYsrc
                        ,wxRasterOperationMode     nRop = wxCOPY
                        ,bool    bUseMask = FALSE
                        ,wxCoord vXsrcMask = -1
                        ,wxCoord vYsrcMask = -1
                       );

    // init the dc
    void Init(void);

    wxPrintData                     m_printData;
private:
    DECLARE_CLASS(wxPrinterDCImpl)
    wxDECLARE_NO_COPY_CLASS(wxPrinterDCImpl);
}; // end of CLASS wxPrinterDC

// Gets an HDC for the specified printer configuration
WXHDC WXDLLIMPEXP_CORE wxGetPrinterDC(const wxPrintData& rData);

#endif // wxUSE_PRINTING_ARCHITECTURE

#endif
    // _WX_DCPRINT_H_

