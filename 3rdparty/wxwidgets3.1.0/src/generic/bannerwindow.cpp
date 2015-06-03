///////////////////////////////////////////////////////////////////////////////
// Name:        wx/bannerwindow.h
// Purpose:     wxBannerWindow class implementation
// Author:      Vadim Zeitlin
// Created:     2011-08-16
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_BANNERWINDOW

#include "wx/bannerwindow.h"

#ifndef WX_PRECOMP
    #include "wx/bitmap.h"
    #include "wx/colour.h"
#endif

#include "wx/dcbuffer.h"

namespace
{

// Some constants for banner layout, currently they're hard coded but we could
// easily make them configurable if needed later.
const int MARGIN_X = 5;
const int MARGIN_Y = 5;

} // anonymous namespace

const char wxBannerWindowNameStr[] = "bannerwindow";

wxBEGIN_EVENT_TABLE(wxBannerWindow, wxWindow)
    EVT_SIZE(wxBannerWindow::OnSize)
    EVT_PAINT(wxBannerWindow::OnPaint)
wxEND_EVENT_TABLE()

void wxBannerWindow::Init()
{
    m_direction = wxLEFT;

    m_colStart = *wxWHITE;
    m_colEnd = *wxBLUE;
}

bool
wxBannerWindow::Create(wxWindow* parent,
                       wxWindowID winid,
                       wxDirection dir,
                       const wxPoint& pos,
                       const wxSize& size,
                       long style,
                       const wxString& name)
{
    if ( !wxWindow::Create(parent, winid, pos, size, style, name) )
        return false;

    wxASSERT_MSG
    (
        dir == wxLEFT || dir == wxRIGHT || dir == wxTOP || dir == wxBOTTOM,
        wxS("Invalid banner direction")
    );

    m_direction = dir;

    SetBackgroundStyle(wxBG_STYLE_PAINT);

    return true;
}

void wxBannerWindow::SetBitmap(const wxBitmap& bmp)
{
    m_bitmap = bmp;

    m_colBitmapBg = wxColour();

    InvalidateBestSize();

    Refresh();
}

void wxBannerWindow::SetText(const wxString& title, const wxString& message)
{
    m_title = title;
    m_message = message;

    InvalidateBestSize();

    Refresh();
}

void wxBannerWindow::SetGradient(const wxColour& start, const wxColour& end)
{
    m_colStart = start;
    m_colEnd = end;

    Refresh();
}

wxFont wxBannerWindow::GetTitleFont() const
{
    wxFont font = GetFont();
    font.MakeBold().MakeLarger();
    return font;
}

wxSize wxBannerWindow::DoGetBestClientSize() const
{
    if ( m_bitmap.IsOk() )
    {
        return m_bitmap.GetSize();
    }
    else
    {
        wxClientDC dc(const_cast<wxBannerWindow *>(this));
        const wxSize sizeText = dc.GetMultiLineTextExtent(m_message);

        dc.SetFont(GetTitleFont());

        const wxSize sizeTitle = dc.GetTextExtent(m_title);

        wxSize sizeWin(wxMax(sizeTitle.x, sizeText.x), sizeTitle.y + sizeText.y);

        // If we draw the text vertically width and height are swapped.
        if ( m_direction == wxLEFT || m_direction == wxRIGHT )
            wxSwap(sizeWin.x, sizeWin.y);

        sizeWin += 2*wxSize(MARGIN_X, MARGIN_Y);

        return sizeWin;
    }
}

void wxBannerWindow::OnSize(wxSizeEvent& event)
{
    Refresh();

    event.Skip();
}

void wxBannerWindow::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    if ( m_bitmap.IsOk() && m_title.empty() && m_message.empty() )
    {
        // No need for buffering in this case.
        wxPaintDC dc(this);

        DrawBitmapBackground(dc);
    }
    else // We need to compose our contents ourselves.
    {
        wxAutoBufferedPaintDC dc(this);

        // Deal with the background first.
        if ( m_bitmap.IsOk() )
        {
            DrawBitmapBackground(dc);
        }
        else // Draw gradient background.
        {
            wxDirection gradientDir;
            if ( m_direction == wxLEFT )
            {
                gradientDir = wxTOP;
            }
            else if ( m_direction == wxRIGHT )
            {
                gradientDir = wxBOTTOM;
            }
            else // For both wxTOP and wxBOTTOM.
            {
                gradientDir = wxRIGHT;
            }

            dc.GradientFillLinear(GetClientRect(), m_colStart, m_colEnd,
                                  gradientDir);
        }

        // Now draw the text on top of it.
        dc.SetFont(GetTitleFont());

        wxPoint pos(MARGIN_X, MARGIN_Y);
        DrawBannerTextLine(dc, m_title, pos);
        pos.y += dc.GetTextExtent(m_title).y;

        dc.SetFont(GetFont());

        wxArrayString lines = wxSplit(m_message, '\n', '\0');
        const unsigned numLines = lines.size();
        for ( unsigned n = 0; n < numLines; n++ )
        {
            const wxString& line = lines[n];

            DrawBannerTextLine(dc, line, pos);
            pos.y += dc.GetTextExtent(line).y;
        }
    }
}

