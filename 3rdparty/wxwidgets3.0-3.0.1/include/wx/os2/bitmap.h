/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/bitmap.h
// Purpose:     wxBitmap class
// Author:      David Webster
// Modified by:
// Created:     11/28/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_BITMAP_H_
#define _WX_BITMAP_H_

#include "wx/os2/private.h"
#include "wx/os2/gdiimage.h"
#include "wx/gdicmn.h"
#include "wx/palette.h"

class WXDLLIMPEXP_FWD_CORE wxDC;
class WXDLLIMPEXP_FWD_CORE wxControl;
class WXDLLIMPEXP_FWD_CORE wxBitmap;
class WXDLLIMPEXP_FWD_CORE wxBitmapHandler;
class WXDLLIMPEXP_FWD_CORE wxIcon;
class WXDLLIMPEXP_FWD_CORE wxMask;
class WXDLLIMPEXP_FWD_CORE wxCursor;
class WXDLLIMPEXP_FWD_CORE wxControl;
class WXDLLIMPEXP_FWD_CORE wxPixelDataBase;

// ----------------------------------------------------------------------------
// Bitmap data
//
// NB: this class is private, but declared here to make it possible inline
//     wxBitmap functions accessing it
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxBitmapRefData : public wxGDIImageRefData
{
public:
    wxBitmapRefData();
    wxBitmapRefData(const wxBitmapRefData &tocopy);
    virtual ~wxBitmapRefData() { Free(); }

    virtual void Free();

public:
    int                             m_nNumColors;
    wxPalette                       m_vBitmapPalette;
    int                             m_nQuality;

    // OS2-specific
    // ------------

    wxDC*                           m_pSelectedInto;

    //
    // Optional mask for transparent drawing
    //
    wxMask*                         m_pBitmapMask;
}; // end of CLASS wxBitmapRefData

