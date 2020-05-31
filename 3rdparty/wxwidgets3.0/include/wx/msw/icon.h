/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/icon.h
// Purpose:     wxIcon class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ICON_H_
#define _WX_ICON_H_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/msw/gdiimage.h"

// ---------------------------------------------------------------------------
// icon data
// ---------------------------------------------------------------------------

// notice that although wxIconRefData inherits from wxBitmapRefData, it is not
// a valid wxBitmapRefData
class WXDLLIMPEXP_CORE wxIconRefData : public wxGDIImageRefData
{
public:
    wxIconRefData() { }
    virtual ~wxIconRefData() { Free(); }

    virtual void Free();
};

// ---------------------------------------------------------------------------
// Icon
// ---------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxIcon : public wxGDIImage
{
public:
    // ctors
        // default
    wxIcon() { }

        // from raw data
    wxIcon(const char bits[], int width, int height);

        // from XPM data
    wxIcon(const char* const* data) { CreateIconFromXpm(data); }
#ifdef wxNEEDS_CHARPP
    wxIcon(char **data) { CreateIconFromXpm(const_cast<const char* const*>(data)); }
#endif
        // from resource/file
    wxIcon(const wxString& name,
           wxBitmapType type = wxICON_DEFAULT_TYPE,
           int desiredWidth = -1, int desiredHeight = -1);

    wxIcon(const wxIconLocation& loc);

    virtual ~wxIcon();

    virtual bool LoadFile(const wxString& name,
                          wxBitmapType type = wxICON_DEFAULT_TYPE,
                          int desiredWidth = -1, int desiredHeight = -1);

    bool CreateFromHICON(WXHICON icon);

    // implementation only from now on
    wxIconRefData *GetIconData() const { return (wxIconRefData *)m_refData; }

    void SetHICON(WXHICON icon) { SetHandle((WXHANDLE)icon); }
    WXHICON GetHICON() const { return (WXHICON)GetHandle(); }

    // create from bitmap (which should have a mask unless it's monochrome):
    // there shouldn't be any implicit bitmap -> icon conversion (i.e. no
    // ctors, assignment operators...), but it's ok to have such function
    void CopyFromBitmap(const wxBitmap& bmp);

protected:
    virtual wxGDIImageRefData *CreateData() const
    {
        return new wxIconRefData;
    }

    virtual wxObjectRefData *CloneRefData(const wxObjectRefData *data) const;

    // create from XPM data
    void CreateIconFromXpm(const char* const* data);

private:
    DECLARE_DYNAMIC_CLASS(wxIcon)
};

#endif
    // _WX_ICON_H_
