/////////////////////////////////////////////////////////////////////////////
// Name:        src/propgrid/property.cpp
// Purpose:     wxPGProperty and related support classes
// Author:      Jaakko Salli
// Modified by:
// Created:     2008-08-23
// Copyright:   (c) Jaakko Salli
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_PROPGRID

#ifndef WX_PRECOMP
    #include "wx/defs.h"
    #include "wx/object.h"
    #include "wx/hash.h"
    #include "wx/string.h"
    #include "wx/log.h"
    #include "wx/math.h"
    #include "wx/event.h"
    #include "wx/window.h"
    #include "wx/panel.h"
    #include "wx/dc.h"
    #include "wx/dcmemory.h"
    #include "wx/pen.h"
    #include "wx/brush.h"
    #include "wx/settings.h"
    #include "wx/intl.h"
#endif

#include "wx/image.h"

#include "wx/propgrid/propgrid.h"


#define PWC_CHILD_SUMMARY_LIMIT         16 // Maximum number of children summarized in a parent property's
                                           // value field.

#define PWC_CHILD_SUMMARY_CHAR_LIMIT    64 // Character limit of summary field when not editing

#if wxPG_COMPATIBILITY_1_4

// Used to establish backwards compatibility
const char* g_invalidStringContent = "@__TOTALLY_INVALID_STRING__@";

#endif

// -----------------------------------------------------------------------

static void wxPGDrawFocusRect( wxDC& dc, const wxRect& rect )
{
#if defined(__WXMSW__) && !defined(__WXWINCE__)
    // FIXME: Use DrawFocusRect code above (currently it draws solid line
    //   for caption focus but works ok for other stuff).
    //   Also, it seems that this code may not work in future wx versions.
    dc.SetLogicalFunction(wxINVERT);

    wxPen pen(*wxBLACK,1,wxDOT);
    pen.SetCap(wxCAP_BUTT);
    dc.SetPen(pen);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);

    dc.DrawRectangle(rect);

    dc.SetLogicalFunction(wxCOPY);
#else
    dc.SetLogicalFunction(wxINVERT);

    dc.SetPen(wxPen(*wxBLACK,1,wxDOT));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);

    dc.DrawRectangle(rect);

    dc.SetLogicalFunction(wxCOPY);
#endif
}

// -----------------------------------------------------------------------
// wxPGCellRenderer
// -----------------------------------------------------------------------

wxSize wxPGCellRenderer::GetImageSize( const wxPGProperty* WXUNUSED(property),
                                       int WXUNUSED(column),
                                       int WXUNUSED(item) ) const
{
     return wxSize(0, 0);
}

void wxPGCellRenderer::DrawText( wxDC& dc, const wxRect& rect,
                                 int xOffset, const wxString& text ) const
{
    dc.DrawText( text,
                 rect.x+xOffset+wxPG_XBEFORETEXT,
                 rect.y+((rect.height-dc.GetCharHeight())/2) );
}

void wxPGCellRenderer::DrawEditorValue( wxDC& dc, const wxRect& rect,
                                        int xOffset, const wxString& text,
                                        wxPGProperty* property,
                                        const wxPGEditor* editor ) const
{
    int yOffset = ((rect.height-dc.GetCharHeight())/2);

    if ( editor )
    {
        wxRect rect2(rect);
        rect2.x += xOffset;
        rect2.y += yOffset;
        rect2.height -= yOffset;
        editor->DrawValue( dc, rect2, property, text );
    }
    else
    {
        dc.DrawText( text,
                     rect.x+xOffset+wxPG_XBEFORETEXT,
                     rect.y+yOffset );
    }
}

void wxPGCellRenderer::DrawCaptionSelectionRect( wxDC& dc, int x, int y, int w, int h ) const
{
    wxRect focusRect(x,y+((h-dc.GetCharHeight())/2),w,h);
    wxPGDrawFocusRect(dc,focusRect);
}

int wxPGCellRenderer::PreDrawCell( wxDC& dc, const wxRect& rect, const wxPGCell& cell, int flags ) const
{
    int imageWidth = 0;

    // If possible, use cell colours
    if ( !(flags & DontUseCellBgCol) )
    {
        const wxColour& bgCol = cell.GetBgCol();
        dc.SetPen(bgCol);
        dc.SetBrush(bgCol);
    }

    if ( !(flags & DontUseCellFgCol) )
    {
        dc.SetTextForeground(cell.GetFgCol());
    }

    // Draw Background, but only if not rendering in control
    // (as control already has rendered correct background).
    if ( !(flags & (Control|ChoicePopup)) )
        dc.DrawRectangle(rect);

    // Use cell font, if provided
    const wxFont& font = cell.GetFont();
    if ( font.IsOk() )
        dc.SetFont(font);

    const wxBitmap& bmp = cell.GetBitmap();
    if ( bmp.IsOk() &&
        // Do not draw oversized bitmap outside choice popup
         ((flags & ChoicePopup) || bmp.GetHeight() < rect.height )
        )
    {
        dc.DrawBitmap( bmp,
                       rect.x + wxPG_CONTROL_MARGIN + wxCC_CUSTOM_IMAGE_MARGIN1,
                       rect.y + wxPG_CUSTOM_IMAGE_SPACINGY,
                       true );
        imageWidth = bmp.GetWidth();
    }

    return imageWidth;
}

void wxPGCellRenderer::PostDrawCell( wxDC& dc,
                                     const wxPropertyGrid* propGrid,
                                     const wxPGCell& cell,
                                     int WXUNUSED(flags) ) const
{
    // Revert font
    const wxFont& font = cell.GetFont();
    if ( font.IsOk() )
        dc.SetFont(propGrid->GetFont());
}

// -----------------------------------------------------------------------
// wxPGDefaultRenderer
// -----------------------------------------------------------------------

bool wxPGDefaultRenderer::Render( wxDC& dc, const wxRect& rect,
                                  const wxPropertyGrid* propertyGrid, wxPGProperty* property,
                                  int column, int item, int flags ) const
{
    const wxPGEditor* editor = NULL;
    const wxPGCell* cell = NULL;

    wxString text;
    bool isUnspecified = property->IsValueUnspecified();

    if ( column == 1 && item == -1 )
    {
        int cmnVal = property->GetCommonValue();
        if ( cmnVal >= 0 )
        {
            // Common Value
            if ( !isUnspecified )
            {
                text = propertyGrid->GetCommonValueLabel(cmnVal);
                DrawText( dc, rect, 0, text );
                if ( !text.empty() )
                    return true;
            }
            return false;
        }
    }

    int imageWidth = 0;
    int preDrawFlags = flags;
    bool res = false;

    property->GetDisplayInfo(column, item, flags, &text, &cell);

    imageWidth = PreDrawCell( dc, rect, *cell, preDrawFlags );

    if ( column == 1 )
    {
        editor = property->GetColumnEditor(column);

        if ( !isUnspecified )
        {
            // Regular property value

            wxSize imageSize = propertyGrid->GetImageSize(property, item);

            wxPGPaintData paintdata;
            paintdata.m_parent = propertyGrid;
            paintdata.m_choiceItem = item;

            if ( imageSize.x > 0 )
            {
                wxRect imageRect(rect.x + wxPG_CONTROL_MARGIN + wxCC_CUSTOM_IMAGE_MARGIN1,
                                 rect.y+wxPG_CUSTOM_IMAGE_SPACINGY,
                                 wxPG_CUSTOM_IMAGE_WIDTH,
                                 rect.height-(wxPG_CUSTOM_IMAGE_SPACINGY*2));

                dc.SetPen( wxPen(propertyGrid->GetCellTextColour(), 1, wxSOLID) );

                paintdata.m_drawnWidth = imageSize.x;
                paintdata.m_drawnHeight = imageSize.y;

                property->OnCustomPaint( dc, imageRect, paintdata );

                imageWidth = paintdata.m_drawnWidth;
            }

            text = property->GetValueAsString();

            // Add units string?
            if ( propertyGrid->GetColumnCount() <= 2 )
            {
                wxString unitsString = property->GetAttribute(wxPGGlobalVars->m_strUnits, wxEmptyString);
                if ( !unitsString.empty() )
                    text = wxString::Format(wxS("%s %s"), text.c_str(), unitsString.c_str() );
            }
        }

        if ( text.empty() )
        {
            text = property->GetHintText();
            if ( !text.empty() )
            {
                res = true;

                const wxColour& hCol =
                    propertyGrid->GetCellDisabledTextColour();
                dc.SetTextForeground(hCol);

                // Must make the editor NULL to override its own rendering
                // code.
                editor = NULL;
            }
        }
        else
        {
            res = true;
        }
    }

    int imageOffset = property->GetImageOffset(imageWidth);

    DrawEditorValue( dc, rect, imageOffset, text, property, editor );

    // active caption gets nice dotted rectangle
    if ( property->IsCategory() && column == 0 )
    {
        if ( flags & Selected )
        {
            if ( imageOffset > 0 )
            {
                imageOffset -= DEFAULT_IMAGE_OFFSET_INCREMENT;
                imageOffset += wxCC_CUSTOM_IMAGE_MARGIN2 + 4;
            }

            DrawCaptionSelectionRect( dc,
                                      rect.x+wxPG_XBEFORETEXT-wxPG_CAPRECTXMARGIN+imageOffset,
                                      rect.y-wxPG_CAPRECTYMARGIN+1,
                                      ((wxPropertyCategory*)property)->GetTextExtent(propertyGrid,
                                                                                     propertyGrid->GetCaptionFont())
                                      +(wxPG_CAPRECTXMARGIN*2),
                                      propertyGrid->GetFontHeight()+(wxPG_CAPRECTYMARGIN*2) );
        }
    }

    PostDrawCell(dc, propertyGrid, *cell, preDrawFlags);

    return res;
}

wxSize wxPGDefaultRenderer::GetImageSize( const wxPGProperty* property,
                                          int column,
                                          int item ) const
{
    if ( property && column == 1 )
    {
        if ( item == -1 )
        {
            wxBitmap* bmp = property->GetValueImage();

            if ( bmp && bmp->IsOk() )
                return wxSize(bmp->GetWidth(),bmp->GetHeight());
        }
    }
    return wxSize(0,0);
}

// -----------------------------------------------------------------------
// wxPGCellData
// -----------------------------------------------------------------------

wxPGCellData::wxPGCellData()
    : wxObjectRefData()
{
    m_hasValidText = false;
}

// -----------------------------------------------------------------------
// wxPGCell
// -----------------------------------------------------------------------