// ----------------------------------------------------------------------------
// wxBitmap: a mono or colour bitmap
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxBitmap : public wxGDIImage,
                                  public wxBitmapHelpers
{
public:
    // default ctor creates an invalid bitmap, you must Create() it later
    wxBitmap() { Init(); }

    // Copy constructors
    inline wxBitmap(const wxBitmap& rBitmap)
        : wxGDIImage(rBitmap)
    {
        Init();
        SetHandle(rBitmap.GetHandle());
    }

    // Initialize with raw data
    wxBitmap( const char bits[]
             ,int        nWidth
             ,int        nHeight
             ,int        nDepth = 1
            );

    // Initialize with XPM data
    wxBitmap(const char* const* bits);
#ifdef wxNEEDS_CHARPP
    // needed for old GCC
    wxBitmap(char** data)
    {
        *this = wxBitmap(const_cast<const char* const*>(data));
    }
#endif

    // Load a resource
    wxBitmap( int             nId
             ,wxBitmapType    lType = wxBITMAP_DEFAULT_TYPE
            );

    // For compatiability with other ports, under OS/2 does same as default ctor
    inline wxBitmap( const wxString& WXUNUSED(rFilename)
                    ,wxBitmapType    WXUNUSED(lType)
                   )
    { Init(); }
    // New constructor for generalised creation from data
    wxBitmap( const void* pData
             ,wxBitmapType lType
             ,int   nWidth
             ,int   nHeight
             ,int   nDepth = 1
            );

    // If depth is omitted, will create a bitmap compatible with the display
    wxBitmap( int nWidth, int nHeight, int nDepth = -1 )
    {
        Init();
        (void)Create(nWidth, nHeight, nDepth);
    }
    wxBitmap( const wxSize& sz, int nDepth = -1 )
    {
        Init();
        (void)Create(sz, nDepth);
    }

    wxBitmap( const wxImage& image, int depth = -1 )
                         { (void)CreateFromImage(image, depth); }

    // we must have this, otherwise icons are silently copied into bitmaps using
    // the copy ctor but the resulting bitmap is invalid!
    inline wxBitmap(const wxIcon& rIcon)
      { Init(); CopyFromIcon(rIcon); }

    wxBitmap& operator=(const wxIcon& rIcon)
    {
        (void)CopyFromIcon(rIcon);

        return(*this);
    }

    wxBitmap& operator=(const wxCursor& rCursor)
    {
        (void)CopyFromCursor(rCursor);
        return (*this);
    }

    virtual ~wxBitmap();

    wxImage ConvertToImage() const;
    wxBitmap ConvertToDisabled(unsigned char brightness = 255) const;

    // get the given part of bitmap
    wxBitmap GetSubBitmap(const wxRect& rRect) const;

    // copies the contents and mask of the given (colour) icon to the bitmap
    bool CopyFromIcon(const wxIcon& rIcon);

    // copies the contents and mask of the given cursor to the bitmap
    bool CopyFromCursor(const wxCursor& rCursor);

    virtual bool Create( int nWidth
                        ,int nHeight
                        ,int nDepth = wxBITMAP_SCREEN_DEPTH
                       );
    virtual bool Create(const wxSize& sz, int depth = wxBITMAP_SCREEN_DEPTH)
        { return Create(sz.GetWidth(), sz.GetHeight(), depth); }
    virtual bool Create(int width, int height, const wxDC& WXUNUSED(dc))
        { return Create(width,height); }

    virtual bool Create( const void* pData
                        ,wxBitmapType lType
                        ,int   nWidth
                        ,int   nHeight
                        ,int   nDepth = 1
                       );
    virtual bool LoadFile( int             nId
                          ,wxBitmapType    lType = wxBITMAP_DEFAULT_TYPE
                         );
    virtual bool LoadFile( const wxString& rName
                          ,wxBitmapType    lType = wxBITMAP_DEFAULT_TYPE
                         );
    virtual bool SaveFile( const wxString&  rName
                          ,wxBitmapType     lType
                          ,const wxPalette* pCmap = NULL
                         );

    inline wxBitmapRefData* GetBitmapData() const
      { return (wxBitmapRefData *)m_refData; }

    // raw bitmap access support functions
    void *GetRawData(wxPixelDataBase& data, int bpp);
    void UngetRawData(wxPixelDataBase& data);

    inline int GetQuality() const
      { return (GetBitmapData() ? GetBitmapData()->m_nQuality : 0); }

    void SetQuality(int nQ);

    wxPalette* GetPalette() const
      { return (GetBitmapData() ? (& GetBitmapData()->m_vBitmapPalette) : NULL); }

    void       SetPalette(const wxPalette& rPalette);

    inline wxMask* GetMask() const
      { return (GetBitmapData() ? GetBitmapData()->m_pBitmapMask : NULL); }

    void SetMask(wxMask* pMask) ;

    // Implementation
public:
    inline void SetHBITMAP(WXHBITMAP hBmp)
      { SetHandle((WXHANDLE)hBmp); }

    inline WXHBITMAP GetHBITMAP() const
      { return (WXHBITMAP)GetHandle(); }

    inline void  SetSelectedInto(wxDC* pDc)
      { if (GetBitmapData()) GetBitmapData()->m_pSelectedInto = pDc; }

    inline wxDC* GetSelectedInto() const
      { return (GetBitmapData() ? GetBitmapData()->m_pSelectedInto : NULL); }

    inline bool IsMono(void) const { return m_bIsMono; }

    // An OS/2 version that probably doesn't do anything like the msw version
    wxBitmap GetBitmapForDC(wxDC& rDc) const;

protected:
    // common part of all ctors
    void Init();

    inline virtual wxGDIImageRefData* CreateData() const
        { return new wxBitmapRefData; }

    bool CreateFromImage(const wxImage& image, int depth);

    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

private:
    bool CopyFromIconOrCursor(const wxGDIImage& rIcon);

    bool                            m_bIsMono;
    DECLARE_DYNAMIC_CLASS(wxBitmap)
}; // end of CLASS wxBitmap

