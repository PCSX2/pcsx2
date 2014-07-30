/////////////////////////////////////////////////////////////////////////////
// Name:        src/propgrid/propgridiface.cpp
// Purpose:     wxPropertyGridInterface class
// Author:      Jaakko Salli
// Modified by:
// Created:     2008-08-24
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
    #include "wx/event.h"
    #include "wx/window.h"
    #include "wx/panel.h"
    #include "wx/dc.h"
    #include "wx/dcmemory.h"
    #include "wx/button.h"
    #include "wx/pen.h"
    #include "wx/brush.h"
    #include "wx/settings.h"
    #include "wx/sizer.h"
    #include "wx/intl.h"
#endif

#include "wx/propgrid/property.h"
#include "wx/propgrid/propgrid.h"


const wxChar *wxPGTypeName_long = wxT("long");
const wxChar *wxPGTypeName_bool = wxT("bool");
const wxChar *wxPGTypeName_double = wxT("double");
const wxChar *wxPGTypeName_wxString = wxT("string");
const wxChar *wxPGTypeName_void = wxT("void*");
const wxChar *wxPGTypeName_wxArrayString = wxT("arrstring");


// ----------------------------------------------------------------------------
// VariantDatas
// ----------------------------------------------------------------------------

WX_PG_IMPLEMENT_VARIANT_DATA_EXPORTED(wxPoint, WXDLLIMPEXP_PROPGRID)
WX_PG_IMPLEMENT_VARIANT_DATA_EXPORTED(wxSize, WXDLLIMPEXP_PROPGRID)
WX_PG_IMPLEMENT_VARIANT_DATA_EXPORTED_DUMMY_EQ(wxArrayInt, WXDLLIMPEXP_PROPGRID)
IMPLEMENT_VARIANT_OBJECT_EXPORTED(wxFont, WXDLLIMPEXP_PROPGRID)

// -----------------------------------------------------------------------
// wxPGPropArgCls
// -----------------------------------------------------------------------

wxPGProperty* wxPGPropArgCls::GetPtr( wxPropertyGridInterface* iface ) const
{
    if ( m_flags == IsProperty )
    {
        wxASSERT_MSG( m_ptr.property, wxT("invalid property ptr") );
        return m_ptr.property;
    }
    else if ( m_flags & IsWxString )
        return iface->GetPropertyByNameA(*m_ptr.stringName);
    else if ( m_flags & IsCharPtr )
        return iface->GetPropertyByNameA(m_ptr.charName);
    else if ( m_flags & IsWCharPtr )
        return iface->GetPropertyByNameA(m_ptr.wcharName);

    return NULL;
}

// -----------------------------------------------------------------------
// wxPropertyGridInterface
// -----------------------------------------------------------------------

void wxPropertyGridInterface::RefreshGrid( wxPropertyGridPageState* state )
{
    if ( !state )
        state = m_pState;

    wxPropertyGrid* grid = state->GetGrid();
    if ( grid->GetState() == state && !grid->IsFrozen() )
    {
        grid->Refresh();
    }
}

// -----------------------------------------------------------------------

wxPGProperty* wxPropertyGridInterface::Append( wxPGProperty* property )
{
    wxPGProperty* retp = m_pState->DoAppend(property);

    wxPropertyGrid* grid = m_pState->GetGrid();
    if ( grid )
        grid->RefreshGrid();

    return retp;
}

// -----------------------------------------------------------------------

wxPGProperty* wxPropertyGridInterface::AppendIn( wxPGPropArg id, wxPGProperty* newproperty )
{
    wxPG_PROP_ARG_CALL_PROLOG_RETVAL(wxNullProperty)
    wxPGProperty* pwc = (wxPGProperty*) p;
    wxPGProperty* retp = m_pState->DoInsert(pwc, pwc->GetChildCount(), newproperty);
    return retp;
}

// -----------------------------------------------------------------------

wxPGProperty* wxPropertyGridInterface::Insert( wxPGPropArg id, wxPGProperty* property )
{
    wxPG_PROP_ARG_CALL_PROLOG_RETVAL(wxNullProperty)
    wxPGProperty* retp = m_pState->DoInsert(p->GetParent(), p->GetIndexInParent(), property);
    RefreshGrid();
    return retp;
}