wxPGCell::wxPGCell()
    : wxObject()
{
}

wxPGCell::wxPGCell( const wxString& text,
                    const wxBitmap& bitmap,
                    const wxColour& fgCol,
                    const wxColour& bgCol )
    : wxObject()
{
    wxPGCellData* data = new wxPGCellData();
    m_refData = data;
    data->m_text = text;
    data->m_bitmap = bitmap;
    data->m_fgCol = fgCol;
    data->m_bgCol = bgCol;
    data->m_hasValidText = true;
}

wxObjectRefData *wxPGCell::CloneRefData( const wxObjectRefData *data ) const
{
    wxPGCellData* c = new wxPGCellData();
    const wxPGCellData* o = (const wxPGCellData*) data;
    c->m_text = o->m_text;
    c->m_bitmap = o->m_bitmap;
    c->m_fgCol = o->m_fgCol;
    c->m_bgCol = o->m_bgCol;
    c->m_hasValidText = o->m_hasValidText;
    return c;
}

void wxPGCell::SetText( const wxString& text )
{
    AllocExclusive();

    GetData()->SetText(text);
}

void wxPGCell::SetBitmap( const wxBitmap& bitmap )
{
    AllocExclusive();

    GetData()->SetBitmap(bitmap);
}

void wxPGCell::SetFgCol( const wxColour& col )
{
    AllocExclusive();

    GetData()->SetFgCol(col);
}

void wxPGCell::SetFont( const wxFont& font )
{
    AllocExclusive();

    GetData()->SetFont(font);
}

void wxPGCell::SetBgCol( const wxColour& col )
{
    AllocExclusive();

    GetData()->SetBgCol(col);
}

void wxPGCell::MergeFrom( const wxPGCell& srcCell )
{
    AllocExclusive();

    wxPGCellData* data = GetData();

    if ( srcCell.HasText() )
        data->SetText(srcCell.GetText());

    if ( srcCell.GetFgCol().IsOk() )
        data->SetFgCol(srcCell.GetFgCol());

    if ( srcCell.GetBgCol().IsOk() )
        data->SetBgCol(srcCell.GetBgCol());

    if ( srcCell.GetBitmap().IsOk() )
        data->SetBitmap(srcCell.GetBitmap());
}

void wxPGCell::SetEmptyData()
{
    AllocExclusive();
}


// -----------------------------------------------------------------------
// wxPGProperty
// -----------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxPGProperty, wxObject)

wxString* wxPGProperty::sm_wxPG_LABEL = NULL;

void wxPGProperty::Init()
{
    m_commonValue = -1;
    m_arrIndex = 0xFFFF;
    m_parent = NULL;

    m_parentState = NULL;

    m_clientData = NULL;
    m_clientObject = NULL;

    m_customEditor = NULL;
#if wxUSE_VALIDATORS
    m_validator = NULL;
#endif
    m_valueBitmap = NULL;

    m_maxLen = 0; // infinite maximum length

    m_flags = wxPG_PROP_PROPERTY;

    m_depth = 1;

    SetExpanded(true);
}


void wxPGProperty::Init( const wxString& label, const wxString& name )
{
    // We really need to check if &label and &name are NULL pointers
    // (this can if we are called before property grid has been initalized)

    if ( (&label) != NULL && label != wxPG_LABEL )
        m_label = label;

    if ( (&name) != NULL && name != wxPG_LABEL )
        DoSetName( name );
    else
        DoSetName( m_label );

    Init();
}

void wxPGProperty::InitAfterAdded( wxPropertyGridPageState* pageState,
                                   wxPropertyGrid* propgrid )
{
    //
    // Called after property has been added to grid or page
    // (so propgrid can be NULL, too).

    wxPGProperty* parent = m_parent;
    bool parentIsRoot = parent->IsKindOf(wxCLASSINFO(wxPGRootProperty));

    //
    // Convert invalid cells to default ones in this grid
    for ( unsigned int i=0; i<m_cells.size(); i++ )
    {
        wxPGCell& cell = m_cells[i];
        if ( cell.IsInvalid() )
        {
            const wxPGCell& propDefCell = propgrid->GetPropertyDefaultCell();
            const wxPGCell& catDefCell = propgrid->GetCategoryDefaultCell();

            if ( !HasFlag(wxPG_PROP_CATEGORY) )
                cell = propDefCell;
            else
                cell = catDefCell;
        }
    }

    m_parentState = pageState;

#if wxPG_COMPATIBILITY_1_4
    // Make sure deprecated virtual functions are not implemented
    wxString s = GetValueAsString( 0xFFFF );
    wxASSERT_MSG( s == g_invalidStringContent,
                  "Implement ValueToString() instead of GetValueAsString()" );
#endif

    if ( !parentIsRoot && !parent->IsCategory() )
    {
        m_cells = parent->m_cells;
    }

    // If in hideable adding mode, or if assigned parent is hideable, then
    // make this one hideable.
    if (
         ( !parentIsRoot && parent->HasFlag(wxPG_PROP_HIDDEN) ) ||
         ( propgrid && (propgrid->HasInternalFlag(wxPG_FL_ADDING_HIDEABLES)) )
       )
        SetFlag( wxPG_PROP_HIDDEN );

    // Set custom image flag.
    int custImgHeight = OnMeasureImage().y;
    if ( custImgHeight < 0 )
    {
        SetFlag(wxPG_PROP_CUSTOMIMAGE);
    }

    if ( propgrid && (propgrid->HasFlag(wxPG_LIMITED_EDITING)) )
        SetFlag(wxPG_PROP_NOEDITOR);

    // Make sure parent has some parental flags
    if ( !parent->HasFlag(wxPG_PROP_PARENTAL_FLAGS) )
        parent->SetParentalType(wxPG_PROP_MISC_PARENT);

    if ( !IsCategory() )
    {
        // This is not a category.

        // Depth.
        //
        unsigned char depth = 1;
        if ( !parentIsRoot )
        {
            depth = parent->m_depth;
            if ( !parent->IsCategory() )
                depth++;
        }
        m_depth = depth;
        unsigned char greyDepth = depth;

        if ( !parentIsRoot )
        {
            wxPropertyCategory* pc;

            if ( parent->IsCategory() )
                pc = (wxPropertyCategory* ) parent;
            else
                // This conditional compile is necessary to
                // bypass some compiler bug.
                pc = pageState->GetPropertyCategory(parent);

            if ( pc )
                greyDepth = pc->GetDepth();
            else
                greyDepth = parent->m_depthBgCol;
        }

        m_depthBgCol = greyDepth;
    }
    else
    {
        // This is a category.

        // depth
        unsigned char depth = 1;
        if ( !parentIsRoot )
        {
            depth = parent->m_depth + 1;
        }
        m_depth = depth;
        m_depthBgCol = depth;
    }

    //
    // Has initial children
    if ( GetChildCount() )
    {
        // Check parental flags
        wxASSERT_MSG( ((m_flags & wxPG_PROP_PARENTAL_FLAGS) ==
                            wxPG_PROP_AGGREGATE) ||
                      ((m_flags & wxPG_PROP_PARENTAL_FLAGS) ==
                            wxPG_PROP_MISC_PARENT),
                      "wxPGProperty parental flags set incorrectly at "
                      "this time" );

        if ( HasFlag(wxPG_PROP_AGGREGATE) )
        {
            // Properties with private children are not expanded by default.
            SetExpanded(false);
        }
        else if ( propgrid && propgrid->HasFlag(wxPG_HIDE_MARGIN) )
        {
            // ...unless it cannot be expanded by user and therefore must
            // remain visible at all times
            SetExpanded(true);
        }

        //
        // Prepare children recursively
        for ( unsigned int i=0; i<GetChildCount(); i++ )
        {
            wxPGProperty* child = Item(i);
            child->InitAfterAdded(pageState, pageState->GetGrid());
        }

        if ( propgrid && (propgrid->GetExtraStyle() & wxPG_EX_AUTO_UNSPECIFIED_VALUES) )
            SetFlagRecursively(wxPG_PROP_AUTO_UNSPECIFIED, true);
    }
}

void wxPGProperty::OnDetached(wxPropertyGridPageState* WXUNUSED(state),
                              wxPropertyGrid* propgrid)
{
    if ( propgrid )
    {
        const wxPGCell& propDefCell = propgrid->GetPropertyDefaultCell();
        const wxPGCell& catDefCell = propgrid->GetCategoryDefaultCell();

        // Make default cells invalid
        for ( unsigned int i=0; i<m_cells.size(); i++ )
        {
            wxPGCell& cell = m_cells[i];
            if ( cell.IsSameAs(propDefCell) ||
                 cell.IsSameAs(catDefCell) )
            {
                cell.UnRef();
            }
        }
    }
}

wxPGProperty::wxPGProperty()
    : wxObject()
{
    Init();
}


wxPGProperty::wxPGProperty( const wxString& label, const wxString& name )
    : wxObject()
{
    Init( label, name );
}


wxPGProperty::~wxPGProperty()
{
    delete m_clientObject;

    Empty();  // this deletes items

    delete m_valueBitmap;
#if wxUSE_VALIDATORS
    delete m_validator;
#endif

    // This makes it easier for us to detect dangling pointers
    m_parent = NULL;
}


bool wxPGProperty::IsSomeParent( wxPGProperty* candidate ) const
{
    wxPGProperty* parent = m_parent;
    do
    {
        if ( parent == candidate )
            return true;
        parent = parent->m_parent;
    } while ( parent );
    return false;
}

void wxPGProperty::SetName( const wxString& newName )
{
    wxPropertyGrid* pg = GetGrid();

    if ( pg )
        pg->SetPropertyName(this, newName);
    else
        DoSetName(newName);
}

wxString wxPGProperty::GetName() const
{
    wxPGProperty* parent = GetParent();

    if ( m_name.empty() || !parent || parent->IsCategory() || parent->IsRoot() )
        return m_name;

    return m_parent->GetName() + wxS(".") + m_name;
}

wxPropertyGrid* wxPGProperty::GetGrid() const
{
    if ( !m_parentState )
        return NULL;
    return m_parentState->GetGrid();
}

int wxPGProperty::Index( const wxPGProperty* p ) const
{
    return wxPGFindInVector(m_children, p);
}

bool wxPGProperty::ValidateValue( wxVariant& WXUNUSED(value), wxPGValidationInfo& WXUNUSED(validationInfo) ) const
{
    return true;
}

void wxPGProperty::OnSetValue()
{
}

void wxPGProperty::RefreshChildren ()
{
}

