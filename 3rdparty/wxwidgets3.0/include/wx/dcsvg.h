/////////////////////////////////////////////////////////////////////////////
// Name:        wx/dcsvg.h
// Purpose:     wxSVGFileDC
// Author:      Chris Elliott
// Modified by:
// Created:
// Copyright:   (c) Chris Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DCSVG_H_
#define _WX_DCSVG_H_

#include "wx/string.h"
#include "wx/dc.h"

#if wxUSE_SVG

#define wxSVGVersion wxT("v0100")

#ifdef __BORLANDC__
#pragma warn -8008
#pragma warn -8066
#endif

class WXDLLIMPEXP_FWD_BASE wxFileOutputStream;



class WXDLLIMPEXP_FWD_CORE wxSVGFileDC;

class WXDLLIMPEXP_CORE wxSVGFileDCImpl : public wxDCImpl
{
public:
    wxSVGFileDCImpl( wxSVGFileDC *owner, const wxString &filename,
                     int width=320, int height=240, double dpi=72.0 );

    virtual ~wxSVGFileDCImpl();

    bool IsOk() const { return m_OK; }

    virtual bool CanDrawBitmap() const { return true; }
    virtual bool CanGetTextExtent() const { return true; }

    virtual int GetDepth() const
    {
        wxFAIL_MSG(wxT("wxSVGFILEDC::GetDepth Call not implemented"));
        return -1;
    }

    virtual void Clear()
    {
        wxFAIL_MSG(wxT("wxSVGFILEDC::Clear() Call not implemented \nNot sensible for an output file?"));
    }

    virtual void DestroyClippingRegion();

    virtual wxCoord GetCharHeight() const;
    virtual wxCoord GetCharWidth() const;

    virtual void SetClippingRegion(wxCoord WXUNUSED(x), wxCoord WXUNUSED(y),
                                   wxCoord WXUNUSED(w), wxCoord WXUNUSED(h))
    {
        wxFAIL_MSG(wxT("wxSVGFILEDC::SetClippingRegion not implemented"));
    }

    virtual void SetPalette(const wxPalette&  WXUNUSED(palette))
    {
        wxFAIL_MSG(wxT("wxSVGFILEDC::SetPalette not implemented"));
    }

    virtual void GetClippingBox(wxCoord *WXUNUSED(x), wxCoord *WXUNUSED(y),
                                wxCoord *WXUNUSED(w), wxCoord *WXUNUSED(h))
    {
        wxFAIL_MSG(wxT("wxSVGFILEDC::GetClippingBox not implemented"));
    }

    virtual void SetLogicalFunction(wxRasterOperationMode WXUNUSED(function))
    {
        wxFAIL_MSG(wxT("wxSVGFILEDC::SetLogicalFunction Call not implemented"));
    }

    virtual wxRasterOperationMode GetLogicalFunction() const
    {
        wxFAIL_MSG(wxT("wxSVGFILEDC::GetLogicalFunction() not implemented"));
        return wxCOPY;
    }

    virtual void SetBackground( const wxBrush &brush );
    virtual void SetBackgroundMode( int mode );
    virtual void SetBrush(const wxBrush& brush);
    virtual void SetFont(const wxFont& font);
    virtual void SetPen(const wxPen& pen);

    virtual void* GetHandle() const { return NULL; }

private:
   virtual bool DoGetPixel(wxCoord, wxCoord, wxColour *) const
   {
       wxFAIL_MSG(wxT("wxSVGFILEDC::DoGetPixel Call not implemented"));
       return true;
   }

   virtual bool DoBlit(wxCoord, wxCoord, wxCoord, wxCoord, wxDC *,
                       wxCoord, wxCoord, wxRasterOperationMode = wxCOPY,
                       bool = 0, int = -1, int = -1);

   virtual void DoCrossHair(wxCoord, wxCoord)
   {
       wxFAIL_MSG(wxT("wxSVGFILEDC::CrossHair Call not implemented"));
   }

   virtual void DoDrawArc(wxCoord, wxCoord, wxCoord, wxCoord, wxCoord, wxCoord);