// -----------------------------------------------------------------------

wxPGProperty* wxPropertyGridInterface::Insert( wxPGPropArg id, int index, wxPGProperty* newproperty )
{
    wxPG_PROP_ARG_CALL_PROLOG_RETVAL(wxNullProperty)
    wxPGProperty* retp = m_pState->DoInsert((wxPGProperty*)p,index,newproperty);
    RefreshGrid();
    return retp;
}

// -----------------------------------------------------------------------

void wxPropertyGridInterface::DeleteProperty( wxPGPropArg id )
{
    wxPG_PROP_ARG_CALL_PROLOG()

    wxPropertyGridPageState* state = p->GetParentState();

    state->DoDelete( p, true );

    RefreshGrid(state);
}

// -----------------------------------------------------------------------

wxPGProperty* wxPropertyGridInterface::RemoveProperty( wxPGPropArg id )
{
    wxPG_PROP_ARG_CALL_PROLOG_RETVAL(wxNullProperty)

    wxCHECK( !p->GetChildCount() || p->HasFlag(wxPG_PROP_AGGREGATE),
             wxNullProperty);

    wxPropertyGridPageState* state = p->GetParentState();

    state->DoDelete( p, false );

    RefreshGrid(state);

    return p;
}

// -----------------------------------------------------------------------

wxPGProperty* wxPropertyGridInterface::ReplaceProperty( wxPGPropArg id, wxPGProperty* property )
{
    wxPG_PROP_ARG_CALL_PROLOG_RETVAL(wxNullProperty)

    wxPGProperty* replaced = p;
    wxCHECK_MSG( replaced && property,
                 wxNullProperty,
                 wxT("NULL property") );
    wxCHECK_MSG( !replaced->IsCategory(),
                 wxNullProperty,
                 wxT("cannot replace this type of property") );
    wxCHECK_MSG( !m_pState->IsInNonCatMode(),
                 wxNullProperty,
                 wxT("cannot replace properties in alphabetic mode") );

    // Get address to the slot
    wxPGProperty* parent = replaced->GetParent();
    int ind = replaced->GetIndexInParent();

    wxPropertyGridPageState* state = replaced->GetParentState();
    DeleteProperty(replaced); // Must use generic Delete
    state->DoInsert(parent,ind,property);

    return property;
}

// -----------------------------------------------------------------------
// wxPropertyGridInterface property operations
// -----------------------------------------------------------------------

wxPGProperty* wxPropertyGridInterface::GetSelection() const
{
    return m_pState->GetSelection();
}

// -----------------------------------------------------------------------

bool wxPropertyGridInterface::ClearSelection( bool validation )
{
    bool res = DoClearSelection(validation, wxPG_SEL_DONT_SEND_EVENT);
    wxPropertyGrid* pg = GetPropertyGrid();
    if ( pg )
        pg->Refresh();
    return res;
}

// -----------------------------------------------------------------------

bool wxPropertyGridInterface::DoClearSelection( bool validation,
                                                int selFlags )
{
    if ( !validation )
        selFlags |= wxPG_SEL_NOVALIDATE;

    wxPropertyGridPageState* state = m_pState;

    if ( state )
    {
        wxPropertyGrid* pg = state->GetGrid();
        if ( pg->GetState() == state )
            return pg->DoSelectProperty(NULL, selFlags);
        else
            state->DoSetSelection(NULL);
    }

    return true;
}

// -----------------------------------------------------------------------

void wxPropertyGridInterface::LimitPropertyEditing( wxPGPropArg id, bool limit )
{
    wxPG_PROP_ARG_CALL_PROLOG()

    m_pState->DoLimitPropertyEditing(p, limit);
    RefreshProperty(p);
}

// -----------------------------------------------------------------------

bool wxPropertyGridInterface::EnableProperty( wxPGPropArg id, bool enable )
{
    wxPG_PROP_ARG_CALL_PROLOG_RETVAL(false)

    wxPropertyGridPageState* state = p->GetParentState();
    wxPropertyGrid* grid = state->GetGrid();

    if ( enable )
    {
        if ( !(p->m_flags & wxPG_PROP_DISABLED) )
            return false;

        // If active, Set active Editor.
        if ( grid && grid->GetState() == state && p == grid->GetSelection() )
            grid->DoSelectProperty( p, wxPG_SEL_FORCE );
    }
    else
    {
        if ( p->m_flags & wxPG_PROP_DISABLED )
            return false;

        // If active, Disable as active Editor.
        if ( grid && grid->GetState() == state && p == grid->GetSelection() )
            grid->DoSelectProperty( p, wxPG_SEL_FORCE );
    }

    p->DoEnable(enable);

    RefreshProperty( p );

    return true;
}

