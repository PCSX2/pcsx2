/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/dc.h
// Purpose:     wxDC class
// Author:      David Webster
// Modified by:
// Created:     08/26/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DC_H_
#define _WX_DC_H_

#include "wx/defs.h"
#include "wx/dc.h"

// ---------------------------------------------------------------------------
// macros
// ---------------------------------------------------------------------------

// Logical to device
// Absolute
#define XLOG2DEV(x) (x)
#define YLOG2DEV(y) (y)

// Relative
#define XLOG2DEVREL(x) (x)
#define YLOG2DEVREL(y) (y)

// Device to logical
// Absolute
#define XDEV2LOG(x) (x)

#define YDEV2LOG(y) (y)

// Relative
#define XDEV2LOGREL(x) (x)
#define YDEV2LOGREL(y) (y)

/*
 * Have the same macros as for XView but not for every operation:
 * just for calculating window/viewport extent (a better way of scaling).
 */

// Logical to device
// Absolute
#define MS_XLOG2DEV(x) LogicalToDevice(x)

#define MS_YLOG2DEV(y) LogicalToDevice(y)

// Relative
#define MS_XLOG2DEVREL(x) LogicalToDeviceXRel(x)
#define MS_YLOG2DEVREL(y) LogicalToDeviceYRel(y)

// Device to logical
// Absolute
#define MS_XDEV2LOG(x) DeviceToLogicalX(x)

#define MS_YDEV2LOG(y) DeviceToLogicalY(y)

// Relative
#define MS_XDEV2LOGREL(x) DeviceToLogicalXRel(x)
#define MS_YDEV2LOGREL(y) DeviceToLogicalYRel(y)

#define YSCALE(y) (yorigin - (y))

#define     wx_round(a)    (int)((a)+.5)

#if wxUSE_DC_CACHEING
/*
 * Cached blitting, maintaining a cache
 * of bitmaps required for transparent blitting
 * instead of constant creation/deletion
 */

class wxDCCacheEntry : public wxObject
{
public:
    wxDCCacheEntry( WXHBITMAP hBitmap
                   ,int       nWidth
                   ,int       nHeight
                   ,int       nDepth
                  );
    wxDCCacheEntry( HPS   hPS
                   ,int   nDepth
                  );
    virtual ~wxDCCacheEntry();

    WXHBITMAP                       m_hBitmap;
    HPS                             m_hPS;
    int                             m_nWidth;
    int                             m_nHeight;
    int                             m_nDepth;
}; // end of CLASS wxDCCacheEntry
#endif

// this is an ABC: use one of the derived classes to create a DC associated
// with a window, screen, printer and so on
class WXDLLIMPEXP_CORE wxPMDCImpl : public wxDCImpl
{
    DECLARE_DYNAMIC_CLASS(wxDC)

public:
    wxPMDCImpl(wxDC *owner, WXHDC hDC);
    virtual ~wxPMDCImpl();

    // implement base class pure virtuals
    // ----------------------------------

    virtual void Clear();

    virtual bool    StartDoc(const wxString& rsMessage);
    virtual void    EndDoc();

    virtual void    StartPage();
    virtual void    EndPage();

    virtual void    SetFont(const wxFont& rFont);
    virtual void    SetPen(const wxPen& rPen);
    virtual void    SetBrush(const wxBrush& rBrush);
    virtual void    SetBackground(const wxBrush& rBrush);
    virtual void    SetBackgroundMode(int nMode);
    virtual void    SetPalette(const wxPalette& rPalette);

    virtual void    DestroyClippingRegion();

    virtual wxCoord GetCharHeight() const;
    virtual wxCoord GetCharWidth() const;

    virtual bool    CanDrawBitmap() const;
    virtual bool    CanGetTextExtent() const;
    virtual int     GetDepth() const;
    virtual wxSize  GetPPI() const;

    virtual void    SetMapMode(wxMappingMode nMode);
    virtual void    SetUserScale( double dX
                                 ,double dY
                                );
    virtual void    SetLogicalScale( double dX
                                    ,double dY
                                   );
    virtual void    SetLogicalOrigin( wxCoord vX
                                     ,wxCoord vY
                                    );
    virtual void    SetDeviceOrigin( wxCoord vX
                                    ,wxCoord vY
                                   );
    virtual void    SetAxisOrientation( bool bXLeftRight
                                       ,bool bYBottomUp
                                      );
    virtual void    SetLogicalFunction(wxRasterOperationMode nFunction);

