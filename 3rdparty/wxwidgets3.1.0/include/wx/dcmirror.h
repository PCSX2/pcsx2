///////////////////////////////////////////////////////////////////////////////
// Name:        wx/dcmirror.h
// Purpose:     wxMirrorDC class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     21.07.2003
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DCMIRROR_H_
#define _WX_DCMIRROR_H_

#include "wx/dc.h"

// ----------------------------------------------------------------------------
// wxMirrorDC allows to write the same code for horz/vertical layout
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMirrorDCImpl : public wxDCImpl
{
public:
    // constructs a mirror DC associated with the given real DC
    //
    // if mirror parameter is true, all vertical and horizontal coordinates are
    // exchanged, otherwise this class behaves in exactly the same way as a
    // plain DC
    wxMirrorDCImpl(wxDC *owner, wxDCImpl& dc, bool mirror)
        : wxDCImpl(owner),
          m_dc(dc)
    {
        m_mirror = mirror;
    }

    // wxDCBase operations
    virtual void Clear() wxOVERRIDE { m_dc.Clear(); }
    virtual void SetFont(const wxFont& font) wxOVERRIDE { m_dc.SetFont(font); }
    virtual void SetPen(const wxPen& pen) wxOVERRIDE { m_dc.SetPen(pen); }
    virtual void SetBrush(const wxBrush& brush) wxOVERRIDE { m_dc.SetBrush(brush); }
    virtual void SetBackground(const wxBrush& brush) wxOVERRIDE
        { m_dc.SetBackground(brush); }
    virtual void SetBackgroundMode(int mode) wxOVERRIDE { m_dc.SetBackgroundMode(mode); }
#if wxUSE_PALETTE
    virtual void SetPalette(const wxPalette& palette) wxOVERRIDE
        { m_dc.SetPalette(palette); }
#endif // wxUSE_PALETTE
    virtual void DestroyClippingRegion() wxOVERRIDE { m_dc.DestroyClippingRegion(); }
    virtual wxCoord GetCharHeight() const wxOVERRIDE { return m_dc.GetCharHeight(); }
    virtual wxCoord GetCharWidth() const wxOVERRIDE { return m_dc.GetCharWidth(); }
    virtual bool CanDrawBitmap() const wxOVERRIDE { return m_dc.CanDrawBitmap(); }
    virtual bool CanGetTextExtent() const wxOVERRIDE { return m_dc.CanGetTextExtent(); }
    virtual int GetDepth() const wxOVERRIDE { return m_dc.GetDepth(); }
    virtual wxSize GetPPI() const wxOVERRIDE { return m_dc.GetPPI(); }
    virtual bool IsOk() const wxOVERRIDE { return m_dc.IsOk(); }
    virtual void SetMapMode(wxMappingMode mode) wxOVERRIDE { m_dc.SetMapMode(mode); }
    virtual void SetUserScale(double x, double y) wxOVERRIDE
        { m_dc.SetUserScale(GetX(x, y), GetY(x, y)); }
    virtual void SetLogicalOrigin(wxCoord x, wxCoord y) wxOVERRIDE
        { m_dc.SetLogicalOrigin(GetX(x, y), GetY(x, y)); }
    virtual void SetDeviceOrigin(wxCoord x, wxCoord y) wxOVERRIDE
        { m_dc.SetDeviceOrigin(GetX(x, y), GetY(x, y)); }
    virtual void SetAxisOrientation(bool xLeftRight, bool yBottomUp) wxOVERRIDE
        { m_dc.SetAxisOrientation(GetX(xLeftRight, yBottomUp),
                                  GetY(xLeftRight, yBottomUp)); }
    virtual void SetLogicalFunction(wxRasterOperationMode function) wxOVERRIDE
        { m_dc.SetLogicalFunction(function); }

    virtual void* GetHandle() const wxOVERRIDE
        { return m_dc.GetHandle(); }

protected:
    // returns x and y if not mirroring or y and x if mirroring
    wxCoord GetX(wxCoord x, wxCoord y) const { return m_mirror ? y : x; }
    wxCoord GetY(wxCoord x, wxCoord y) const { return m_mirror ? x : y; }
    double GetX(double x, double y) const { return m_mirror ? y : x; }
    double GetY(double x, double y) const { return m_mirror ? x : y; }
    bool GetX(bool x, bool y) const { return m_mirror ? y : x; }
    bool GetY(bool x, bool y) const { return m_mirror ? x : y; }

    // same thing but for pointers
    wxCoord *GetX(wxCoord *x, wxCoord *y) const { return m_mirror ? y : x; }
    wxCoord *GetY(wxCoord *x, wxCoord *y) const { return m_mirror ? x : y; }

    // exchange x and y components of all points in the array if necessary
    wxPoint* Mirror(int n, const wxPoint*& points) const
    {
        wxPoint* points_alloc = NULL;
        if ( m_mirror )
        {
            points_alloc = new wxPoint[n];
            for ( int i = 0; i < n; i++ )
            {
                points_alloc[i].x = points[i].y;
                points_alloc[i].y = points[i].x;
            }
            points = points_alloc;
        }
        return points_alloc;
    }

    // wxDCBase functions
    virtual bool DoFloodFill(wxCoord x, wxCoord y, const wxColour& col,
                             wxFloodFillStyle style = wxFLOOD_SURFACE) wxOVERRIDE
    {
        return m_dc.DoFloodFill(GetX(x, y), GetY(x, y), col, style);
    }

    virtual bool DoGetPixel(wxCoord x, wxCoord y, wxColour *col) const wxOVERRIDE
    {
        return m_dc.DoGetPixel(GetX(x, y), GetY(x, y), col);
    }


    virtual void DoDrawPoint(wxCoord x, wxCoord y) wxOVERRIDE
    {
        m_dc.DoDrawPoint(GetX(x, y), GetY(x, y));
    }

    virtual void DoDrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2) wxOVERRIDE
    {
        m_dc.DoDrawLine(GetX(x1, y1), GetY(x1, y1), GetX(x2, y2), GetY(x2, y2));
    }

    virtual void DoDrawArc(wxCoord x1, wxCoord y1,
                           wxCoord x2, wxCoord y2,
                           wxCoord xc, wxCoord yc) wxOVERRIDE
    {
        wxFAIL_MSG( wxT("this is probably wrong") );

        m_dc.DoDrawArc(GetX(x1, y1), GetY(x1, y1),
                       GetX(x2, y2), GetY(x2, y2),
                       xc, yc);
    }

    virtual void DoDrawCheckMark(wxCoord x, wxCoord y,
                                 wxCoord w, wxCoord h) wxOVERRIDE
    {
        m_dc.DoDrawCheckMark(GetX(x, y), GetY(x, y),
                             GetX(w, h), GetY(w, h));
    }

    virtual void DoDrawEllipticArc(wxCoord x, wxCoord y, wxCoord w, wxCoord h,
                                   double sa, double ea) wxOVERRIDE
    {
        wxFAIL_MSG( wxT("this is probably wrong") );

        m_dc.DoDrawEllipticArc(GetX(x, y), GetY(x, y),
                               GetX(w, h), GetY(w, h),
                               sa, ea);
    }

    virtual void DoDrawRectangle(wxCoord x, wxCoord y, wxCoord w, wxCoord h) wxOVERRIDE
    {
        m_dc.DoDrawRectangle(GetX(x, y), GetY(x, y), GetX(w, h), GetY(w, h));
    }

    virtual void DoDrawRoundedRectangle(wxCoord x, wxCoord y,
                                        wxCoord w, wxCoord h,
                                        double radius) wxOVERRIDE
    {
        m_dc.DoDrawRoundedRectangle(GetX(x, y), GetY(x, y),
                                    GetX(w, h), GetY(w, h),
                                    radius);
    }

    virtual void DoDrawEllipse(wxCoord x, wxCoord y, wxCoord w, wxCoord h) wxOVERRIDE
    {
        m_dc.DoDrawEllipse(GetX(x, y), GetY(x, y), GetX(w, h), GetY(w, h));
    }

    virtual void DoCrossHair(wxCoord x, wxCoord y) wxOVERRIDE
    {
        m_dc.DoCrossHair(GetX(x, y), GetY(x, y));
    }

    virtual void DoDrawIcon(const wxIcon& icon, wxCoord x, wxCoord y) wxOVERRIDE
    {
        m_dc.DoDrawIcon(icon, GetX(x, y), GetY(x, y));
    }

    virtual void DoDrawBitmap(const wxBitmap &bmp, wxCoord x, wxCoord y,
                              bool useMask = false) wxOVERRIDE
    {
        m_dc.DoDrawBitmap(bmp, GetX(x, y), GetY(x, y), useMask);
    }

    virtual void DoDrawText(const wxString& text, wxCoord x, wxCoord y) wxOVERRIDE
    {
        // this is never mirrored
        m_dc.DoDrawText(text, x, y);
    }

    virtual void DoDrawRotatedText(const wxString& text,
                                   wxCoord x, wxCoord y, double angle) wxOVERRIDE
    {
        // this is never mirrored
        m_dc.DoDrawRotatedText(text, x, y, angle);
    }

    virtual bool DoBlit(wxCoord xdest, wxCoord ydest,
                        wxCoord w, wxCoord h,
                        wxDC *source, wxCoord xsrc, wxCoord ysrc,
                        wxRasterOperationMode rop = wxCOPY,
                        bool useMask = false,
                        wxCoord xsrcMask = wxDefaultCoord, wxCoord ysrcMask = wxDefaultCoord) wxOVERRIDE
    {
        return m_dc.DoBlit(GetX(xdest, ydest), GetY(xdest, ydest),
                           GetX(w, h), GetY(w, h),
                           source, GetX(xsrc, ysrc), GetY(xsrc, ysrc),
                           rop, useMask,
                           GetX(xsrcMask, ysrcMask), GetX(xsrcMask, ysrcMask));
    }

    virtual void DoGetSize(int *w, int *h) const wxOVERRIDE
    {
        m_dc.DoGetSize(GetX(w, h), GetY(w, h));
    }

    virtual void DoGetSizeMM(int *w, int *h) const wxOVERRIDE
    {
        m_dc.DoGetSizeMM(GetX(w, h), GetY(w, h));
    }

    virtual void DoDrawLines(int n, const wxPoint points[],
                             wxCoord xoffset, wxCoord yoffset) wxOVERRIDE
    {
        wxPoint* points_alloc = Mirror(n, points);

        m_dc.DoDrawLines(n, points,
                         GetX(xoffset, yoffset), GetY(xoffset, yoffset));

        delete[] points_alloc;
    }

    virtual void DoDrawPolygon(int n, const wxPoint points[],
                               wxCoord xoffset, wxCoord yoffset,
                               wxPolygonFillMode fillStyle = wxODDEVEN_RULE) wxOVERRIDE
    {
        wxPoint* points_alloc = Mirror(n, points);

        m_dc.DoDrawPolygon(n, points,
                           GetX(xoffset, yoffset), GetY(xoffset, yoffset),
                           fillStyle);

        delete[] points_alloc;
    }

    virtual void DoSetDeviceClippingRegion(const wxRegion& WXUNUSED(region)) wxOVERRIDE
    {
        wxFAIL_MSG( wxT("not implemented") );
    }

    virtual void DoSetClippingRegion(wxCoord x, wxCoord y,
                                     wxCoord w, wxCoord h) wxOVERRIDE
    {
        m_dc.DoSetClippingRegion(GetX(x, y), GetY(x, y), GetX(w, h), GetY(w, h));
    }

    virtual void DoGetTextExtent(const wxString& string,
                                 wxCoord *x, wxCoord *y,
                                 wxCoord *descent = NULL,
                                 wxCoord *externalLeading = NULL,
                                 const wxFont *theFont = NULL) const wxOVERRIDE
    {
        // never mirrored
        m_dc.DoGetTextExtent(string, x, y, descent, externalLeading, theFont);
    }

private:
    wxDCImpl& m_dc;

    bool m_mirror;

    wxDECLARE_NO_COPY_CLASS(wxMirrorDCImpl);
};

class WXDLLIMPEXP_CORE wxMirrorDC : public wxDC
{
public:
    wxMirrorDC(wxDC& dc, bool mirror)
        : wxDC(new wxMirrorDCImpl(this, *dc.GetImpl(), mirror))
    {
        m_mirror = mirror;
    }

    // helper functions which may be useful for the users of this class
    wxSize Reflect(const wxSize& sizeOrig)
    {
        return m_mirror ? wxSize(sizeOrig.y, sizeOrig.x) : sizeOrig;
    }

private:
    bool m_mirror;

    wxDECLARE_NO_COPY_CLASS(wxMirrorDC);
};

#endif // _WX_DCMIRROR_H_

