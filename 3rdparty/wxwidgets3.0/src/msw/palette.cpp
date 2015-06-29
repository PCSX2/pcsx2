/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/palette.cpp
// Purpose:     wxPalette
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_PALETTE

#include "wx/palette.h"

#include "wx/msw/private.h"

// ============================================================================
// wxPaletteRefData
// ============================================================================

class WXDLLEXPORT wxPaletteRefData: public wxGDIRefData
{
public:
    wxPaletteRefData() { Init(); }

    wxPaletteRefData(int n,
                     const unsigned char *red,
                     const unsigned char *green,
                     const unsigned char *blue)
    {
        Init();

        LOGPALETTE *pPal = Alloc(n);
        if ( !pPal )
            return;

        for ( int i = 0; i < n; i++ )
        {
            pPal->palPalEntry[i].peRed = red[i];
            pPal->palPalEntry[i].peGreen = green[i];
            pPal->palPalEntry[i].peBlue = blue[i];
            pPal->palPalEntry[i].peFlags = 0;
        }

        m_hPalette = ::CreatePalette(pPal);
        free(pPal);
    }

    wxPaletteRefData(const wxPaletteRefData& data)
        : wxGDIRefData()
    {
        Init();

        const UINT n = data.GetEntries();
        if ( !n )
            return;

        LOGPALETTE *pPal = Alloc(n);
        if ( !pPal )
            return;

        if ( ::GetPaletteEntries(data.m_hPalette, 0, n, pPal->palPalEntry) )
            m_hPalette = ::CreatePalette(pPal);

        free(pPal);
    }

    virtual ~wxPaletteRefData()
    {
        if ( m_hPalette )
            ::DeleteObject(m_hPalette);
    }

    virtual bool IsOk() const { return m_hPalette != 0; }

    UINT GetEntries() const
    {
        return ::GetPaletteEntries(m_hPalette, 0, 0, NULL);
    }

private:
    // caller must free() the pointer
    static LOGPALETTE *Alloc(UINT numEntries)
    {
        LOGPALETTE *pPal = (LOGPALETTE *)
            malloc(sizeof(LOGPALETTE) + numEntries*sizeof(PALETTEENTRY));
        if ( pPal )
        {
            pPal->palVersion = 0x300;
            pPal->palNumEntries = numEntries;
        }

        return pPal;
    }

    void Init() { m_hPalette = 0; }

    HPALETTE m_hPalette;

    friend class WXDLLIMPEXP_FWD_CORE wxPalette;
};

// ============================================================================
// wxPalette
// ============================================================================

IMPLEMENT_DYNAMIC_CLASS(wxPalette, wxGDIObject)

#define M_PALETTEDATA ((wxPaletteRefData *)m_refData)

bool wxPalette::Create(int n,
                       const unsigned char *red,
                       const unsigned char *green,
                       const unsigned char *blue)
{
    m_refData = new wxPaletteRefData(n, red, green, blue);

    return IsOk();
}

wxGDIRefData *wxPalette::CreateGDIRefData() const
{
    return new wxPaletteRefData;
}

wxGDIRefData *wxPalette::CloneGDIRefData(const wxGDIRefData *data) const
{
    return new wxPaletteRefData(*static_cast<const wxPaletteRefData *>(data));
}

int wxPalette::GetColoursCount() const
{
    return IsOk() ? M_PALETTEDATA->GetEntries() : 0;
}

int wxPalette::GetPixel(unsigned char red,
                        unsigned char green,
                        unsigned char blue) const
{
    if ( !m_refData )
        return wxNOT_FOUND;

    return ::GetNearestPaletteIndex(M_PALETTEDATA->m_hPalette,
                                    PALETTERGB(red, green, blue));
}

bool wxPalette::GetRGB(int index,
                       unsigned char *red,
                       unsigned char *green,
                       unsigned char *blue) const
{
    if ( !m_refData )
        return false;

    if (index < 0 || index > 255)
        return false;

    PALETTEENTRY entry;
    if ( !::GetPaletteEntries(M_PALETTEDATA->m_hPalette, index, 1, &entry) )
        return false;

    *red = entry.peRed;
    *green = entry.peGreen;
    *blue = entry.peBlue;

    return true;
}

WXHPALETTE wxPalette::GetHPALETTE() const
{
    return M_PALETTEDATA ? (WXHPALETTE)M_PALETTEDATA->m_hPalette : 0;
}

void wxPalette::SetHPALETTE(WXHPALETTE pal)
{
    AllocExclusive();

    M_PALETTEDATA->m_hPalette = (HPALETTE)pal;
}

#endif // wxUSE_PALETTE
