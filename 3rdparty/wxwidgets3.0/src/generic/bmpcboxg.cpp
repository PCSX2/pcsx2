/////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/bmpcboxg.cpp
// Purpose:     wxBitmapComboBox
// Author:      Jaakko Salli
// Modified by:
// Created:     Aug-31-2006
// Copyright:   (c) 2005 Jaakko Salli
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

#if wxUSE_BITMAPCOMBOBOX

#include "wx/bmpcbox.h"

#if defined(wxGENERIC_BITMAPCOMBOBOX)

#ifndef WX_PRECOMP
    #include "wx/log.h"
#endif

#include "wx/odcombo.h"
#include "wx/settings.h"
#include "wx/dc.h"

#if wxUSE_IMAGE
    #include "wx/image.h"
#endif


#define IMAGE_SPACING_CTRL_VERTICAL 7  // Spacing used in control size calculation


// ============================================================================
// implementation
// ============================================================================


BEGIN_EVENT_TABLE(wxBitmapComboBox, wxOwnerDrawnComboBox)
    EVT_SIZE(wxBitmapComboBox::OnSize)
END_EVENT_TABLE()


IMPLEMENT_DYNAMIC_CLASS(wxBitmapComboBox, wxOwnerDrawnComboBox)

void wxBitmapComboBox::Init()
{
    m_inResize = false;
}

wxBitmapComboBox::wxBitmapComboBox(wxWindow *parent,
                                  wxWindowID id,
                                  const wxString& value,
                                  const wxPoint& pos,
                                  const wxSize& size,
                                  const wxArrayString& choices,
                                  long style,
                                  const wxValidator& validator,
                                  const wxString& name)
    : wxOwnerDrawnComboBox(),
      wxBitmapComboBoxBase()
{
    Init();

    Create(parent,id,value,pos,size,choices,style,validator,name);
}

bool wxBitmapComboBox::Create(wxWindow *parent,
                              wxWindowID id,
                              const wxString& value,
                              const wxPoint& pos,
                              const wxSize& size,
                              const wxArrayString& choices,
                              long style,
                              const wxValidator& validator,
                              const wxString& name)
{
    if ( !wxOwnerDrawnComboBox::Create(parent, id, value,
                                       pos, size,
                                       choices, style,
                                       validator, name) )
    {
        return false;
    }

    UpdateInternals();

    return true;
}

bool wxBitmapComboBox::Create(wxWindow *parent,
                              wxWindowID id,
                              const wxString& value,
                              const wxPoint& pos,
                              const wxSize& size,
                              int n,
                              const wxString choices[],
                              long style,
                              const wxValidator& validator,
                              const wxString& name)
{
    if ( !wxOwnerDrawnComboBox::Create(parent, id, value,
                                       pos, size, n,
                                       choices, style,
                                       validator, name) )
    {
        return false;
    }

    UpdateInternals();

    return true;
}

wxBitmapComboBox::~wxBitmapComboBox()
{
    DoClear();
}

// ----------------------------------------------------------------------------
// Item manipulation
// ----------------------------------------------------------------------------

void wxBitmapComboBox::SetItemBitmap(unsigned int n, const wxBitmap& bitmap)
{
    OnAddBitmap(bitmap);
    DoSetItemBitmap(n, bitmap);

    if ( (int)n == GetSelection() )
        Refresh();
}

int wxBitmapComboBox::DoInsertItems(const wxArrayStringsAdapter & items,
                                    unsigned int pos,
                                    void **clientData, wxClientDataType type)
{
    const unsigned int numItems = items.GetCount();
    const unsigned int countNew = GetCount() + numItems;

    wxASSERT( numItems == 1 || !HasFlag(wxCB_SORT) );  // Sanity check

    m_bitmaps.Alloc(countNew);

    for ( unsigned int i = 0; i < numItems; i++ )
    {
        m_bitmaps.Insert(new wxBitmap(wxNullBitmap), pos + i);
    }

    const int index = wxOwnerDrawnComboBox::DoInsertItems(items, pos,
                                                          clientData, type);

    if ( index == wxNOT_FOUND )
    {
        for ( int i = numItems-1; i >= 0; i-- )
            BCBDoDeleteOneItem(pos + i);
    }
    else if ( ((unsigned int)index) != pos )
    {
        // Move pre-inserted empty bitmap into correct position
        // (usually happens when combo box has wxCB_SORT style)
        wxBitmap* bmp = static_cast<wxBitmap*>(m_bitmaps[pos]);
        m_bitmaps.RemoveAt(pos);
        m_bitmaps.Insert(bmp, index);
    }

    return index;
}

int wxBitmapComboBox::Append(const wxString& item, const wxBitmap& bitmap)
{
    const int n = wxOwnerDrawnComboBox::Append(item);
    if(n != wxNOT_FOUND)
        SetItemBitmap(n, bitmap);
    return n;
}

int wxBitmapComboBox::Append(const wxString& item, const wxBitmap& bitmap,
                             void *clientData)
{
    const int n = wxOwnerDrawnComboBox::Append(item, clientData);
    if(n != wxNOT_FOUND)
        SetItemBitmap(n, bitmap);
    return n;
}

