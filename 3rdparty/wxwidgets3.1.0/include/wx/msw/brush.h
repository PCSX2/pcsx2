/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/brush.h
// Purpose:     wxBrush class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_BRUSH_H_
#define _WX_BRUSH_H_

class WXDLLIMPEXP_FWD_CORE wxBrush;
class WXDLLIMPEXP_FWD_CORE wxColour;
class WXDLLIMPEXP_FWD_CORE wxBitmap;

// ----------------------------------------------------------------------------
// wxBrush
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxBrush : public wxBrushBase
{
public:
    wxBrush();
    wxBrush(const wxColour& col, wxBrushStyle style = wxBRUSHSTYLE_SOLID);
    wxBrush(const wxBitmap& stipple);
    virtual ~wxBrush();

    virtual void SetColour(const wxColour& col);
    virtual void SetColour(unsigned char r, unsigned char g, unsigned char b);
    virtual void SetStyle(wxBrushStyle style);
    virtual void SetStipple(const wxBitmap& stipple);

    bool operator==(const wxBrush& brush) const;
    bool operator!=(const wxBrush& brush) const { return !(*this == brush); }

    wxColour GetColour() const;
    wxBrushStyle GetStyle() const;
    wxBitmap *GetStipple() const;


    wxDEPRECATED_MSG("use wxBRUSHSTYLE_XXX constants")
    wxBrush(const wxColour& col, int style);

    wxDEPRECATED_MSG("use wxBRUSHSTYLE_XXX constants")
    void SetStyle(int style) { SetStyle((wxBrushStyle)style); }

    // return the HBRUSH for this brush
    virtual WXHANDLE GetResourceHandle() const;

protected:
    virtual wxGDIRefData *CreateGDIRefData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

private:
    wxDECLARE_DYNAMIC_CLASS(wxBrush);
};

#endif // _WX_BRUSH_H_
