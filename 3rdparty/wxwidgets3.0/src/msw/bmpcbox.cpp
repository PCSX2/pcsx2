/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/bmpcbox.cpp
// Purpose:     wxBitmapComboBox
// Author:      Jaakko Salli
// Created:     2008-04-06
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

#if wxUSE_BITMAPCOMBOBOX

#include "wx/bmpcbox.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
#endif

#include "wx/settings.h"
#include "wx/vector.h"

#include "wx/msw/dcclient.h"
#include "wx/msw/private.h"

// For wxODCB_XXX flags
#include "wx/odcombo.h"


#define IMAGE_SPACING_CTRL_VERTICAL 7  // Spacing used in control size calculation


// ============================================================================
// implementation
// ============================================================================


BEGIN_EVENT_TABLE(wxBitmapComboBox, wxComboBox)
    EVT_SIZE(wxBitmapComboBox::OnSize)
END_EVENT_TABLE()


IMPLEMENT_DYNAMIC_CLASS(wxBitmapComboBox, wxComboBox)


// ----------------------------------------------------------------------------
// wxBitmapComboBox creation
// ----------------------------------------------------------------------------

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
    : wxComboBox(),
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
    wxCArrayString chs(choices);
    return Create(parent, id, value, pos, size, chs.GetCount(),
                  chs.GetStrings(), style, validator, name);
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
    if ( !wxComboBox::Create(parent, id, value, pos, size,
                             n, choices, style, validator, name) )
        return false;

    UpdateInternals();

    return true;
}

WXDWORD wxBitmapComboBox::MSWGetStyle(long style, WXDWORD *exstyle) const
{
    return wxComboBox::MSWGetStyle(style, exstyle) | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
}

void wxBitmapComboBox::RecreateControl()
{
    //
    // Recreate control so that WM_MEASUREITEM gets called again.
    // Can't use CBS_OWNERDRAWVARIABLE because it has odd
    // mouse-wheel behaviour.
    //
    wxString value = GetValue();
    wxPoint pos = GetPosition();
    wxSize size = GetSize();
    size.y = GetBestSize().y;
    const wxArrayString strings = GetStrings();
    const unsigned numItems = strings.size();
    unsigned i;

    // Save the client data pointers before clearing the control, if any.
    const wxClientDataType clientDataType = GetClientDataType();
    wxVector<wxClientData*> objectClientData;
    wxVector<void*> voidClientData;
    switch ( clientDataType )
    {
        case wxClientData_None:
            break;

        case wxClientData_Object:
            objectClientData.reserve(numItems);
            for ( i = 0; i < numItems; ++i )
                objectClientData.push_back(GetClientObject(i));
            break;

        case wxClientData_Void:
            voidClientData.reserve(numItems);
            for ( i = 0; i < numItems; ++i )
                voidClientData.push_back(GetClientData(i));
            break;
    }

    wxComboBox::DoClear();

    HWND hwnd = GetHwnd();
    DissociateHandle();
    ::DestroyWindow(hwnd);

    if ( !MSWCreateControl(wxT("COMBOBOX"), wxEmptyString, pos, size) )
        return;

    // initialize the controls contents
    for ( i = 0; i < numItems; i++ )
    {
        wxComboBox::Append(strings[i]);

        if ( !objectClientData.empty() )
            SetClientObject(i, objectClientData[i]);
        else if ( !voidClientData.empty() )
            SetClientData(i, voidClientData[i]);
    }

    // and make sure it has the same attributes as before
    if ( m_hasFont )
    {
        // calling SetFont(m_font) would do nothing as the code would
        // notice that the font didn't change, so force it to believe
        // that it did
        wxFont font = m_font;
        m_font = wxNullFont;
        SetFont(font);
    }

    if ( m_hasFgCol )
    {
        wxColour colFg = m_foregroundColour;
        m_foregroundColour = wxNullColour;
        SetForegroundColour(colFg);
    }

    if ( m_hasBgCol )
    {
        wxColour colBg = m_backgroundColour;
        m_backgroundColour = wxNullColour;
        SetBackgroundColour(colBg);
    }
    else
    {
        SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
    }

    ::SendMessage(GetHwnd(), CB_SETITEMHEIGHT, 0, MeasureItem(0));

    // Revert the old string value
    if ( !HasFlag(wxCB_READONLY) )
        ChangeValue(value);
}

wxBitmapComboBox::~wxBitmapComboBox()
{
    Clear();
}