// -----------------------------------------------------------------------

bool wxPropertyGridInterface::ExpandAll( bool doExpand )
{
    wxPropertyGridPageState* state = m_pState;

    if ( !state->DoGetRoot()->GetChildCount() )
        return true;

    wxPropertyGrid* pg = state->GetGrid();

    if ( GetSelection() && GetSelection() != state->DoGetRoot() &&
         !doExpand )
    {
        pg->DoClearSelection();
    }

    wxPGVIterator it;

    for ( it = GetVIterator( wxPG_ITERATE_ALL ); !it.AtEnd(); it.Next() )
    {
        wxPGProperty* p = (wxPGProperty*) it.GetProperty();
        if ( p->GetChildCount() )
        {
            if ( doExpand )
            {
                if ( !p->IsExpanded() )
                {
                    state->DoExpand(p);
                }
            }
            else
            {
                if ( p->IsExpanded() )
                {
                    state->DoCollapse(p);
                }
            }
        }
    }

    pg->RecalculateVirtualSize();

    RefreshGrid();

    return true;
}

// -----------------------------------------------------------------------

void wxPropertyGridInterface::ClearModifiedStatus()
{
    unsigned int pageIndex = 0;

    for (;;)
    {
        wxPropertyGridPageState* page = GetPageState(pageIndex);
        if ( !page ) break;

        page->DoGetRoot()->SetFlagRecursively(wxPG_PROP_MODIFIED, false);
        page->m_anyModified = false;

        pageIndex++;
    }

    // Update active editor control, if any
    GetPropertyGrid()->RefreshEditor();
}

bool wxPropertyGridInterface::SetColumnProportion( unsigned int column,
                                                   int proportion )
{
    wxCHECK(m_pState, false);
    wxPropertyGrid* pg = m_pState->GetGrid();
    wxCHECK(pg, false);
    wxCHECK(pg->HasFlag(wxPG_SPLITTER_AUTO_CENTER), false);
    m_pState->DoSetColumnProportion(column, proportion);
    return true;
}

// -----------------------------------------------------------------------
// wxPropertyGridInterface property value setting and getting
// -----------------------------------------------------------------------

void wxPGGetFailed( const wxPGProperty* p, const wxString& typestr )
{
    wxPGTypeOperationFailed(p, typestr, wxS("Get"));
}

// -----------------------------------------------------------------------

void wxPGTypeOperationFailed( const wxPGProperty* p,
                              const wxString& typestr,
                              const wxString& op )
{
    wxASSERT( p != NULL );
    wxLogError( _("Type operation \"%s\" failed: Property labeled \"%s\" is of type \"%s\", NOT \"%s\"."),
        op.c_str(), p->GetLabel().c_str(), p->GetValue().GetType().c_str(), typestr.c_str() );
}

// -----------------------------------------------------------------------

void wxPropertyGridInterface::SetPropVal( wxPGPropArg id, wxVariant& value )
{
    wxPG_PROP_ARG_CALL_PROLOG()

    if ( p )
        p->SetValue(value);
}

// -----------------------------------------------------------------------

void wxPropertyGridInterface::SetPropertyValueString( wxPGPropArg id, const wxString& value )
{
    wxPG_PROP_ARG_CALL_PROLOG()

    if ( p )
        m_pState->DoSetPropertyValueString(p, value);
}

// -----------------------------------------------------------------------

void wxPropertyGridInterface::SetValidationFailureBehavior( int vfbFlags )
{
    GetPropertyGrid()->m_permanentValidationFailureBehavior = vfbFlags;
}

// -----------------------------------------------------------------------

wxPGProperty* wxPropertyGridInterface::GetPropertyByNameA( const wxString& name ) const
{
    wxPGProperty* p = GetPropertyByName(name);
    wxASSERT_MSG(p,wxString::Format(wxT("no property with name '%s'"),name.c_str()));
    return p;
}

