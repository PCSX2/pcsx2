/////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/colour.h
// Purpose:     wxColour class
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COLOUR_H_
#define _WX_COLOUR_H_

#include "wx/object.h"
#include "wx/string.h"

// Colour
class WXDLLIMPEXP_CORE wxColour : public wxColourBase
{
    DECLARE_DYNAMIC_CLASS(wxColour)
public:
    // constructors
    // ------------
    DEFINE_STD_WXCOLOUR_CONSTRUCTORS

    // copy ctors and assignment operators
    wxColour( const wxColour& col );
    wxColour& operator = ( const wxColour& col );

    // dtor
    virtual ~wxColour();


    // accessors
    virtual bool IsOk() const {return m_isInit; }
    unsigned char Red() const { return m_red; }
    unsigned char Green() const { return m_green; }
    unsigned char Blue() const { return m_blue; }

    WXPixel GetPixel() const { return m_pixel; }
    void SetPixel(WXPixel pixel) { m_pixel = pixel; m_isInit = true; }

    inline bool operator == (const wxColour& colour) const { return (m_red == colour.m_red && m_green == colour.m_green && m_blue == colour.m_blue); }

    inline bool operator != (const wxColour& colour) const { return (!(m_red == colour.m_red && m_green == colour.m_green && m_blue == colour.m_blue)); }

    // Allocate a colour, or nearest colour, using the given display.
    // If realloc is true, ignore the existing pixel, otherwise just return
    // the existing one.
    // Returns the allocated pixel.

    // TODO: can this handle mono displays? If not, we should have an extra
    // flag to specify whether this should be black or white by default.

    WXPixel AllocColour(WXDisplay* display, bool realloc = false);

protected:
    // Helper function
    void Init();

    virtual void
    InitRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

private:
    bool          m_isInit;
    unsigned char m_red;
    unsigned char m_blue;
    unsigned char m_green;

public:
    WXPixel       m_pixel;
};

#endif
// _WX_COLOUR_H_
