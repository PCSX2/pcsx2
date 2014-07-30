/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/bitmap.h
// Purpose:     wxBitmap class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_BITMAP_H_
#define _WX_BITMAP_H_

#include "wx/palette.h"

// Bitmap
class WXDLLIMPEXP_FWD_CORE wxBitmap;
class wxBitmapRefData ;
class WXDLLIMPEXP_FWD_CORE wxBitmapHandler;
class WXDLLIMPEXP_FWD_CORE wxControl;
class WXDLLIMPEXP_FWD_CORE wxCursor;
class WXDLLIMPEXP_FWD_CORE wxDC;
class WXDLLIMPEXP_FWD_CORE wxIcon;
class WXDLLIMPEXP_FWD_CORE wxImage;
class WXDLLIMPEXP_FWD_CORE wxPixelDataBase;

// A mask is a bitmap used for drawing bitmaps
// Internally it is stored as a 8 bit deep memory chunk, 0 = black means the source will be drawn
// 255 = white means the source will not be drawn, no other values will be present
// 8 bit is chosen only for performance reasons, note also that this is the inverse value range
// from alpha, where 0 = invisible , 255 = fully drawn

class WXDLLIMPEXP_CORE wxMask: public wxObject
{
    DECLARE_DYNAMIC_CLASS(wxMask)

public:
    wxMask();

    // Copy constructor
    wxMask(const wxMask& mask);

    // Construct a mask from a bitmap and a colour indicating
    // the transparent area
    wxMask(const wxBitmap& bitmap, const wxColour& colour);

    // Construct a mask from a mono bitmap (black meaning show pixels, white meaning transparent)
    wxMask(const wxBitmap& bitmap);

    // implementation helper only : construct a mask from a 32 bit memory buffer
    wxMask(const wxMemoryBuffer& buf, int width , int height , int bytesPerRow ) ;

    virtual ~wxMask();

    bool Create(const wxBitmap& bitmap, const wxColour& colour);
    bool Create(const wxBitmap& bitmap);
    bool Create(const wxMemoryBuffer& buf, int width , int height , int bytesPerRow ) ;

    wxBitmap GetBitmap() const;

    // Implementation below

    void Init() ;

    // a 8 bit depth mask
    void* GetRawAccess() const;
    int GetBytesPerRow() const { return m_bytesPerRow ; }
    // renders/updates native representation when necessary
    void RealizeNative() ;

    WXHBITMAP GetHBITMAP() const ;


private:
    wxMemoryBuffer m_memBuf ;
    int m_bytesPerRow ;
    int m_width ;
    int m_height ;

    WXHBITMAP m_maskBitmap ;

};

class WXDLLIMPEXP_CORE wxBitmap: public wxBitmapBase
{
    DECLARE_DYNAMIC_CLASS(wxBitmap)

    friend class WXDLLIMPEXP_FWD_CORE wxBitmapHandler;

public:
    wxBitmap() {} // Platform-specific

    // Initialize with raw data.
    wxBitmap(const char bits[], int width, int height, int depth = 1);

    // Initialize with XPM data
    wxBitmap(const char* const* bits);

    // Load a file or resource
    wxBitmap(const wxString& name, wxBitmapType type = wxBITMAP_DEFAULT_TYPE);

    // Constructor for generalised creation from data
    wxBitmap(const void* data, wxBitmapType type, int width, int height, int depth = 1);
    
    // creates an bitmap from the native image format
    wxBitmap(CGImageRef image, double scale = 1.0);
    wxBitmap(WX_NSImage image);
    wxBitmap(CGContextRef bitmapcontext);

    // Create a bitmap compatible with the given DC
    wxBitmap(int width, int height, const wxDC& dc);
    
    // If depth is omitted, will create a bitmap compatible with the display
    wxBitmap(int width, int height, int depth = -1) { (void)Create(width, height, depth); }
    wxBitmap(const wxSize& sz, int depth = -1) { (void)Create(sz, depth); }

    // Convert from wxImage:
    wxBitmap(const wxImage& image, int depth = -1, double scale = 1.0);

    // Convert from wxIcon
    wxBitmap(const wxIcon& icon) { CopyFromIcon(icon); }

    virtual ~wxBitmap() {}

    wxImage ConvertToImage() const;

    // get the given part of bitmap
    wxBitmap GetSubBitmap( const wxRect& rect ) const;

    virtual bool Create(int width, int height, int depth = wxBITMAP_SCREEN_DEPTH);
    virtual bool Create(const wxSize& sz, int depth = wxBITMAP_SCREEN_DEPTH)
        { return Create(sz.GetWidth(), sz.GetHeight(), depth); }

    virtual bool Create(const void* data, wxBitmapType type, int width, int height, int depth = 1);
    bool Create( CGImageRef image, double scale = 1.0 );
    bool Create( WX_NSImage image );
    bool Create( CGContextRef bitmapcontext);
    
    // Create a bitmap compatible with the given DC, inheriting its magnification factor
    bool Create(int width, int height, const wxDC& dc);

    // Create a bitmap with a scale factor, width and height are multiplied with that factor
    bool CreateScaled(int logwidth, int logheight, int depth, double logicalScale);
    
    // virtual bool Create( WXHICON icon) ;
    virtual bool LoadFile(const wxString& name, wxBitmapType type = wxBITMAP_DEFAULT_TYPE);
    virtual bool SaveFile(const wxString& name, wxBitmapType type, const wxPalette *cmap = NULL) const;

    wxBitmapRefData *GetBitmapData() const
        { return (wxBitmapRefData *)m_refData; }

    // copies the contents and mask of the given (colour) icon to the bitmap
    virtual bool CopyFromIcon(const wxIcon& icon);

    int GetWidth() const;
    int GetHeight() const;
    int GetDepth() const;
    void SetWidth(int w);
    void SetHeight(int h);
    void SetDepth(int d);
    void SetOk(bool isOk);

#if wxUSE_PALETTE
    wxPalette* GetPalette() const;
    void SetPalette(const wxPalette& palette);
#endif // wxUSE_PALETTE

    wxMask *GetMask() const;
    void SetMask(wxMask *mask) ;

    static void InitStandardHandlers();

    // raw bitmap access support functions, for internal use only
    void *GetRawData(wxPixelDataBase& data, int bpp);
    void UngetRawData(wxPixelDataBase& data);

    // these functions are internal and shouldn't be used, they risk to
    // disappear in the future
    bool HasAlpha() const;
    void UseAlpha();

    // returns the 'native' implementation, a GWorldPtr for the content and one for the mask
    WXHBITMAP GetHBITMAP( WXHBITMAP * mask = NULL ) const;

    // returns a CGImageRef which must released after usage with CGImageRelease
    CGImageRef CreateCGImage() const ;

#if wxOSX_USE_COCOA
    // returns an autoreleased version of the image
    WX_NSImage GetNSImage() const;
#endif
#if wxOSX_USE_IPHONE
    // returns an autoreleased version of the image
    WX_UIImage GetUIImage() const;
#endif
    // returns a IconRef which must be retained before and released after usage
    IconRef GetIconRef() const;
    // returns a IconRef which must be released after usage
    IconRef CreateIconRef() const;
    // get read only access to the underlying buffer
    void *GetRawAccess() const ;
    // brackets to the underlying OS structure for read/write access
    // makes sure that no cached images will be constructed until terminated
    void *BeginRawAccess() ;
    void EndRawAccess() ;

    double GetScaleFactor() const;
protected:
    virtual wxGDIRefData *CreateGDIRefData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;
};

#endif // _WX_BITMAP_H_