void wxPGProperty::OnValidationFailure( wxVariant& WXUNUSED(pendingValue) )
{
}

void wxPGProperty::GetDisplayInfo( unsigned int column,
                                   int choiceIndex,
                                   int flags,
                                   wxString* pString,
                                   const wxPGCell** pCell )
{
    const wxPGCell* cell = NULL;

    if ( !(flags & wxPGCellRenderer::ChoicePopup) )
    {
        // Not painting list of choice popups, so get text from property
        if ( column != 1 || !IsValueUnspecified() || IsCategory() )
        {
            cell = &GetCell(column);
        }
        else
        {
            // Use special unspecified value cell
            cell = &GetGrid()->GetUnspecifiedValueAppearance();
        }

        if ( cell->HasText() )
        {
            *pString = cell->GetText();
        }
        else
        {
            if ( column == 0 )
                *pString = GetLabel();
            else if ( column == 1 )
                *pString = GetDisplayedString();
            else if ( column == 2 )
                *pString = GetAttribute(wxPGGlobalVars->m_strUnits, wxEmptyString);
        }
    }
    else
    {
        wxASSERT( column == 1 );

        if ( choiceIndex != wxNOT_FOUND )
        {
            const wxPGChoiceEntry& entry = m_choices[choiceIndex];
            if ( entry.GetBitmap().IsOk() ||
                 entry.GetFgCol().IsOk() ||
                 entry.GetBgCol().IsOk() )
                cell = &entry;
            *pString = m_choices.GetLabel(choiceIndex);
        }
    }

    if ( !cell )
        cell = &GetCell(column);

    wxASSERT_MSG( cell->GetData(),
                  wxString::Format("Invalid cell for property %s",
                                   GetName().c_str()) );

    *pCell = cell;
}

/*
wxString wxPGProperty::GetColumnText( unsigned int col, int choiceIndex ) const
{

    if ( col != 1 || choiceIndex == wxNOT_FOUND )
    {
        const wxPGCell& cell = GetCell(col);
        if ( cell->HasText() )
        {
            return cell->GetText();
        }
        else
        {
            if ( col == 0 )
                return GetLabel();
            else if ( col == 1 )
                return GetDisplayedString();
            else if ( col == 2 )
                return GetAttribute(wxPGGlobalVars->m_strUnits, wxEmptyString);
        }
    }
    else
    {
        // Use choice
        return m_choices.GetLabel(choiceIndex);
    }

    return wxEmptyString;
}
*/

void wxPGProperty::DoGenerateComposedValue( wxString& text,
                                            int argFlags,
                                            const wxVariantList* valueOverrides,
                                            wxPGHashMapS2S* childResults ) const
{
    int i;
    int iMax = m_children.size();

    text.clear();
    if ( iMax == 0 )
        return;

    if ( iMax > PWC_CHILD_SUMMARY_LIMIT &&
         !(argFlags & wxPG_FULL_VALUE) )
        iMax = PWC_CHILD_SUMMARY_LIMIT;

    int iMaxMinusOne = iMax-1;

    if ( !IsTextEditable() )
        argFlags |= wxPG_UNEDITABLE_COMPOSITE_FRAGMENT;

    wxPGProperty* curChild = m_children[0];

    bool overridesLeft = false;
    wxVariant overrideValue;
    wxVariantList::const_iterator node;

    if ( valueOverrides )
    {
        node = valueOverrides->begin();
        if ( node != valueOverrides->end() )
        {
            overrideValue = *node;
            overridesLeft = true;
        }
    }

    for ( i = 0; i < iMax; i++ )
    {
        wxVariant childValue;

        wxString childLabel = curChild->GetLabel();

        // Check for value override
        if ( overridesLeft && overrideValue.GetName() == childLabel )
        {
            if ( !overrideValue.IsNull() )
                childValue = overrideValue;
            else
                childValue = curChild->GetValue();
            ++node;
            if ( node != valueOverrides->end() )
                overrideValue = *node;
            else
                overridesLeft = false;
        }
        else
        {
            childValue = curChild->GetValue();
        }

        wxString s;
        if ( !childValue.IsNull() )
        {
            if ( overridesLeft &&
                 curChild->HasFlag(wxPG_PROP_COMPOSED_VALUE) &&
                 childValue.GetType() == wxPG_VARIANT_TYPE_LIST )
            {
                wxVariantList& childList = childValue.GetList();
                DoGenerateComposedValue(s, argFlags|wxPG_COMPOSITE_FRAGMENT,
                                        &childList, childResults);
            }
            else
            {
                s = curChild->ValueToString(childValue,
                                            argFlags|wxPG_COMPOSITE_FRAGMENT);
            }
        }

        if ( childResults && curChild->GetChildCount() )
            (*childResults)[curChild->GetName()] = s;

        bool skip = false;
        if ( (argFlags & wxPG_UNEDITABLE_COMPOSITE_FRAGMENT) && s.empty() )
            skip = true;

        if ( !curChild->GetChildCount() || skip )
            text += s;
        else
            text += wxS("[") + s + wxS("]");

        if ( i < iMaxMinusOne )
        {
            if ( text.length() > PWC_CHILD_SUMMARY_CHAR_LIMIT &&
                 !(argFlags & wxPG_EDITABLE_VALUE) &&
                 !(argFlags & wxPG_FULL_VALUE) )
                break;

            if ( !skip )
            {
                if ( !curChild->GetChildCount() )
                    text += wxS("; ");
                else
                    text += wxS(" ");
            }

            curChild = m_children[i+1];
        }
    }

    if ( (unsigned int)i < m_children.size() )
    {
        if ( !text.EndsWith(wxS("; ")) )
            text += wxS("; ...");
        else
            text += wxS("...");
    }
}

wxString wxPGProperty::ValueToString( wxVariant& WXUNUSED(value),
                                      int argFlags ) const
{
    wxCHECK_MSG( GetChildCount() > 0,
                 wxString(),
                 "If user property does not have any children, it must "
                 "override GetValueAsString" );

    // FIXME: Currently code below only works if value is actually m_value
    wxASSERT_MSG( argFlags & wxPG_VALUE_IS_CURRENT,
                  "Sorry, currently default wxPGProperty::ValueToString() "
                  "implementation only works if value is m_value." );

    wxString text;
    DoGenerateComposedValue(text, argFlags);
    return text;
}

wxString wxPGProperty::GetValueAsString( int argFlags ) const
{
#if wxPG_COMPATIBILITY_1_4
    // This is backwards compatibility test
    // That is, to make sure this function is not overridden
    // (instead, ValueToString() should be).
    if ( argFlags == 0xFFFF )
    {
        // Do not override! (for backwards compliancy)
        return g_invalidStringContent;
    }
#endif

    wxPropertyGrid* pg = GetGrid();

    if ( IsValueUnspecified() )
        return pg->GetUnspecifiedValueText(argFlags);

    if ( m_commonValue == -1 )
    {
        wxVariant value(GetValue());
        return ValueToString(value, argFlags|wxPG_VALUE_IS_CURRENT);
    }

    //
    // Return common value's string representation
    const wxPGCommonValue* cv = pg->GetCommonValue(m_commonValue);

    if ( argFlags & wxPG_FULL_VALUE )
    {
        return cv->GetLabel();
    }
    else if ( argFlags & wxPG_EDITABLE_VALUE )
    {
        return cv->GetEditableText();
    }
    else
    {
        return cv->GetLabel();
    }
}

wxString wxPGProperty::GetValueString( int argFlags ) const
{
    return GetValueAsString(argFlags);
}

bool wxPGProperty::IntToValue( wxVariant& variant, int number, int WXUNUSED(argFlags) ) const
{
    variant = (long)number;
    return true;
}

// Convert semicolon delimited tokens into child values.
bool wxPGProperty::StringToValue( wxVariant& v, const wxString& text, int argFlags ) const
{
    if ( !GetChildCount() )
        return false;

    unsigned int curChild = 0;

    unsigned int iMax = m_children.size();

    if ( iMax > PWC_CHILD_SUMMARY_LIMIT &&
         !(argFlags & wxPG_FULL_VALUE) )
        iMax = PWC_CHILD_SUMMARY_LIMIT;

    bool changed = false;

    wxString token;
    size_t pos = 0;

    // Its best only to add non-empty group items
    bool addOnlyIfNotEmpty = false;
    const wxChar delimeter = wxS(';');

    size_t tokenStart = 0xFFFFFF;

    wxVariantList temp_list;
    wxVariant list(temp_list);

    int propagatedFlags = argFlags & (wxPG_REPORT_ERROR|wxPG_PROGRAMMATIC_VALUE);

    wxLogTrace("propgrid",
               wxT(">> %s.StringToValue('%s')"), GetLabel(), text);

    wxString::const_iterator it = text.begin();
    wxUniChar a;

    if ( it != text.end() )
        a = *it;
    else
        a = 0;

    for ( ;; )
    {
        // How many units we iterate string forward at the end of loop?
        // We need to keep track of this or risk going to negative
        // with it-- operation.
        unsigned int strPosIncrement = 1;

        if ( tokenStart != 0xFFFFFF )
        {
            // Token is running
            if ( a == delimeter || a == 0 )
            {
                token = text.substr(tokenStart,pos-tokenStart);
                token.Trim(true);
                size_t len = token.length();

                if ( !addOnlyIfNotEmpty || len > 0 )
                {
                    const wxPGProperty* child = Item(curChild);
                    wxVariant variant(child->GetValue());
                    wxString childName = child->GetBaseName();

                    wxLogTrace("propgrid",
                               wxT("token = '%s', child = %s"),
                               token, childName);

                    // Add only if editable or setting programmatically
                    if ( (argFlags & wxPG_PROGRAMMATIC_VALUE) ||
                         (!child->HasFlag(wxPG_PROP_DISABLED) &&
                          !child->HasFlag(wxPG_PROP_READONLY)) )
                    {
                        if ( len > 0 )
                        {
                            if ( child->StringToValue(variant, token,
                                 propagatedFlags|wxPG_COMPOSITE_FRAGMENT) )
                            {
                                // We really need to set the variant's name
                                // *after* child->StringToValue() has been
                                // called, since variant's value may be set by
                                // assigning another variant into it, which
                                // then usually causes name to be copied (ie.
                                // usually cleared) as well. wxBoolProperty
                                // being case in point with its use of
                                // wxPGVariant_Bool macro as an optimization.
                                variant.SetName(childName);
                                list.Append(variant);

                                changed = true;
                            }
                        }
                        else
                        {
                            // Empty, becomes unspecified
                            variant.MakeNull();
                            variant.SetName(childName);
                            list.Append(variant);
                            changed = true;
                        }
                    }

                    curChild++;
                    if ( curChild >= iMax )
                        break;
                }

                tokenStart = 0xFFFFFF;
            }
        }
        else
        {
            // Token is not running
            if ( a != wxS(' ') )
            {

                addOnlyIfNotEmpty = false;

                // Is this a group of tokens?
                if ( a == wxS('[') )
                {
                    int depth = 1;

                    if ( it != text.end() ) ++it;
                    pos++;
                    size_t startPos = pos;

                    // Group item - find end
                    while ( it != text.end() && depth > 0 )
                    {
                        a = *it;
                        ++it;
                        pos++;

                        if ( a == wxS(']') )
                            depth--;
                        else if ( a == wxS('[') )
                            depth++;
                    }

                    token = text.substr(startPos,pos-startPos-1);

                    if ( token.empty() )
                        break;

                    const wxPGProperty* child = Item(curChild);

                    wxVariant oldChildValue = child->GetValue();
                    wxVariant variant(oldChildValue);

                    if ( (argFlags & wxPG_PROGRAMMATIC_VALUE) ||
                         (!child->HasFlag(wxPG_PROP_DISABLED) &&
                          !child->HasFlag(wxPG_PROP_READONLY)) )
                    {
                        wxString childName = child->GetBaseName();

                        bool stvRes = child->StringToValue( variant, token,
                                                            propagatedFlags );
                        if ( stvRes || (variant != oldChildValue) )
                        {
                            variant.SetName(childName);
                            list.Append(variant);

                            changed = true;
                        }
                        else
                        {
                            // No changes...
                        }
                    }

                    curChild++;
                    if ( curChild >= iMax )
                        break;

                    addOnlyIfNotEmpty = true;

                    tokenStart = 0xFFFFFF;
                }
                else
                {
                    tokenStart = pos;

                    if ( a == delimeter )
                        strPosIncrement -= 1;
                }
            }
        }

        if ( a == 0 )
            break;

        it += strPosIncrement;

        if ( it != text.end() )
        {
            a = *it;
        }
        else
        {
            a = 0;
        }

        pos += strPosIncrement;
    }

    if ( changed )
        v = list;

    return changed;
}

