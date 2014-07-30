/////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/dcclient.h
// Purpose:     wxClientDCImpl, wxPaintDCImpl and wxWindowDCImpl classes
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DCCLIENT_H_
#define _WX_DCCLIENT_H_

#include "wx/motif/dc.h"

class WXDLLIMPEXP_FWD_CORE wxWindow;

//-----------------------------------------------------------------------------
// wxWindowDCImpl
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxWindowDCImpl : public wxMotifDCImpl
{
public:
    wxWindowDCImpl(wxDC *owner);
    wxWindowDCImpl(wxDC *owner, wxWindow *win);

    virtual ~wxWindowDCImpl();

    // TODO this function is Motif-only for now - should it go into base class?
    void Clear(const wxRect& rect);

    // implement base class pure virtuals
    // ----------------------------------

    virtual void Clear();

    virtual void SetFont(const wxFont& font);
    virtual void SetPen(const wxPen& pen);
    virtual void SetBrush(const wxBrush& brush);
    virtual void SetBackground(const wxBrush& brush);
    virtual void SetBackgroundMode(int mode);
    virtual void SetPalette(const wxPalette& palette);
    virtual void SetLogicalFunction( wxRasterOperationMode function );

    virtual void SetTextForeground(const wxColour& colour);
    virtual void SetTextBackground(const wxColour& colour);

    virtual wxCoord GetCharHeight() const;
    virtual wxCoord GetCharWidth() const;
    virtual void DoGetTextExtent(const wxString& string,
        wxCoord *x, wxCoord *y,
        wxCoord *descent = NULL,
        wxCoord *externalLeading = NULL,
        const wxFont *theFont = NULL) const;

    virtual bool CanDrawBitmap() const;
    virtual bool CanGetTextExtent() const;

    virtual int GetDepth() const;
    virtual wxSize GetPPI() const;

    virtual void DestroyClippingRegion();

    // Helper function for setting clipping
    void SetDCClipping(WXRegion region);

    // implementation from now on
    // --------------------------

    WXGC GetGC() const { return m_gc; }
    WXGC GetBackingGC() const { return m_gcBacking; }
    WXDisplay* GetDisplay() const { return m_display; }
    bool GetAutoSetting() const { return (m_autoSetting != 0); } // See comment in dcclient.cpp
    void SetAutoSetting(bool flag) { m_autoSetting = flag; }

protected:
    // note that this function will call colour.SetPixel,
    // and will do one of curCol = colour, curCol = wxWHITE, curCol = wxBLACK
    // roundToWhite has an effect for monochrome display only
    // if roundToWhite == true then the colour will be set to white unless
    // it is RGB 0x000000;if roundToWhite == true the colour wull be set to
    // black unless it id RGB 0xffffff
    WXPixel CalculatePixel(wxColour& colour, wxColour& curCol,
                           bool roundToWhite) const;
    // sets the foreground pixel taking into account the
    // currently selected logical operation
    void SetForegroundPixelWithLogicalFunction(WXPixel pixel);

    virtual bool DoFloodFill(wxCoord x, wxCoord y, const wxColour& col,
                             wxFloodFillStyle style = wxFLOOD_SURFACE);

    virtual bool DoGetPixel(wxCoord x, wxCoord y, wxColour *col) const;

    virtual void DoDrawPoint(wxCoord x, wxCoord y);
    virtual void DoDrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2);

    virtual void DoDrawArc(wxCoord x1, wxCoord y1,
        wxCoord x2, wxCoord y2,
        wxCoord xc, wxCoord yc);
    virtual void DoDrawEllipticArc(wxCoord x, wxCoord y, wxCoord w, wxCoord h,
        double sa, double ea);

    virtual void DoDrawRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height);
    virtual void DoDrawRoundedRectangle(wxCoord x, wxCoord y,
        wxCoord width, wxCoord height,
        double radius);
    virtual void DoDrawEllipse(wxCoord x, wxCoord y, wxCoord width, wxCoord height);

    virtual void DoCrossHair(wxCoord x, wxCoord y);

    virtual void DoDrawText(const wxString& text, wxCoord x, wxCoord y);
    virtual void DoDrawRotatedText(const wxString &text, wxCoord x, wxCoord y, double angle);

    virtual bool DoBlit(wxCoord xdest, wxCoord ydest, wxCoord width, wxCoord height,
        wxDC *source, wxCoord xsrc, wxCoord ysrc,
        wxRasterOperationMode rop = wxCOPY, bool useMask = false, wxCoord xsrcMask = -1, wxCoord ysrcMask = -1);

    virtual void DoSetClippingRegion(wxCoord x, wxCoord y,
        wxCoord width, wxCoord height);
    virtual void DoSetDeviceClippingRegion(const wxRegion& region);

    virtual void DoDrawLines(int n, const wxPoint points[],
        wxCoord xoffset, wxCoord yoffset);
    virtual void DoDrawPolygon(int n, const wxPoint points[],
        wxCoord xoffset, wxCoord yoffset,
        wxPolygonFillMode fillStyle = wxODDEVEN_RULE);

    void DoGetSize( int *width, int *height ) const;

    // common part of constructors
    void Init();

    WXGC         m_gc;
    WXGC         m_gcBacking;
    WXDisplay*   m_display;
    wxWindow*    m_window;
    // Pixmap for drawing on
    WXPixmap     m_pixmap;
    // Last clipping region set on th GC, this is the combination
    // of paint clipping region and all user-defined clipping regions
    WXRegion     m_clipRegion;

    // Not sure if we'll need all of these
    WXPixel      m_backgroundPixel;
    wxColour     m_currentColour;
    int          m_currentPenWidth ;
    int          m_currentPenJoin ;
    int          m_currentPenCap ;
    int          m_currentPenDashCount ;
    wxX11Dash*   m_currentPenDash ;
    wxBitmap     m_currentStipple ;
    int          m_currentStyle ;
    int          m_currentFill ;
    int          m_autoSetting ; // See comment in dcclient.cpp

    DECLARE_DYNAMIC_CLASS(wxWindowDCImpl)
};

class WXDLLIMPEXP_CORE wxPaintDCImpl: public wxWindowDCImpl
{
public:
    wxPaintDCImpl(wxDC *owner) : wxWindowDCImpl(owner) { }
    wxPaintDCImpl(wxDC *owner, wxWindow* win);

    virtual ~wxPaintDCImpl();

    DECLARE_DYNAMIC_CLASS(wxPaintDCImpl)
};

class WXDLLIMPEXP_CORE wxClientDCImpl: public wxWindowDCImpl
{
public:
    wxClientDCImpl(wxDC *owner) : wxWindowDCImpl(owner) { }
    wxClientDCImpl(wxDC *owner, wxWindow* win)
        : wxWindowDCImpl(owner, win) { }

    DECLARE_DYNAMIC_CLASS(wxClientDCImpl)
};

#endif // _WX_DCCLIENT_H_
