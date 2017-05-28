///////////////////////////////////////////////////////////////////////////////
// Name:        wx/ownerdrw.h
// Purpose:     interface for owner-drawn GUI elements
// Author:      Vadim Zeitlin
// Modified by: Marcin Malich
// Created:     11.11.97
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OWNERDRW_H_BASE
#define _WX_OWNERDRW_H_BASE

#include "wx/defs.h"

#if wxUSE_OWNER_DRAWN

#include "wx/font.h"
#include "wx/colour.h"

class WXDLLIMPEXP_FWD_CORE wxDC;

// ----------------------------------------------------------------------------
// wxOwnerDrawn - a mix-in base class, derive from it to implement owner-drawn
//                behaviour
//
// wxOwnerDrawn supports drawing of an item with non standard font, color and
// also supports 3 bitmaps: either a checked/unchecked bitmap for a checkable
// element or one unchangeable bitmap otherwise.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxOwnerDrawnBase
{
public:
    wxOwnerDrawnBase()
    {
        m_ownerDrawn = false;
        m_margin = ms_defaultMargin;
    }

    virtual ~wxOwnerDrawnBase() {}

    void SetFont(const wxFont& font)
        { m_font = font; m_ownerDrawn = true; }

    wxFont& GetFont() const
        { return (wxFont&) m_font; }


    void SetTextColour(const wxColour& colText)
        { m_colText = colText; m_ownerDrawn = true; }

    wxColour& GetTextColour() const
        { return (wxColour&) m_colText; }

    void SetBackgroundColour(const wxColour& colBack)
        { m_colBack = colBack; m_ownerDrawn = true; }

    wxColour& GetBackgroundColour() const
        { return (wxColour&) m_colBack ; }


    void SetMarginWidth(int width)
        { m_margin = width; }

    int GetMarginWidth() const
        { return m_margin; }

    static int GetDefaultMarginWidth()
        { return ms_defaultMargin; }


    // get item name (with mnemonics if exist)
    virtual wxString GetName() const = 0;


  // this function might seem strange, but if it returns false it means that
  // no non-standard attribute are set, so there is no need for this control
  // to be owner-drawn. Moreover, you can force owner-drawn to false if you
  // want to change, say, the color for the item but only if it is owner-drawn
  // (see wxMenuItem::wxMenuItem for example)
    bool IsOwnerDrawn() const
        { return m_ownerDrawn; }

    // switch on/off owner-drawing the item
    void SetOwnerDrawn(bool ownerDrawn = true)
        { m_ownerDrawn = ownerDrawn; }


    // constants used in OnDrawItem
    // (they have the same values as corresponding Win32 constants)
    enum wxODAction
    {
        wxODDrawAll       = 0x0001,     // redraw entire control
        wxODSelectChanged = 0x0002,     // selection changed (see Status.Select)
        wxODFocusChanged  = 0x0004      // keyboard focus changed (see Status.Focus)
    };

    enum wxODStatus
    {
        wxODSelected  = 0x0001,         // control is currently selected
        wxODGrayed    = 0x0002,         // item is to be grayed
        wxODDisabled  = 0x0004,         // item is to be drawn as disabled
        wxODChecked   = 0x0008,         // item is to be checked
        wxODHasFocus  = 0x0010,         // item has the keyboard focus
        wxODDefault   = 0x0020,         // item is the default item
        wxODHidePrefix= 0x0100          // hide keyboard cues (w2k and xp only)
    };

    // virtual functions to implement drawing (return true if processed)
    virtual bool OnMeasureItem(size_t *width, size_t *height);
    virtual bool OnDrawItem(wxDC& dc, const wxRect& rc, wxODAction act, wxODStatus stat) = 0;

protected:

    // get the font and colour to use, whether it is set or not
    virtual void GetFontToUse(wxFont& font) const;
    virtual void GetColourToUse(wxODStatus stat, wxColour& colText, wxColour& colBack) const;

private:
    bool        m_ownerDrawn;       // true if something is non standard

    wxFont      m_font;             // font to use for drawing
    wxColour    m_colText,          // color ----"---"---"----
                m_colBack;          // background color

    int         m_margin;           // space occupied by bitmap to the left of the item

    static int  ms_defaultMargin;
};

// ----------------------------------------------------------------------------
// include the platform-specific class declaration
// ----------------------------------------------------------------------------

#if defined(__WXMSW__)
    #include "wx/msw/ownerdrw.h"
#endif

#endif // wxUSE_OWNER_DRAWN

#endif // _WX_OWNERDRW_H_BASE
