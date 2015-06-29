/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/bitmap.h
// Purpose:     wxBitmap class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_BITMAP_H_
#define _WX_BITMAP_H_

#include "wx/msw/gdiimage.h"
#include "wx/math.h"
#include "wx/palette.h"

class WXDLLIMPEXP_FWD_CORE wxBitmap;
class WXDLLIMPEXP_FWD_CORE wxBitmapHandler;
class WXDLLIMPEXP_FWD_CORE wxBitmapRefData;
class WXDLLIMPEXP_FWD_CORE wxControl;
class WXDLLIMPEXP_FWD_CORE wxCursor;
class WXDLLIMPEXP_FWD_CORE wxDC;
#if wxUSE_WXDIB
class WXDLLIMPEXP_FWD_CORE wxDIB;
#endif
class WXDLLIMPEXP_FWD_CORE wxIcon;
class WXDLLIMPEXP_FWD_CORE wxMask;
class WXDLLIMPEXP_FWD_CORE wxPalette;
class WXDLLIMPEXP_FWD_CORE wxPixelDataBase;

// What kind of transparency should a bitmap copied from an icon or cursor
// have?
enum wxBitmapTransparency
{
  wxBitmapTransparency_Auto,    // default: copy alpha if the source has it
  wxBitmapTransparency_None,    // never create alpha
  wxBitmapTransparency_Always   // always use alpha
};

// ----------------------------------------------------------------------------
// wxBitmap: a mono or colour bitmap
// NOTE: for wxMSW we don't use the wxBitmapBase base class declared in bitmap.h!
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxBitmap : public wxGDIImage,
                                  public wxBitmapHelpers
{
public:
    // default ctor creates an invalid bitmap, you must Create() it later
    wxBitmap() { }

    // Initialize with raw data
    wxBitmap(const char bits[], int width, int height, int depth = 1);

    // Initialize with XPM data
    wxBitmap(const char* const* data);
#ifdef wxNEEDS_CHARPP
    wxBitmap(char** data)
    {
        *this = wxBitmap(const_cast<const char* const*>(data));
    }
#endif

    // Load a file or resource
    wxBitmap(const wxString& name, wxBitmapType type = wxBITMAP_DEFAULT_TYPE);

    // New constructor for generalised creation from data
    wxBitmap(const void* data, wxBitmapType type, int width, int height, int depth = 1);

    // Create a new, uninitialized bitmap of the given size and depth (if it
    // is omitted, will create a bitmap compatible with the display)
    //
    // NB: this ctor will create a DIB for 24 and 32bpp bitmaps, use ctor
    //     taking a DC argument if you want to force using DDB in this case
    wxBitmap(int width, int height, int depth = -1) { (void)Create(width, height, depth); }
    wxBitmap(const wxSize& sz, int depth = -1) { (void)Create(sz, depth); }

    // Create a bitmap compatible with the given DC
    wxBitmap(int width, int height, const wxDC& dc);

#if wxUSE_IMAGE
    // Convert from wxImage
    wxBitmap(const wxImage& image, int depth = -1)
        { (void)CreateFromImage(image, depth); }

    // Create a DDB compatible with the given DC from wxImage
    wxBitmap(const wxImage& image, const wxDC& dc)
        { (void)CreateFromImage(image, dc); }
#endif // wxUSE_IMAGE

    // we must have this, otherwise icons are silently copied into bitmaps using
    // the copy ctor but the resulting bitmap is invalid!
    wxBitmap(const wxIcon& icon,
             wxBitmapTransparency transp = wxBitmapTransparency_Auto)
    {
        CopyFromIcon(icon, transp);
    }

    wxBitmap& operator=(const wxIcon& icon)
    {
        (void)CopyFromIcon(icon);

        return *this;
    }

    wxBitmap& operator=(const wxCursor& cursor)
    {
        (void)CopyFromCursor(cursor);

        return *this;
    }

    virtual ~wxBitmap();

#if wxUSE_IMAGE
    wxImage ConvertToImage() const;
    wxBitmap ConvertToDisabled(unsigned char brightness = 255) const;
#endif // wxUSE_IMAGE

    // get the given part of bitmap
    wxBitmap GetSubBitmap( const wxRect& rect ) const;

    // NB: This should not be called from user code. It is for wx internal
    // use only.
    wxBitmap GetSubBitmapOfHDC( const wxRect& rect, WXHDC hdc ) const;

    // copies the contents and mask of the given (colour) icon to the bitmap
    bool CopyFromIcon(const wxIcon& icon,
                      wxBitmapTransparency transp = wxBitmapTransparency_Auto);

    // copies the contents and mask of the given cursor to the bitmap
    bool CopyFromCursor(const wxCursor& cursor,
                        wxBitmapTransparency transp = wxBitmapTransparency_Auto);

#if wxUSE_WXDIB
    // copies from a device independent bitmap
    bool CopyFromDIB(const wxDIB& dib);
    bool IsDIB() const;
    bool ConvertToDIB();
#endif

    virtual bool Create(int width, int height, int depth = wxBITMAP_SCREEN_DEPTH);
    virtual bool Create(const wxSize& sz, int depth = wxBITMAP_SCREEN_DEPTH)
        { return Create(sz.GetWidth(), sz.GetHeight(), depth); }

    virtual bool Create(int width, int height, const wxDC& dc);
    virtual bool Create(const void* data, wxBitmapType type, int width, int height, int depth = 1);
    virtual bool CreateScaled(int w, int h, int d, double logicalScale)
        { return Create(wxRound(w*logicalScale), wxRound(h*logicalScale), d); }

    virtual bool LoadFile(const wxString& name, wxBitmapType type = wxBITMAP_DEFAULT_TYPE);
    virtual bool SaveFile(const wxString& name, wxBitmapType type, const wxPalette *cmap = NULL) const;

    wxBitmapRefData *GetBitmapData() const
        { return (wxBitmapRefData *)m_refData; }

    // raw bitmap access support functions
    void *GetRawData(wxPixelDataBase& data, int bpp);
    void UngetRawData(wxPixelDataBase& data);

#if wxUSE_PALETTE
    wxPalette* GetPalette() const;
    void SetPalette(const wxPalette& palette);
#endif // wxUSE_PALETTE

    wxMask *GetMask() const;
    void SetMask(wxMask *mask);

    // these functions are internal and shouldn't be used, they risk to
    // disappear in the future
    bool HasAlpha() const;
    void UseAlpha();
    void ResetAlpha();

    // support for scaled bitmaps
    virtual double GetScaleFactor() const { return 1.0; }
    virtual double GetScaledWidth() const { return GetWidth() / GetScaleFactor(); }
    virtual double GetScaledHeight() const { return GetHeight() / GetScaleFactor(); }
    virtual wxSize GetScaledSize() const
        { return wxSize(wxRound(GetScaledWidth()), wxRound(GetScaledHeight())); }

    // implementation only from now on
    // -------------------------------

    // Set alpha flag to true if this is a 32bpp bitmap which has any non-0
    // values in its alpha channel.
    void MSWUpdateAlpha();

public:
    void SetHBITMAP(WXHBITMAP bmp) { SetHandle((WXHANDLE)bmp); }
    WXHBITMAP GetHBITMAP() const { return (WXHBITMAP)GetHandle(); }

    void SetSelectedInto(wxDC *dc);
    wxDC *GetSelectedInto() const;

protected:
    virtual wxGDIImageRefData *CreateData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

    // creates an uninitialized bitmap, called from Create()s above
    bool DoCreate(int w, int h, int depth, WXHDC hdc);

#if wxUSE_IMAGE
    // creates the bitmap from wxImage, supposed to be called from ctor
    bool CreateFromImage(const wxImage& image, int depth);

    // creates a DDB from wxImage, supposed to be called from ctor
    bool CreateFromImage(const wxImage& image, const wxDC& dc);

    // common part of the 2 methods above (hdc may be 0)
    bool CreateFromImage(const wxImage& image, int depth, WXHDC hdc);
#endif // wxUSE_IMAGE

private:
    // common part of CopyFromIcon/CopyFromCursor for Win32
    bool
    CopyFromIconOrCursor(const wxGDIImage& icon,
                         wxBitmapTransparency transp = wxBitmapTransparency_Auto);


    DECLARE_DYNAMIC_CLASS(wxBitmap)
};