bool wxPGProperty::SetValueFromString( const wxString& text, int argFlags )
{
    wxVariant variant(m_value);
    bool res = StringToValue(variant, text, argFlags);
    if ( res )
        SetValue(variant);
    return res;
}

bool wxPGProperty::SetValueFromInt( long number, int argFlags )
{
    wxVariant variant(m_value);
    bool res = IntToValue(variant, number, argFlags);
    if ( res )
        SetValue(variant);
    return res;
}

wxSize wxPGProperty::OnMeasureImage( int WXUNUSED(item) ) const
{
    if ( m_valueBitmap )
        return wxSize(m_valueBitmap->GetWidth(),-1);

    return wxSize(0,0);
}

int wxPGProperty::GetImageOffset( int imageWidth ) const
{
    int imageOffset = 0;

    if ( imageWidth )
    {
        // Do not increment offset too much for wide images
        if ( imageWidth <= (wxPG_CUSTOM_IMAGE_WIDTH+5) )
            imageOffset = imageWidth + DEFAULT_IMAGE_OFFSET_INCREMENT;
        else
            imageOffset = imageWidth + 1;
    }

    return imageOffset;
}

wxPGCellRenderer* wxPGProperty::GetCellRenderer( int WXUNUSED(column) ) const
{
    return wxPGGlobalVars->m_defaultRenderer;
}

void wxPGProperty::OnCustomPaint( wxDC& dc,
                                  const wxRect& rect,
                                  wxPGPaintData& )
{
    wxBitmap* bmp = m_valueBitmap;

    wxCHECK_RET( bmp && bmp->IsOk(), wxT("invalid bitmap") );

    wxCHECK_RET( rect.x >= 0, wxT("unexpected measure call") );

    dc.DrawBitmap(*bmp,rect.x,rect.y);
}

const wxPGEditor* wxPGProperty::DoGetEditorClass() const
{
    return wxPGEditor_TextCtrl;
}

// Default extra property event handling - that is, none at all.
bool wxPGProperty::OnEvent( wxPropertyGrid*, wxWindow*, wxEvent& )
{
    return false;
}


void wxPGProperty::SetValue( wxVariant value, wxVariant* pList, int flags )
{
    // If auto unspecified values are not wanted (via window or property style),
    // then get default value instead of wxNullVariant.
    if ( value.IsNull() && (flags & wxPG_SETVAL_BY_USER) &&
         !UsesAutoUnspecified() )
    {
        value = GetDefaultValue();
    }

    if ( !value.IsNull() )
    {
        wxVariant tempListVariant;

        SetCommonValue(-1);
        // List variants are reserved a special purpose
        // as intermediate containers for child values
        // of properties with children.
        if ( value.GetType() == wxPG_VARIANT_TYPE_LIST )
        {
            //
            // However, situation is different for composed string properties
            if ( HasFlag(wxPG_PROP_COMPOSED_VALUE) )
            {
                tempListVariant = value;
                pList = &tempListVariant;
            }

            wxVariant newValue;
            AdaptListToValue(value, &newValue);
            value = newValue;
            //wxLogDebug(wxT(">> %s.SetValue() adapted list value to type '%s'"),GetName().c_str(),value.GetType().c_str());
        }

        if ( HasFlag( wxPG_PROP_AGGREGATE) )
            flags |= wxPG_SETVAL_AGGREGATED;

        if ( pList && !pList->IsNull() )
        {
            wxASSERT( pList->GetType() == wxPG_VARIANT_TYPE_LIST );
            wxASSERT( GetChildCount() );
            wxASSERT( !IsCategory() );

            wxVariantList& list = pList->GetList();
            wxVariantList::iterator node;
            unsigned int i = 0;

            //wxLogDebug(wxT(">> %s.SetValue() pList parsing"),GetName().c_str());

            // Children in list can be in any order, but we will give hint to
            // GetPropertyByNameWH(). This optimizes for full list parsing.
            for ( node = list.begin(); node != list.end(); ++node )
            {
                wxVariant& childValue = *((wxVariant*)*node);
                wxPGProperty* child = GetPropertyByNameWH(childValue.GetName(), i);
                if ( child )
                {
                    //wxLogDebug(wxT("%i: child = %s, childValue.GetType()=%s"),i,child->GetBaseName().c_str(),childValue.GetType().c_str());
                    if ( childValue.GetType() == wxPG_VARIANT_TYPE_LIST )
                    {
                        if ( child->HasFlag(wxPG_PROP_AGGREGATE) && !(flags & wxPG_SETVAL_AGGREGATED) )
                        {
                            wxVariant listRefCopy = childValue;
                            child->SetValue(childValue, &listRefCopy, flags|wxPG_SETVAL_FROM_PARENT);
                        }
                        else
                        {
                            wxVariant oldVal = child->GetValue();
                            child->SetValue(oldVal, &childValue, flags|wxPG_SETVAL_FROM_PARENT);
                        }
                    }
                    else if ( child->GetValue() != childValue )
                    {
                        // For aggregate properties, we will trust RefreshChildren()
                        // to update child values.
                        if ( !HasFlag(wxPG_PROP_AGGREGATE) )
                            child->SetValue(childValue, NULL, flags|wxPG_SETVAL_FROM_PARENT);
                        if ( flags & wxPG_SETVAL_BY_USER )
                            child->SetFlag(wxPG_PROP_MODIFIED);
                    }
                }
                i++;
            }

            // Always call OnSetValue() for a parent property (do not call it
            // here if the value is non-null because it will then be called
            // below)
            if ( value.IsNull() )
                OnSetValue();
        }

        if ( !value.IsNull() )
        {
            m_value = value;
            OnSetValue();
        }

        if ( flags & wxPG_SETVAL_BY_USER )
            SetFlag(wxPG_PROP_MODIFIED);

        if ( HasFlag(wxPG_PROP_AGGREGATE) )
            RefreshChildren();
    }
    else
    {
        if ( m_commonValue != -1 )
        {
            wxPropertyGrid* pg = GetGrid();
            if ( !pg || m_commonValue != pg->GetUnspecifiedCommonValue() )
                SetCommonValue(-1);
        }

        m_value = value;

        // Set children to unspecified, but only if aggregate or
        // value is <composed>
        if ( AreChildrenComponents() )
        {
            unsigned int i;
            for ( i=0; i<GetChildCount(); i++ )
                Item(i)->SetValue(value, NULL, flags|wxPG_SETVAL_FROM_PARENT);
        }
    }

    if ( !(flags & wxPG_SETVAL_FROM_PARENT) )
        UpdateParentValues();

    //
    // Update editor control.
    if ( flags & wxPG_SETVAL_REFRESH_EDITOR )
    {
        wxPropertyGrid* pg = GetGridIfDisplayed();
        if ( pg )
        {
            wxPGProperty* selected = pg->GetSelectedProperty();

            // Only refresh the control if this was selected, or
            // this was some parent of selected, or vice versa)
            if ( selected && (selected == this ||
                              selected->IsSomeParent(this) ||
                              this->IsSomeParent(selected)) )
                RefreshEditor();

            pg->DrawItemAndValueRelated(this);
        }
    }
}


void wxPGProperty::SetValueInEvent( wxVariant value ) const
{
    GetGrid()->ValueChangeInEvent(value);
}

void wxPGProperty::SetFlagRecursively( wxPGPropertyFlags flag, bool set )
{
    ChangeFlag(flag, set);

    unsigned int i;
    for ( i = 0; i < GetChildCount(); i++ )
        Item(i)->SetFlagRecursively(flag, set);
}

void wxPGProperty::RefreshEditor()
{
    if ( !m_parent )
        return;

    wxPropertyGrid* pg = GetGrid();
    if ( pg && pg->GetSelectedProperty() == this )
        pg->RefreshEditor();
}