// ----------------------------------------------------------------------------

wxPGProperty* wxPropertyGridInterface::GetPropertyByLabel( const wxString& label ) const
{
    wxPGVIterator it;

    for ( it = GetVIterator( wxPG_ITERATE_PROPERTIES ); !it.AtEnd(); it.Next() )
    {
        if ( it.GetProperty()->GetLabel() == label )
            return it.GetProperty();
    }

    return wxNullProperty;
}

// ----------------------------------------------------------------------------

void wxPropertyGridInterface::DoSetPropertyAttribute( wxPGPropArg id, const wxString& name,
                                                      wxVariant& value, long argFlags )
{
    wxPG_PROP_ARG_CALL_PROLOG()

    p->SetAttribute( name, value );

    if ( argFlags & wxPG_RECURSE )
    {
        unsigned int i;
        for ( i = 0; i < p->GetChildCount(); i++ )
            DoSetPropertyAttribute(p->Item(i), name, value, argFlags);
    }
}

// -----------------------------------------------------------------------

void wxPropertyGridInterface::SetPropertyAttributeAll( const wxString& attrName,
                                                       wxVariant value )
{
    unsigned int pageIndex = 0;

    for (;;)
    {
        wxPropertyGridPageState* page = GetPageState(pageIndex);
        if ( !page ) break;

        DoSetPropertyAttribute(page->DoGetRoot(), attrName, value, wxPG_RECURSE);

        pageIndex++;
    }
}

// -----------------------------------------------------------------------

void wxPropertyGridInterface::GetPropertiesWithFlag( wxArrayPGProperty* targetArr,
                                                     wxPGProperty::FlagType flags,
                                                     bool inverse,
                                                     int iterFlags ) const
{
    wxASSERT( targetArr );
    wxPGVIterator it = GetVIterator( iterFlags );

    for ( ;
          !it.AtEnd();
          it.Next() )
    {
        const wxPGProperty* property = it.GetProperty();

        if ( !inverse )
        {
            if ( (property->GetFlags() & flags) == flags )
                targetArr->push_back((wxPGProperty*)property);
        }
        else
        {
            if ( (property->GetFlags() & flags) != flags )
                targetArr->push_back((wxPGProperty*)property);
        }
    }
}

// -----------------------------------------------------------------------

void wxPropertyGridInterface::SetBoolChoices( const wxString& trueChoice,
                                                 const wxString& falseChoice )
{
    wxPGGlobalVars->m_boolChoices[0] = falseChoice;
    wxPGGlobalVars->m_boolChoices[1] = trueChoice;
}

// -----------------------------------------------------------------------

wxPGProperty* wxPropertyGridInterface::DoGetPropertyByName( const wxString& name ) const
{
    return m_pState->BaseGetPropertyByName(name);
}

// -----------------------------------------------------------------------

wxPGProperty* wxPropertyGridInterface::GetPropertyByName( const wxString& name,
                                                             const wxString& subname ) const
{
    wxPGProperty* p = DoGetPropertyByName(name);
    if ( !p || !p->GetChildCount() )
        return wxNullProperty;

    return p->GetPropertyByName(subname);
}

// -----------------------------------------------------------------------

// Since GetPropertyByName is used *a lot*, this makes sense
// since non-virtual method can be called with less code.
wxPGProperty* wxPropertyGridInterface::GetPropertyByName( const wxString& name ) const
{
    wxPGProperty* p = DoGetPropertyByName(name);
    if ( p )
        return p;

    // Check if its "Property.SubProperty" format
    int pos = name.Find(wxT('.'));
    if ( pos <= 0 )
        return NULL;

    return GetPropertyByName(name.substr(0,pos),
                             name.substr(pos+1,name.length()-pos-1));
}

// -----------------------------------------------------------------------

bool wxPropertyGridInterface::HideProperty( wxPGPropArg id, bool hide, int flags )
{
    wxPG_PROP_ARG_CALL_PROLOG_RETVAL(false)

    wxPropertyGrid* pg = m_pState->GetGrid();

    if ( pg == p->GetGrid() )
        return pg->DoHideProperty(p, hide, flags);
    else
        m_pState->DoHideProperty(p, hide, flags);

    return true;
}

