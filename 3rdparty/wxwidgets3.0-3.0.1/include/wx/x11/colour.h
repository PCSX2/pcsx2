/////////////////////////////////////////////////////////////////////////////
// Name:        wx/x11/colour.h
// Purpose:     wxColour class
// Author:      Julian Smart, Robert Roebling
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart, Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COLOUR_H_
#define _WX_COLOUR_H_

#include "wx/defs.h"
#include "wx/object.h"
#include "wx/string.h"
#include "wx/gdiobj.h"
#include "wx/palette.h"

//-----------------------------------------------------------------------------
// classes
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxDC;
class WXDLLIMPEXP_FWD_CORE wxPaintDC;
class WXDLLIMPEXP_FWD_CORE wxBitmap;
class WXDLLIMPEXP_FWD_CORE wxWindow;

class WXDLLIMPEXP_FWD_CORE wxColour;

//-----------------------------------------------------------------------------
// wxColour
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxColour : public wxColourBase
{
public:
    // constructors
    // ------------
    DEFINE_STD_WXCOLOUR_CONSTRUCTORS

    virtual ~wxColour();

    bool operator==(const wxColour& col) const;
    bool operator!=(const wxColour& col) const { return !(*this == col); }

    unsigned char Red() const;
    unsigned char Green() const;
    unsigned char Blue() const;

    // Implementation part

    void CalcPixel( WXColormap cmap );
    unsigned long GetPixel() const;
    WXColor *GetColor() const;

protected:
    virtual wxGDIRefData *CreateGDIRefData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

    virtual void
    InitRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

    virtual bool FromString(const wxString& str);

private:
    DECLARE_DYNAMIC_CLASS(wxColour)
};

#endif // _WX_COLOUR_H_