    // implementation from now on
    // --------------------------

    virtual void    SetRop(WXHDC hCdc);
    virtual void    SelectOldObjects(WXHDC hDc);

    wxWindow*       GetWindow() const { return m_pCanvas; }
    void            SetWindow(wxWindow* pWin) { m_pCanvas = pWin; }

    WXHDC           GetHDC() const { return m_hDC; }
    void            SetHDC( WXHDC hDc
                           ,bool  bOwnsDC = FALSE
                          )
    {
        m_hDC = hDc;
        m_bOwnsDC = bOwnsDC;
    }

    HPS             GetHPS() const { return m_hPS; }
    void            SetHPS(HPS hPS)
    {
        m_hPS = hPS;
    }
    const wxBitmap& GetSelectedBitmap() const { return m_vSelectedBitmap; }
    wxBitmap&       GetSelectedBitmap() { return m_vSelectedBitmap; }

    void            UpdateClipBox();

#if wxUSE_DC_CACHEING
    static wxDCCacheEntry* FindBitmapInCache( HPS hPS
                                             ,int nWidth
                                             ,int nHeight
                                            );
    static wxDCCacheEntry* FindDCInCache( wxDCCacheEntry* pNotThis
                                         ,HPS             hPS
                                        );

    static void AddToBitmapCache(wxDCCacheEntry* pEntry);
    static void AddToDCCache(wxDCCacheEntry* pEntry);
    static void ClearCache();
#endif

protected:
    void Init()
    {
        m_pCanvas      = NULL;
        m_hOldBitmap   = 0;
        m_hOldPen      = 0;
        m_hOldBrush    = 0;
        m_hOldFont     = 0;
#if wxUSE_PALETTE
        m_hOldPalette  = 0;
#endif // wxUSE_PALETTE

        m_bOwnsDC      = false;
        m_hDC          = 0;
        m_hOldPS       = NULL;
        m_hPS          = NULL;
        m_bIsPaintTime = false; // True at Paint Time

        m_pen.SetColour(*wxBLACK);
        m_brush.SetColour(*wxWHITE);
    }

    // create an uninitialized DC: this should be only used by the derived
    // classes
    wxPMDCImpl( wxDC *owner ) : wxDCImpl( owner ) { Init(); }

public:
    virtual void    DoGetTextExtent( const wxString& rsString
                                    ,wxCoord*        pX
                                    ,wxCoord*        pY
                                    ,wxCoord*        pDescent = NULL
                                    ,wxCoord*        pExternalLeading = NULL
                                    ,const wxFont*   pTheFont = NULL
                                   ) const;
    virtual bool DoFloodFill( wxCoord          vX
                             ,wxCoord          vY
                             ,const wxColour&  rCol
                             ,wxFloodFillStyle nStyle = wxFLOOD_SURFACE
                            );

    virtual bool DoGetPixel( wxCoord   vX
                            ,wxCoord   vY
                            ,wxColour* pCol
                           ) const;

    virtual void DoDrawPoint( wxCoord vX
                             ,wxCoord vY
                            );
    virtual void DoDrawLine( wxCoord vX1
                            ,wxCoord vY1
                            ,wxCoord vX2
                            ,wxCoord vY2
                           );

    virtual void DoDrawArc( wxCoord vX1
                           ,wxCoord vY1
                           ,wxCoord vX2
                           ,wxCoord vY2
                           ,wxCoord vXc
                           ,wxCoord vYc
                          );
    virtual void DoDrawCheckMark( wxCoord vX
                                 ,wxCoord vY
                                 ,wxCoord vWidth
                                 ,wxCoord vHeight
                                );
    virtual void DoDrawEllipticArc( wxCoord vX
                                   ,wxCoord vY
                                   ,wxCoord vW
                                   ,wxCoord vH
                                   ,double  dSa
                                   ,double  dEa
                                  );

