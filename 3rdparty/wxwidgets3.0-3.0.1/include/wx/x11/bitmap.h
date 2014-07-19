/////////////////////////////////////////////////////////////////////////////
// Name:        wx/x11/bitmap.h
// Purpose:     wxBitmap class
// Author:      Julian Smart, Robert Roebling
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart, Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_BITMAP_H_
#define _WX_BITMAP_H_

#include "wx/defs.h"
#include "wx/object.h"
#include "wx/string.h"
#include "wx/palette.h"
#include "wx/gdiobj.h"

//-----------------------------------------------------------------------------
// classes
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxMask;
class WXDLLIMPEXP_FWD_CORE wxBitmap;
class WXDLLIMPEXP_FWD_CORE wxImage;

//-----------------------------------------------------------------------------
// wxMask
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMask: public wxObject
{
public:
    wxMask();
    wxMask(const wxMask& mask);
    wxMask( const wxBitmap& bitmap, const wxColour& colour );
    wxMask( const wxBitmap& bitmap, int paletteIndex );
    wxMask( const wxBitmap& bitmap );
    virtual ~wxMask();

    bool Create( const wxBitmap& bitmap, const wxColour& colour );
    bool Create( const wxBitmap& bitmap, int paletteIndex );
    bool Create( const wxBitmap& bitmap );

    // implementation
    WXPixmap GetBitmap() const              { return m_bitmap; }
    void SetBitmap( WXPixmap bitmap )       { m_bitmap = bitmap; }

    WXDisplay *GetDisplay() const           { return m_display; }
    void SetDisplay( WXDisplay *display )   { m_display = display; }

private:
    WXPixmap    m_bitmap;
    WXDisplay  *m_display;
    wxSize m_size;

private:
    DECLARE_DYNAMIC_CLASS(wxMask)
};

//-----------------------------------------------------------------------------
// wxBitmap
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxBitmap: public wxBitmapBase
{
public:
    wxBitmap() {}
    wxBitmap( int width, int height, int depth = -1 ) { Create( width, height, depth ); }
    wxBitmap( const wxSize& sz, int depth = -1 ) { Create( sz, depth ); }

    wxBitmap( const char bits[], int width, int height, int depth = 1 );
    wxBitmap( const char* const* bits );
#ifdef wxNEEDS_CHARPP
    // needed for old GCC
    wxBitmap(char** data)
    {
        *this = wxBitmap(const_cast<const char* const*>(data));
    }
#endif
    wxBitmap( const wxString &filename, wxBitmapType type = wxBITMAP_DEFAULT_TYPE );
    virtual ~wxBitmap();

    static void InitStandardHandlers();

    bool Create(int width, int height, int depth = wxBITMAP_SCREEN_DEPTH);
    bool Create(const wxSize& sz, int depth = wxBITMAP_SCREEN_DEPTH)
        { return Create(sz.GetWidth(), sz.GetHeight(), depth); }
    bool Create(int width, int height, const wxDC& WXUNUSED(dc))
        { return Create(width,height); }

    bool Create(const void* data, wxBitmapType type,
                int width, int height, int depth = -1);
    // create the wxBitmap using a _copy_ of the pixmap
    bool Create(WXPixmap pixmap);

    int GetHeight() const;
    int GetWidth() const;
    int GetDepth() const;

#if wxUSE_IMAGE
    wxBitmap( const wxImage& image, int depth = -1 ) { (void)CreateFromImage(image, depth); }
    wxImage ConvertToImage() const;
    bool CreateFromImage(const wxImage& image, int depth = -1);
#endif // wxUSE_IMAGE

    // copies the contents and mask of the given (colour) icon to the bitmap
    virtual bool CopyFromIcon(const wxIcon& icon);

    wxMask *GetMask() const;
    void SetMask( wxMask *mask );

    wxBitmap GetSubBitmap( const wxRect& rect ) const;

    bool SaveFile( const wxString &name, wxBitmapType type, const wxPalette *palette = NULL ) const;
    bool LoadFile( const wxString &name, wxBitmapType type = wxBITMAP_DEFAULT_TYPE );

    wxPalette *GetPalette() const;
    wxPalette *GetColourMap() const
        { return GetPalette(); }
    virtual void SetPalette(const wxPalette& palette);

    // implementation
    // --------------

    void SetHeight( int height );
    void SetWidth( int width );
    void SetDepth( int depth );
    void SetPixmap( WXPixmap pixmap );
    void SetBitmap( WXPixmap bitmap );

    WXPixmap GetPixmap() const;
    WXPixmap GetBitmap() const;

    WXPixmap GetDrawable() const;

    WXDisplay *GetDisplay() const;

protected:
    virtual wxGDIRefData *CreateGDIRefData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

private:
    DECLARE_DYNAMIC_CLASS(wxBitmap)
};

#endif // _WX_BITMAP_H_
