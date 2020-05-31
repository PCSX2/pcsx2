/////////////////////////////////////////////////////////////////////////////
// Name:        wx/palette.h
// Purpose:     Common header and base class for wxPalette
// Author:      Julian Smart
// Modified by:
// Created:
// Copyright:   (c) Julian Smart
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PALETTE_H_BASE_
#define _WX_PALETTE_H_BASE_

#include "wx/defs.h"

#if wxUSE_PALETTE

#include "wx/object.h"
#include "wx/gdiobj.h"

// wxPaletteBase
class WXDLLIMPEXP_CORE wxPaletteBase: public wxGDIObject
{
public:
    virtual ~wxPaletteBase() { }

    virtual int GetColoursCount() const { wxFAIL_MSG( wxT("not implemented") ); return 0; }
};

#if defined(__WXMSW__)
    #include "wx/msw/palette.h"
#elif defined(__WXX11__) || defined(__WXMOTIF__)
    #include "wx/x11/palette.h"
#elif defined(__WXGTK__) || defined(__WXCOCOA__)
    #include "wx/generic/paletteg.h"
#elif defined(__WXMAC__)
    #include "wx/osx/palette.h"
#elif defined(__WXPM__)
    #include "wx/os2/palette.h"
#endif

#endif // wxUSE_PALETTE

#endif // _WX_PALETTE_H_BASE_
