/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/icon.h
// Purpose:     wxIcon class
// Author:      David Elliott
// Modified by:
// Created:     2003/08/11
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COCOA_ICON_H__
#define _WX_COCOA_ICON_H__

#include "wx/gdicmn.h"
#include "wx/gdiobj.h"

// ========================================================================
// wxIcon
// ========================================================================
class WXDLLIMPEXP_CORE wxIcon : public wxGDIObject
{
public:
    wxIcon();

    wxIcon(const char* const* data) { CreateFromXpm(data); }
    wxIcon(const char bits[], int width , int height );
    wxIcon(const wxString& name, int flags = wxICON_DEFAULT_TYPE,
           int desiredWidth = -1, int desiredHeight = -1);
    wxIcon(const wxIconLocation& loc)
    {
        LoadFile(loc.GetFileName(), wxBITMAP_TYPE_ICON);
    }
    virtual ~wxIcon();

    bool LoadFile(const wxString& name, wxBitmapType flags = wxICON_DEFAULT_TYPE,
                  int desiredWidth=-1, int desiredHeight=-1);

    bool operator==(const wxIcon& icon) const
    {   return m_refData == icon.m_refData; }
    bool operator!=(const wxIcon& icon) const { return !(*this == icon); }

    // create from bitmap (which should have a mask unless it's monochrome):
    // there shouldn't be any implicit bitmap -> icon conversion (i.e. no
    // ctors, assignment operators...), but it's ok to have such function
    void CopyFromBitmap(const wxBitmap& bmp);

    int GetWidth() const;
    int GetHeight() const;

    wxSize GetSize() const { return wxSize(GetWidth(), GetHeight()); }

    WX_NSImage GetNSImage() const;
    bool CreateFromXpm(const char* const* bits);

protected:
    virtual wxGDIRefData *CreateGDIRefData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

private:
    DECLARE_DYNAMIC_CLASS(wxIcon)
};

#endif // _WX_COCOA_ICON_H__