int wxBitmapComboBox::Append(const wxString& item, const wxBitmap& bitmap,
                             wxClientData *clientData)
{
    const int n = wxOwnerDrawnComboBox::Append(item, clientData);
    if(n != wxNOT_FOUND)
        SetItemBitmap(n, bitmap);
    return n;
}

int wxBitmapComboBox::Insert(const wxString& item,
                             const wxBitmap& bitmap,
                             unsigned int pos)
{
    const int n = wxOwnerDrawnComboBox::Insert(item, pos);
    if(n != wxNOT_FOUND)
        SetItemBitmap(n, bitmap);
    return n;
}

int wxBitmapComboBox::Insert(const wxString& item, const wxBitmap& bitmap,
                             unsigned int pos, void *clientData)
{
    const int n = wxOwnerDrawnComboBox::Insert(item, pos, clientData);
    if(n != wxNOT_FOUND)
        SetItemBitmap(n, bitmap);
    return n;
}

int wxBitmapComboBox::Insert(const wxString& item, const wxBitmap& bitmap,
                             unsigned int pos, wxClientData *clientData)
{
    const int n = wxOwnerDrawnComboBox::Insert(item, pos, clientData);
    if(n != wxNOT_FOUND)
        SetItemBitmap(n, bitmap);
    return n;
}

void wxBitmapComboBox::DoClear()
{
    wxOwnerDrawnComboBox::DoClear();
    wxBitmapComboBoxBase::BCBDoClear();
}

void wxBitmapComboBox::DoDeleteOneItem(unsigned int n)
{
    wxOwnerDrawnComboBox::DoDeleteOneItem(n);
    wxBitmapComboBoxBase::BCBDoDeleteOneItem(n);
}

// ----------------------------------------------------------------------------
// wxBitmapComboBox event handlers and such
// ----------------------------------------------------------------------------

void wxBitmapComboBox::OnSize(wxSizeEvent& event)
{
    // Prevent infinite looping
    if ( !m_inResize )
    {
        m_inResize = true;
        DetermineIndent();
        m_inResize = false;
    }

    event.Skip();
}

wxSize wxBitmapComboBox::DoGetBestSize() const
{
    wxSize sz = wxOwnerDrawnComboBox::DoGetBestSize();

    if ( HasFlag(wxCB_READONLY) )
    {
        // Scale control to match height of highest image.
        int h2 = m_usedImgSize.y + IMAGE_SPACING_CTRL_VERTICAL;

        if ( h2 > sz.y )
            sz.y = h2;

        CacheBestSize(sz);
    }

    return sz;
}

// ----------------------------------------------------------------------------
// wxBitmapComboBox miscellaneous
// ----------------------------------------------------------------------------

bool wxBitmapComboBox::SetFont(const wxFont& font)
{
    bool res = wxOwnerDrawnComboBox::SetFont(font);
    UpdateInternals();
    return res;
}

// ----------------------------------------------------------------------------
// wxBitmapComboBox item drawing and measuring
// ----------------------------------------------------------------------------

void wxBitmapComboBox::OnDrawBackground(wxDC& dc,
                                        const wxRect& rect,
                                        int item,
                                        int flags) const
{
    if ( GetCustomPaintWidth() == 0 ||
         !(flags & wxODCB_PAINTING_SELECTED) ||
         item < 0 ||
         ( (flags & wxODCB_PAINTING_CONTROL) && (GetInternalFlags() & wxCC_FULL_BUTTON)) )
    {
        wxOwnerDrawnComboBox::OnDrawBackground(dc, rect, item, flags);
        return;
    }

    wxBitmapComboBoxBase::DrawBackground(dc, rect, item, flags);
}

void wxBitmapComboBox::OnDrawItem(wxDC& dc,
                                 const wxRect& rect,
                                 int item,
                                 int flags) const
{
    wxString text;
    int imgAreaWidth = m_imgAreaWidth;

    if ( imgAreaWidth == 0 )
    {
        wxOwnerDrawnComboBox::OnDrawItem(dc, rect, item, flags);
        return;
    }

    if ( flags & wxODCB_PAINTING_CONTROL )
    {
        text = GetValue();
        if ( !HasFlag(wxCB_READONLY) )
            text.clear();
    }
    else
    {
        text = GetString(item);
    }

    wxBitmapComboBoxBase::DrawItem(dc, rect, item, text, flags);
}

wxCoord wxBitmapComboBox::OnMeasureItem(size_t item) const
{
    return wxBitmapComboBoxBase::MeasureItem(item);
}

wxCoord wxBitmapComboBox::OnMeasureItemWidth(size_t item) const
{
    wxCoord x, y;
    GetTextExtent(GetString(item), &x, &y, 0, 0);
    x += m_imgAreaWidth;
    return x;
}

#endif // defined(wxGENERIC_BITMAPCOMBOBOX)

#endif // wxUSE_BITMAPCOMBOBOX
