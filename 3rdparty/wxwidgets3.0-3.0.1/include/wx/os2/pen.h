/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/pen.h
// Purpose:     wxPen class
// Author:      David Webster
// Modified by:
// Created:     10/10/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PEN_H_
#define _WX_PEN_H_

#include "wx/gdiobj.h"
#include "wx/bitmap.h"

typedef long wxPMDash;

// ----------------------------------------------------------------------------
// Pen
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPen : public wxPenBase
{
public:
  wxPen() { }
    wxPen( const wxColour& rColour
          ,int             nWidth = 1
          ,wxPenStyle      nStyle = wxPENSTYLE_SOLID
         );
#if FUTURE_WXWIN_COMPATIBILITY_3_0
    wxDEPRECATED_FUTURE( wxPen(const wxColour& col, int width, int style) );
#endif

    wxPen( const wxBitmap& rStipple
          ,int             nWidth
         );
    virtual ~wxPen() { }

    bool   operator == (const wxPen& rPen) const;
    inline bool   operator != (const wxPen& rPen) const
        { return !(*this == rPen); }

    //
    // Override in order to recreate the pen
    //
    void SetColour(const wxColour& rColour);
    void SetColour(unsigned char cRed, unsigned char cGreen, unsigned char cBlue);

    void SetWidth(int nWidth);
    void SetStyle(wxPenStyle nStyle);
    void SetStipple(const wxBitmap& rStipple);
    void SetDashes( int           nNbDashes
                   ,const wxDash* pDash
                  );
    void SetJoin(wxPenJoin nJoin);
    void SetCap(wxPenCap nCap);
    void SetPS(HPS hPS);

    wxColour GetColour(void) const;
    int       GetWidth(void) const;
    wxPenStyle GetStyle(void) const;
    wxPenJoin  GetJoin(void) const;
    wxPenCap   GetCap(void) const;
    int       GetPS(void) const;
    int       GetDashes(wxDash **ptr) const;
    wxDash*   GetDash() const;
    int       GetDashCount() const;
    wxBitmap* GetStipple(void) const;

#if FUTURE_WXWIN_COMPATIBILITY_3_0
    wxDEPRECATED_FUTURE( void SetStyle(int style) )
        { SetStyle((wxPenStyle)style); }
#endif

    //
    // Implementation
    //

    //
    // Useful helper: create the brush resource
    //
    bool     RealizeResource(void);
    bool     FreeResource(bool bForce = false);
    virtual WXHANDLE GetResourceHandle(void) const;
    bool     IsFree(void) const;

private:
    LINEBUNDLE                     m_vLineBundle;
    AREABUNDLE                     m_vAreaBundle;

protected:
    virtual wxGDIRefData* CreateGDIRefData() const;
    virtual wxGDIRefData* CloneGDIRefData(const wxGDIRefData* data) const;

    // same as FreeResource() + RealizeResource()
    bool Recreate();

    DECLARE_DYNAMIC_CLASS(wxPen)
}; // end of CLASS wxPen

extern int wx2os2PenStyle(wxPenStyle nWxStyle);

#endif
    // _WX_PEN_H_