wxColour wxBannerWindow::GetBitmapBg()
{
    if ( m_colBitmapBg.IsOk() )
        return m_colBitmapBg;

    // Determine the colour to use to extend the bitmap. It's the colour of the
    // bitmap pixels at the edge closest to the area where it can be extended.
    wxImage image(m_bitmap.ConvertToImage());

    // The point we get the colour from. The choice is arbitrary and in general
    // the bitmap should have the same colour on the entire edge of this point
    // for extending it to look good.
    wxPoint p;

    wxSize size = image.GetSize();
    size.x--;
    size.y--;

    switch ( m_direction )
    {
        case wxTOP:
        case wxBOTTOM:
            // The bitmap will be extended to the right.
            p.x = size.x;
            p.y = 0;
            break;

        case wxLEFT:
            // The bitmap will be extended from the top.
            p.x = 0;
            p.y = 0;
            break;

        case wxRIGHT:
            // The bitmap will be extended to the bottom.
            p.x = 0;
            p.y = size.y;
            break;

        // This case is there only to prevent g++ warnings about not handling
        // some enum elements in the switch, it can't really happen.
        case wxALL:
            wxFAIL_MSG( wxS("Unreachable") );
    }

    m_colBitmapBg.Set(image.GetRed(p.x, p.y),
                      image.GetGreen(p.x, p.y),
                      image.GetBlue(p.x, p.y));

    return m_colBitmapBg;
}

void wxBannerWindow::DrawBitmapBackground(wxDC& dc)
{
    // We may need to fill the part of the background not covered by the bitmap
    // with the solid colour extending the bitmap, this rectangle will hold the
    // area to be filled (which could be empty if the bitmap is big enough).
    wxRect rectSolid;

    const wxSize size = GetClientSize();

    switch ( m_direction )
    {
        case wxTOP:
        case wxBOTTOM:
            // Draw the bitmap at the origin, its rightmost could be truncated,
            // as it's meant to be.
            dc.DrawBitmap(m_bitmap, 0, 0);

            rectSolid.x = m_bitmap.GetWidth();
            rectSolid.width = size.x - rectSolid.x;
            rectSolid.height = size.y;
            break;

        case wxLEFT:
            // The top most part of the bitmap may be truncated but its bottom
            // must be always visible so intentionally draw it possibly partly
            // outside of the window.
            rectSolid.width = size.x;
            rectSolid.height = size.y - m_bitmap.GetHeight();
            dc.DrawBitmap(m_bitmap, 0, rectSolid.height);
            break;

        case wxRIGHT:
            // Draw the bitmap at the origin, possibly truncating its
            // bottommost part.
            dc.DrawBitmap(m_bitmap, 0, 0);

            rectSolid.y = m_bitmap.GetHeight();
            rectSolid.height = size.y - rectSolid.y;
            rectSolid.width = size.x;
            break;

        // This case is there only to prevent g++ warnings about not handling
        // some enum elements in the switch, it can't really happen.
        case wxALL:
            wxFAIL_MSG( wxS("Unreachable") );
    }

    if ( rectSolid.width > 0 && rectSolid.height > 0 )
    {
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(GetBitmapBg());
        dc.DrawRectangle(rectSolid);
    }
}

void
wxBannerWindow::DrawBannerTextLine(wxDC& dc,
                                   const wxString& str,
                                   const wxPoint& pos)
{
    switch ( m_direction )
    {
        case wxTOP:
        case wxBOTTOM:
            // The simple case: we just draw the text normally.
            dc.DrawText(str, pos);
            break;

        case wxLEFT:
            // We draw the text vertically and start from the lower left
            // corner and not the upper left one as usual.
            dc.DrawRotatedText(str, pos.y, GetClientSize().y - pos.x, 90);
            break;

        case wxRIGHT:
            // We also draw the text vertically but now we start from the upper
            // right corner and draw it from top to bottom.
            dc.DrawRotatedText(str, GetClientSize().x - pos.y, pos.x, -90);
            break;

        case wxALL:
            wxFAIL_MSG( wxS("Unreachable") );
    }
}

#endif // wxUSE_BANNERWINDOW