    virtual void DoDrawRectangle( wxCoord vX
                                 ,wxCoord vY
                                 ,wxCoord vWidth
                                 ,wxCoord vHeight
                                );
    virtual void DoDrawRoundedRectangle( wxCoord vX
                                        ,wxCoord vY
                                        ,wxCoord vWidth
                                        ,wxCoord vHeight
                                        ,double  dRadius
                                       );
    virtual void DoDrawEllipse( wxCoord vX
                               ,wxCoord vY
                               ,wxCoord vWidth
                               ,wxCoord vHeight
                              );

    virtual void DoCrossHair( wxCoord vX
                             ,wxCoord vY
                            );

    virtual void DoDrawIcon( const wxIcon& rIcon
                            ,wxCoord       vX
                            ,wxCoord       vY
                           );
    virtual void DoDrawBitmap( const wxBitmap& rBmp
                              ,wxCoord         vX
                              ,wxCoord         vY
                              ,bool            bUseMask = FALSE
                             );

    virtual void DoDrawText( const wxString& rsText
                            ,wxCoord         vX
                            ,wxCoord         vY
                           );
    virtual void DoDrawRotatedText( const wxString& rsText
                                   ,wxCoord         vX
                                   ,wxCoord         vY
                                   ,double          dAngle
                                  );

    virtual bool DoBlit( wxCoord vXdest
                        ,wxCoord vYdest
                        ,wxCoord vWidth
                        ,wxCoord vHeight
                        ,wxDC*   pSource
                        ,wxCoord vXsrc
                        ,wxCoord vYsrc
                        ,wxRasterOperationMode     nRop = wxCOPY
                        ,bool    bUseMask = FALSE
                        ,wxCoord vXsrcMask = -1
                        ,wxCoord vYsrcMask = -1
                       );

    virtual void DoSetClippingRegion( wxCoord vX
                                     ,wxCoord vY
                                     ,wxCoord vWidth
                                     ,wxCoord vHeight
                                    );
    virtual void DoSetDeviceClippingRegion(const wxRegion& rRegion);

    virtual void DoGetSize( int* pWidth
                           ,int* pHeight
                          ) const;
    virtual void DoGetSizeMM( int* pWidth
                             ,int* pHeight
                            ) const;

    virtual void DoDrawLines( int     n
                             ,const wxPoint vaPoints[]
                             ,wxCoord vXoffset
                             ,wxCoord yYoffset
                            );
    virtual void DoDrawPolygon( int     n
                               ,const wxPoint vaPoints[]
                               ,wxCoord vXoffset
                               ,wxCoord vYoffset
                               ,wxPolygonFillMode     nFillStyle = wxODDEVEN_RULE
                              );

#if wxUSE_PALETTE
    void DoSelectPalette(bool bRealize = FALSE);
    void InitializePalette();
#endif // wxUSE_PALETTE

protected:
    //
    // common part of DoDrawText() and DoDrawRotatedText()
    //
    void DrawAnyText( const wxString& rsText
                     ,wxCoord         vX
                     ,wxCoord         vY
                    );

    // OS2-specific member variables ?? do we even need this under OS/2?
    int                             m_nWindowExtX;
    int                             m_nWindowExtY;

    //
    // the window associated with this DC (may be NULL)
    //
    wxWindow*                       m_pCanvas;
    wxBitmap                        m_vSelectedBitmap;

public:
    // PM specific stuff
    HPS                             m_hPS;
    HPS                             m_hOldPS;   // old hPS, if any
    bool                            m_bIsPaintTime;// True at Paint Time

    RECTL                           m_vRclPaint; // Bounding rectangle at Paint time etc.
    //
    // TRUE => DeleteDC() in dtor, FALSE => only ReleaseDC() it
    //
    bool                            m_bOwnsDC:1;

    //
    // our HDC
    //
    WXHDC                           m_hDC;

    //
    // Store all old GDI objects when do a SelectObject, so we can select them
    // back in (this unselecting user's objects) so we can safely delete the
    // DC.
    //
    WXHBITMAP                       m_hOldBitmap;
    WXHPEN                          m_hOldPen;
    WXHBRUSH                        m_hOldBrush;
    WXHFONT                         m_hOldFont;
    WXHPALETTE                      m_hOldPalette;

#if wxUSE_DC_CACHEING
    static wxList                   m_svBitmapCache;
    static wxList                   m_svDCCache;
#endif
}; // end of CLASS wxDC
#endif
    // _WX_DC_H_