// ----------------------------------------------------------------------------
// wxMask: a mono bitmap used for drawing bitmaps transparently.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMask : public wxObject
{
public:
    wxMask();
    wxMask( const wxMask& tocopy);

    // Construct a mask from a bitmap and a colour indicating the transparent
    // area
    wxMask( const wxBitmap& rBitmap
           ,const wxColour& rColour
          );

    // Construct a mask from a bitmap and a palette index indicating the
    // transparent area
    wxMask( const wxBitmap& rBitmap
           ,int             nPaletteIndex
          );

    // Construct a mask from a mono bitmap (copies the bitmap).
    wxMask(const wxBitmap& rBitmap);

    // construct a mask from the givne bitmap handle
    wxMask(WXHBITMAP hBmp)
      { m_hMaskBitmap = hBmp; }

    virtual ~wxMask();

    bool Create( const wxBitmap& bitmap
                ,const wxColour& rColour
               );
    bool Create( const wxBitmap& rBitmap
                ,int             nPaletteIndex
               );
    bool Create(const wxBitmap& rBitmap);

    // Implementation
    WXHBITMAP GetMaskBitmap() const
      { return m_hMaskBitmap; }
    void SetMaskBitmap(WXHBITMAP hBmp)
      { m_hMaskBitmap = hBmp; }

protected:
    WXHBITMAP                       m_hMaskBitmap;
    DECLARE_DYNAMIC_CLASS(wxMask)
}; // end of CLASS wxMask

// ----------------------------------------------------------------------------
// wxBitmapHandler is a class which knows how to load/save bitmaps to/from file
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxBitmapHandler : public wxGDIImageHandler
{
public:
    inline wxBitmapHandler()
      { m_lType = wxBITMAP_TYPE_INVALID; }

    inline wxBitmapHandler( const wxString& rName
                    ,const wxString& rExt
                    ,wxBitmapType lType
                   )
                   : wxGDIImageHandler( rName
                                       ,rExt
                                       ,lType)
    {
    }

    // keep wxBitmapHandler derived from wxGDIImageHandler compatible with the
    // old class which worked only with bitmaps
    virtual bool Create( wxBitmap* pBitmap
                        ,const void* pData
                        ,wxBitmapType lType
                        ,int       nWidth
                        ,int       nHeight
                        ,int       nDepth = 1
                       );
    virtual bool LoadFile( wxBitmap*       pBitmap
                          ,int             nId
                          ,wxBitmapType    lType
                          ,int             nDesiredWidth
                          ,int             nDesiredHeight
                         );
    virtual bool LoadFile( wxBitmap*       pBitmap
                          ,const wxString& rName
                          ,wxBitmapType    lType
                          ,int             nDesiredWidth
                          ,int             nDesiredHeight
                         );
    virtual bool SaveFile( wxBitmap*        pBitmap
                          ,const wxString&  rName
                          ,wxBitmapType     lType
                          ,const wxPalette* pPalette = NULL
                         ) const;

    virtual bool Create( wxGDIImage* pImage
                        ,const void* pData
                        ,wxBitmapType lFlags
                        ,int         nWidth
                        ,int         nHeight
                        ,int         nDepth = 1
                       );
    virtual bool Load( wxGDIImage*     pImage
                      ,int             nId
                      ,wxBitmapType    lFlags
                      ,int             nDesiredWidth
                      ,int             nDesiredHeight
                     );
    virtual bool Save( const wxGDIImage* pImage
                      ,const wxString&   rName
                      ,wxBitmapType      lType
                     ) const;
private:
    inline virtual bool Load( wxGDIImage*     WXUNUSED(pImage)
                             ,const wxString& WXUNUSED(rName)
                             ,WXHANDLE        WXUNUSED(hPs)
                             ,wxBitmapType    WXUNUSED(lFlags)
                             ,int             WXUNUSED(nDesiredWidth)
                             ,int             WXUNUSED(nDesiredHeight)
                            )
    { return false; }
    DECLARE_DYNAMIC_CLASS(wxBitmapHandler)
}; // end of CLASS wxBitmapHandler

#endif
  // _WX_BITMAP_H_
