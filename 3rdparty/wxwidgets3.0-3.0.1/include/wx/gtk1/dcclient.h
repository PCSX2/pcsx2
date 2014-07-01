/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk1/dcclient.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __GTKDCCLIENTH__
#define __GTKDCCLIENTH__

#include "wx/gtk1/dc.h"
#include "wx/window.h"

//-----------------------------------------------------------------------------
// classes
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxWindowDCImpl;
class WXDLLIMPEXP_FWD_CORE wxPaintDCImpl;
class WXDLLIMPEXP_FWD_CORE wxClientDCImpl;

//-----------------------------------------------------------------------------
// wxWindowDCImpl
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxWindowDCImpl : public wxGTKDCImpl
{
public:
    wxWindowDCImpl(wxDC *owner);
    wxWindowDCImpl(wxDC *owner, wxWindow *win);

    virtual ~wxWindowDCImpl();

    virtual bool CanDrawBitmap() const { return true; }
    virtual bool CanGetTextExtent() const { return true; }

protected:
    virtual void DoGetSize(int *width, int *height) const;
    virtual bool DoFloodFill( wxCoord x, wxCoord y, const wxColour& col, wxFloodFillStyle style=wxFLOOD_SURFACE );
    virtual bool DoGetPixel( wxCoord x1, wxCoord y1, wxColour *col ) const;

    virtual void DoDrawLine( wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2 );
    virtual void DoCrossHair( wxCoord x, wxCoord y );
    virtual void DoDrawArc( wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2,
                            wxCoord xc, wxCoord yc );
    virtual void DoDrawEllipticArc( wxCoord x, wxCoord y, wxCoord width, wxCoord height,
                                    double sa, double ea );
    virtual void DoDrawPoint( wxCoord x, wxCoord y );

    virtual void DoDrawLines(int n, const wxPoint points[],
                             wxCoord xoffset, wxCoord yoffset);
    virtual void DoDrawPolygon(int n, const wxPoint points[],
                               wxCoord xoffset, wxCoord yoffset,
                               wxPolygonFillMode fillStyle = wxODDEVEN_RULE);

    virtual void DoDrawRectangle( wxCoord x, wxCoord y, wxCoord width, wxCoord height );
    virtual void DoDrawRoundedRectangle( wxCoord x, wxCoord y, wxCoord width, wxCoord height, double radius = 20.0 );
    virtual void DoDrawEllipse( wxCoord x, wxCoord y, wxCoord width, wxCoord height );

    virtual void DoDrawIcon( const wxIcon &icon, wxCoord x, wxCoord y );
    virtual void DoDrawBitmap( const wxBitmap &bitmap, wxCoord x, wxCoord y,
                               bool useMask = false );

    virtual bool DoBlit( wxCoord xdest, wxCoord ydest, wxCoord width, wxCoord height,
                         wxDC *source, wxCoord xsrc, wxCoord ysrc,
                         wxRasterOperationMode logical_func = wxCOPY, bool useMask = false,
                         wxCoord xsrcMask = -1, wxCoord ysrcMask = -1 );

    virtual void DoDrawText( const wxString &text, wxCoord x, wxCoord y );
    virtual void DoDrawRotatedText(const wxString& text, wxCoord x, wxCoord y,
                                   double angle);
    virtual void DoGetTextExtent( const wxString &string,
                                wxCoord *width, wxCoord *height,
                                wxCoord *descent = NULL,
                                wxCoord *externalLeading = NULL,
                                const wxFont *theFont = NULL) const;

public:
    virtual wxCoord GetCharWidth() const;
    virtual wxCoord GetCharHeight() const;

    virtual void Clear();

    virtual void SetFont( const wxFont &font );
    virtual void SetPen( const wxPen &pen );
    virtual void SetBrush( const wxBrush &brush );
    virtual void SetBackground( const wxBrush &brush );
    virtual void SetLogicalFunction( wxRasterOperationMode function );
    virtual void SetTextForeground( const wxColour &col );
    virtual void SetTextBackground( const wxColour &col );
    virtual void SetBackgroundMode( int mode );
    virtual void SetPalette( const wxPalette& palette );

    virtual void DoSetClippingRegion( wxCoord x, wxCoord y, wxCoord width, wxCoord height );
    virtual void DestroyClippingRegion();
    virtual void DoSetDeviceClippingRegion( const wxRegion &region );

    // Resolution in pixels per logical inch
    virtual wxSize GetPPI() const;
    virtual int GetDepth() const;

    virtual GdkWindow* GetGDKWindow() const { return m_window; }

    // implementation
    // --------------

    GdkWindow    *m_window;
    GdkGC        *m_penGC;
    GdkGC        *m_brushGC;
    GdkGC        *m_textGC;
    GdkGC        *m_bgGC;
    GdkColormap  *m_cmap;
    bool          m_isMemDC;
    bool          m_isScreenDC;
    wxWindow     *m_owner;
    wxRegion      m_currentClippingRegion;
    wxRegion      m_paintClippingRegion;

    void SetUpDC();
    void Destroy();
    virtual void ComputeScaleAndOrigin();

    GdkWindow *GetWindow() { return m_window; }

private:
    DECLARE_DYNAMIC_CLASS(wxWindowDCImpl)
};

//-----------------------------------------------------------------------------
// wxClientDCImpl
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxClientDCImpl : public wxWindowDCImpl
{
public:
    wxClientDCImpl(wxDC *owner) : wxWindowDCImpl(owner) { }
    wxClientDCImpl(wxDC *owner, wxWindow *win);

protected:
    virtual void DoGetSize(int *width, int *height) const;

private:
    DECLARE_DYNAMIC_CLASS(wxClientDCImpl)
};

//-----------------------------------------------------------------------------
// wxPaintDCImpl
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPaintDCImpl : public wxClientDCImpl
{
public:
    wxPaintDCImpl(wxDC *owner) : wxClientDCImpl(owner) { }
    wxPaintDCImpl(wxDC *owner, wxWindow *win);

private:
    DECLARE_DYNAMIC_CLASS(wxPaintDCImpl)
};

#endif // __GTKDCCLIENTH__