wxVariant wxPGProperty::GetDefaultValue() const
{
    wxVariant defVal = GetAttribute(wxPG_ATTR_DEFAULT_VALUE);
    if ( !defVal.IsNull() )
        return defVal;

    wxVariant value = GetValue();

    if ( !value.IsNull() )
    {
        wxString valueType(value.GetType());

        if ( valueType == wxPG_VARIANT_TYPE_LONG )
            return wxPGVariant_Zero;
        if ( valueType == wxPG_VARIANT_TYPE_STRING )
            return wxPGVariant_EmptyString;
        if ( valueType == wxPG_VARIANT_TYPE_BOOL )
            return wxPGVariant_False;
        if ( valueType == wxPG_VARIANT_TYPE_DOUBLE )
            return wxVariant(0.0);
        if ( valueType == wxPG_VARIANT_TYPE_ARRSTRING )
            return wxVariant(wxArrayString());
        if ( valueType == wxS("wxLongLong") )
            return WXVARIANT(wxLongLong(0));
        if ( valueType == wxS("wxULongLong") )
            return WXVARIANT(wxULongLong(0));
        if ( valueType == wxS("wxColour") )
            return WXVARIANT(*wxBLACK);
#if wxUSE_DATETIME
        if ( valueType == wxPG_VARIANT_TYPE_DATETIME )
            return wxVariant(wxDateTime::Now());
#endif
        if ( valueType == wxS("wxFont") )
            return WXVARIANT(*wxNORMAL_FONT);
        if ( valueType == wxS("wxPoint") )
            return WXVARIANT(wxPoint(0, 0));
        if ( valueType == wxS("wxSize") )
            return WXVARIANT(wxSize(0, 0));
    }

    return wxVariant();
}

void wxPGProperty::Enable( bool enable )
{
    wxPropertyGrid* pg = GetGrid();

    // Preferably call the version in the owning wxPropertyGrid,
    // since it handles the editor de-activation.
    if ( pg )
        pg->EnableProperty(this, enable);
    else
        DoEnable(enable);
}

void wxPGProperty::DoEnable( bool enable )
{
    if ( enable )
        ClearFlag(wxPG_PROP_DISABLED);
    else
        SetFlag(wxPG_PROP_DISABLED);

    // Apply same to sub-properties as well
    unsigned int i;
    for ( i = 0; i < GetChildCount(); i++ )
        Item(i)->DoEnable( enable );
}

void wxPGProperty::EnsureCells( unsigned int column )
{
    if ( column >= m_cells.size() )
    {
        // Fill empty slots with default cells
        wxPropertyGrid* pg = GetGrid();
        wxPGCell defaultCell;

        if ( pg )
        {
            // Work around possible VC6 bug by using intermediate variables
            const wxPGCell& propDefCell = pg->GetPropertyDefaultCell();
            const wxPGCell& catDefCell = pg->GetCategoryDefaultCell();

            if ( !HasFlag(wxPG_PROP_CATEGORY) )
                defaultCell = propDefCell;
            else
                defaultCell = catDefCell;
        }

        // TODO: Replace with resize() call
        unsigned int cellCountMax = column+1;

        for ( unsigned int i=m_cells.size(); i<cellCountMax; i++ )
            m_cells.push_back(defaultCell);
    }
}

void wxPGProperty::SetCell( int column,
                            const wxPGCell& cell )
{
    EnsureCells(column);

    m_cells[column] = cell;
}

void wxPGProperty::AdaptiveSetCell( unsigned int firstCol,
                                    unsigned int lastCol,
                                    const wxPGCell& cell,
                                    const wxPGCell& srcData,
                                    wxPGCellData* unmodCellData,
                                    FlagType ignoreWithFlags,
                                    bool recursively )
{
    //
    // Sets cell in memory optimizing fashion. That is, if
    // current cell data matches unmodCellData, we will
    // simply get reference to data from cell. Otherwise,
    // cell information from srcData is merged into current.
    //

    if ( !(m_flags & ignoreWithFlags) && !IsRoot() )
    {
        EnsureCells(lastCol);

        for ( unsigned int col=firstCol; col<=lastCol; col++ )
        {
            if ( m_cells[col].GetData() == unmodCellData )
            {
                // Data matches... use cell directly
                m_cells[col] = cell;
            }
            else
            {
                // Data did not match... merge valid information
                m_cells[col].MergeFrom(srcData);
            }
        }
    }

    if ( recursively )
    {
        for ( unsigned int i=0; i<GetChildCount(); i++ )
            Item(i)->AdaptiveSetCell( firstCol,
                                      lastCol,
                                      cell,
                                      srcData,
                                      unmodCellData,
                                      ignoreWithFlags,
                                      recursively );
    }
}

const wxPGCell& wxPGProperty::GetCell( unsigned int column ) const
{
    if ( m_cells.size() > column )
        return m_cells[column];

    wxPropertyGrid* pg = GetGrid();

    if ( IsCategory() )
        return pg->GetCategoryDefaultCell();

    return pg->GetPropertyDefaultCell();
}

wxPGCell& wxPGProperty::GetOrCreateCell( unsigned int column )
{
    EnsureCells(column);
    return m_cells[column];
}

void wxPGProperty::SetBackgroundColour( const wxColour& colour,
                                        int flags )
{
    wxPGProperty* firstProp = this;
    bool recursively = flags & wxPG_RECURSE ? true : false;

    //
    // If category is tried to set recursively, skip it and only
    // affect the children.
    if ( recursively )
    {
        while ( firstProp->IsCategory() )
        {
            if ( !firstProp->GetChildCount() )
                return;
            firstProp = firstProp->Item(0);
        }
    }

    wxPGCell& firstCell = firstProp->GetCell(0);
    wxPGCellData* firstCellData = firstCell.GetData();

    wxPGCell newCell(firstCell);
    newCell.SetBgCol(colour);
    wxPGCell srcCell;
    srcCell.SetBgCol(colour);

    AdaptiveSetCell( 0,
                     GetParentState()->GetColumnCount()-1,
                     newCell,
                     srcCell,
                     firstCellData,
                     recursively ? wxPG_PROP_CATEGORY : 0,
                     recursively );
}

void wxPGProperty::SetTextColour( const wxColour& colour,
                                  int flags )
{
    wxPGProperty* firstProp = this;
    bool recursively = flags & wxPG_RECURSE ? true : false;

    //
    // If category is tried to set recursively, skip it and only
    // affect the children.
    if ( recursively )
    {
        while ( firstProp->IsCategory() )
        {
            if ( !firstProp->GetChildCount() )
                return;
            firstProp = firstProp->Item(0);
        }
    }

    wxPGCell& firstCell = firstProp->GetCell(0);
    wxPGCellData* firstCellData = firstCell.GetData();

    wxPGCell newCell(firstCell);
    newCell.SetFgCol(colour);
    wxPGCell srcCell;
    srcCell.SetFgCol(colour);

    AdaptiveSetCell( 0,
                     GetParentState()->GetColumnCount()-1,
                     newCell,
                     srcCell,
                     firstCellData,
                     recursively ? wxPG_PROP_CATEGORY : 0,
                     recursively );
}

wxPGEditorDialogAdapter* wxPGProperty::GetEditorDialog() const
{
    return NULL;
}

bool wxPGProperty::DoSetAttribute( const wxString& WXUNUSED(name), wxVariant& WXUNUSED(value) )
{
    return false;
}

void wxPGProperty::SetAttribute( const wxString& name, wxVariant value )
{
    if ( DoSetAttribute( name, value ) )
    {
        // Support working without grid, when possible
        if ( wxPGGlobalVars->HasExtraStyle( wxPG_EX_WRITEONLY_BUILTIN_ATTRIBUTES ) )
            return;
    }

    m_attributes.Set( name, value );
}

void wxPGProperty::SetAttributes( const wxPGAttributeStorage& attributes )
{
    wxPGAttributeStorage::const_iterator it = attributes.StartIteration();
    wxVariant variant;

    while ( attributes.GetNext(it, variant) )
        SetAttribute( variant.GetName(), variant );
}

wxVariant wxPGProperty::DoGetAttribute( const wxString& WXUNUSED(name) ) const
{
    return wxVariant();
}


wxVariant wxPGProperty::GetAttribute( const wxString& name ) const
{
    return m_attributes.FindValue(name);
}

wxString wxPGProperty::GetAttribute( const wxString& name, const wxString& defVal ) const
{
    wxVariant variant = m_attributes.FindValue(name);

    if ( !variant.IsNull() )
        return variant.GetString();

    return defVal;
}

long wxPGProperty::GetAttributeAsLong( const wxString& name, long defVal ) const
{
    wxVariant variant = m_attributes.FindValue(name);

    if ( variant.IsNull() )
        return defVal;

    return variant.GetLong();
}

double wxPGProperty::GetAttributeAsDouble( const wxString& name, double defVal ) const
{
    wxVariant variant = m_attributes.FindValue(name);

    if ( variant.IsNull() )
        return defVal;

    return variant.GetDouble();
}

wxVariant wxPGProperty::GetAttributesAsList() const
{
    wxVariantList tempList;
    wxVariant v( tempList, wxString::Format(wxS("@%s@attr"),m_name.c_str()) );

    wxPGAttributeStorage::const_iterator it = m_attributes.StartIteration();
    wxVariant variant;

    while ( m_attributes.GetNext(it, variant) )
        v.Append(variant);

    return v;
}

// Slots of utility flags are NULL
const unsigned int gs_propFlagToStringSize = 14;