// -----------------------------------------------------------------------

bool wxPropertyGridInterface::Collapse( wxPGPropArg id )
{
    wxPG_PROP_ARG_CALL_PROLOG_RETVAL(false)
    wxPropertyGrid* pg = p->GetGridIfDisplayed();
    if ( pg )
        return pg->DoCollapse(p);

    return p->GetParentState()->DoCollapse(p);
}

// -----------------------------------------------------------------------

bool wxPropertyGridInterface::Expand( wxPGPropArg id )
{
    wxPG_PROP_ARG_CALL_PROLOG_RETVAL(false)
    wxPropertyGrid* pg = p->GetGridIfDisplayed();
    if ( pg )
        return pg->DoExpand(p);

    return p->GetParentState()->DoExpand(p);
}

// -----------------------------------------------------------------------

void wxPropertyGridInterface::Sort( int flags )
{
    wxPropertyGrid* pg = GetPropertyGrid();

    unsigned int pageIndex = 0;

    for (;;)
    {
        wxPropertyGridPageState* page = GetPageState(pageIndex);
        if ( !page ) break;
        page->DoSort(flags);
        pageIndex++;
    }

    // Fix positions of any open editor controls
    if ( pg )
        pg->CorrectEditorWidgetPosY();
}

// -----------------------------------------------------------------------

void wxPropertyGridInterface::SetPropertyLabel( wxPGPropArg id, const wxString& newproplabel )
{
    wxPG_PROP_ARG_CALL_PROLOG()

    p->SetLabel( newproplabel );

    wxPropertyGridPageState* state = p->GetParentState();
    wxPropertyGrid* pg = state->GetGrid();

    if ( pg->HasFlag(wxPG_AUTO_SORT) )
        pg->SortChildren(p->GetParent());

    if ( pg->GetState() == state )
    {
        if ( pg->HasFlag(wxPG_AUTO_SORT) )
            pg->Refresh();
        else
            pg->DrawItem( p );
    }
}

// -----------------------------------------------------------------------

bool wxPropertyGridInterface::SetPropertyMaxLength( wxPGPropArg id, int maxLen )
{
    wxPG_PROP_ARG_CALL_PROLOG_RETVAL(false)

    wxPropertyGrid* pg = m_pState->GetGrid();

    p->m_maxLen = (short) maxLen;

    // Adjust control if selected currently
    if ( pg == p->GetGrid() && p == m_pState->GetSelection() )
    {
        wxWindow* wnd = pg->GetEditorControl();
        wxTextCtrl* tc = wxDynamicCast(wnd,wxTextCtrl);
        if ( tc )
            tc->SetMaxLength( maxLen );
        else
        // Not a text ctrl
            return false;
    }

    return true;
}

// -----------------------------------------------------------------------

void
wxPropertyGridInterface::SetPropertyBackgroundColour( wxPGPropArg id,
                                                      const wxColour& colour,
                                                      int flags )
{
    wxPG_PROP_ARG_CALL_PROLOG()
    p->SetBackgroundColour(colour, flags);
    RefreshProperty(p);
}

// -----------------------------------------------------------------------

void wxPropertyGridInterface::SetPropertyTextColour( wxPGPropArg id,
                                                     const wxColour& colour,
                                                     int flags )
{
    wxPG_PROP_ARG_CALL_PROLOG()
    p->SetTextColour(colour, flags);
    RefreshProperty(p);
}

// -----------------------------------------------------------------------

void wxPropertyGridInterface::SetPropertyColoursToDefault( wxPGPropArg id )
{
    wxPG_PROP_ARG_CALL_PROLOG()

    p->m_cells.clear();
}

// -----------------------------------------------------------------------

void wxPropertyGridInterface::SetPropertyCell( wxPGPropArg id,
                                               int column,
                                               const wxString& text,
                                               const wxBitmap& bitmap,
                                               const wxColour& fgCol,
                                               const wxColour& bgCol )
{
    wxPG_PROP_ARG_CALL_PROLOG()

    wxPGCell& cell = p->GetCell(column);
    if ( !text.empty() && text != wxPG_LABEL )
        cell.SetText(text);
    if ( bitmap.IsOk() )
        cell.SetBitmap(bitmap);
    if ( fgCol != wxNullColour )
        cell.SetFgCol(fgCol);
    if ( bgCol != wxNullColour )
        cell.SetBgCol(bgCol);
}

