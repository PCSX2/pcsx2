/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/pen.h
// Purpose:     wxPen class
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PEN_H_
#define _WX_PEN_H_

#include "wx/gdiobj.h"
#include "wx/colour.h"
#include "wx/bitmap.h"

// Pen
class WXDLLIMPEXP_CORE wxPen : public wxPenBase
{
public:
    wxPen();
    wxPen(const wxColour& col, int width = 1, wxPenStyle style = wxPENSTYLE_SOLID);
#if FUTURE_WXWIN_COMPATIBILITY_3_0
    wxDEPRECATED_FUTURE( wxPen(const wxColour& col, int width, int style) );
#endif

    wxPen(const wxBitmap& stipple, int width);
    virtual ~wxPen();

    bool operator==(const wxPen& pen) const;
    bool operator!=(const wxPen& pen) const { return !(*this == pen); }

    // Override in order to recreate the pen
    void SetColour(const wxColour& col) ;
    void SetColour(unsigned char r, unsigned char g, unsigned char b) ;

    void SetWidth(int width)  ;
    void SetStyle(wxPenStyle style)  ;
    void SetStipple(const wxBitmap& stipple)  ;
    void SetDashes(int nb_dashes, const wxDash *dash)  ;
    void SetJoin(wxPenJoin join)  ;
    void SetCap(wxPenCap cap)  ;

    wxColour GetColour() const ;
    int GetWidth() const;
    wxPenStyle GetStyle() const;
    wxPenJoin GetJoin() const;
    wxPenCap GetCap() const;
    int GetDashes(wxDash **ptr) const;
    int GetDashCount() const;

    wxBitmap *GetStipple() const ;

#if FUTURE_WXWIN_COMPATIBILITY_3_0
    wxDEPRECATED_FUTURE( void SetStyle(int style) )
        { SetStyle((wxPenStyle)style); }
#endif

    // Implementation

    // Useful helper: create the brush resource
    bool RealizeResource();

protected:
    virtual wxGDIRefData *CreateGDIRefData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

private:
    void Unshare();

    DECLARE_DYNAMIC_CLASS(wxPen)
};

#endif
    // _WX_PEN_H_
