/////////////////////////////////////////////////////////////////////////////
// Name:        wx/dfb/pen.h
// Purpose:     wxPen class declaration
// Author:      Vaclav Slavik
// Created:     2006-08-04
// Copyright:   (c) 2006 REA Elektronik GmbH
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DFB_PEN_H_
#define _WX_DFB_PEN_H_

#include "wx/defs.h"
#include "wx/object.h"
#include "wx/string.h"
#include "wx/gdiobj.h"
#include "wx/gdicmn.h"

//-----------------------------------------------------------------------------
// classes
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxBitmap;
class WXDLLIMPEXP_FWD_CORE wxPen;

//-----------------------------------------------------------------------------
// wxPen
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPen: public wxPenBase
{
public:
    wxPen() {}
    wxPen(const wxColour &colour, int width = 1, wxPenStyle style = wxPENSTYLE_SOLID);
#if FUTURE_WXWIN_COMPATIBILITY_3_0
    wxDEPRECATED_FUTURE( wxPen(const wxColour& col, int width, int style) );
#endif

    wxPen(const wxBitmap& stipple, int width);

    bool operator==(const wxPen& pen) const;
    bool operator!=(const wxPen& pen) const { return !(*this == pen); }

    void SetColour(const wxColour &colour);
    void SetColour(unsigned char red, unsigned char green, unsigned char blue);
    void SetCap(wxPenCap capStyle);
    void SetJoin(wxPenJoin joinStyle);
    void SetStyle(wxPenStyle style);
    void SetWidth(int width);
    void SetDashes(int number_of_dashes, const wxDash *dash);
    void SetStipple(const wxBitmap& stipple);

    wxColour GetColour() const;
    wxPenCap GetCap() const;
    wxPenJoin GetJoin() const;
    wxPenStyle GetStyle() const;
    int GetWidth() const;
    int GetDashes(wxDash **ptr) const;
    int GetDashCount() const;
    wxDash* GetDash() const;
    wxBitmap *GetStipple() const;

#if FUTURE_WXWIN_COMPATIBILITY_3_0
    wxDEPRECATED_FUTURE( void SetStyle(int style) )
        { SetStyle((wxPenStyle)style); }
#endif

protected:
    virtual wxGDIRefData *CreateGDIRefData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

    DECLARE_DYNAMIC_CLASS(wxPen)
};

#endif // _WX_DFB_PEN_H_
