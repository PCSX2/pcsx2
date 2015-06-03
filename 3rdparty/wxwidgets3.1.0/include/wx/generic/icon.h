/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/icon.h
// Purpose:     wxIcon implementation for ports where it's same as wxBitmap
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_ICON_H_
#define _WX_GENERIC_ICON_H_

#include "wx/bitmap.h"

//-----------------------------------------------------------------------------
// wxIcon
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxIcon: public wxBitmap
{
public:
    wxIcon();

    wxIcon(const char* const* bits);
#ifdef wxNEEDS_CHARPP
    wxIcon(char **bits);
#endif

    // For compatibility with wxMSW where desired size is sometimes required to
    // distinguish between multiple icons in a resource.
    wxIcon( const wxString& filename,
            wxBitmapType type = wxICON_DEFAULT_TYPE,
            int WXUNUSED(desiredWidth)=-1, int WXUNUSED(desiredHeight)=-1 ) :
        wxBitmap(filename, type)
    {
    }

    wxIcon(const wxIconLocation& loc)
        : wxBitmap(loc.GetFileName(), wxBITMAP_TYPE_ANY)
    {
    }

    bool LoadFile(const wxString& name, wxBitmapType flags,
                  int WXUNUSED(desiredWidth), int WXUNUSED(desiredHeight))
        { return wxBitmap::LoadFile(name, flags); }

    // unhide the base class version
    virtual bool LoadFile(const wxString& name,
                          wxBitmapType flags = wxICON_DEFAULT_TYPE) wxOVERRIDE
        { return wxBitmap::LoadFile(name, flags); }

    // create from bitmap (which should have a mask unless it's monochrome):
    // there shouldn't be any implicit bitmap -> icon conversion (i.e. no
    // ctors, assignment operators...), but it's ok to have such function
    void CopyFromBitmap(const wxBitmap& bmp);

private:
    wxDECLARE_DYNAMIC_CLASS(wxIcon);
};

#endif // _WX_GENERIC_ICON_H_