// ----------------------------------------------------------------------------
// wxMask: a mono bitmap used for drawing bitmaps transparently.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMask : public wxObject
{
public:
    wxMask();

    // Copy constructor
    wxMask(const wxMask &mask);

    // Construct a mask from a bitmap and a colour indicating the transparent
    // area
    wxMask(const wxBitmap& bitmap, const wxColour& colour);

    // Construct a mask from a bitmap and a palette index indicating the
    // transparent area
    wxMask(const wxBitmap& bitmap, int paletteIndex);

    // Construct a mask from a mono bitmap (copies the bitmap).
    wxMask(const wxBitmap& bitmap);

    // construct a mask from the givne bitmap handle
    wxMask(WXHBITMAP hbmp) { m_maskBitmap = hbmp; }

    virtual ~wxMask();

    bool Create(const wxBitmap& bitmap, const wxColour& colour);
    bool Create(const wxBitmap& bitmap, int paletteIndex);
    bool Create(const wxBitmap& bitmap);

    wxBitmap GetBitmap() const;

    // Implementation
    WXHBITMAP GetMaskBitmap() const { return m_maskBitmap; }
    void SetMaskBitmap(WXHBITMAP bmp) { m_maskBitmap = bmp; }

protected:
    WXHBITMAP m_maskBitmap;

    DECLARE_DYNAMIC_CLASS(wxMask)
};


// ----------------------------------------------------------------------------
// wxBitmapHandler is a class which knows how to load/save bitmaps to/from file
// NOTE: for wxMSW we don't use the wxBitmapHandler class declared in bitmap.h!
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxBitmapHandler : public wxGDIImageHandler
{
public:
    wxBitmapHandler() { }
    wxBitmapHandler(const wxString& name, const wxString& ext, wxBitmapType type)
        : wxGDIImageHandler(name, ext, type) { }

    // implement wxGDIImageHandler's pure virtuals:

    virtual bool Create(wxGDIImage *image,
                        const void* data,
                        wxBitmapType type,
                        int width, int height, int depth = 1);
    virtual bool Load(wxGDIImage *image,
                      const wxString& name,
                      wxBitmapType type,
                      int desiredWidth, int desiredHeight);
    virtual bool Save(const wxGDIImage *image,
                      const wxString& name,
                      wxBitmapType type) const;


    // make wxBitmapHandler compatible with the wxBitmapHandler interface
    // declared in bitmap.h, even if it's derived from wxGDIImageHandler:

    virtual bool Create(wxBitmap *bitmap,
                        const void* data,
                        wxBitmapType type,
                        int width, int height, int depth = 1);
    virtual bool LoadFile(wxBitmap *bitmap,
                          const wxString& name,
                          wxBitmapType type,
                          int desiredWidth, int desiredHeight);
    virtual bool SaveFile(const wxBitmap *bitmap,
                          const wxString& name,
                          wxBitmapType type,
                          const wxPalette *palette = NULL) const;

private:
    DECLARE_DYNAMIC_CLASS(wxBitmapHandler)
};

#endif
  // _WX_BITMAP_H_