   virtual void DoDrawBitmap(const wxBitmap &, wxCoord, wxCoord, bool = false);

   virtual void DoDrawCheckMark(wxCoord x, wxCoord y, wxCoord w, wxCoord h);

   virtual void DoDrawEllipse(wxCoord x, wxCoord y, wxCoord w, wxCoord h);

   virtual void DoDrawEllipticArc(wxCoord x, wxCoord y, wxCoord w, wxCoord h,
                                  double sa, double ea);

   virtual void DoDrawIcon(const wxIcon &, wxCoord, wxCoord);

   virtual void DoDrawLine (wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2);

   virtual void DoDrawLines(int n, const wxPoint points[],
                            wxCoord xoffset = 0, wxCoord yoffset = 0);

   virtual void DoDrawPoint(wxCoord, wxCoord);

   virtual void DoDrawPolygon(int n, const wxPoint points[],
                              wxCoord xoffset, wxCoord yoffset,
                              wxPolygonFillMode fillStyle);

   virtual void DoDrawRectangle(wxCoord x, wxCoord y, wxCoord w, wxCoord h);

   virtual void DoDrawRotatedText(const wxString& text, wxCoord x, wxCoord y,
                                  double angle);

   virtual void DoDrawRoundedRectangle(wxCoord x, wxCoord y,
                                       wxCoord w, wxCoord h,
                                       double radius = 20) ;

   virtual void DoDrawText(const wxString& text, wxCoord x, wxCoord y);

   virtual bool DoFloodFill(wxCoord WXUNUSED(x), wxCoord WXUNUSED(y),
                            const wxColour& WXUNUSED(col),
                            wxFloodFillStyle WXUNUSED(style) = wxFLOOD_SURFACE)
   {
       wxFAIL_MSG(wxT("wxSVGFILEDC::DoFloodFill Call not implemented"));
       return false;
   }

   virtual void DoGetSize(int * x, int *y) const
   {
       if ( x )
           *x = m_width;
       if ( y )
           *y = m_height;
   }

   virtual void DoGetTextExtent(const wxString& string, wxCoord *w, wxCoord *h,
                                wxCoord *descent = NULL,
                                wxCoord *externalLeading = NULL,
                                const wxFont *font = NULL) const;

   virtual void DoSetDeviceClippingRegion(const wxRegion& WXUNUSED(region))
   {
       wxFAIL_MSG(wxT("wxSVGFILEDC::DoSetDeviceClippingRegion not yet implemented"));
   }

   virtual void DoSetClippingRegion(int x,  int y, int width, int height);

   virtual void DoGetSizeMM( int *width, int *height ) const;

   virtual wxSize GetPPI() const;

   void Init (const wxString &filename, int width, int height, double dpi);

   void write( const wxString &s );

private:
   // If m_graphics_changed is true, close the current <g> element and start a
   // new one for the last pen/brush change.
   void NewGraphicsIfNeeded();

   // Open a new graphics group setting up all the attributes according to
   // their current values in wxDC.
   void DoStartNewGraphics();

   wxFileOutputStream *m_outfile;
   wxString            m_filename;
   int                 m_sub_images; // number of png format images we have
   bool                m_OK;
   bool                m_graphics_changed;  // set by Set{Brush,Pen}()
   int                 m_width, m_height;
   double              m_dpi;

   // The clipping nesting level is incremented by every call to
   // SetClippingRegion() and reset when DestroyClippingRegion() is called.
   size_t m_clipNestingLevel;

   // Unique ID for every clipping graphics group: this is simply always
   // incremented in each SetClippingRegion() call.
   size_t m_clipUniqueId;

   DECLARE_ABSTRACT_CLASS(wxSVGFileDCImpl)
};


class WXDLLIMPEXP_CORE wxSVGFileDC : public wxDC
{
public:
    wxSVGFileDC(const wxString& filename,
                int width = 320,
                int height = 240,
                double dpi = 72.0)
        : wxDC(new wxSVGFileDCImpl(this, filename, width, height, dpi))
    {
    }
};

#endif // wxUSE_SVG

#endif // _WX_DCSVG_H_