// -----------------------------------------------------------------------
// GetPropertyValueAsXXX methods

#define IMPLEMENT_GET_VALUE(T,TRET,BIGNAME,DEFRETVAL) \
TRET wxPropertyGridInterface::GetPropertyValueAs##BIGNAME( wxPGPropArg id ) const \
{ \
    wxPG_PROP_ARG_CALL_PROLOG_RETVAL(DEFRETVAL) \
    wxVariant value = p->GetValue(); \
    if ( wxStrcmp(value.GetType(), wxPGTypeName_##T) != 0 ) \
    { \
        wxPGGetFailed(p,wxPGTypeName_##T); \
        return (TRET)DEFRETVAL; \
    } \
    return (TRET)value.Get##BIGNAME(); \
}

// String is different than others.
wxString wxPropertyGridInterface::GetPropertyValueAsString( wxPGPropArg id ) const
{
    wxPG_PROP_ARG_CALL_PROLOG_RETVAL(wxEmptyString)
    return p->GetValueAsString(wxPG_FULL_VALUE);
}

bool wxPropertyGridInterface::GetPropertyValueAsBool( wxPGPropArg id ) const
{
    wxPG_PROP_ARG_CALL_PROLOG_RETVAL(false)
    wxVariant value = p->GetValue();
    if ( wxStrcmp(value.GetType(), wxPGTypeName_bool) == 0 )
    {
        return value.GetBool();
    }
    if ( wxStrcmp(value.GetType(), wxPGTypeName_long) == 0 )
    {
        return value.GetLong()?true:false;
    }
    wxPGGetFailed(p,wxPGTypeName_bool);
    return false;
}

IMPLEMENT_GET_VALUE(long,long,Long,0)
IMPLEMENT_GET_VALUE(double,double,Double,0.0)

bool wxPropertyGridInterface::IsPropertyExpanded( wxPGPropArg id ) const
{
    wxPG_PROP_ARG_CALL_PROLOG_RETVAL(false)
    return p->IsExpanded();
}

// -----------------------------------------------------------------------
// wxPropertyGridInterface wrappers
// -----------------------------------------------------------------------

bool wxPropertyGridInterface::ChangePropertyValue( wxPGPropArg id, wxVariant newValue )
{
    return GetPropertyGrid()->ChangePropertyValue(id, newValue);
}

// -----------------------------------------------------------------------

void wxPropertyGridInterface::BeginAddChildren( wxPGPropArg id )
{
    wxPG_PROP_ARG_CALL_PROLOG()
    wxCHECK_RET( p->HasFlag(wxPG_PROP_AGGREGATE), wxT("only call on properties with fixed children") );
    p->ClearFlag(wxPG_PROP_AGGREGATE);
    p->SetFlag(wxPG_PROP_MISC_PARENT);
}

// -----------------------------------------------------------------------

bool wxPropertyGridInterface::EditorValidate()
{
    return GetPropertyGrid()->DoEditorValidate();
}

// -----------------------------------------------------------------------

void wxPropertyGridInterface::EndAddChildren( wxPGPropArg id )
{
    wxPG_PROP_ARG_CALL_PROLOG()
    wxCHECK_RET( p->HasFlag(wxPG_PROP_MISC_PARENT), wxT("only call on properties for which BeginAddChildren was called prior") );
    p->ClearFlag(wxPG_PROP_MISC_PARENT);
    p->SetFlag(wxPG_PROP_AGGREGATE);
}

// -----------------------------------------------------------------------
// wxPGVIterator_State
// -----------------------------------------------------------------------

// Default returned by wxPropertyGridInterface::GetVIterator().
class wxPGVIteratorBase_State : public wxPGVIteratorBase
{
public:
    wxPGVIteratorBase_State( wxPropertyGridPageState* state, int flags )
    {
        m_it.Init( state, flags );
    }
    virtual ~wxPGVIteratorBase_State() { }
    virtual void Next() { m_it.Next(); }
};

wxPGVIterator wxPropertyGridInterface::GetVIterator( int flags ) const
{
    return wxPGVIterator( new wxPGVIteratorBase_State( m_pState, flags ) );
}

// -----------------------------------------------------------------------
// wxPGEditableState related functions
// -----------------------------------------------------------------------

// EscapeDelimiters() changes ";" into "\;" and "|" into "\|"
// in the input string.  This is an internal functions which is
// used for saving states
// NB: Similar function exists in aui/framemanager.cpp
static wxString EscapeDelimiters(const wxString& s)
{
    wxString result;
    result.Alloc(s.length());
    const wxChar* ch = s.c_str();
    while (*ch)
    {
        if (*ch == wxT(';') || *ch == wxT('|') || *ch == wxT(','))
            result += wxT('\\');
        result += *ch;
        ++ch;
    }
    return result;
}

wxString wxPropertyGridInterface::SaveEditableState( int includedStates ) const
{
    wxString result;

    //
    // Save state on page basis
    unsigned int pageIndex = 0;
    wxArrayPtrVoid pageStates;

    for (;;)
    {
        wxPropertyGridPageState* page = GetPageState(pageIndex);
        if ( !page ) break;

        pageStates.Add(page);

        pageIndex++;
    }

    for ( pageIndex=0; pageIndex < pageStates.size(); pageIndex++ )
    {
        wxPropertyGridPageState* pageState = (wxPropertyGridPageState*) pageStates[pageIndex];

        if ( includedStates & SelectionState )
        {
            wxString sel;
            if ( pageState->GetSelection() )
                sel = pageState->GetSelection()->GetName();
            result += wxS("selection=");
            result += EscapeDelimiters(sel);
            result += wxS(";");
        }
        if ( includedStates & ExpandedState )
        {
            wxArrayPGProperty ptrs;
            wxPropertyGridConstIterator it =
                wxPropertyGridConstIterator( pageState,
                                             wxPG_ITERATE_ALL_PARENTS_RECURSIVELY|wxPG_ITERATE_HIDDEN,
                                             wxNullProperty );

            result += wxS("expanded=");

            for ( ;
                  !it.AtEnd();
                  it.Next() )
            {
                const wxPGProperty* p = it.GetProperty();

                if ( !p->HasFlag(wxPG_PROP_COLLAPSED) )
                    result += EscapeDelimiters(p->GetName());
                result += wxS(",");

            }

            if ( result.Last() == wxS(',') )
                result.RemoveLast();

            result += wxS(";");
        }
        if ( includedStates & ScrollPosState )
        {
            int x, y;
            GetPropertyGrid()->GetViewStart(&x,&y);
            result += wxString::Format(wxS("scrollpos=%i,%i;"), x, y);
        }
        if ( includedStates & SplitterPosState )
        {
            result += wxS("splitterpos=");

            for ( size_t i=0; i<pageState->GetColumnCount(); i++ )
                result += wxString::Format(wxS("%i,"), pageState->DoGetSplitterPosition(i));

            result.RemoveLast();  // Remove last comma
            result += wxS(";");
        }
        if ( includedStates & PageState )
        {
            result += wxS("ispageselected=");

            if ( GetPageState(-1) == pageState )
                result += wxS("1;");
            else
                result += wxS("0;");
        }
        if ( includedStates & DescBoxState )
        {
            wxVariant v = GetEditableStateItem(wxS("descboxheight"));
            if ( !v.IsNull() )
                result += wxString::Format(wxS("descboxheight=%i;"), (int)v.GetLong());
        }
        result.RemoveLast();  // Remove last semicolon
        result += wxS("|");
    }

    // Remove last '|'
    if ( !result.empty() )
        result.RemoveLast();

    return result;
}

bool wxPropertyGridInterface::RestoreEditableState( const wxString& src, int restoreStates )
{
    wxPropertyGrid* pg = GetPropertyGrid();
    wxPGProperty* newSelection = NULL;
    size_t pageIndex;
    long vx = -1;
    long vy = -1;
    long selectedPage = -1;
    bool pgSelectionSet = false;
    bool res = true;

    pg->Freeze();
    wxArrayString pageStrings = ::wxSplit(src, wxS('|'), wxS('\\'));

    for ( pageIndex=0; pageIndex<pageStrings.size(); pageIndex++ )
    {
        wxPropertyGridPageState* pageState = GetPageState(pageIndex);
        if ( !pageState )
            break;

        wxArrayString kvpairStrings = ::wxSplit(pageStrings[pageIndex], wxS(';'), wxS('\\'));

        for ( size_t i=0; i<kvpairStrings.size(); i++ )
        {
            const wxString& kvs = kvpairStrings[i];
            int eq_pos = kvs.Find(wxS('='));
            if ( eq_pos != wxNOT_FOUND )
            {
                wxString key = kvs.substr(0, eq_pos);
                wxString value = kvs.substr(eq_pos+1);

                // Further split value by commas
                wxArrayString values = ::wxSplit(value, wxS(','), wxS('\\'));

                if ( key == wxS("expanded") )
                {
                    if ( restoreStates & ExpandedState )
                    {
                        wxPropertyGridIterator it =
                            wxPropertyGridIterator( pageState,
                                                    wxPG_ITERATE_ALL,
                                                    wxNullProperty );

                        // First collapse all
                        for ( ; !it.AtEnd(); it.Next() )
                        {
                            wxPGProperty* p = it.GetProperty();
                            pageState->DoCollapse(p);
                        }

                        // Then expand those which names are in values
                        for ( size_t n=0; n<values.size(); n++ )
                        {
                            const wxString& name = values[n];
                            wxPGProperty* prop = GetPropertyByName(name);
                            if ( prop )
                                pageState->DoExpand(prop);
                        }
                    }
                }
                else if ( key == wxS("scrollpos") )
                {
                    if ( restoreStates & ScrollPosState )
                    {
                        if ( values.size() == 2 )
                        {
                            values[0].ToLong(&vx);
                            values[1].ToLong(&vy);
                        }
                        else
                        {
                            res = false;
                        }
                    }
                }
                else if ( key == wxS("splitterpos") )
                {
                    if ( restoreStates & SplitterPosState )
                    {
                        for ( size_t n=1; n<values.size(); n++ )
                        {
                            long pos = 0;
                            values[n].ToLong(&pos);
                            if ( pos > 0 )
                                pageState->DoSetSplitterPosition(pos, n);
                        }
                    }
                }
                else if ( key == wxS("selection") )
                {
                    if ( restoreStates & SelectionState )
                    {
                        if ( values.size() > 0 )
                        {
                            if ( pageState->IsDisplayed() )
                            {
                                if ( !values[0].empty() )
                                    newSelection = GetPropertyByName(value);
                                pgSelectionSet = true;
                            }
                            else
                            {
                                if ( !values[0].empty() )
                                    pageState->DoSetSelection(GetPropertyByName(value));
                                else
                                    pageState->DoClearSelection();
                            }
                        }
                    }
                }
                else if ( key == wxS("ispageselected") )
                {
                    if ( restoreStates & PageState )
                    {
                        long pageSelStatus;
                        if ( values.size() == 1 && values[0].ToLong(&pageSelStatus) )
                        {
                            if ( pageSelStatus )
                                selectedPage = pageIndex;
                        }
                        else
                        {
                            res = false;
                        }
                    }
                }
                else if ( key == wxS("descboxheight") )
                {
                    if ( restoreStates & DescBoxState )
                    {
                        long descBoxHeight;
                        if ( values.size() == 1 && values[0].ToLong(&descBoxHeight) )
                        {
                            SetEditableStateItem(wxS("descboxheight"), descBoxHeight);
                        }
                        else
                        {
                            res = false;
                        }
                    }
                }
                else
                {
                    res = false;
                }
            }
        }
    }

    //
    // Force recalculation of virtual heights of all pages
    // (may be needed on unclean source string).
    pageIndex = 0;
    wxPropertyGridPageState* pageState = GetPageState(pageIndex);
    while ( pageState )
    {
        pageState->VirtualHeightChanged();
        pageIndex += 1;
        pageState = GetPageState(pageIndex);
    }

    pg->Thaw();

    //
    // Selection of visible grid page must be set after Thaw() call
    if ( pgSelectionSet )
    {
        if ( newSelection )
            pg->DoSelectProperty(newSelection);
        else
            pg->DoClearSelection();
    }

    if ( selectedPage != -1 )
    {
        DoSelectPage(selectedPage);
    }

    if ( vx >= 0 )
    {
        pg->Scroll(vx, vy);
    }

    return res;
}

#endif  // wxUSE_PROPGRID

