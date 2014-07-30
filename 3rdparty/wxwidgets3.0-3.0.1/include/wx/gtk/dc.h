/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/dc.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTKDC_H_
#define _WX_GTKDC_H_

#ifdef __WXGTK3__

#include "wx/dcgraph.h"

class wxGTKCairoDCImpl: public wxGCDCImpl
{
    typedef wxGCDCImpl base_type;
public:
    wxGTKCairoDCImpl(wxDC* owner);
    wxGTKCairoDCImpl(wxDC* owner, int);
    wxGTKCairoDCImpl(wxDC* owner, wxWindow* window);

    virtual void DoDrawBitmap(const wxBitmap& bitmap, int x, int y, bool useMask);
    virtual void DoDrawIcon(const wxIcon& icon, int x, int y);
#if wxUSE_IMAGE
    virtual bool DoFloodFill(int x, int y, const wxColour& col, wxFloodFillStyle style);
#endif
    virtual wxBitmap DoGetAsBitmap(const wxRect* subrect) const;
    virtual bool DoGetPixel(int x, int y, wxColour* col) const;
    virtual void DoGetSize(int* width, int* height) const;
    virtual bool DoStretchBlit(int xdest, int ydest, int dstWidth, int dstHeight, wxDC* source, int xsrc, int ysrc, int srcWidth, int srcHeight, wxRasterOperationMode rop, bool useMask, int xsrcMask, int ysrcMask);
    virtual void* GetCairoContext() const;

protected:
    int m_width, m_height;

    wxDECLARE_NO_COPY_CLASS(wxGTKCairoDCImpl);
};
//-----------------------------------------------------------------------------

class wxWindowDCImpl: public wxGTKCairoDCImpl
{
    typedef wxGTKCairoDCImpl base_type;
public:
    wxWindowDCImpl(wxWindowDC* owner, wxWindow* window);

    wxDECLARE_NO_COPY_CLASS(wxWindowDCImpl);
};
//-----------------------------------------------------------------------------

class wxClientDCImpl: public wxGTKCairoDCImpl
{
    typedef wxGTKCairoDCImpl base_type;
public:
    wxClientDCImpl(wxClientDC* owner, wxWindow* window);

    wxDECLARE_NO_COPY_CLASS(wxClientDCImpl);
};
//-----------------------------------------------------------------------------

class wxPaintDCImpl: public wxGTKCairoDCImpl
{
    typedef wxGTKCairoDCImpl base_type;
public:
    wxPaintDCImpl(wxPaintDC* owner, wxWindow* window);

    wxDECLARE_NO_COPY_CLASS(wxPaintDCImpl);
};
//-----------------------------------------------------------------------------

class wxScreenDCImpl: public wxGTKCairoDCImpl
{
    typedef wxGTKCairoDCImpl base_type;
public:
    wxScreenDCImpl(wxScreenDC* owner);

    wxDECLARE_NO_COPY_CLASS(wxScreenDCImpl);
};
//-----------------------------------------------------------------------------

class wxMemoryDCImpl: public wxGTKCairoDCImpl
{
    typedef wxGTKCairoDCImpl base_type;
public:
    wxMemoryDCImpl(wxMemoryDC* owner);
    wxMemoryDCImpl(wxMemoryDC* owner, wxBitmap& bitmap);
    wxMemoryDCImpl(wxMemoryDC* owner, wxDC* dc);
    virtual wxBitmap DoGetAsBitmap(const wxRect* subrect) const;
    virtual void DoSelect(const wxBitmap& bitmap);
    virtual const wxBitmap& GetSelectedBitmap() const;
    virtual wxBitmap& GetSelectedBitmap();

private:
    void Setup();
    wxBitmap m_bitmap;

    wxDECLARE_NO_COPY_CLASS(wxMemoryDCImpl);
};
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGTKCairoDC: public wxDC
{
    typedef wxDC base_type;
public:
    wxGTKCairoDC(cairo_t* cr);

    wxDECLARE_NO_COPY_CLASS(wxGTKCairoDC);
};

#else

#include "wx/dc.h"

//-----------------------------------------------------------------------------
// wxDC
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGTKDCImpl : public wxDCImpl
{
public:
    wxGTKDCImpl( wxDC *owner );
    virtual ~wxGTKDCImpl();

#if wxUSE_PALETTE
    void SetColourMap( const wxPalette& palette ) { SetPalette(palette); }
#endif // wxUSE_PALETTE

    // Resolution in pixels per logical inch
    virtual wxSize GetPPI() const;

    virtual bool StartDoc( const wxString& WXUNUSED(message) ) { return true; }
    virtual void EndDoc() { }
    virtual void StartPage() { }
    virtual void EndPage() { }

    virtual GdkWindow* GetGDKWindow() const { return NULL; }
    virtual void* GetHandle() const { return GetGDKWindow(); }
    
    // base class pure virtuals implemented here
    virtual void DoSetClippingRegion(wxCoord x, wxCoord y, wxCoord width, wxCoord height);
    virtual void DoGetSizeMM(int* width, int* height) const;

    DECLARE_ABSTRACT_CLASS(wxGTKDCImpl)
};

// this must be defined when wxDC::Blit() honours the DC origin and needed to
// allow wxUniv code in univ/winuniv.cpp to work with versions of wxGTK
// 2.3.[23]
#ifndef wxHAS_WORKING_GTK_DC_BLIT
    #define wxHAS_WORKING_GTK_DC_BLIT
#endif

#endif
#endif // _WX_GTKDC_H_
