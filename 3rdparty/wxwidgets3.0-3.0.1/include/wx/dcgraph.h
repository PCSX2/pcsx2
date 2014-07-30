/////////////////////////////////////////////////////////////////////////////
// Name:        wx/dcgraph.h
// Purpose:     graphics context device bridge header
// Author:      Stefan Csomor
// Modified by:
// Created:
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GRAPHICS_DC_H_
#define _WX_GRAPHICS_DC_H_

#if wxUSE_GRAPHICS_CONTEXT

#include "wx/dc.h"
#include "wx/geometry.h"
#include "wx/graphics.h"

class WXDLLIMPEXP_FWD_CORE wxWindowDC;


class WXDLLIMPEXP_CORE wxGCDC: public wxDC
{
public:
    wxGCDC( const wxWindowDC& dc );
    wxGCDC( const wxMemoryDC& dc );
#if wxUSE_PRINTING_ARCHITECTURE
    wxGCDC( const wxPrinterDC& dc );
#endif
#if defined(__WXMSW__) && wxUSE_ENH_METAFILE
    wxGCDC( const wxEnhMetaFileDC& dc );
#endif
    wxGCDC(wxGraphicsContext* context);
    
    wxGCDC();
    virtual ~wxGCDC();

    wxGraphicsContext* GetGraphicsContext() const;
    void SetGraphicsContext( wxGraphicsContext* ctx );

#ifdef __WXMSW__
    // override wxDC virtual functions to provide access to HDC associated with
    // this Graphics object (implemented in src/msw/graphics.cpp)
    virtual WXHDC AcquireHDC();
    virtual void ReleaseHDC(WXHDC hdc);
#endif // __WXMSW__

private:
    DECLARE_DYNAMIC_CLASS(wxGCDC)
    wxDECLARE_NO_COPY_CLASS(wxGCDC);
};


class WXDLLIMPEXP_CORE wxGCDCImpl: public wxDCImpl
{
public:
    wxGCDCImpl( wxDC *owner, const wxWindowDC& dc );
    wxGCDCImpl( wxDC *owner, const wxMemoryDC& dc );
#if wxUSE_PRINTING_ARCHITECTURE
    wxGCDCImpl( wxDC *owner, const wxPrinterDC& dc );
#endif
#if defined(__WXMSW__) && wxUSE_ENH_METAFILE
    wxGCDCImpl( wxDC *owner, const wxEnhMetaFileDC& dc );
#endif
    wxGCDCImpl( wxDC *owner );

    virtual ~wxGCDCImpl();

    // implement base class pure virtuals
    // ----------------------------------

    virtual void Clear();

    virtual bool StartDoc( const wxString& message );
    virtual void EndDoc();

    virtual void StartPage();
    virtual void EndPage();

    // flushing the content of this dc immediately onto screen
    virtual void Flush();

    virtual void SetFont(const wxFont& font);
    virtual void SetPen(const wxPen& pen);
    virtual void SetBrush(const wxBrush& brush);
    virtual void SetBackground(const wxBrush& brush);
    virtual void SetBackgroundMode(int mode);
    virtual void SetPalette(const wxPalette& palette);

    virtual void DestroyClippingRegion();

    virtual wxCoord GetCharHeight() const;
    virtual wxCoord GetCharWidth() const;

    virtual bool CanDrawBitmap() const;
    virtual bool CanGetTextExtent() const;
    virtual int GetDepth() const;
    virtual wxSize GetPPI() const;

    virtual void SetLogicalFunction(wxRasterOperationMode function);

    virtual void SetTextForeground(const wxColour& colour);
    virtual void SetTextBackground(const wxColour& colour);

    virtual void ComputeScaleAndOrigin();

    wxGraphicsContext* GetGraphicsContext() const { return m_graphicContext; }
    virtual void SetGraphicsContext( wxGraphicsContext* ctx );

    virtual void* GetHandle() const;

    // the true implementations
    virtual bool DoFloodFill(wxCoord x, wxCoord y, const wxColour& col,
                             wxFloodFillStyle style = wxFLOOD_SURFACE);

    virtual void DoGradientFillLinear(const wxRect& rect,
        const wxColour& initialColour,
        const wxColour& destColour,
        wxDirection nDirection = wxEAST);

    virtual void DoGradientFillConcentric(const wxRect& rect,
        const wxColour& initialColour,
        const wxColour& destColour,
        const wxPoint& circleCenter);

