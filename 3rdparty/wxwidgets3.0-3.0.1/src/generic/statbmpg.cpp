///////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/statbmpg.cpp
// Purpose:     wxGenericStaticBitmap
// Author:      Marcin Wojdyr, Stefan Csomor
// Created:     2008-06-16
// Copyright:   wxWidgets developers
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_STATBMP

#ifndef WX_PRECOMP
    #include "wx/dcclient.h"
#endif

#include "wx/generic/statbmpg.h"

bool wxGenericStaticBitmap::Create(wxWindow *parent, wxWindowID id,
                                   const wxBitmap& bitmap,
                                   const wxPoint& pos, const wxSize& size,
                                   long style, const wxString& name)
{
    if (! wxControl::Create(parent, id, pos, size, style,
                            wxDefaultValidator, name))
        return false;
    SetBitmap(bitmap);
    Connect(wxEVT_PAINT, wxPaintEventHandler(wxGenericStaticBitmap::OnPaint));
    return true;
}

void wxGenericStaticBitmap::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);
    if (m_bitmap.IsOk())
        dc.DrawBitmap(m_bitmap, 0, 0, true);
}

// under OSX_cocoa is a define, avoid duplicate info
#ifndef wxGenericStaticBitmap

IMPLEMENT_DYNAMIC_CLASS(wxGenericStaticBitmap, wxStaticBitmapBase)

#endif

#endif // wxUSE_STATBMP

