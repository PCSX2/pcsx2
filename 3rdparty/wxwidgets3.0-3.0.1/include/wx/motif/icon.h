/////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/icon.h
// Purpose:     wxIcon class
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ICON_H_
#define _WX_ICON_H_

#include "wx/bitmap.h"

// Icon
class WXDLLIMPEXP_CORE wxIcon : public wxBitmap
{
public:
    wxIcon();

    // Initialize with XBM data
    wxIcon(const char bits[], int width, int height);

    // Initialize with XPM data
    wxIcon(const char* const* data);
#ifdef wxNEEDS_CHARPP
    wxIcon(char **data);
#endif

    wxIcon(const wxString& name, wxBitmapType type = wxICON_DEFAULT_TYPE,
           int desiredWidth = -1, int desiredHeight = -1)
    {
        LoadFile(name, type, desiredWidth, desiredHeight);
    }

    wxIcon(const wxIconLocation& loc)
    {
        LoadFile(loc.GetFileName(), wxBITMAP_TYPE_ANY);
    }

    virtual ~wxIcon();

    bool LoadFile(const wxString& name, wxBitmapType type,
                  int desiredWidth, int desiredHeight);

    // unhide the base class version
    virtual bool LoadFile(const wxString& name,
                          wxBitmapType flags = wxICON_DEFAULT_TYPE)
        { return LoadFile(name, flags); }

    // create from bitmap (which should have a mask unless it's monochrome):
    // there shouldn't be any implicit bitmap -> icon conversion (i.e. no
    // ctors, assignment operators...), but it's ok to have such function
    void CopyFromBitmap(const wxBitmap& bmp);


    DECLARE_DYNAMIC_CLASS(wxIcon)
};

#endif // _WX_ICON_H_
