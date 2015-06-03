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

// Base class for bitmap handlers used by wxSVGFileDC, used by the standard
// "embed" and "link" handlers below but can also be used to create a custom
// handler.
class WXDLLIMPEXP_CORE wxSVGBitmapHandler
{
public:
    // Write the representation of the given bitmap, appearing at the specified
    // position, to the provided stream.
    virtual bool ProcessBitmap(const wxBitmap& bitmap,
                               wxCoord x, wxCoord y,
                               wxOutputStream& stream) const = 0;

    virtual ~wxSVGBitmapHandler() {}
};

// Predefined standard bitmap handler: creates a file, stores the bitmap in
// this file and uses the file URI in the generated SVG.
class WXDLLIMPEXP_CORE wxSVGBitmapFileHandler : public wxSVGBitmapHandler
{
public:
    virtual bool ProcessBitmap(const wxBitmap& bitmap,
                               wxCoord x, wxCoord y,
                               wxOutputStream& stream) const wxOVERRIDE;
};

// Predefined handler which embeds the bitmap (base64-encoding it) inside the
// generated SVG file.
class WXDLLIMPEXP_CORE wxSVGBitmapEmbedHandler : public wxSVGBitmapHandler
{
public:
    virtual bool ProcessBitmap(const wxBitmap& bitmap,
                               wxCoord x, wxCoord y,
                               wxOutputStream& stream) const wxOVERRIDE;
};

class WXDLLIMPEXP_CORE wxSVGFileDCImpl : public wxDCImpl
{
public:
    wxSVGFileDCImpl( wxSVGFileDC *owner, const wxString &filename,
                     int width=320, int height=240, double dpi=72.0 );

    virtual ~wxSVGFileDCImpl();

    bool IsOk() const wxOVERRIDE { return m_OK; }

    virtual bool CanDrawBitmap() const wxOVERRIDE { return true; }
    virtual bool CanGetTextExtent() const wxOVERRIDE { return true; }

    virtual int GetDepth() const wxOVERRIDE
    {
        wxFAIL_MSG(wxT("wxSVGFILEDC::GetDepth Call not implemented"));
        return -1;
    }

    virtual void Clear() wxOVERRIDE
    {
        wxFAIL_MSG(wxT("wxSVGFILEDC::Clear() Call not implemented \nNot sensible for an output file?"));
    }

    virtual void DestroyClippingRegion() wxOVERRIDE;

    virtual wxCoord GetCharHeight() const wxOVERRIDE;
    virtual wxCoord GetCharWidth() const wxOVERRIDE;

    virtual void SetClippingRegion(wxCoord WXUNUSED(x), wxCoord WXUNUSED(y),
                                   wxCoord WXUNUSED(w), wxCoord WXUNUSED(h))
    {
        wxFAIL_MSG(wxT("wxSVGFILEDC::SetClippingRegion not implemented"));
    }

    virtual void SetPalette(const wxPalette&  WXUNUSED(palette)) wxOVERRIDE
    {
        wxFAIL_MSG(wxT("wxSVGFILEDC::SetPalette not implemented"));
    }

    virtual void GetClippingBox(wxCoord *WXUNUSED(x), wxCoord *WXUNUSED(y),
                                wxCoord *WXUNUSED(w), wxCoord *WXUNUSED(h))
    {
        wxFAIL_MSG(wxT("wxSVGFILEDC::GetClippingBox not implemented"));
    }

    virtual void SetLogicalFunction(wxRasterOperationMode WXUNUSED(function)) wxOVERRIDE
    {
        wxFAIL_MSG(wxT("wxSVGFILEDC::SetLogicalFunction Call not implemented"));
    }

    virtual wxRasterOperationMode GetLogicalFunction() const wxOVERRIDE
    {
        wxFAIL_MSG(wxT("wxSVGFILEDC::GetLogicalFunction() not implemented"));
        return wxCOPY;
    }

    virtual void SetBackground( const wxBrush &brush ) wxOVERRIDE;
    virtual void SetBackgroundMode( int mode ) wxOVERRIDE;
    virtual void SetBrush(const wxBrush& brush) wxOVERRIDE;
    virtual void SetFont(const wxFont& font) wxOVERRIDE;
    virtual void SetPen(const wxPen& pen) wxOVERRIDE;

    virtual void* GetHandle() const wxOVERRIDE { return NULL; }

    void SetBitmapHandler(wxSVGBitmapHandler* handler);

private:
   virtual bool DoGetPixel(wxCoord, wxCoord, wxColour *) const wxOVERRIDE
   {
       wxFAIL_MSG(wxT("wxSVGFILEDC::DoGetPixel Call not implemented"));
       return true;
   }

