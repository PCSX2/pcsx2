/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/bmpcboxcmn.cpp
// Purpose:     wxBitmapComboBox
// Author:      Jaakko Salli
// Created:     2008-04-09
// Copyright:   (c) 2008 Jaakko Salli
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/bmpcbox.h"

#if wxUSE_BITMAPCOMBOBOX

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/dc.h"
    #include "wx/ctrlsub.h"
#endif

#include "wx/settings.h"

// For wxODCB_XXX flags
#include "wx/odcombo.h"


const char wxBitmapComboBoxNameStr[] = "bitmapComboBox";

#if defined(wxBITMAPCOMBOBOX_OWNERDRAWN_BASED)

#define IMAGE_SPACING_RIGHT         4  // Space left of image

#define IMAGE_SPACING_LEFT          4  // Space right of image, left of text

#define EXTRA_FONT_HEIGHT           0  // Add to increase min. height of list items

#define wxBCB_DEFAULT_ITEM_HEIGHT  13


// This macros allows wxArrayPtrVoid to be used in more convenient manner
#define GetBitmapPtr(n)     ((wxBitmap*)m_bitmaps[n])


// ----------------------------------------------------------------------------
// Initialization
// ----------------------------------------------------------------------------

void wxBitmapComboBoxBase::Init()
{
    m_fontHeight = 0;
    m_imgAreaWidth = 0;
    m_indent = 0;
    m_usedImgSize = wxSize(-1, -1);
}

void wxBitmapComboBoxBase::UpdateInternals()
{
    m_fontHeight = GetControl()->GetCharHeight() + EXTRA_FONT_HEIGHT;

    while ( m_bitmaps.GetCount() < GetItemContainer()->GetCount() )
        m_bitmaps.Add( new wxBitmap() );
}

// ----------------------------------------------------------------------------
// Item manipulation
// ----------------------------------------------------------------------------

void wxBitmapComboBoxBase::DoSetItemBitmap(unsigned int n, const wxBitmap& bitmap)
{
    wxCHECK_RET( n < m_bitmaps.size(), "invalid item index" );
    *GetBitmapPtr(n) = bitmap;
}

wxBitmap wxBitmapComboBoxBase::GetItemBitmap(unsigned int n) const
{
    wxCHECK_MSG( n < m_bitmaps.size(), wxNullBitmap, "invalid item index" );
    return *GetBitmapPtr(n);
}

// ----------------------------------------------------------------------------
// wxItemContainer methods
// ----------------------------------------------------------------------------

void wxBitmapComboBoxBase::BCBDoClear()
{
    for ( unsigned i = 0; i < m_bitmaps.size(); i++ )
        delete GetBitmapPtr(i);

    m_bitmaps.Empty();

    m_usedImgSize.x = -1;
    m_usedImgSize.y = -1;

    DetermineIndent();
}

void wxBitmapComboBoxBase::BCBDoDeleteOneItem(unsigned int n)
{
    delete GetBitmapPtr(n);
    m_bitmaps.RemoveAt(n);
}

// ----------------------------------------------------------------------------
// Preparation and Calculations
// ----------------------------------------------------------------------------

bool wxBitmapComboBoxBase::OnAddBitmap(const wxBitmap& bitmap)
{
    if ( bitmap.IsOk() )
    {
        int width = bitmap.GetWidth();
        int height = bitmap.GetHeight();

        if ( m_usedImgSize.x < 0 )
        {
            // If size not yet determined, get it from this image.
            m_usedImgSize.x = width;
            m_usedImgSize.y = height;

            // Adjust control size to vertically fit the bitmap
            wxWindow* ctrl = GetControl();
            ctrl->InvalidateBestSize();
            wxSize newSz = ctrl->GetBestSize();
            wxSize sz = ctrl->GetSize();
            if ( newSz.y > sz.y )
                ctrl->SetSize(sz.x, newSz.y);
            else
                DetermineIndent();
        }

        wxCHECK_MSG( width == m_usedImgSize.x && height == m_usedImgSize.y,
                     false,
                     "you can only add images of same size" );

        return true;
    }

    return false;
}

int wxBitmapComboBoxBase::DetermineIndent()
{
    // Recalculate amount of empty space needed in front of
    // text in control itself.
    int indent = m_imgAreaWidth = 0;

    if ( m_usedImgSize.x > 0 )
    {
        indent = m_usedImgSize.x + IMAGE_SPACING_LEFT + IMAGE_SPACING_RIGHT;
        m_imgAreaWidth = indent;

        indent -= 3;
    }

    return indent;
}

// ----------------------------------------------------------------------------
// Item drawing and measuring
// ----------------------------------------------------------------------------

void wxBitmapComboBoxBase::DrawBackground(wxDC& dc,
                                          const wxRect& rect,
                                          int WXUNUSED(item),
                                          int flags) const
{
    if ( flags & wxODCB_PAINTING_SELECTED )
    {
        const int vSizeDec = 0;  // Vertical size reduction of selection rectangle edges

        dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT));

        wxColour selCol = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT);
        dc.SetPen(selCol);
        dc.SetBrush(selCol);
        dc.DrawRectangle(rect.x,
                         rect.y+vSizeDec,
                         rect.width,
                         rect.height-(vSizeDec*2));
    }
    else
    {
        dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));

        wxColour selCol = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
        dc.SetPen(selCol);
        dc.SetBrush(selCol);
        dc.DrawRectangle(rect);
    }
}

void wxBitmapComboBoxBase::DrawItem(wxDC& dc,
                                    const wxRect& rect,
                                    int item,
                                    const wxString& text,
                                    int WXUNUSED(flags)) const
{
    const wxBitmap& bmp = *GetBitmapPtr(item);
    if ( bmp.IsOk() )
    {
        wxCoord w = bmp.GetWidth();
        wxCoord h = bmp.GetHeight();

        // Draw the image centered
        dc.DrawBitmap(bmp,
                      rect.x + (m_usedImgSize.x-w)/2 + IMAGE_SPACING_LEFT,
                      rect.y + (rect.height-h)/2,
                      true);
    }

    if ( !text.empty() )
        dc.DrawText(text,
                    rect.x + m_imgAreaWidth + 1,
                    rect.y + (rect.height-dc.GetCharHeight())/2);
}

wxCoord wxBitmapComboBoxBase::MeasureItem(size_t WXUNUSED(item)) const
{
    if ( m_usedImgSize.y >= 0 )
    {
        int imgHeightArea = m_usedImgSize.y + 2;
        return imgHeightArea > m_fontHeight ? imgHeightArea : m_fontHeight;
    }

    return wxBCB_DEFAULT_ITEM_HEIGHT;
}

#endif // wxBITMAPCOMBOBOX_OWNERDRAWN_BASED

#endif // wxUSE_BITMAPCOMBOBOX