static const wxChar* const gs_propFlagToString[gs_propFlagToStringSize] = {
    NULL,
    wxT("DISABLED"),
    wxT("HIDDEN"),
    NULL,
    wxT("NOEDITOR"),
    wxT("COLLAPSED"),
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

wxString wxPGProperty::GetFlagsAsString( FlagType flagsMask ) const
{
    wxString s;
    int relevantFlags = m_flags & flagsMask & wxPG_STRING_STORED_FLAGS;
    FlagType a = 1;

    unsigned int i = 0;
    for ( i=0; i<gs_propFlagToStringSize; i++ )
    {
        if ( relevantFlags & a )
        {
            const wxChar* fs = gs_propFlagToString[i];
            wxASSERT(fs);
            if ( !s.empty() )
                s << wxS("|");
            s << fs;
        }
        a = a << 1;
    }

    return s;
}

void wxPGProperty::SetFlagsFromString( const wxString& str )
{
    FlagType flags = 0;

    WX_PG_TOKENIZER1_BEGIN(str, wxS('|'))
        unsigned int i;
        for ( i=0; i<gs_propFlagToStringSize; i++ )
        {
            const wxChar* fs = gs_propFlagToString[i];
            if ( fs && str == fs )
            {
                flags |= (1<<i);
                break;
            }
        }
    WX_PG_TOKENIZER1_END()

    m_flags = (m_flags & ~wxPG_STRING_STORED_FLAGS) | flags;
}

wxValidator* wxPGProperty::DoGetValidator() const
{
    return NULL;
}

int wxPGProperty::InsertChoice( const wxString& label, int index, int value )
{
    wxPropertyGrid* pg = GetGrid();
    int sel = GetChoiceSelection();

    int newSel = sel;

    if ( index == wxNOT_FOUND )
        index = m_choices.GetCount();

    if ( index <= sel )
        newSel++;

    m_choices.Insert(label, index, value);

    if ( sel != newSel )
        SetChoiceSelection(newSel);

    if ( this == pg->GetSelection() )
        GetEditorClass()->InsertItem(pg->GetEditorControl(),label,index);

    return index;
}


void wxPGProperty::DeleteChoice( int index )
{
    wxPropertyGrid* pg = GetGrid();

    int sel = GetChoiceSelection();
    int newSel = sel;

    // Adjust current value
    if ( sel == index )
    {
        SetValueToUnspecified();
        newSel = 0;
    }
    else if ( index < sel )
    {
        newSel--;
    }

    m_choices.RemoveAt(index);

    if ( sel != newSel )
        SetChoiceSelection(newSel);

    if ( this == pg->GetSelection() )
        GetEditorClass()->DeleteItem(pg->GetEditorControl(), index);
}

int wxPGProperty::GetChoiceSelection() const
{
    wxVariant value = GetValue();
    wxString valueType = value.GetType();
    int index = wxNOT_FOUND;

    if ( IsValueUnspecified() || !m_choices.GetCount() )
        return wxNOT_FOUND;

    if ( valueType == wxPG_VARIANT_TYPE_LONG )
    {
        index = value.GetLong();
    }
    else if ( valueType == wxPG_VARIANT_TYPE_STRING )
    {
        index = m_choices.Index(value.GetString());
    }
    else if ( valueType == wxPG_VARIANT_TYPE_BOOL )
    {
        index = value.GetBool()? 1 : 0;
    }

    return index;
}

void wxPGProperty::SetChoiceSelection( int newValue )
{
    // Changes value of a property with choices, but only
    // works if the value type is long or string.
    wxString valueType = GetValue().GetType();

    wxCHECK_RET( m_choices.IsOk(), wxT("invalid choiceinfo") );

    if ( valueType == wxPG_VARIANT_TYPE_STRING )
    {
        SetValue( m_choices.GetLabel(newValue) );
    }
    else  // if ( valueType == wxPG_VARIANT_TYPE_LONG )
    {
        SetValue( (long) newValue );
    }
}

bool wxPGProperty::SetChoices( const wxPGChoices& choices )
{
    // Property must be de-selected first (otherwise choices in
    // the control would be de-synced with true choices)
    wxPropertyGrid* pg = GetGrid();
    if ( pg && pg->GetSelection() == this )
        pg->ClearSelection();

    m_choices.Assign(choices);

    {
        // This may be needed to trigger some initialization
        // (but don't do it if property is somewhat uninitialized)
        wxVariant defVal = GetDefaultValue();
        if ( defVal.IsNull() )
            return false;

        SetValue(defVal);
    }

    return true;
}


const wxPGEditor* wxPGProperty::GetEditorClass() const
{
    const wxPGEditor* editor;

    if ( !m_customEditor )
    {
        editor = DoGetEditorClass();
    }
    else
        editor = m_customEditor;

    //
    // Maybe override editor if common value specified
    if ( GetDisplayedCommonValueCount() )
    {
        // TextCtrlAndButton -> ComboBoxAndButton
        if ( wxDynamicCast(editor, wxPGTextCtrlAndButtonEditor) )
            editor = wxPGEditor_ChoiceAndButton;

        // TextCtrl -> ComboBox
        else if ( wxDynamicCast(editor, wxPGTextCtrlEditor) )
            editor = wxPGEditor_ComboBox;
    }

    return editor;
}

bool wxPGProperty::Hide( bool hide, int flags )
{
    wxPropertyGrid* pg = GetGrid();
    if ( pg )
        return pg->HideProperty(this, hide, flags);

    return DoHide( hide, flags );
}

bool wxPGProperty::DoHide( bool hide, int flags )
{
    if ( !hide )
        ClearFlag( wxPG_PROP_HIDDEN );
    else
        SetFlag( wxPG_PROP_HIDDEN );

    if ( flags & wxPG_RECURSE )
    {
        unsigned int i;
        for ( i = 0; i < GetChildCount(); i++ )
            Item(i)->DoHide(hide, flags | wxPG_RECURSE_STARTS);
    }

    return true;
}

bool wxPGProperty::HasVisibleChildren() const
{
    unsigned int i;

    for ( i=0; i<GetChildCount(); i++ )
    {
        wxPGProperty* child = Item(i);

        if ( !child->HasFlag(wxPG_PROP_HIDDEN) )
            return true;
    }

    return false;
}

bool wxPGProperty::RecreateEditor()
{
    wxPropertyGrid* pg = GetGrid();
    wxASSERT(pg);

    wxPGProperty* selected = pg->GetSelection();
    if ( this == selected )
    {
        pg->DoSelectProperty(this, wxPG_SEL_FORCE);
        return true;
    }
    return false;
}


void wxPGProperty::SetValueImage( wxBitmap& bmp )
{
    delete m_valueBitmap;

    if ( &bmp && bmp.IsOk() )
    {
        // Resize the image
        wxSize maxSz = GetGrid()->GetImageSize();
        wxSize imSz(bmp.GetWidth(),bmp.GetHeight());

        if ( imSz.y != maxSz.y )
        {
        #if wxUSE_IMAGE
            // Here we use high-quality wxImage scaling functions available
            wxImage img = bmp.ConvertToImage();
            double scaleY = (double)maxSz.y / (double)imSz.y;
            img.Rescale(wxRound(bmp.GetWidth()*scaleY),
                        wxRound(bmp.GetHeight()*scaleY),
                        wxIMAGE_QUALITY_HIGH);
            wxBitmap* bmpNew = new wxBitmap(img, 32);
        #else
            // This is the old, deprecated method of scaling the image
            wxBitmap* bmpNew = new wxBitmap(maxSz.x,maxSz.y,bmp.GetDepth());
            wxMemoryDC dc;
            dc.SelectObject(*bmpNew);
            double scaleY = (double)maxSz.y / (double)imSz.y;
            dc.SetUserScale(scaleY, scaleY);
            dc.DrawBitmap(bmp, 0, 0);
        #endif

            m_valueBitmap = bmpNew;
        }
        else
        {
            m_valueBitmap = new wxBitmap(bmp);
        }

        m_flags |= wxPG_PROP_CUSTOMIMAGE;
    }
    else
    {
        m_valueBitmap = NULL;
        m_flags &= ~(wxPG_PROP_CUSTOMIMAGE);
    }
}


wxPGProperty* wxPGProperty::GetMainParent() const
{
    const wxPGProperty* curChild = this;
    const wxPGProperty* curParent = m_parent;

    while ( curParent && !curParent->IsCategory() )
    {
        curChild = curParent;
        curParent = curParent->m_parent;
    }

    return (wxPGProperty*) curChild;
}


const wxPGProperty* wxPGProperty::GetLastVisibleSubItem() const
{
    //
    // Returns last visible sub-item, recursively.
    if ( !IsExpanded() || !GetChildCount() )
        return this;

    return Last()->GetLastVisibleSubItem();
}


bool wxPGProperty::IsVisible() const
{
    const wxPGProperty* parent;

    if ( HasFlag(wxPG_PROP_HIDDEN) )
        return false;

    for ( parent = GetParent(); parent != NULL; parent = parent->GetParent() )
    {
        if ( !parent->IsExpanded() || parent->HasFlag(wxPG_PROP_HIDDEN) )
            return false;
    }

    return true;
}

wxPropertyGrid* wxPGProperty::GetGridIfDisplayed() const
{
    wxPropertyGridPageState* state = GetParentState();
    if ( !state )
        return NULL;
    wxPropertyGrid* propGrid = state->GetGrid();
    if ( state == propGrid->GetState() )
        return propGrid;
    return NULL;
}


int wxPGProperty::GetY2( int lh ) const
{
    const wxPGProperty* parent;
    const wxPGProperty* child = this;

    int y = 0;

    for ( parent = GetParent(); parent != NULL; parent = child->GetParent() )
    {
        if ( !parent->IsExpanded() )
            return parent->GetY2(lh);
        y += parent->GetChildrenHeight(lh, child->GetIndexInParent());
        y += lh;
        child = parent;
    }

    y -= lh;  // need to reduce one level

    return y;
}


int wxPGProperty::GetY() const
{
    return GetY2(GetGrid()->GetRowHeight());
}

// This is used by Insert etc.
void wxPGProperty::DoAddChild( wxPGProperty* prop, int index,
                               bool correct_mode )
{
    if ( index < 0 || (size_t)index >= m_children.size() )
    {
        if ( correct_mode ) prop->m_arrIndex = m_children.size();
        m_children.push_back( prop );
    }
    else
    {
        m_children.insert( m_children.begin()+index, prop);
        if ( correct_mode ) FixIndicesOfChildren( index );
    }

    prop->m_parent = this;
}

void wxPGProperty::DoPreAddChild( int index, wxPGProperty* prop )
{
    wxASSERT_MSG( prop->GetBaseName().length(),
                  "Property's children must have unique, non-empty "
                  "names within their scope" );

    prop->m_arrIndex = index;
    m_children.insert( m_children.begin()+index,
                       prop );

    int custImgHeight = prop->OnMeasureImage().y;
    if ( custImgHeight < 0 /*|| custImgHeight > 1*/ )
        prop->m_flags |= wxPG_PROP_CUSTOMIMAGE;

    prop->m_parent = this;
}

void wxPGProperty::AddPrivateChild( wxPGProperty* prop )
{
    if ( !(m_flags & wxPG_PROP_PARENTAL_FLAGS) )
        SetParentalType(wxPG_PROP_AGGREGATE);

    wxASSERT_MSG( (m_flags & wxPG_PROP_PARENTAL_FLAGS) ==
                    wxPG_PROP_AGGREGATE,
                  "Do not mix up AddPrivateChild() calls with other "
                  "property adders." );

    DoPreAddChild( m_children.size(), prop );
}

#if wxPG_COMPATIBILITY_1_4
void wxPGProperty::AddChild( wxPGProperty* prop )
{
    AddPrivateChild(prop);
}
#endif

wxPGProperty* wxPGProperty::InsertChild( int index,
                                         wxPGProperty* childProperty )
{
    if ( index < 0 )
        index = m_children.size();

    if ( m_parentState )
    {
        m_parentState->DoInsert(this, index, childProperty);
    }
    else
    {
        if ( !(m_flags & wxPG_PROP_PARENTAL_FLAGS) )
            SetParentalType(wxPG_PROP_MISC_PARENT);

        wxASSERT_MSG( (m_flags & wxPG_PROP_PARENTAL_FLAGS) ==
                        wxPG_PROP_MISC_PARENT,
                      "Do not mix up AddPrivateChild() calls with other "
                      "property adders." );

        DoPreAddChild( index, childProperty );
    }

    return childProperty;
}

void wxPGProperty::RemoveChild( wxPGProperty* p )
{
    wxArrayPGProperty::iterator it;
    wxArrayPGProperty& children = m_children;

    for ( it=children.begin(); it != children.end(); it++ )
    {
        if ( *it == p )
        {
            children.erase(it);
            break;
        }
    }
}

void wxPGProperty::AdaptListToValue( wxVariant& list, wxVariant* value ) const
{
    wxASSERT( GetChildCount() );
    wxASSERT( !IsCategory() );

    *value = GetValue();

    if ( !list.GetCount() )
        return;

    wxASSERT( GetChildCount() >= (unsigned int)list.GetCount() );

    bool allChildrenSpecified;

    // Don't fully update aggregate properties unless all children have
    // specified value
    if ( HasFlag(wxPG_PROP_AGGREGATE) )
        allChildrenSpecified = AreAllChildrenSpecified(&list);
    else
        allChildrenSpecified = true;

    unsigned int i;
    unsigned int n = 0;
    wxVariant childValue = list[n];

    //wxLogDebug(wxT(">> %s.AdaptListToValue()"),GetBaseName().c_str());

    for ( i=0; i<GetChildCount(); i++ )
    {
        const wxPGProperty* child = Item(i);

        if ( childValue.GetName() == child->GetBaseName() )
        {
            //wxLogDebug(wxT("  %s(n=%i), %s"),childValue.GetName().c_str(),n,childValue.GetType().c_str());

            if ( childValue.GetType() == wxPG_VARIANT_TYPE_LIST )
            {
                wxVariant cv2(child->GetValue());
                child->AdaptListToValue(childValue, &cv2);
                childValue = cv2;
            }

            if ( allChildrenSpecified )
            {
                *value = ChildChanged(*value, i, childValue);
            }

            n++;
            if ( n == (unsigned int)list.GetCount() )
                break;
            childValue = list[n];
        }
    }
}


void wxPGProperty::FixIndicesOfChildren( unsigned int starthere )
{
    size_t i;
    for ( i=starthere;i<GetChildCount();i++)
        Item(i)->m_arrIndex = i;
}


// Returns (direct) child property with given name (or NULL if not found)
wxPGProperty* wxPGProperty::GetPropertyByName( const wxString& name ) const
{
    size_t i;

    for ( i=0; i<GetChildCount(); i++ )
    {
        wxPGProperty* p = Item(i);
        if ( p->m_name == name )
            return p;
    }

    // Does it have point, then?
    int pos = name.Find(wxS('.'));
    if ( pos <= 0 )
        return NULL;

    wxPGProperty* p = GetPropertyByName(name. substr(0,pos));

    if ( !p || !p->GetChildCount() )
        return NULL;

    return p->GetPropertyByName(name.substr(pos+1,name.length()-pos-1));
}

wxPGProperty* wxPGProperty::GetPropertyByNameWH( const wxString& name, unsigned int hintIndex ) const
{
    unsigned int i = hintIndex;

    if ( i >= GetChildCount() )
        i = 0;

    unsigned int lastIndex = i - 1;

    if ( lastIndex >= GetChildCount() )
        lastIndex = GetChildCount() - 1;

    for (;;)
    {
        wxPGProperty* p = Item(i);
        if ( p->m_name == name )
            return p;

        if ( i == lastIndex )
            break;

        i++;
        if ( i == GetChildCount() )
            i = 0;
    };

    return NULL;
}

int wxPGProperty::GetChildrenHeight( int lh, int iMax_ ) const
{
    // Returns height of children, recursively, and
    // by taking expanded/collapsed status into account.
    //
    // iMax is used when finding property y-positions.
    //
    unsigned int i = 0;
    int h = 0;

    if ( iMax_ == -1 )
        iMax_ = GetChildCount();

    unsigned int iMax = iMax_;

    wxASSERT( iMax <= GetChildCount() );

    if ( !IsExpanded() && GetParent() )
        return 0;

    while ( i < iMax )
    {
        wxPGProperty* pwc = (wxPGProperty*) Item(i);

        if ( !pwc->HasFlag(wxPG_PROP_HIDDEN) )
        {
            if ( !pwc->IsExpanded() ||
                 pwc->GetChildCount() == 0 )
                h += lh;
            else
                h += pwc->GetChildrenHeight(lh) + lh;
        }

        i++;
    }

    return h;
}

wxPGProperty* wxPGProperty::GetItemAtY( unsigned int y,
                                        unsigned int lh,
                                        unsigned int* nextItemY ) const
{
    wxASSERT( nextItemY );

    // Linear search at the moment
    //
    // nextItemY = y of next visible property, final value will be written back.
    wxPGProperty* result = NULL;
    wxPGProperty* current = NULL;
    unsigned int iy = *nextItemY;
    unsigned int i = 0;
    unsigned int iMax = GetChildCount();

    while ( i < iMax )
    {
        wxPGProperty* pwc = Item(i);

        if ( !pwc->HasFlag(wxPG_PROP_HIDDEN) )
        {
            // Found?
            if ( y < iy )
            {
                result = current;
                break;
            }

            iy += lh;

            if ( pwc->IsExpanded() &&
                 pwc->GetChildCount() > 0 )
            {
                result = (wxPGProperty*) pwc->GetItemAtY( y, lh, &iy );
                if ( result )
                    break;
            }

            current = pwc;
        }

        i++;
    }

    // Found?
    if ( !result && y < iy )
        result = current;

    *nextItemY = iy;

    /*
    if ( current )
    {
        wxLogDebug(wxT("%s::GetItemAtY(%i) -> %s"),this->GetLabel().c_str(),y,current->GetLabel().c_str());
    }
    else
    {
        wxLogDebug(wxT("%s::GetItemAtY(%i) -> NULL"),this->GetLabel().c_str(),y);
    }
    */

    return (wxPGProperty*) result;
}

void wxPGProperty::Empty()
{
    size_t i;
    if ( !HasFlag(wxPG_PROP_CHILDREN_ARE_COPIES) )
    {
        for ( i=0; i<GetChildCount(); i++ )
        {
            delete m_children[i];
        }
    }

    m_children.clear();
}

wxPGProperty* wxPGProperty::GetItemAtY( unsigned int y ) const
{
    unsigned int nextItem;
    return GetItemAtY( y, GetGrid()->GetRowHeight(), &nextItem);
}

void wxPGProperty::DeleteChildren()
{
    wxPropertyGridPageState* state = m_parentState;

    if ( !GetChildCount() )
        return;

    // Because deletion is sometimes deferred, we have to use
    // this sort of code for enumerating the child properties.
    unsigned int i = GetChildCount();
    while ( i > 0 )
    {
        i--;
        state->DoDelete(Item(i), true);
    }
}

bool wxPGProperty::IsChildSelected( bool recursive ) const
{
    size_t i;
    for ( i = 0; i < GetChildCount(); i++ )
    {
        wxPGProperty* child = Item(i);

        // Test child
        if ( m_parentState->DoIsPropertySelected( child ) )
            return true;

        // Test sub-childs
        if ( recursive && child->IsChildSelected( recursive ) )
            return true;
    }

    return false;
}

wxVariant wxPGProperty::ChildChanged( wxVariant& WXUNUSED(thisValue),
                                      int WXUNUSED(childIndex),
                                      wxVariant& WXUNUSED(childValue) ) const
{
    return wxNullVariant;
}

bool wxPGProperty::AreAllChildrenSpecified( wxVariant* pendingList ) const
{
    unsigned int i;

    const wxVariantList* pList = NULL;
    wxVariantList::const_iterator node;

    if ( pendingList )
    {
        pList = &pendingList->GetList();
        node = pList->begin();
    }

    for ( i=0; i<GetChildCount(); i++ )
    {
        wxPGProperty* child = Item(i);
        const wxVariant* listValue = NULL;
        wxVariant value;

        if ( pendingList )
        {
            const wxString& childName = child->GetBaseName();

            for ( ; node != pList->end(); ++node )
            {
                const wxVariant& item = *((const wxVariant*)*node);
                if ( item.GetName() == childName )
                {
                    listValue = &item;
                    value = item;
                    break;
                }
            }
        }

        if ( !listValue )
            value = child->GetValue();

        if ( value.IsNull() )
            return false;

        // Check recursively
        if ( child->GetChildCount() )
        {
            const wxVariant* childList = NULL;

            if ( listValue && listValue->GetType() == wxPG_VARIANT_TYPE_LIST )
                childList = listValue;

            if ( !child->AreAllChildrenSpecified((wxVariant*)childList) )
                return false;
        }
    }

    return true;
}

wxPGProperty* wxPGProperty::UpdateParentValues()
{
    wxPGProperty* parent = m_parent;
    if ( parent && parent->HasFlag(wxPG_PROP_COMPOSED_VALUE) &&
         !parent->IsCategory() && !parent->IsRoot() )
    {
        wxString s;
        parent->DoGenerateComposedValue(s);
        parent->m_value = s;
        return parent->UpdateParentValues();
    }
    return this;
}

bool wxPGProperty::IsTextEditable() const
{
    if ( HasFlag(wxPG_PROP_READONLY) )
        return false;

    if ( HasFlag(wxPG_PROP_NOEDITOR) &&
         (GetChildCount() ||
          wxString(GetEditorClass()->GetClassInfo()->GetClassName()).EndsWith(wxS("Button")))
       )
        return false;

    return true;
}

// Call after fixed sub-properties added/removed after creation.
// if oldSelInd >= 0 and < new max items, then selection is
// moved to it. Note: oldSelInd -2 indicates that this property
// should be selected.
void wxPGProperty::SubPropsChanged( int oldSelInd )
{
    wxPropertyGridPageState* state = GetParentState();
    wxPropertyGrid* grid = state->GetGrid();

    //
    // Re-repare children (recursively)
    for ( unsigned int i=0; i<GetChildCount(); i++ )
    {
        wxPGProperty* child = Item(i);
        child->InitAfterAdded(state, grid);
    }

    wxPGProperty* sel = NULL;
    if ( oldSelInd >= (int)m_children.size() )
        oldSelInd = (int)m_children.size() - 1;

    if ( oldSelInd >= 0 )
        sel = m_children[oldSelInd];
    else if ( oldSelInd == -2 )
        sel = this;

    if ( sel )
        state->DoSelectProperty(sel);

    if ( state == grid->GetState() )
    {
        grid->GetPanel()->Refresh();
    }
}

// -----------------------------------------------------------------------
// wxPGRootProperty
// -----------------------------------------------------------------------

WX_PG_IMPLEMENT_PROPERTY_CLASS_PLAIN(wxPGRootProperty,none,TextCtrl)
IMPLEMENT_DYNAMIC_CLASS(wxPGRootProperty, wxPGProperty)


wxPGRootProperty::wxPGRootProperty( const wxString& name )
    : wxPGProperty()
{
    m_name = name;
    m_label = m_name;
    SetParentalType(0);
    m_depth = 0;
}


wxPGRootProperty::~wxPGRootProperty()
{
}


// -----------------------------------------------------------------------
// wxPropertyCategory
// -----------------------------------------------------------------------

WX_PG_IMPLEMENT_PROPERTY_CLASS_PLAIN(wxPropertyCategory,none,TextCtrl)
IMPLEMENT_DYNAMIC_CLASS(wxPropertyCategory, wxPGProperty)

void wxPropertyCategory::Init()
{
    // don't set colour - prepareadditem method should do this
    SetParentalType(wxPG_PROP_CATEGORY);
    m_capFgColIndex = 1;
    m_textExtent = -1;
}

wxPropertyCategory::wxPropertyCategory()
    : wxPGProperty()
{
    Init();
}


wxPropertyCategory::wxPropertyCategory( const wxString &label, const wxString& name )
    : wxPGProperty(label,name)
{
    Init();
}


wxPropertyCategory::~wxPropertyCategory()
{
}


wxString wxPropertyCategory::ValueToString( wxVariant& WXUNUSED(value),
                                            int WXUNUSED(argFlags) ) const
{
    if ( m_value.GetType() == wxPG_VARIANT_TYPE_STRING )
        return m_value.GetString();
    return wxEmptyString;
}

wxString wxPropertyCategory::GetValueAsString( int argFlags ) const
{
#if wxPG_COMPATIBILITY_1_4
    // This is backwards compatibility test
    // That is, to make sure this function is not overridden
    // (instead, ValueToString() should be).
    if ( argFlags == 0xFFFF )
    {
        // Do not override! (for backwards compliancy)
        return g_invalidStringContent;
    }
#endif

    // Unspecified value is always empty string
    if ( IsValueUnspecified() )
        return wxEmptyString;

    return wxPGProperty::GetValueAsString(argFlags);
}

int wxPropertyCategory::GetTextExtent( const wxWindow* wnd, const wxFont& font ) const
{
    if ( m_textExtent > 0 )
        return m_textExtent;
    int x = 0, y = 0;
    ((wxWindow*)wnd)->GetTextExtent( m_label, &x, &y, 0, 0, &font );
    return x;
}

void wxPropertyCategory::CalculateTextExtent( wxWindow* wnd, const wxFont& font )
{
    int x = 0, y = 0;
    wnd->GetTextExtent( m_label, &x, &y, 0, 0, &font );
    m_textExtent = x;
}

// -----------------------------------------------------------------------
// wxPGChoices
// -----------------------------------------------------------------------

wxPGChoiceEntry& wxPGChoices::Add( const wxString& label, int value )
{
    AllocExclusive();

    wxPGChoiceEntry entry(label, value);
    return m_data->Insert( -1, entry );
}

// -----------------------------------------------------------------------

wxPGChoiceEntry& wxPGChoices::Add( const wxString& label, const wxBitmap& bitmap, int value )
{
    AllocExclusive();

    wxPGChoiceEntry entry(label, value);
    entry.SetBitmap(bitmap);
    return m_data->Insert( -1, entry );
}

// -----------------------------------------------------------------------

wxPGChoiceEntry& wxPGChoices::Insert( const wxPGChoiceEntry& entry, int index )
{
    AllocExclusive();

    return m_data->Insert( index, entry );
}

// -----------------------------------------------------------------------

wxPGChoiceEntry& wxPGChoices::Insert( const wxString& label, int index, int value )
{
    AllocExclusive();

    wxPGChoiceEntry entry(label, value);
    return m_data->Insert( index, entry );
}

// -----------------------------------------------------------------------

wxPGChoiceEntry& wxPGChoices::AddAsSorted( const wxString& label, int value )
{
    AllocExclusive();

    size_t index = 0;

    while ( index < GetCount() )
    {
        int cmpRes = GetLabel(index).Cmp(label);
        if ( cmpRes > 0 )
            break;
        index++;
    }

    wxPGChoiceEntry entry(label, value);
    return m_data->Insert( index, entry );
}

// -----------------------------------------------------------------------

void wxPGChoices::Add( const wxChar* const* labels, const ValArrItem* values )
{
    AllocExclusive();

    unsigned int itemcount = 0;
    const wxChar* const* p = &labels[0];
    while ( *p ) { p++; itemcount++; }

    unsigned int i;
    for ( i = 0; i < itemcount; i++ )
    {
        int value = i;
        if ( values )
            value = values[i];
        wxPGChoiceEntry entry(labels[i], value);
        m_data->Insert( i, entry );
    }
}

// -----------------------------------------------------------------------

void wxPGChoices::Add( const wxArrayString& arr, const wxArrayInt& arrint )
{
    AllocExclusive();

    unsigned int i;
    unsigned int itemcount = arr.size();

    for ( i = 0; i < itemcount; i++ )
    {
        int value = i;
        if ( &arrint && arrint.size() )
            value = arrint[i];
        wxPGChoiceEntry entry(arr[i], value);
        m_data->Insert( i, entry );
    }
}

// -----------------------------------------------------------------------

void wxPGChoices::RemoveAt(size_t nIndex, size_t count)
{
    AllocExclusive();

    wxASSERT( m_data->GetRefCount() != -1 );
    m_data->m_items.erase(m_data->m_items.begin()+nIndex,
                          m_data->m_items.begin()+nIndex+count);
}

// -----------------------------------------------------------------------

void wxPGChoices::Clear()
{
    if ( m_data != wxPGChoicesEmptyData )
    {
        AllocExclusive();
        m_data->Clear();
    }
}

// -----------------------------------------------------------------------

int wxPGChoices::Index( const wxString& str ) const
{
    if ( IsOk() )
    {
        unsigned int i;
        for ( i=0; i< m_data->GetCount(); i++ )
        {
            const wxPGChoiceEntry& entry = m_data->Item(i);
            if ( entry.HasText() && entry.GetText() == str )
                return i;
        }
    }
    return -1;
}

// -----------------------------------------------------------------------

int wxPGChoices::Index( int val ) const
{
    if ( IsOk() )
    {
        unsigned int i;
        for ( i=0; i< m_data->GetCount(); i++ )
        {
            const wxPGChoiceEntry& entry = m_data->Item(i);
            if ( entry.GetValue() == val )
                return i;
        }
    }
    return -1;
}

// -----------------------------------------------------------------------

wxArrayString wxPGChoices::GetLabels() const
{
    wxArrayString arr;
    unsigned int i;

    if ( this && IsOk() )
        for ( i=0; i<GetCount(); i++ )
            arr.push_back(GetLabel(i));

    return arr;
}

// -----------------------------------------------------------------------

wxArrayInt wxPGChoices::GetValuesForStrings( const wxArrayString& strings ) const
{
    wxArrayInt arr;

    if ( IsOk() )
    {
        unsigned int i;
        for ( i=0; i< strings.size(); i++ )
        {
            int index = Index(strings[i]);
            if ( index >= 0 )
                arr.Add(GetValue(index));
            else
                arr.Add(wxPG_INVALID_VALUE);
        }
    }

    return arr;
}

// -----------------------------------------------------------------------

wxArrayInt wxPGChoices::GetIndicesForStrings( const wxArrayString& strings,
                                              wxArrayString* unmatched ) const
{
    wxArrayInt arr;

    if ( IsOk() )
    {
        unsigned int i;
        for ( i=0; i< strings.size(); i++ )
        {
            const wxString& str = strings[i];
            int index = Index(str);
            if ( index >= 0 )
                arr.Add(index);
            else if ( unmatched )
                unmatched->Add(str);
        }
    }

    return arr;
}

// -----------------------------------------------------------------------

void wxPGChoices::AllocExclusive()
{
    EnsureData();

    if ( m_data->GetRefCount() != 1 )
    {
        wxPGChoicesData* data = new wxPGChoicesData();
        data->CopyDataFrom(m_data);
        Free();
        m_data = data;
    }
}

// -----------------------------------------------------------------------

void wxPGChoices::AssignData( wxPGChoicesData* data )
{
    Free();

    if ( data != wxPGChoicesEmptyData )
    {
        m_data = data;
        data->IncRef();
    }
}

// -----------------------------------------------------------------------

void wxPGChoices::Init()
{
    m_data = wxPGChoicesEmptyData;
}

// -----------------------------------------------------------------------

void wxPGChoices::Free()
{
    if ( m_data != wxPGChoicesEmptyData )
    {
        m_data->DecRef();
        m_data = wxPGChoicesEmptyData;
    }
}

// -----------------------------------------------------------------------
// wxPGAttributeStorage
// -----------------------------------------------------------------------

wxPGAttributeStorage::wxPGAttributeStorage()
{
}

wxPGAttributeStorage::~wxPGAttributeStorage()
{
    wxPGHashMapS2P::iterator it;

    for ( it = m_map.begin(); it != m_map.end(); ++it )
    {
        wxVariantData* data = (wxVariantData*) it->second;
        data->DecRef();
    }
}

void wxPGAttributeStorage::Set( const wxString& name, const wxVariant& value )
{
    wxVariantData* data = value.GetData();

    // Free old, if any
    wxPGHashMapS2P::iterator it = m_map.find(name);
    if ( it != m_map.end() )
    {
        ((wxVariantData*)it->second)->DecRef();

        if ( !data )
        {
            // If Null variant, just remove from set
            m_map.erase(it);
            return;
        }
    }

    if ( data )
    {
        data->IncRef();

        m_map[name] = data;
    }
}

#endif  // wxUSE_PROPGRID
