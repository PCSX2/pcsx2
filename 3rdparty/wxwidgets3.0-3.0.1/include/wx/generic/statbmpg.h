///////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/statbmpg.h
// Purpose:     wxGenericStaticBitmap header
// Author:      Marcin Wojdyr, Stefan Csomor
// Created:     2008-06-16
// Copyright:   wxWidgets developers
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_STATBMP_H_
#define _WX_GENERIC_STATBMP_H_

#include "wx/statbmp.h"

class WXDLLIMPEXP_CORE wxGenericStaticBitmap : public wxStaticBitmapBase
{
public:
    wxGenericStaticBitmap() {}
    wxGenericStaticBitmap(wxWindow *parent,
                          wxWindowID id,
                          const wxBitmap& bitmap,
                          const wxPoint& pos = wxDefaultPosition,
                          const wxSize& size = wxDefaultSize,
                          long style = 0,
                          const wxString& name = wxStaticBitmapNameStr)
    {
            Create(parent, id, bitmap, pos, size, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxBitmap& bitmap,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxStaticBitmapNameStr);

    virtual void SetBitmap(const wxBitmap& bitmap)
    {
        m_bitmap = bitmap;
        SetInitialSize(GetBitmapSize());
        Refresh();
    }

    virtual wxBitmap GetBitmap() const { return m_bitmap; }

    virtual void SetIcon(const wxIcon& icon)
    {
        m_bitmap.CopyFromIcon(icon);
        SetInitialSize(GetBitmapSize());
        Refresh();
    }

#if defined(__WXGTK20__) || defined(__WXMAC__)
    // icons and bitmaps are really the same thing in wxGTK and wxMac
    wxIcon GetIcon() const  { return (const wxIcon &)m_bitmap; }
#endif


private:
    wxSize GetBitmapSize()
    {
        return m_bitmap.IsOk() ? m_bitmap.GetScaledSize()
                               : wxSize(16, 16); // this is completely arbitrary
    }

    void OnPaint(wxPaintEvent& event);

    wxBitmap m_bitmap;

    DECLARE_DYNAMIC_CLASS(wxGenericStaticBitmap)
};


#endif //_WX_GENERIC_STATBMP_H_