   virtual bool DoBlit(wxCoord, wxCoord, wxCoord, wxCoord, wxDC *,
                       wxCoord, wxCoord, wxRasterOperationMode = wxCOPY,
                       bool = 0, int = -1, int = -1) wxOVERRIDE;

   virtual void DoCrossHair(wxCoord, wxCoord) wxOVERRIDE
   {
       wxFAIL_MSG(wxT("wxSVGFILEDC::CrossHair Call not implemented"));
   }

   virtual void DoDrawArc(wxCoord, wxCoord, wxCoord, wxCoord, wxCoord, wxCoord) wxOVERRIDE;

   virtual void DoDrawBitmap(const wxBitmap &, wxCoord, wxCoord, bool = false) wxOVERRIDE;

   virtual void DoDrawCheckMark(wxCoord x, wxCoord y, wxCoord w, wxCoord h) wxOVERRIDE;

   virtual void DoDrawEllipse(wxCoord x, wxCoord y, wxCoord w, wxCoord h) wxOVERRIDE;

   virtual void DoDrawEllipticArc(wxCoord x, wxCoord y, wxCoord w, wxCoord h,
                                  double sa, double ea) wxOVERRIDE;

   virtual void DoDrawIcon(const wxIcon &, wxCoord, wxCoord) wxOVERRIDE;

   virtual void DoDrawLine (wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2) wxOVERRIDE;

   virtual void DoDrawLines(int n, const wxPoint points[],
                            wxCoord xoffset = 0, wxCoord yoffset = 0) wxOVERRIDE;

   virtual void DoDrawPoint(wxCoord, wxCoord) wxOVERRIDE;

   virtual void DoDrawPolygon(int n, const wxPoint points[],
                              wxCoord xoffset, wxCoord yoffset,
                              wxPolygonFillMode fillStyle) wxOVERRIDE;

   virtual void DoDrawRectangle(wxCoord x, wxCoord y, wxCoord w, wxCoord h) wxOVERRIDE;

   virtual void DoDrawRotatedText(const wxString& text, wxCoord x, wxCoord y,
                                  double angle) wxOVERRIDE;

   virtual void DoDrawRoundedRectangle(wxCoord x, wxCoord y,
                                       wxCoord w, wxCoord h,
                                       double radius = 20) wxOVERRIDE ;

   virtual void DoDrawText(const wxString& text, wxCoord x, wxCoord y) wxOVERRIDE;

   virtual bool DoFloodFill(wxCoord WXUNUSED(x), wxCoord WXUNUSED(y),
                            const wxColour& WXUNUSED(col),
                            wxFloodFillStyle WXUNUSED(style) = wxFLOOD_SURFACE) wxOVERRIDE
   {
       wxFAIL_MSG(wxT("wxSVGFILEDC::DoFloodFill Call not implemented"));
       return false;
   }

   virtual void DoGetSize(int * x, int *y) const wxOVERRIDE
   {
       if ( x )
           *x = m_width;
       if ( y )
           *y = m_height;
   }

   virtual void DoGetTextExtent(const wxString& string, wxCoord *w, wxCoord *h,
                                wxCoord *descent = NULL,
                                wxCoord *externalLeading = NULL,
                                const wxFont *font = NULL) const wxOVERRIDE;

   virtual void DoSetDeviceClippingRegion(const wxRegion& WXUNUSED(region)) wxOVERRIDE
   {
       wxFAIL_MSG(wxT("wxSVGFILEDC::DoSetDeviceClippingRegion not yet implemented"));
   }

   virtual void DoSetClippingRegion(int x,  int y, int width, int height) wxOVERRIDE;

   virtual void DoGetSizeMM( int *width, int *height ) const wxOVERRIDE;

   virtual wxSize GetPPI() const wxOVERRIDE;

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
   wxSVGBitmapHandler* m_bmp_handler; // class to handle bitmaps

   // The clipping nesting level is incremented by every call to
   // SetClippingRegion() and reset when DestroyClippingRegion() is called.
   size_t m_clipNestingLevel;

   // Unique ID for every clipping graphics group: this is simply always
   // incremented in each SetClippingRegion() call.
   size_t m_clipUniqueId;

   wxDECLARE_ABSTRACT_CLASS(wxSVGFileDCImpl);
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

    // wxSVGFileDC-specific methods:

    // Use a custom bitmap handler: takes ownership of the handler.
    void SetBitmapHandler(wxSVGBitmapHandler* handler);
};

#endif // wxUSE_SVG

#endif // _WX_DCSVG_H_
