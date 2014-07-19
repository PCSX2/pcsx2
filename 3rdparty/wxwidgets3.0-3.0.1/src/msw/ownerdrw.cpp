///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/ownerdrw.cpp
// Purpose:     implementation of wxOwnerDrawn class
// Author:      Vadim Zeitlin
// Modified by: Marcin Malich
// Created:     13.11.97
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_OWNER_DRAWN

#include "wx/ownerdrw.h"
#include "wx/msw/dc.h"
#include "wx/msw/private.h"
#include "wx/msw/private/dc.h"
#include "wx/msw/wrapcctl.h"            // for HIMAGELIST

// ----------------------------------------------------------------------------
// constants for base class
// ----------------------------------------------------------------------------

int wxOwnerDrawnBase::ms_defaultMargin = 3;

// ============================================================================
// implementation of wxOwnerDrawn class
// ============================================================================

// draw the item
bool wxOwnerDrawn::OnDrawItem(wxDC& dc, const wxRect& rc,
                              wxODAction, wxODStatus stat)
{
    // we do nothing if item isn't ownerdrawn
    if ( !IsOwnerDrawn() )
        return true;

    wxMSWDCImpl *impl = (wxMSWDCImpl*) dc.GetImpl();
    HDC hdc = GetHdcOf(*impl);

    RECT rect;
    wxCopyRectToRECT(rc, rect);

    {
        // set the font and colors
        wxFont font;
        GetFontToUse(font);

        wxColour colText, colBack;
        GetColourToUse(stat, colText, colBack);

        SelectInHDC selFont(hdc, GetHfontOf(font));

        wxMSWImpl::wxTextColoursChanger textCol(hdc, colText, colBack);
        wxMSWImpl::wxBkModeChanger bkMode(hdc, wxBRUSHSTYLE_TRANSPARENT);


        AutoHBRUSH hbr(wxColourToPalRGB(colBack));
        SelectInHDC selBrush(hdc, hbr);

        ::FillRect(hdc, &rect, hbr);

        // using native API because it recognizes '&'

        wxString text = GetName();

        SIZE sizeRect;
        ::GetTextExtentPoint32(hdc, text.c_str(), text.length(), &sizeRect);

        int flags = DST_PREFIXTEXT;
        if ( (stat & wxODDisabled) && !(stat & wxODSelected) )
            flags |= DSS_DISABLED;

        if ( (stat & wxODHidePrefix) )
            flags |= DSS_HIDEPREFIX;

        int x = rc.x + GetMarginWidth();
        int y = rc.y + (rc.GetHeight() - sizeRect.cy) / 2;
        int cx = rc.GetWidth() - GetMarginWidth();
        int cy = sizeRect.cy;

        ::DrawState(hdc, NULL, NULL, wxMSW_CONV_LPARAM(text),
                    text.length(), x, y, cx, cy, flags);

    } // reset to default the font, colors and brush

    if (stat & wxODHasFocus)
        ::DrawFocusRect(hdc, &rect);

    return true;
}

// ----------------------------------------------------------------------------
// global helper functions implemented here
// ----------------------------------------------------------------------------

BOOL wxDrawStateBitmap(HDC hDC, HBITMAP hBitmap, int x, int y, UINT uState)
{
    // determine size of bitmap image
    BITMAP bmp;
    if ( !::GetObject(hBitmap, sizeof(BITMAP), &bmp) )
        return FALSE;

    BOOL result;

    switch ( uState )
    {
        case wxDSB_NORMAL:
        case wxDSB_SELECTED:
            {
                // uses image list functions to draw
                //  - normal bitmap with support transparency
                //    (image list internally create mask etc.)
                //  - blend bitmap with the background colour
                //    (like default selected items)
                HIMAGELIST hIml = ::ImageList_Create(bmp.bmWidth, bmp.bmHeight,
                                                     ILC_COLOR32 | ILC_MASK, 1, 1);
                ::ImageList_Add(hIml, hBitmap, NULL);
                UINT fStyle = uState == wxDSB_SELECTED ? ILD_SELECTED : ILD_NORMAL;
                result = ::ImageList_Draw(hIml, 0, hDC, x, y, fStyle);
                ::ImageList_Destroy(hIml);
            }
            break;

        case wxDSB_DISABLED:
            result = ::DrawState(hDC, NULL, NULL, (LPARAM)hBitmap, 0, x, y,
                                 bmp.bmWidth, bmp.bmHeight,
                                 DST_BITMAP | DSS_DISABLED);
            break;

        default:
            wxFAIL_MSG( wxT("DrawStateBitmap: unknown wxDSBStates value") );
            result = FALSE;
    }

    return result;
}

#endif // wxUSE_OWNER_DRAWN
