/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/bitmap.h
// Purpose:     wxBitmap class
// Author:      David Elliott
// Modified by:
// Created:     2003/07/19
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_BITMAP_H__
#define __WX_COCOA_BITMAP_H__

#include "wx/palette.h"

// Bitmap
class WXDLLIMPEXP_FWD_CORE wxBitmap;
class WXDLLIMPEXP_FWD_CORE wxIcon;
class WXDLLIMPEXP_FWD_CORE wxCursor;
class WXDLLIMPEXP_FWD_CORE wxImage;
class WXDLLIMPEXP_FWD_CORE wxPixelDataBase;

// ========================================================================
// wxMask
// ========================================================================

// A mask is a 1-bit alpha bitmap used for drawing bitmaps transparently.
class WXDLLIMPEXP_CORE wxMask: public wxObject
{
    DECLARE_DYNAMIC_CLASS(wxMask)
public:
    wxMask();

    // Construct a mask from a bitmap and a colour indicating
    // the transparent area
    wxMask(const wxBitmap& bitmap, const wxColour& colour);

    // Construct a mask from a bitmap and a palette index indicating
    // the transparent area
    wxMask(const wxBitmap& bitmap, int paletteIndex);

    // Construct a mask from a mono bitmap (copies the bitmap).
    wxMask(const wxBitmap& bitmap);

    // Copy constructor
    wxMask(const wxMask& src);

    virtual ~wxMask();

    bool Create(const wxBitmap& bitmap, const wxColour& colour);
    bool Create(const wxBitmap& bitmap, int paletteIndex);
    bool Create(const wxBitmap& bitmap);

    // wxCocoa
    inline WX_NSBitmapImageRep GetNSBitmapImageRep()
    {   return m_cocoaNSBitmapImageRep; }
protected:
    WX_NSBitmapImageRep m_cocoaNSBitmapImageRep;
};


// ========================================================================
// wxBitmap
// ========================================================================

class WXDLLIMPEXP_CORE wxBitmap: public wxGDIObject,
                                 public wxBitmapHelpers
{
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    // Platform-specific default constructor
    wxBitmap();
    // Initialize with raw data.
    wxBitmap(const char bits[], int width, int height, int depth = 1);
    // Initialize with XPM data
    wxBitmap(const char* const* bits);
    // Load a file or resource
    wxBitmap(const wxString& name, wxBitmapType type = wxBITMAP_DEFAULT_TYPE);
    // Construct from Cocoa's NSImage
    wxBitmap(NSImage* cocoaNSImage);
    // Construct from Cocoa's NSBitmapImageRep
    wxBitmap(NSBitmapImageRep* cocoaNSBitmapImageRep);
    // Constructor for generalised creation from data
    wxBitmap(const void* data, wxBitmapType type, int width, int height, int depth = 1);
    // If depth is omitted, will create a bitmap compatible with the display
    wxBitmap(int width, int height, int depth = -1)
        { (void)Create(width, height, depth); }
    wxBitmap(const wxSize& sz, int depth = -1)
        { (void)Create(sz, depth); }
    // Convert from wxImage:
    wxBitmap(const wxImage& image, int depth = -1)
        { CreateFromImage(image, depth); }
    // Convert from wxIcon
    wxBitmap(const wxIcon& icon) { CopyFromIcon(icon); }

    // destructor
    virtual ~wxBitmap();

// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    // Initialize from wxImage
    bool CreateFromImage(const wxImage& image, int depth=-1);

    virtual bool Create(int width, int height, int depth = wxBITMAP_SCREEN_DEPTH);
    virtual bool Create(const wxSize& sz, int depth = wxBITMAP_SCREEN_DEPTH)
        { return Create(sz.GetWidth(), sz.GetHeight(), depth); }

    bool Create(NSImage* cocoaNSImage);
    bool Create(NSBitmapImageRep* cocoaNSBitmapImageRep);
    virtual bool Create(const void* data, wxBitmapType type, int width, int height, int depth = 1);
    virtual bool LoadFile(const wxString& name, wxBitmapType type = wxBITMAP_DEFAULT_TYPE);
    virtual bool SaveFile(const wxString& name, wxBitmapType type, const wxPalette *cmap = NULL) const;

    // copies the contents and mask of the given (colour) icon to the bitmap
    virtual bool CopyFromIcon(const wxIcon& icon);

    wxImage ConvertToImage() const;

    // get the given part of bitmap
    wxBitmap GetSubBitmap( const wxRect& rect ) const;

    int GetWidth() const;
    int GetHeight() const;
    int GetDepth() const;
    int GetQuality() const;
    void SetWidth(int w);
    void SetHeight(int h);
    void SetDepth(int d);
    void SetQuality(int q);
    void SetOk(bool isOk);

    // raw bitmap access support functions
    void *GetRawData(wxPixelDataBase& data, int bpp);
    void UngetRawData(wxPixelDataBase& data);

    wxPalette* GetPalette() const;
    void SetPalette(const wxPalette& palette);

    wxMask *GetMask() const;
    void SetMask(wxMask *mask) ;

    wxBitmapType GetBitmapType() const;

    // wxCocoa
    WX_NSBitmapImageRep GetNSBitmapImageRep();
    void SetNSBitmapImageRep(WX_NSBitmapImageRep bitmapImageRep);
    WX_NSImage GetNSImage(bool useMask) const;

    static void InitStandardHandlers() { }
    static void CleanUpHandlers() { }

protected:
    wxGDIRefData *CreateGDIRefData() const;
    wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

    DECLARE_DYNAMIC_CLASS(wxBitmap)
};


#endif // __WX_COCOA_BITMAP_H__
