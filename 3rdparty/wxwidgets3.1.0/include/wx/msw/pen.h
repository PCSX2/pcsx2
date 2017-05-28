/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/pen.h
// Purpose:     wxPen class
// Author:      Julian Smart
// Modified by: Vadim Zeitlin: fixed operator=(), ==(), !=()
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PEN_H_
#define _WX_PEN_H_

#include "wx/gdiobj.h"
#include "wx/gdicmn.h"

// ----------------------------------------------------------------------------
// Pen
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPen : public wxPenBase
{
public:
    wxPen() { }
    wxPen(const wxColour& col, int width = 1, wxPenStyle style = wxPENSTYLE_SOLID);

    wxPen(const wxBitmap& stipple, int width);
    virtual ~wxPen() { }

    bool operator==(const wxPen& pen) const;
    bool operator!=(const wxPen& pen) const { return !(*this == pen); }

    // Override in order to recreate the pen
    void SetColour(const wxColour& col);
    void SetColour(unsigned char r, unsigned char g, unsigned char b);

    void SetWidth(int width);
    void SetStyle(wxPenStyle style);
    void SetStipple(const wxBitmap& stipple);
    void SetDashes(int nb_dashes, const wxDash *dash);
    void SetJoin(wxPenJoin join);
    void SetCap(wxPenCap cap);

    wxColour GetColour() const;
    int GetWidth() const;
    wxPenStyle GetStyle() const;
    wxPenJoin GetJoin() const;
    wxPenCap GetCap() const;
    int GetDashes(wxDash** ptr) const;
    wxDash* GetDash() const;
    int GetDashCount() const;
    wxBitmap* GetStipple() const;


    wxDEPRECATED_MSG("use wxPENSTYLE_XXX constants")
    wxPen(const wxColour& col, int width, int style);

    wxDEPRECATED_MSG("use wxPENSTYLE_XXX constants")
    void SetStyle(int style) { SetStyle((wxPenStyle)style); }


    // internal: wxGDIObject methods
    virtual bool RealizeResource();
    virtual bool FreeResource(bool force = false);
    virtual WXHANDLE GetResourceHandle() const;
    virtual bool IsFree() const;

protected:
    virtual wxGDIRefData* CreateGDIRefData() const;
    virtual wxGDIRefData* CloneGDIRefData(const wxGDIRefData* data) const;

    // same as FreeResource() + RealizeResource()
    bool Recreate();

    wxDECLARE_DYNAMIC_CLASS(wxPen);
};

#endif // _WX_PEN_H_
