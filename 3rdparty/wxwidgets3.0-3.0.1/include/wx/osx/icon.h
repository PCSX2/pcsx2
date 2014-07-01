/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/icon.h
// Purpose:     wxIcon class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ICON_H_
#define _WX_ICON_H_

#include "wx/bitmap.h"

// Icon
class WXDLLIMPEXP_CORE wxIcon : public wxGDIObject
{
public:
    wxIcon();

    wxIcon(const char* const* data);
    wxIcon(const char bits[], int width , int height );
    wxIcon(const wxString& name, wxBitmapType flags = wxICON_DEFAULT_TYPE,
           int desiredWidth = -1, int desiredHeight = -1);
    wxIcon(const wxIconLocation& loc)
    {
      LoadFile(loc.GetFileName(), wxBITMAP_TYPE_ICON);
    }

    wxIcon(WXHICON icon, const wxSize& size);

    virtual ~wxIcon();

    bool LoadFile(const wxString& name, wxBitmapType flags = wxICON_DEFAULT_TYPE,
                  int desiredWidth = -1, int desiredHeight = -1);


    // create from bitmap (which should have a mask unless it's monochrome):
    // there shouldn't be any implicit bitmap -> icon conversion (i.e. no
    // ctors, assignment operators...), but it's ok to have such function
    void CopyFromBitmap(const wxBitmap& bmp);

    int GetWidth() const;
    int GetHeight() const;
    int GetDepth() const;
    void SetWidth(int w);
    void SetHeight(int h);
    void SetDepth(int d);
    void SetOk(bool isOk);

    wxSize GetSize() const { return wxSize(GetWidth(), GetHeight()); }

    WXHICON GetHICON() const;

protected:
    virtual wxGDIRefData *CreateGDIRefData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

private:
    DECLARE_DYNAMIC_CLASS(wxIcon)

    bool LoadIconFromSystemResource(const wxString& resourceName, int desiredWidth, int desiredHeight);
    bool LoadIconFromBundleResource(const wxString& resourceName, int desiredWidth, int desiredHeight);
    bool LoadIconFromFile(const wxString& filename, int desiredWidth, int desiredHeight);
    bool LoadIconAsBitmap(const wxString& filename, wxBitmapType flags = wxICON_DEFAULT_TYPE, int desiredWidth = -1, int desiredHeight = -1);
};

class WXDLLIMPEXP_CORE wxICONResourceHandler: public wxBitmapHandler
{
public:
    wxICONResourceHandler()
    {
        SetName(wxT("ICON resource"));
        SetExtension(wxEmptyString);
        SetType(wxBITMAP_TYPE_ICON_RESOURCE);
    }

    virtual bool LoadFile(wxBitmap *bitmap,
                          const wxString& name,
                          wxBitmapType flags,
                          int desiredWidth = -1,
                          int desiredHeight = -1);

    // unhide the base class virtual
    virtual bool LoadFile(wxBitmap *bitmap,
            const wxString& name,
            wxBitmapType flags)
        { return LoadFile(bitmap, name, flags, -1, -1); }

private:
    DECLARE_DYNAMIC_CLASS(wxICONResourceHandler)
};

#endif
    // _WX_ICON_H_
