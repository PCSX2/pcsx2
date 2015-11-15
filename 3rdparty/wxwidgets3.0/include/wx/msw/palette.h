/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/palette.h
// Purpose:     wxPalette class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PALETTE_H_
#define _WX_PALETTE_H_

#include "wx/gdiobj.h"

class WXDLLIMPEXP_CORE wxPalette : public wxPaletteBase
{
public:
    wxPalette() { }
    wxPalette(int n,
              const unsigned char *red, const unsigned char *green, const unsigned char *blue)
    {
        Create(n, red, green, blue);
    }

    bool Create(int n,
                const unsigned char *red, const unsigned char *green, const unsigned char *blue);

    virtual int GetColoursCount() const;

    int
    GetPixel(unsigned char red, unsigned char green, unsigned char blue) const;

    bool
    GetRGB(int pixel,
           unsigned char *red, unsigned char *green, unsigned char *blue) const;

    // implemetation
    WXHPALETTE GetHPALETTE() const;
    void SetHPALETTE(WXHPALETTE pal);

protected:
    virtual wxGDIRefData *CreateGDIRefData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

private:
    DECLARE_DYNAMIC_CLASS(wxPalette)
};

#endif // _WX_PALETTE_H_
