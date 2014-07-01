/////////////////////////////////////////////////////////////////////////////
// Name:        wx/dfb/dc.h
// Purpose:     wxDC class
// Author:      Vaclav Slavik
// Created:     2006-08-07
// Copyright:   (c) 2006 REA Elektronik GmbH
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DFB_DC_H_
#define _WX_DFB_DC_H_

#include "wx/defs.h"
#include "wx/region.h"
#include "wx/dc.h"
#include "wx/dfb/dfbptr.h"

wxDFB_DECLARE_INTERFACE(IDirectFBSurface);

//-----------------------------------------------------------------------------
// wxDFBDCImpl
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxDFBDCImpl : public wxDCImpl
{
public:
    // ctors
    wxDFBDCImpl(wxDC *owner) : wxDCImpl(owner) { m_surface = NULL; }
    wxDFBDCImpl(wxDC *owner, const wxIDirectFBSurfacePtr& surface)
        : wxDCImpl(owner)
    {
        DFBInit(surface);
    }

    bool IsOk() const { return m_surface != NULL; }

    // implement base class pure virtuals
    // ----------------------------------

    virtual void Clear();

    virtual bool StartDoc(const wxString& message);
    virtual void EndDoc();

    virtual void StartPage();
    virtual void EndPage();

    virtual void SetFont(const wxFont& font);
    virtual void SetPen(const wxPen& pen);
    virtual void SetBrush(const wxBrush& brush);
    virtual void SetBackground(const wxBrush& brush);
    virtual void SetBackgroundMode(int mode);
#if wxUSE_PALETTE
    virtual void SetPalette(const wxPalette& palette);
#endif

    virtual void SetLogicalFunction(wxRasterOperationMode function);

    virtual void DestroyClippingRegion();

    virtual wxCoord GetCharHeight() const;
    virtual wxCoord GetCharWidth() const;
    virtual void DoGetTextExtent(const wxString& string,
                                 wxCoord *x, wxCoord *y,
                                 wxCoord *descent = NULL,
                                 wxCoord *externalLeading = NULL,
                                 const wxFont *theFont = NULL) const;

    virtual bool CanDrawBitmap() const { return true; }
    virtual bool CanGetTextExtent() const { return true; }
    virtual int GetDepth() const;
    virtual wxSize GetPPI() const;

    // Returns the surface (and increases its ref count)
    wxIDirectFBSurfacePtr GetDirectFBSurface() const { return m_surface; }

protected:
    // implementation
    wxCoord XDEV2LOG(wxCoord x) const       { return DeviceToLogicalX(x); }
    wxCoord XDEV2LOGREL(wxCoord x) const    { return DeviceToLogicalXRel(x); }
    wxCoord YDEV2LOG(wxCoord y) const       { return DeviceToLogicalY(y); }
    wxCoord YDEV2LOGREL(wxCoord y) const    { return DeviceToLogicalYRel(y); }
    wxCoord XLOG2DEV(wxCoord x) const       { return LogicalToDeviceX(x); }
    wxCoord XLOG2DEVREL(wxCoord x) const    { return LogicalToDeviceXRel(x); }
    wxCoord YLOG2DEV(wxCoord y) const       { return LogicalToDeviceY(y); }
    wxCoord YLOG2DEVREL(wxCoord y) const    { return LogicalToDeviceYRel(y); }

    // initializes the DC from a surface, must be called if default ctor
    // was used
    void DFBInit(const wxIDirectFBSurfacePtr& surface);

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

    virtual void DoSetClippingRegion(wxCoord x, wxCoord y,
                                     wxCoord width, wxCoord height);
    virtual void DoSetDeviceClippingRegion(const wxRegion& region);

    virtual void DoGetSize(int *width, int *height) const;
    virtual void DoGetSizeMM(int* width, int* height) const;

    virtual void DoDrawLines(int n, const wxPoint points[],
                             wxCoord xoffset, wxCoord yoffset);
    virtual void DoDrawPolygon(int n, const wxPoint points[],
                               wxCoord xoffset, wxCoord yoffset,
                               wxPolygonFillMode fillStyle = wxODDEVEN_RULE);

    // implementation from now on:
protected:
    wxIDirectFBFontPtr GetCurrentFont() const;

private:
    // Unified implementation of DrawIcon, DrawBitmap and Blit:
    void DoDrawSubBitmap(const wxBitmap &bmp,
                         wxCoord x, wxCoord y, wxCoord w, wxCoord h,
                         wxCoord destx, wxCoord desty, int rop, bool useMask);
    bool DoBlitFromSurface(const wxIDirectFBSurfacePtr& src,
                           wxCoord srcx, wxCoord srcy,
                           wxCoord w, wxCoord h,
                           wxCoord dstx, wxCoord dsty);

    // selects colour into surface's state
    void SelectColour(const wxColour& clr);

protected:
    wxIDirectFBSurfacePtr m_surface;

    double            m_mm_to_pix_x, m_mm_to_pix_y;

    friend class WXDLLIMPEXP_FWD_CORE wxOverlayImpl; // for Init

    DECLARE_ABSTRACT_CLASS(wxDFBDCImpl)
};

#endif // _WX_DFB_DC_H_