wxSize wxBitmapComboBox::DoGetBestSize() const
{
    wxSize best = wxComboBox::DoGetBestSize();
    wxSize bitmapSize = GetBitmapSize();

    wxCoord useHeightBitmap = EDIT_HEIGHT_FROM_CHAR_HEIGHT(bitmapSize.y);
    if ( best.y < useHeightBitmap )
    {
        best.y = useHeightBitmap;
        CacheBestSize(best);
    }
    return best;
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

int wxBitmapComboBox::Append(const wxString& item, const wxBitmap& bitmap)
{
    OnAddBitmap(bitmap);
    const int n = wxComboBox::Append(item);
    if ( n != wxNOT_FOUND )
        DoSetItemBitmap(n, bitmap);
    return n;
}

int wxBitmapComboBox::Append(const wxString& item, const wxBitmap& bitmap,
                             void *clientData)
{
    OnAddBitmap(bitmap);
    const int n = wxComboBox::Append(item, clientData);
    if ( n != wxNOT_FOUND )
        DoSetItemBitmap(n, bitmap);
    return n;
}

int wxBitmapComboBox::Append(const wxString& item, const wxBitmap& bitmap,
                             wxClientData *clientData)
{
    OnAddBitmap(bitmap);
    const int n = wxComboBox::Append(item, clientData);
    if ( n != wxNOT_FOUND )
        DoSetItemBitmap(n, bitmap);
    return n;
}

int wxBitmapComboBox::Insert(const wxString& item,
                             const wxBitmap& bitmap,
                             unsigned int pos)
{
    OnAddBitmap(bitmap);
    const int n = wxComboBox::Insert(item, pos);
    if ( n != wxNOT_FOUND )
        DoSetItemBitmap(n, bitmap);
    return n;
}

int wxBitmapComboBox::Insert(const wxString& item, const wxBitmap& bitmap,
                             unsigned int pos, void *clientData)
{
    OnAddBitmap(bitmap);
    const int n = wxComboBox::Insert(item, pos, clientData);
    if ( n != wxNOT_FOUND )
        DoSetItemBitmap(n, bitmap);
    return n;
}

int wxBitmapComboBox::Insert(const wxString& item, const wxBitmap& bitmap,
                             unsigned int pos, wxClientData *clientData)
{
    OnAddBitmap(bitmap);
    const int n = wxComboBox::Insert(item, pos, clientData);
    if ( n != wxNOT_FOUND )
        DoSetItemBitmap(n, bitmap);
    return n;
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

    const int index = wxComboBox::DoInsertItems(items, pos,
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

bool wxBitmapComboBox::OnAddBitmap(const wxBitmap& bitmap)
{
    if ( wxBitmapComboBoxBase::OnAddBitmap(bitmap) )
    {
        // Need to recreate control for a new measureitem call?
        int prevItemHeight = ::SendMessage(GetHwnd(), CB_GETITEMHEIGHT, 0, 0);

        if ( prevItemHeight != MeasureItem(0) )
            RecreateControl();

        return true;
    }

    return false;
}

void wxBitmapComboBox::DoClear()
{
    wxComboBox::DoClear();
    wxBitmapComboBoxBase::BCBDoClear();
}

void wxBitmapComboBox::DoDeleteOneItem(unsigned int n)
{
    wxComboBox::DoDeleteOneItem(n);
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

// ----------------------------------------------------------------------------
// wxBitmapComboBox miscellaneous
// ----------------------------------------------------------------------------

bool wxBitmapComboBox::SetFont(const wxFont& font)
{
    bool res = wxComboBox::SetFont(font);
    UpdateInternals();
    return res;
}

// ----------------------------------------------------------------------------
// wxBitmapComboBox item drawing and measuring
// ----------------------------------------------------------------------------

bool wxBitmapComboBox::MSWOnDraw(WXDRAWITEMSTRUCT *item)
{
    LPDRAWITEMSTRUCT lpDrawItem = (LPDRAWITEMSTRUCT) item;
    int pos = lpDrawItem->itemID;

    // Draw default for item -1, which means 'focus rect only'
    if ( pos == -1 )
        return FALSE;

    int flags = 0;
    if ( lpDrawItem->itemState & ODS_COMBOBOXEDIT )
        flags |= wxODCB_PAINTING_CONTROL;
    if ( lpDrawItem->itemState & ODS_SELECTED )
        flags |= wxODCB_PAINTING_SELECTED;

    wxString text;

    if ( flags & wxODCB_PAINTING_CONTROL )
    {
        text = GetValue();
        if ( !HasFlag(wxCB_READONLY) )
            text.clear();
    }
    else
    {
        text = GetString(pos);
    }

    wxPaintDCEx dc(this, lpDrawItem->hDC);
    wxRect rect = wxRectFromRECT(lpDrawItem->rcItem);
    wxBitmapComboBoxBase::DrawBackground(dc, rect, pos, flags);
    wxBitmapComboBoxBase::DrawItem(dc, rect, pos, text, flags);

    // If the item has the focus, draw focus rectangle.
    // Commented out since regular combo box doesn't
    // seem to do it either.
    //if ( lpDrawItem->itemState & ODS_FOCUS )
    //    DrawFocusRect(lpDrawItem->hDC, &lpDrawItem->rcItem);

    return true;
}

bool wxBitmapComboBox::MSWOnMeasure(WXMEASUREITEMSTRUCT *item)
{
    LPMEASUREITEMSTRUCT lpMeasureItem = (LPMEASUREITEMSTRUCT) item;
    int pos = lpMeasureItem->itemID;

    // Measure edit field height if item list is not empty,
    // otherwise leave default system value.
    if ( m_usedImgSize.y >= 0 || pos >= 0 )
    {
        lpMeasureItem->itemHeight = wxBitmapComboBoxBase::MeasureItem(pos);
    }

    return true;
}

#endif // wxUSE_BITMAPCOMBOBOX
