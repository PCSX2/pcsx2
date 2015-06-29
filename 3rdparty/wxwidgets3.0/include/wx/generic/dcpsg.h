/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/dcpsg.h
// Purpose:     wxPostScriptDC class
// Author:      Julian Smart and others
// Modified by:
// Copyright:   (c) Julian Smart and Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DCPSG_H_
#define _WX_DCPSG_H_

#include "wx/defs.h"

#if wxUSE_PRINTING_ARCHITECTURE && wxUSE_POSTSCRIPT

#include "wx/dc.h"
#include "wx/dcprint.h"
#include "wx/dialog.h"
#include "wx/module.h"
#include "wx/cmndata.h"
#include "wx/strvararg.h"

//-----------------------------------------------------------------------------
// wxPostScriptDC
//-----------------------------------------------------------------------------


class WXDLLIMPEXP_CORE wxPostScriptDC : public wxDC
{
public:
    wxPostScriptDC();

    // Recommended constructor
    wxPostScriptDC(const wxPrintData& printData);

private:
    DECLARE_DYNAMIC_CLASS(wxPostScriptDC)
};

class WXDLLIMPEXP_CORE wxPostScriptDCImpl : public wxDCImpl
{
public:
    wxPostScriptDCImpl( wxPrinterDC *owner );
    wxPostScriptDCImpl( wxPrinterDC *owner, const wxPrintData& data );
    wxPostScriptDCImpl( wxPostScriptDC *owner );
    wxPostScriptDCImpl( wxPostScriptDC *owner, const wxPrintData& data );

    void Init();

    virtual ~wxPostScriptDCImpl();

    virtual bool Ok() const { return IsOk(); }
    virtual bool IsOk() const;

    bool CanDrawBitmap() const { return true; }

    void Clear();
    void SetFont( const wxFont& font );
    void SetPen( const wxPen& pen );
    void SetBrush( const wxBrush& brush );
    void SetLogicalFunction( wxRasterOperationMode function );
    void SetBackground( const wxBrush& brush );

    void DestroyClippingRegion();

    bool StartDoc(const wxString& message);
    void EndDoc();
    void StartPage();
    void EndPage();

    wxCoord GetCharHeight() const;
    wxCoord GetCharWidth() const;
    bool CanGetTextExtent() const { return true; }

    // Resolution in pixels per logical inch
    wxSize GetPPI() const;

    virtual void ComputeScaleAndOrigin();

    void SetBackgroundMode(int WXUNUSED(mode)) { }
    void SetPalette(const wxPalette& WXUNUSED(palette)) { }

    void SetPrintData(const wxPrintData& data);
    wxPrintData& GetPrintData() { return m_printData; }

    virtual int GetDepth() const { return 24; }

    void PsPrint( const wxString& psdata );

    // Overrridden for wxPrinterDC Impl

    virtual int GetResolution() const;
    virtual wxRect GetPaperRect() const;

    virtual void* GetHandle() const { return NULL; }
    
protected:
    bool DoFloodFill(wxCoord x1, wxCoord y1, const wxColour &col,
                     wxFloodFillStyle style = wxFLOOD_SURFACE);
    bool DoGetPixel(wxCoord x1, wxCoord y1, wxColour *col) const;
    void DoDrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2);
    void DoCrossHair(wxCoord x, wxCoord y) ;
    void DoDrawArc(wxCoord x1,wxCoord y1,wxCoord x2,wxCoord y2,wxCoord xc,wxCoord yc);
    void DoDrawEllipticArc(wxCoord x,wxCoord y,wxCoord w,wxCoord h,double sa,double ea);
    void DoDrawPoint(wxCoord x, wxCoord y);
    void DoDrawLines(int n, const wxPoint points[], wxCoord xoffset = 0, wxCoord yoffset = 0);
    void DoDrawPolygon(int n, const wxPoint points[],
                       wxCoord xoffset = 0, wxCoord yoffset = 0,
                       wxPolygonFillMode fillStyle = wxODDEVEN_RULE);
    void DoDrawPolyPolygon(int n, const int count[], const wxPoint points[],
                           wxCoord xoffset = 0, wxCoord yoffset = 0,
                           wxPolygonFillMode fillStyle = wxODDEVEN_RULE);
    void DoDrawRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height);
    void DoDrawRoundedRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height, double radius = 20);
    void DoDrawEllipse(wxCoord x, wxCoord y, wxCoord width, wxCoord height);
#if wxUSE_SPLINES
    void DoDrawSpline(const wxPointList *points);
#endif
    bool DoBlit(wxCoord xdest, wxCoord ydest, wxCoord width, wxCoord height,
                wxDC *source, wxCoord xsrc, wxCoord ysrc,
                wxRasterOperationMode rop = wxCOPY, bool useMask = false,
                wxCoord xsrcMask = wxDefaultCoord, wxCoord ysrcMask = wxDefaultCoord);
    void DoDrawIcon(const wxIcon& icon, wxCoord x, wxCoord y);
    void DoDrawBitmap(const wxBitmap& bitmap, wxCoord x, wxCoord y, bool useMask = false);
    void DoDrawText(const wxString& text, wxCoord x, wxCoord y);
    void DoDrawRotatedText(const wxString& text, wxCoord x, wxCoord y, double angle);
    void DoSetClippingRegion(wxCoord x, wxCoord y, wxCoord width, wxCoord height);
    void DoSetDeviceClippingRegion( const wxRegion &WXUNUSED(clip))
    {
        wxFAIL_MSG( "not implemented" );
    }
    void DoGetTextExtent(const wxString& string, wxCoord *x, wxCoord *y,
                         wxCoord *descent = NULL,
                         wxCoord *externalLeading = NULL,
                         const wxFont *theFont = NULL) const;
    void DoGetSize(int* width, int* height) const;
    void DoGetSizeMM(int *width, int *height) const;

    FILE*             m_pstream;    // PostScript output stream
    unsigned char     m_currentRed;
    unsigned char     m_currentGreen;
    unsigned char     m_currentBlue;
    int               m_pageNumber;
    bool              m_clipping;
    double            m_underlinePosition;
    double            m_underlineThickness;
    wxPrintData       m_printData;
    double            m_pageHeight;

private:
    DECLARE_DYNAMIC_CLASS(wxPostScriptDCImpl)
};

#endif
    // wxUSE_POSTSCRIPT && wxUSE_PRINTING_ARCHITECTURE

#endif
        // _WX_DCPSG_H_
