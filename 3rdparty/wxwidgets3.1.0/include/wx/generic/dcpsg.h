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
    wxDECLARE_DYNAMIC_CLASS(wxPostScriptDC);
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
    virtual bool IsOk() const wxOVERRIDE;

    bool CanDrawBitmap() const wxOVERRIDE { return true; }

    void Clear() wxOVERRIDE;
    void SetFont( const wxFont& font ) wxOVERRIDE;
    void SetPen( const wxPen& pen ) wxOVERRIDE;
    void SetBrush( const wxBrush& brush ) wxOVERRIDE;
    void SetLogicalFunction( wxRasterOperationMode function ) wxOVERRIDE;
    void SetBackground( const wxBrush& brush ) wxOVERRIDE;

    void DestroyClippingRegion() wxOVERRIDE;

    bool StartDoc(const wxString& message) wxOVERRIDE;
    void EndDoc() wxOVERRIDE;
    void StartPage() wxOVERRIDE;
    void EndPage() wxOVERRIDE;

    wxCoord GetCharHeight() const wxOVERRIDE;
    wxCoord GetCharWidth() const wxOVERRIDE;
    bool CanGetTextExtent() const wxOVERRIDE { return true; }

    // Resolution in pixels per logical inch
    wxSize GetPPI() const wxOVERRIDE;

    virtual void ComputeScaleAndOrigin() wxOVERRIDE;

    void SetBackgroundMode(int WXUNUSED(mode)) wxOVERRIDE { }
    void SetPalette(const wxPalette& WXUNUSED(palette)) wxOVERRIDE { }

    void SetPrintData(const wxPrintData& data);
    wxPrintData& GetPrintData() { return m_printData; }

    virtual int GetDepth() const wxOVERRIDE { return 24; }

    void PsPrint( const wxString& psdata );

    // Overrridden for wxPrinterDC Impl

    virtual int GetResolution() const wxOVERRIDE;
    virtual wxRect GetPaperRect() const wxOVERRIDE;

    virtual void* GetHandle() const wxOVERRIDE { return NULL; }
    
protected:
    bool DoFloodFill(wxCoord x1, wxCoord y1, const wxColour &col,
                     wxFloodFillStyle style = wxFLOOD_SURFACE) wxOVERRIDE;
    bool DoGetPixel(wxCoord x1, wxCoord y1, wxColour *col) const wxOVERRIDE;
    void DoDrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2) wxOVERRIDE;
    void DoCrossHair(wxCoord x, wxCoord y) wxOVERRIDE ;
    void DoDrawArc(wxCoord x1,wxCoord y1,wxCoord x2,wxCoord y2,wxCoord xc,wxCoord yc) wxOVERRIDE;
    void DoDrawEllipticArc(wxCoord x,wxCoord y,wxCoord w,wxCoord h,double sa,double ea) wxOVERRIDE;
    void DoDrawPoint(wxCoord x, wxCoord y) wxOVERRIDE;
    void DoDrawLines(int n, const wxPoint points[], wxCoord xoffset = 0, wxCoord yoffset = 0) wxOVERRIDE;
    void DoDrawPolygon(int n, const wxPoint points[],
                       wxCoord xoffset = 0, wxCoord yoffset = 0,
                       wxPolygonFillMode fillStyle = wxODDEVEN_RULE) wxOVERRIDE;
    void DoDrawPolyPolygon(int n, const int count[], const wxPoint points[],
                           wxCoord xoffset = 0, wxCoord yoffset = 0,
                           wxPolygonFillMode fillStyle = wxODDEVEN_RULE) wxOVERRIDE;
    void DoDrawRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height) wxOVERRIDE;
    void DoDrawRoundedRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height, double radius = 20) wxOVERRIDE;
    void DoDrawEllipse(wxCoord x, wxCoord y, wxCoord width, wxCoord height) wxOVERRIDE;
#if wxUSE_SPLINES
    void DoDrawSpline(const wxPointList *points) wxOVERRIDE;
#endif
    bool DoBlit(wxCoord xdest, wxCoord ydest, wxCoord width, wxCoord height,
                wxDC *source, wxCoord xsrc, wxCoord ysrc,
                wxRasterOperationMode rop = wxCOPY, bool useMask = false,
                wxCoord xsrcMask = wxDefaultCoord, wxCoord ysrcMask = wxDefaultCoord) wxOVERRIDE;
    void DoDrawIcon(const wxIcon& icon, wxCoord x, wxCoord y) wxOVERRIDE;
    void DoDrawBitmap(const wxBitmap& bitmap, wxCoord x, wxCoord y, bool useMask = false) wxOVERRIDE;
    void DoDrawText(const wxString& text, wxCoord x, wxCoord y) wxOVERRIDE;
    void DoDrawRotatedText(const wxString& text, wxCoord x, wxCoord y, double angle) wxOVERRIDE;
    void DoSetClippingRegion(wxCoord x, wxCoord y, wxCoord width, wxCoord height) wxOVERRIDE;
    void DoSetDeviceClippingRegion( const wxRegion &WXUNUSED(clip)) wxOVERRIDE
    {
        wxFAIL_MSG( "not implemented" );
    }
    void DoGetTextExtent(const wxString& string, wxCoord *x, wxCoord *y,
                         wxCoord *descent = NULL,
                         wxCoord *externalLeading = NULL,
                         const wxFont *theFont = NULL) const wxOVERRIDE;
    void DoGetSize(int* width, int* height) const wxOVERRIDE;
    void DoGetSizeMM(int *width, int *height) const wxOVERRIDE;

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
    wxDECLARE_DYNAMIC_CLASS(wxPostScriptDCImpl);
};

#endif
    // wxUSE_POSTSCRIPT && wxUSE_PRINTING_ARCHITECTURE

#endif
        // _WX_DCPSG_H_
