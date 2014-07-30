/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/icon.h
// Purpose:     wxIcon class
// Author:      David Webster
// Modified by:
// Created:     10/09/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ICON_H_
#define _WX_ICON_H_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/bitmap.h"
#include "wx/os2/gdiimage.h"

#define wxIconRefDataBase   wxGDIImageRefData
#define wxIconBase          wxGDIImage

class WXDLLIMPEXP_CORE wxIconRefData: public wxIconRefDataBase
{
public:
    wxIconRefData() { }
    virtual ~wxIconRefData() { Free(); }

    virtual void Free();
}; // end of

// ---------------------------------------------------------------------------
// Icon
// ---------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxIcon: public wxIconBase
{
public:
    wxIcon();

    wxIcon( const char bits[]
           ,int        nWidth
           ,int        nHeight
          );
    wxIcon(const char* const* ppData) { CreateIconFromXpm(ppData); }
#ifdef wxNEEDS_CHARPP
    wxIcon(char** ppData) { CreateIconFromXpm(const_cast<const char* const*>(ppData)); }
#endif
    wxIcon( const wxString& rName
           ,wxBitmapType    lFlags = wxICON_DEFAULT_TYPE
           ,int             nDesiredWidth = -1
           ,int             nDesiredHeight = -1
          );
    wxIcon(const wxIconLocation& loc)
    {
        LoadFile(loc.GetFileName(), wxBITMAP_TYPE_ICO);
    }

    virtual ~wxIcon();

    bool LoadFile( const wxString& rName
                  ,wxBitmapType    lFlags = wxICON_DEFAULT_TYPE
                  ,int             nDesiredWidth = -1
                  ,int             nDesiredHeight = -1
                 );

    wxIconRefData *GetIconData() const { return (wxIconRefData *)m_refData; }

    inline void SetHICON(WXHICON hIcon) { SetHandle((WXHANDLE)hIcon); }
    inline WXHICON GetHICON() const { return (WXHICON)GetHandle(); }
    inline bool    IsXpm(void) const { return m_bIsXpm; }
    inline const wxBitmap& GetXpmSrc(void) const { return m_vXpmSrc; }

    void CopyFromBitmap(const wxBitmap& rBmp);
protected:
    virtual wxGDIImageRefData* CreateData() const
    {
        return new wxIconRefData;
    }
    void    CreateIconFromXpm(const char* const* ppData);

private:
    bool                            m_bIsXpm;
    wxBitmap                        m_vXpmSrc;

    DECLARE_DYNAMIC_CLASS(wxIcon)
};

#endif
    // _WX_ICON_H_
