/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/palette.h
// Purpose:     wxPalette class
// Author:      David Webster
// Modified by:
// Created:     10/12/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PALETTE_H_
#define _WX_PALETTE_H_

#include "wx/gdiobj.h"
#include "wx/os2/private.h"

class WXDLLIMPEXP_FWD_CORE wxPalette;

class WXDLLIMPEXP_CORE wxPaletteRefData: public wxGDIRefData
{
    friend class WXDLLIMPEXP_FWD_CORE wxPalette;
public:
    wxPaletteRefData();
    virtual ~wxPaletteRefData();
// protected:
    WXHPALETTE                      m_hPalette;
    HPS                             m_hPS;
}; // end of CLASS wxPaletteRefData

#define M_PALETTEDATA ((wxPaletteRefData *)m_refData)

class WXDLLIMPEXP_CORE wxPalette: public wxPaletteBase
{
public:
    wxPalette();

    wxPalette( int                  n
              ,const unsigned char* pRed
              ,const unsigned char* pGreen
              ,const unsigned char* pBlue
             );
    virtual ~wxPalette();

    bool Create( int                  n
                ,const unsigned char* pRed
                ,const unsigned char* pGreen
                ,const unsigned char* pBlue
               );
    int  GetPixel( unsigned char cRed
                  ,unsigned char cGreen
                  ,unsigned char cBlue
                 ) const;
    bool GetRGB( int            nPixel
                ,unsigned char* pRed
                ,unsigned char* pGreen
                ,unsigned char* pBlue
               ) const;

    virtual bool FreeResource(bool bForce = false);

    inline WXHPALETTE GetHPALETTE(void) const { return (M_PALETTEDATA ? M_PALETTEDATA->m_hPalette : 0); }
    void              SetHPALETTE(WXHPALETTE hPalette);
    void              SetPS(HPS hPS);

protected:
    virtual wxGDIRefData *CreateGDIRefData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

private:
    DECLARE_DYNAMIC_CLASS(wxPalette)
}; // end of CLASS wxPalette

#endif
    // _WX_PALETTE_H_