    virtual bool DoGetPixel(wxCoord x, wxCoord y, wxColour *col) const;

    virtual void DoDrawPoint(wxCoord x, wxCoord y);

#if wxUSE_SPLINES
    virtual void DoDrawSpline(const wxPointList *points);
#endif

    virtual void DoDrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2);

    virtual void DoDrawArc(wxCoord x1, wxCoord y1,
        wxCoord x2, wxCoord y2,
        wxCoord xc, wxCoord yc);

    virtual void DoDrawCheckMark(wxCoord x, wxCoord y,
        wxCoord width, wxCoord height);

    virtual void DoDrawEllipticArc(wxCoord x, wxCoord y, wxCoord w, wxCoord h,
        double sa, double ea);

    virtual void DoDrawRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height);
    virtual void DoDrawRoundedRectangle(wxCoord x, wxCoord y,
        wxCoord width, wxCoord height,
        double radius);
    virtual void DoDrawEllipse(wxCoord x, wxCoord y, wxCoord width, wxCoord height);

    virtual void DoCrossHair(wxCoord x, wxCoord y);

    virtual void DoDrawIcon(const wxIcon& icon, wxCoord x, wxCoord y);
    virtual void DoDrawBitmap(const wxBitmap &bmp, wxCoord x, wxCoord y,
        bool useMask = false);

    virtual void DoDrawText(const wxString& text, wxCoord x, wxCoord y);
    virtual void DoDrawRotatedText(const wxString& text, wxCoord x, wxCoord y,
        double angle);

    virtual bool DoBlit(wxCoord xdest, wxCoord ydest, wxCoord width, wxCoord height,
                        wxDC *source, wxCoord xsrc, wxCoord ysrc,
                        wxRasterOperationMode rop = wxCOPY, bool useMask = false,
                        wxCoord xsrcMask = -1, wxCoord ysrcMask = -1);

    virtual bool DoStretchBlit(wxCoord xdest, wxCoord ydest,
                               wxCoord dstWidth, wxCoord dstHeight,
                               wxDC *source,
                               wxCoord xsrc, wxCoord ysrc,
                               wxCoord srcWidth, wxCoord srcHeight,
                               wxRasterOperationMode = wxCOPY, bool useMask = false,
                               wxCoord xsrcMask = wxDefaultCoord, wxCoord ysrcMask = wxDefaultCoord);

    virtual void DoGetSize(int *,int *) const;
    virtual void DoGetSizeMM(int* width, int* height) const;

    virtual void DoDrawLines(int n, const wxPoint points[],
        wxCoord xoffset, wxCoord yoffset);
    virtual void DoDrawPolygon(int n, const wxPoint points[],
                               wxCoord xoffset, wxCoord yoffset,
                               wxPolygonFillMode fillStyle = wxODDEVEN_RULE);
    virtual void DoDrawPolyPolygon(int n, const int count[], const wxPoint points[],
                                   wxCoord xoffset, wxCoord yoffset,
                                   wxPolygonFillMode fillStyle);

    virtual void DoSetDeviceClippingRegion(const wxRegion& region);
    virtual void DoSetClippingRegion(wxCoord x, wxCoord y,
        wxCoord width, wxCoord height);

    virtual void DoGetTextExtent(const wxString& string,
        wxCoord *x, wxCoord *y,
        wxCoord *descent = NULL,
        wxCoord *externalLeading = NULL,
        const wxFont *theFont = NULL) const;

    virtual bool DoGetPartialTextExtents(const wxString& text, wxArrayInt& widths) const;

#ifdef __WXMSW__
    virtual wxRect MSWApplyGDIPlusTransform(const wxRect& r) const;
#endif // __WXMSW__

protected:
    // unused int parameter distinguishes this version, which does not create a
    // wxGraphicsContext, in the expectation that the derived class will do it
    wxGCDCImpl(wxDC* owner, int);

    // scaling variables
    bool m_logicalFunctionSupported;
    wxGraphicsMatrix m_matrixOriginal;
    wxGraphicsMatrix m_matrixCurrent;

    double m_formerScaleX, m_formerScaleY;

    wxGraphicsContext* m_graphicContext;

private:
    void Init(wxGraphicsContext*);

    DECLARE_CLASS(wxGCDCImpl)
    wxDECLARE_NO_COPY_CLASS(wxGCDCImpl);
};

#endif // wxUSE_GRAPHICS_CONTEXT
#endif // _WX_GRAPHICS_DC_H_
