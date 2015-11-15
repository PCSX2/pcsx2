/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/datavcmn.cpp
// Purpose:     wxDataViewCtrl base classes and common parts
// Author:      Robert Roebling
// Created:     2006/02/20
// Copyright:   (c) 2006, Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_DATAVIEWCTRL

#include "wx/dataview.h"

#ifndef WX_PRECOMP
    #include "wx/dc.h"
    #include "wx/settings.h"
    #include "wx/log.h"
    #include "wx/crt.h"
#endif

#include "wx/datectrl.h"
#include "wx/spinctrl.h"
#include "wx/choice.h"
#include "wx/imaglist.h"

const char wxDataViewCtrlNameStr[] = "dataviewCtrl";

namespace
{

// Custom handler pushed on top of the edit control used by wxDataViewCtrl to
// forward some events to the main control itself.
class wxDataViewEditorCtrlEvtHandler: public wxEvtHandler
{
public:
    wxDataViewEditorCtrlEvtHandler(wxWindow *editor, wxDataViewRenderer *owner)
    {
        m_editorCtrl = editor;
        m_owner = owner;

        m_finished = false;
    }

    void AcceptChangesAndFinish();
    void SetFocusOnIdle( bool focus = true ) { m_focusOnIdle = focus; }

protected:
    void OnChar( wxKeyEvent &event );
    void OnTextEnter( wxCommandEvent &event );
    void OnKillFocus( wxFocusEvent &event );
    void OnIdle( wxIdleEvent &event );

private:
    wxDataViewRenderer     *m_owner;
    wxWindow               *m_editorCtrl;
    bool                    m_finished;
    bool                    m_focusOnIdle;

private:
    DECLARE_EVENT_TABLE()
};

} // anonymous namespace

// ---------------------------------------------------------
// wxDataViewItemAttr
// ---------------------------------------------------------

wxFont wxDataViewItemAttr::GetEffectiveFont(const wxFont& font) const
{
    if ( !HasFont() )
        return font;

    wxFont f(font);
    if ( GetBold() )
        f.MakeBold();
    if ( GetItalic() )
        f.MakeItalic();
    return f;
}


// ---------------------------------------------------------
// wxDataViewModelNotifier
// ---------------------------------------------------------

#include "wx/listimpl.cpp"
WX_DEFINE_LIST(wxDataViewModelNotifiers)

bool wxDataViewModelNotifier::ItemsAdded( const wxDataViewItem &parent, const wxDataViewItemArray &items )
{
    size_t count = items.GetCount();
    size_t i;
    for (i = 0; i < count; i++)
        if (!ItemAdded( parent, items[i] )) return false;

    return true;
}

bool wxDataViewModelNotifier::ItemsDeleted( const wxDataViewItem &parent, const wxDataViewItemArray &items )
{
    size_t count = items.GetCount();
    size_t i;
    for (i = 0; i < count; i++)
        if (!ItemDeleted( parent, items[i] )) return false;

    return true;
}

bool wxDataViewModelNotifier::ItemsChanged( const wxDataViewItemArray &items )
{
    size_t count = items.GetCount();
    size_t i;
    for (i = 0; i < count; i++)
        if (!ItemChanged( items[i] )) return false;

    return true;
}

// ---------------------------------------------------------
// wxDataViewModel
// ---------------------------------------------------------

wxDataViewModel::wxDataViewModel()
{
    m_notifiers.DeleteContents( true );
}

bool wxDataViewModel::ItemAdded( const wxDataViewItem &parent, const wxDataViewItem &item )
{
    bool ret = true;

    wxDataViewModelNotifiers::iterator iter;
    for (iter = m_notifiers.begin(); iter != m_notifiers.end(); ++iter)
    {
        wxDataViewModelNotifier* notifier = *iter;
        if (!notifier->ItemAdded( parent, item ))
            ret = false;
    }

    return ret;
}

bool wxDataViewModel::ItemDeleted( const wxDataViewItem &parent, const wxDataViewItem &item )
{
    bool ret = true;

    wxDataViewModelNotifiers::iterator iter;
    for (iter = m_notifiers.begin(); iter != m_notifiers.end(); ++iter)
    {
        wxDataViewModelNotifier* notifier = *iter;
        if (!notifier->ItemDeleted( parent, item ))
            ret = false;
    }

    return ret;
}

bool wxDataViewModel::ItemChanged( const wxDataViewItem &item )
{
    bool ret = true;

    wxDataViewModelNotifiers::iterator iter;
    for (iter = m_notifiers.begin(); iter != m_notifiers.end(); ++iter)
    {
        wxDataViewModelNotifier* notifier = *iter;
        if (!notifier->ItemChanged( item ))
            ret = false;
    }

    return ret;
}

bool wxDataViewModel::ItemsAdded( const wxDataViewItem &parent, const wxDataViewItemArray &items )
{
    bool ret = true;

    wxDataViewModelNotifiers::iterator iter;
    for (iter = m_notifiers.begin(); iter != m_notifiers.end(); ++iter)
    {
        wxDataViewModelNotifier* notifier = *iter;
        if (!notifier->ItemsAdded( parent, items ))
            ret = false;
    }

    return ret;
}

bool wxDataViewModel::ItemsDeleted( const wxDataViewItem &parent, const wxDataViewItemArray &items )
{
    bool ret = true;

    wxDataViewModelNotifiers::iterator iter;
    for (iter = m_notifiers.begin(); iter != m_notifiers.end(); ++iter)
    {
        wxDataViewModelNotifier* notifier = *iter;
        if (!notifier->ItemsDeleted( parent, items ))
            ret = false;
    }

    return ret;
}

bool wxDataViewModel::ItemsChanged( const wxDataViewItemArray &items )
{
    bool ret = true;

    wxDataViewModelNotifiers::iterator iter;
    for (iter = m_notifiers.begin(); iter != m_notifiers.end(); ++iter)
    {
        wxDataViewModelNotifier* notifier = *iter;
        if (!notifier->ItemsChanged( items ))
            ret = false;
    }

    return ret;
}

bool wxDataViewModel::ValueChanged( const wxDataViewItem &item, unsigned int col )
{
    bool ret = true;

    wxDataViewModelNotifiers::iterator iter;
    for (iter = m_notifiers.begin(); iter != m_notifiers.end(); ++iter)
    {
        wxDataViewModelNotifier* notifier = *iter;
        if (!notifier->ValueChanged( item, col ))
            ret = false;
    }

    return ret;
}

bool wxDataViewModel::Cleared()
{
    bool ret = true;

    wxDataViewModelNotifiers::iterator iter;
    for (iter = m_notifiers.begin(); iter != m_notifiers.end(); ++iter)
    {
        wxDataViewModelNotifier* notifier = *iter;
        if (!notifier->Cleared())
            ret = false;
    }

    return ret;
}

bool wxDataViewModel::BeforeReset()
{
    bool ret = true;

    wxDataViewModelNotifiers::iterator iter;
    for (iter = m_notifiers.begin(); iter != m_notifiers.end(); ++iter)
    {
        wxDataViewModelNotifier* notifier = *iter;
        if (!notifier->BeforeReset())
            ret = false;
    }

    return ret;
}

bool wxDataViewModel::AfterReset()
{
    bool ret = true;

    wxDataViewModelNotifiers::iterator iter;
    for (iter = m_notifiers.begin(); iter != m_notifiers.end(); ++iter)
    {
        wxDataViewModelNotifier* notifier = *iter;
        if (!notifier->AfterReset())
            ret = false;
    }

    return ret;
}

void wxDataViewModel::Resort()
{
    wxDataViewModelNotifiers::iterator iter;
    for (iter = m_notifiers.begin(); iter != m_notifiers.end(); ++iter)
    {
        wxDataViewModelNotifier* notifier = *iter;
        notifier->Resort();
    }
}

void wxDataViewModel::AddNotifier( wxDataViewModelNotifier *notifier )
{
    m_notifiers.push_back( notifier );
    notifier->SetOwner( this );
}

void wxDataViewModel::RemoveNotifier( wxDataViewModelNotifier *notifier )
{
    m_notifiers.DeleteObject( notifier );
}

int wxDataViewModel::Compare( const wxDataViewItem &item1, const wxDataViewItem &item2,
                              unsigned int column, bool ascending ) const
{
    wxVariant value1,value2;
    GetValue( value1, item1, column );
    GetValue( value2, item2, column );

    if (!ascending)
    {
        wxVariant temp = value1;
        value1 = value2;
        value2 = temp;
    }

    if (value1.GetType() == wxT("string"))
    {
        wxString str1 = value1.GetString();
        wxString str2 = value2.GetString();
        int res = str1.Cmp( str2 );
        if (res)
            return res;
    }
    else if (value1.GetType() == wxT("long"))
    {
        long l1 = value1.GetLong();
        long l2 = value2.GetLong();
        if (l1 < l2)
            return -1;
        else if (l1 > l2)
            return 1;
    }
    else if (value1.GetType() == wxT("double"))
    {
        double d1 = value1.GetDouble();
        double d2 = value2.GetDouble();
        if (d1 < d2)
            return -1;
        else if (d1 > d2)
            return 1;
    }
    else if (value1.GetType() == wxT("datetime"))
    {
        wxDateTime dt1 = value1.GetDateTime();
        wxDateTime dt2 = value2.GetDateTime();
        if (dt1.IsEarlierThan(dt2))
            return -1;
        if (dt2.IsEarlierThan(dt1))
            return 1;
    }
    else if (value1.GetType() == wxT("bool"))
    {
        bool b1 = value1.GetBool();
        bool b2 = value2.GetBool();

        if (b1 != b2)
            return b1 ? 1 : -1;
    }
    else if (value1.GetType() == wxT("wxDataViewIconText"))
    {
        wxDataViewIconText iconText1, iconText2;

        iconText1 << value1;
        iconText2 << value2;

        int res = iconText1.GetText().Cmp(iconText2.GetText());
        if (res != 0)
          return res;
    }


    // items must be different
    wxUIntPtr id1 = wxPtrToUInt(item1.GetID()),
              id2 = wxPtrToUInt(item2.GetID());

    return ascending ? id1 - id2 : id2 - id1;
}

// ---------------------------------------------------------
// wxDataViewIndexListModel
// ---------------------------------------------------------

static int my_sort( int *v1, int *v2 )
{
   return *v2-*v1;
}


wxDataViewIndexListModel::wxDataViewIndexListModel( unsigned int initial_size )
{
    // IDs are ordered until an item gets deleted or inserted
    m_ordered = true;

    // build initial index
    unsigned int i;
    for (i = 1; i < initial_size+1; i++)
            m_hash.Add( wxDataViewItem(wxUIntToPtr(i)) );
    m_nextFreeID = initial_size + 1;
}

void wxDataViewIndexListModel::Reset( unsigned int new_size )
{
    /* wxDataViewModel:: */ BeforeReset();

    m_hash.Clear();

    // IDs are ordered until an item gets deleted or inserted
    m_ordered = true;

    // build initial index
    unsigned int i;
    for (i = 1; i < new_size+1; i++)
            m_hash.Add( wxDataViewItem(wxUIntToPtr(i)) );

    m_nextFreeID = new_size + 1;

    /* wxDataViewModel:: */ AfterReset();
}

void wxDataViewIndexListModel::RowPrepended()
{
    m_ordered = false;

    unsigned int id = m_nextFreeID;
    m_nextFreeID++;

    wxDataViewItem item( wxUIntToPtr(id) );
    m_hash.Insert( item, 0 );
    ItemAdded( wxDataViewItem(0), item );

}

void wxDataViewIndexListModel::RowInserted( unsigned int before )
{
    m_ordered = false;

    unsigned int id = m_nextFreeID;
    m_nextFreeID++;

    wxDataViewItem item( wxUIntToPtr(id) );
    m_hash.Insert( item, before );
    ItemAdded( wxDataViewItem(0), item );
}

void wxDataViewIndexListModel::RowAppended()
{
    unsigned int id = m_nextFreeID;
    m_nextFreeID++;

    wxDataViewItem item( wxUIntToPtr(id) );
    m_hash.Add( item );
    ItemAdded( wxDataViewItem(0), item );
}

void wxDataViewIndexListModel::RowDeleted( unsigned int row )
{
    m_ordered = false;

    wxDataViewItem item( m_hash[row] );
    m_hash.RemoveAt( row );
    /* wxDataViewModel:: */ ItemDeleted( wxDataViewItem(0), item );
}

void wxDataViewIndexListModel::RowsDeleted( const wxArrayInt &rows )
{
    m_ordered = false;

    wxDataViewItemArray array;
    unsigned int i;
    for (i = 0; i < rows.GetCount(); i++)
    {
            wxDataViewItem item( m_hash[rows[i]] );
            array.Add( item );
    }

    wxArrayInt sorted = rows;
    sorted.Sort( my_sort );
    for (i = 0; i < sorted.GetCount(); i++)
           m_hash.RemoveAt( sorted[i] );

    /* wxDataViewModel:: */ ItemsDeleted( wxDataViewItem(0), array );
}

void wxDataViewIndexListModel::RowChanged( unsigned int row )
{
    /* wxDataViewModel:: */ ItemChanged( GetItem(row) );
}

void wxDataViewIndexListModel::RowValueChanged( unsigned int row, unsigned int col )
{
    /* wxDataViewModel:: */ ValueChanged( GetItem(row), col );
}

unsigned int wxDataViewIndexListModel::GetRow( const wxDataViewItem &item ) const
{
    if (m_ordered)
        return wxPtrToUInt(item.GetID())-1;

    // assert for not found
    return (unsigned int) m_hash.Index( item );
}

wxDataViewItem wxDataViewIndexListModel::GetItem( unsigned int row ) const
{
    wxASSERT( row < m_hash.GetCount() );
    return wxDataViewItem( m_hash[row] );
}

unsigned int wxDataViewIndexListModel::GetChildren( const wxDataViewItem &item, wxDataViewItemArray &children ) const
{
    if (item.IsOk())
        return 0;

    children = m_hash;

    return m_hash.GetCount();
}

// ---------------------------------------------------------
// wxDataViewVirtualListModel
// ---------------------------------------------------------

#ifndef __WXMAC__

wxDataViewVirtualListModel::wxDataViewVirtualListModel( unsigned int initial_size )
{
    m_size = initial_size;
}

void wxDataViewVirtualListModel::Reset( unsigned int new_size )
{
    /* wxDataViewModel:: */ BeforeReset();

    m_size = new_size;

    /* wxDataViewModel:: */ AfterReset();
}

void wxDataViewVirtualListModel::RowPrepended()
{
    m_size++;
    wxDataViewItem item( wxUIntToPtr(1) );
    ItemAdded( wxDataViewItem(0), item );
}

void wxDataViewVirtualListModel::RowInserted( unsigned int before )
{
    m_size++;
    wxDataViewItem item( wxUIntToPtr(before+1) );
    ItemAdded( wxDataViewItem(0), item );
}

void wxDataViewVirtualListModel::RowAppended()
{
    m_size++;
    wxDataViewItem item( wxUIntToPtr(m_size) );
    ItemAdded( wxDataViewItem(0), item );
}

void wxDataViewVirtualListModel::RowDeleted( unsigned int row )
{
    m_size--;
    wxDataViewItem item( wxUIntToPtr(row+1) );
    /* wxDataViewModel:: */ ItemDeleted( wxDataViewItem(0), item );
}

void wxDataViewVirtualListModel::RowsDeleted( const wxArrayInt &rows )
{
    m_size -= rows.GetCount();

    wxArrayInt sorted = rows;
    sorted.Sort( my_sort );

    wxDataViewItemArray array;
    unsigned int i;
    for (i = 0; i < sorted.GetCount(); i++)
    {
        wxDataViewItem item( wxUIntToPtr(sorted[i]+1) );
        array.Add( item );
    }
    /* wxDataViewModel:: */ ItemsDeleted( wxDataViewItem(0), array );
}

void wxDataViewVirtualListModel::RowChanged( unsigned int row )
{
    /* wxDataViewModel:: */ ItemChanged( GetItem(row) );
}

void wxDataViewVirtualListModel::RowValueChanged( unsigned int row, unsigned int col )
{
    /* wxDataViewModel:: */ ValueChanged( GetItem(row), col );
}

unsigned int wxDataViewVirtualListModel::GetRow( const wxDataViewItem &item ) const
{
    return wxPtrToUInt( item.GetID() ) -1;
}

wxDataViewItem wxDataViewVirtualListModel::GetItem( unsigned int row ) const
{
    return wxDataViewItem( wxUIntToPtr(row+1) );
}

bool wxDataViewVirtualListModel::HasDefaultCompare() const
{
    return true;
}

int wxDataViewVirtualListModel::Compare(const wxDataViewItem& item1,
                                      const wxDataViewItem& item2,
                                      unsigned int WXUNUSED(column),
                                      bool ascending) const
{
    unsigned int pos1 = wxPtrToUInt(item1.GetID());  // -1 not needed here
    unsigned int pos2 = wxPtrToUInt(item2.GetID());  // -1 not needed here

    if (ascending)
       return pos1 - pos2;
    else
       return pos2 - pos1;
}

unsigned int wxDataViewVirtualListModel::GetChildren( const wxDataViewItem &WXUNUSED(item), wxDataViewItemArray &WXUNUSED(children) ) const
{
    return 0;  // should we report an error ?
}

#endif  // __WXMAC__

//-----------------------------------------------------------------------------
// wxDataViewIconText
//-----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxDataViewIconText,wxObject)

IMPLEMENT_VARIANT_OBJECT_EXPORTED(wxDataViewIconText, WXDLLIMPEXP_ADV)

// ---------------------------------------------------------
// wxDataViewRendererBase
// ---------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxDataViewRendererBase, wxObject)

wxDataViewRendererBase::wxDataViewRendererBase( const wxString &varianttype,
                                                wxDataViewCellMode WXUNUSED(mode),
                                                int WXUNUSED(align) )
{
    m_variantType = varianttype;
    m_owner = NULL;
}

wxDataViewRendererBase::~wxDataViewRendererBase()
{
    if ( m_editorCtrl )
        DestroyEditControl();
}

wxDataViewCtrl* wxDataViewRendererBase::GetView() const
{
    return const_cast<wxDataViewRendererBase*>(this)->GetOwner()->GetOwner();
}

bool wxDataViewRendererBase::StartEditing( const wxDataViewItem &item, wxRect labelRect )
{
    wxDataViewCtrl* dv_ctrl = GetOwner()->GetOwner();

    // Before doing anything we send an event asking if editing of this item is really wanted.
    wxDataViewEvent start_event( wxEVT_DATAVIEW_ITEM_START_EDITING, dv_ctrl->GetId() );
    start_event.SetDataViewColumn( GetOwner() );
    start_event.SetModel( dv_ctrl->GetModel() );
    start_event.SetItem( item );
    start_event.SetEventObject( dv_ctrl );
    dv_ctrl->GetEventHandler()->ProcessEvent( start_event );
    if( !start_event.IsAllowed() )
        return false;

    m_item = item; // remember for later

    unsigned int col = GetOwner()->GetModelColumn();
    wxVariant value;
    dv_ctrl->GetModel()->GetValue( value, item, col );

    m_editorCtrl = CreateEditorCtrl( dv_ctrl->GetMainWindow(), labelRect, value );

    // there might be no editor control for the given item
    if(!m_editorCtrl)
        return false;

    wxDataViewEditorCtrlEvtHandler *handler =
        new wxDataViewEditorCtrlEvtHandler( m_editorCtrl, (wxDataViewRenderer*) this );

    m_editorCtrl->PushEventHandler( handler );

#if defined(__WXGTK20__) && !defined(wxUSE_GENERICDATAVIEWCTRL)
    handler->SetFocusOnIdle();
#else
    m_editorCtrl->SetFocus();
#endif

    // Now we should send Editing Started event
    wxDataViewEvent event( wxEVT_DATAVIEW_ITEM_EDITING_STARTED, dv_ctrl->GetId() );
    event.SetDataViewColumn( GetOwner() );
    event.SetModel( dv_ctrl->GetModel() );
    event.SetItem( item );
    event.SetEventObject( dv_ctrl );
    dv_ctrl->GetEventHandler()->ProcessEvent( event );

    return true;
}

void wxDataViewRendererBase::DestroyEditControl()
{
    // Remove our event handler first to prevent it from (recursively) calling
    // us again as it would do via a call to FinishEditing() when the editor
    // loses focus when we hide it below.
    wxEvtHandler * const handler = m_editorCtrl->PopEventHandler();

    // Hide the control immediately but don't delete it yet as there could be
    // some pending messages for it.
    m_editorCtrl->Hide();

    wxPendingDelete.Append(handler);
    wxPendingDelete.Append(m_editorCtrl);

    // Ensure that DestroyEditControl() is not called again for this control.
    m_editorCtrl.Release();
}

void wxDataViewRendererBase::CancelEditing()
{
    if (!m_editorCtrl)
        return;

    DestroyEditControl();
}

bool wxDataViewRendererBase::FinishEditing()
{
    if (!m_editorCtrl)
        return true;

    wxVariant value;
    GetValueFromEditorCtrl( m_editorCtrl, value );

    wxDataViewCtrl* dv_ctrl = GetOwner()->GetOwner();

    DestroyEditControl();

    dv_ctrl->GetMainWindow()->SetFocus();

    bool isValid = Validate(value);
    unsigned int col = GetOwner()->GetModelColumn();

    // Now we should send Editing Done event
    wxDataViewEvent event( wxEVT_DATAVIEW_ITEM_EDITING_DONE, dv_ctrl->GetId() );
    event.SetDataViewColumn( GetOwner() );
    event.SetModel( dv_ctrl->GetModel() );
    event.SetItem( m_item );
    event.SetValue( value );
    event.SetColumn( col );
    event.SetEditCanceled( !isValid );
    event.SetEventObject( dv_ctrl );
    dv_ctrl->GetEventHandler()->ProcessEvent( event );

    if ( isValid && event.IsAllowed() )
    {
        dv_ctrl->GetModel()->ChangeValue(value, m_item, col);
        return true;
    }

    return false;
}

void wxDataViewRendererBase::PrepareForItem(const wxDataViewModel *model,
                                            const wxDataViewItem& item,
                                            unsigned column)
{
    wxVariant value;
    model->GetValue(value, item, column);
    SetValue(value);

    wxDataViewItemAttr attr;
    model->GetAttr(item, column, attr);
    SetAttr(attr);

    SetEnabled(model->IsEnabled(item, column));
}


int wxDataViewRendererBase::GetEffectiveAlignment() const
{
    int alignment = GetAlignment();

    if ( alignment == wxDVR_DEFAULT_ALIGNMENT )
    {
        // if we don't have an explicit alignment ourselves, use that of the
        // column in horizontal direction and default vertical alignment
        alignment = GetOwner()->GetAlignment() | wxALIGN_CENTRE_VERTICAL;
    }

    return alignment;
}

// ----------------------------------------------------------------------------
// wxDataViewCustomRendererBase
// ----------------------------------------------------------------------------

bool wxDataViewCustomRendererBase::ActivateCell(const wxRect& cell,
                                                wxDataViewModel *model,
                                                const wxDataViewItem & item,
                                                unsigned int col,
                                                const wxMouseEvent* mouseEvent)
{
    // Compatibility code
    if ( mouseEvent )
        return LeftClick(mouseEvent->GetPosition(), cell, model, item, col);
    else
        return Activate(cell, model, item, col);
}

void wxDataViewCustomRendererBase::RenderBackground(wxDC* dc, const wxRect& rect)
{
    if ( !m_attr.HasBackgroundColour() )
        return;

    const wxColour& colour = m_attr.GetBackgroundColour();
    wxDCPenChanger changePen(*dc, colour);
    wxDCBrushChanger changeBrush(*dc, colour);

    dc->DrawRectangle(rect);
}

void
wxDataViewCustomRendererBase::WXCallRender(wxRect rectCell, wxDC *dc, int state)
{
    wxCHECK_RET( dc, "no DC to draw on in custom renderer?" );

    // adjust the rectangle ourselves to account for the alignment
    wxRect rectItem = rectCell;
    const int align = GetEffectiveAlignment();

    const wxSize size = GetSize();

    // take alignment into account only if there is enough space, otherwise
    // show as much contents as possible
    //
    // notice that many existing renderers (e.g. wxDataViewSpinRenderer)
    // return hard-coded size which can be more than they need and if we
    // trusted their GetSize() we'd draw the text out of cell bounds
    // entirely

    if ( size.x >= 0 && size.x < rectCell.width )
    {
        if ( align & wxALIGN_CENTER_HORIZONTAL )
            rectItem.x += (rectCell.width - size.x)/2;
        else if ( align & wxALIGN_RIGHT )
            rectItem.x += rectCell.width - size.x;
        // else: wxALIGN_LEFT is the default

        rectItem.width = size.x;
    }

    if ( size.y >= 0 && size.y < rectCell.height )
    {
        if ( align & wxALIGN_CENTER_VERTICAL )
            rectItem.y += (rectCell.height - size.y)/2;
        else if ( align & wxALIGN_BOTTOM )
            rectItem.y += rectCell.height - size.y;
        // else: wxALIGN_TOP is the default

        rectItem.height = size.y;
    }


    // set up the DC attributes

    // override custom foreground with the standard one for the selected items
    // because we currently don't allow changing the selection background and
    // custom colours may be unreadable on it
    wxColour col;
    if ( state & wxDATAVIEW_CELL_SELECTED )
        col = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT);
    else if ( m_attr.HasColour() )
        col = m_attr.GetColour();
    else // use default foreground
        col = GetOwner()->GetOwner()->GetForegroundColour();

    wxDCTextColourChanger changeFg(*dc, col);

    wxDCFontChanger changeFont(*dc);
    if ( m_attr.HasFont() )
        changeFont.Set(m_attr.GetEffectiveFont(dc->GetFont()));

    Render(rectItem, dc, state);
}

wxSize wxDataViewCustomRendererBase::GetTextExtent(const wxString& str) const
{
    const wxDataViewCtrl *view = GetView();

    if ( m_attr.HasFont() )
    {
        wxFont font(m_attr.GetEffectiveFont(view->GetFont()));
        wxSize size;
        view->GetTextExtent(str, &size.x, &size.y, NULL, NULL, &font);
        return size;
    }
    else
    {
        return view->GetTextExtent(str);
    }
}

void
wxDataViewCustomRendererBase::RenderText(const wxString& text,
                                         int xoffset,
                                         wxRect rect,
                                         wxDC *dc,
                                         int WXUNUSED(state))
{
    wxRect rectText = rect;
    rectText.x += xoffset;
    rectText.width -= xoffset;

    // check if we want to ellipsize the text if it doesn't fit
    wxString ellipsizedText;
    if ( GetEllipsizeMode() != wxELLIPSIZE_NONE )
    {
        ellipsizedText = wxControl::Ellipsize
                                    (
                                        text,
                                        *dc,
                                        GetEllipsizeMode(),
                                        rectText.width,
                                        wxELLIPSIZE_FLAGS_NONE
                                    );
    }

    // get the alignment to use
    dc->DrawLabel(ellipsizedText.empty() ? text : ellipsizedText,
                  rectText, GetEffectiveAlignment());
}

//-----------------------------------------------------------------------------
// wxDataViewEditorCtrlEvtHandler
//-----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxDataViewEditorCtrlEvtHandler, wxEvtHandler)
    EVT_CHAR           (wxDataViewEditorCtrlEvtHandler::OnChar)
    EVT_KILL_FOCUS     (wxDataViewEditorCtrlEvtHandler::OnKillFocus)
    EVT_IDLE           (wxDataViewEditorCtrlEvtHandler::OnIdle)
    EVT_TEXT_ENTER     (-1, wxDataViewEditorCtrlEvtHandler::OnTextEnter)
END_EVENT_TABLE()

void wxDataViewEditorCtrlEvtHandler::OnIdle( wxIdleEvent &event )
{
    if (m_focusOnIdle)
    {
        m_focusOnIdle = false;
        if (wxWindow::FindFocus() != m_editorCtrl)
            m_editorCtrl->SetFocus();
    }

    event.Skip();
}

void wxDataViewEditorCtrlEvtHandler::OnTextEnter( wxCommandEvent &WXUNUSED(event) )
{
    m_finished = true;
    m_owner->FinishEditing();
}

void wxDataViewEditorCtrlEvtHandler::OnChar( wxKeyEvent &event )
{
    switch ( event.m_keyCode )
    {
        case WXK_RETURN:
            m_finished = true;
            m_owner->FinishEditing();
            break;

        case WXK_ESCAPE:
        {
            m_finished = true;
            m_owner->CancelEditing();
            break;
        }
        default:
            event.Skip();
    }
}

void wxDataViewEditorCtrlEvtHandler::OnKillFocus( wxFocusEvent &event )
{
    if (!m_finished)
    {
        m_finished = true;
        m_owner->FinishEditing();
    }

    event.Skip();
}

// ---------------------------------------------------------
// wxDataViewColumnBase
// ---------------------------------------------------------

void wxDataViewColumnBase::Init(wxDataViewRenderer *renderer,
                                unsigned int model_column)
{
    m_renderer = renderer;
    m_model_column = model_column;
    m_owner = NULL;
    m_renderer->SetOwner( (wxDataViewColumn*) this );
}

wxDataViewColumnBase::~wxDataViewColumnBase()
{
    delete m_renderer;
}

// ---------------------------------------------------------
// wxDataViewCtrlBase
// ---------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxDataViewCtrlBase, wxControl)

wxDataViewCtrlBase::wxDataViewCtrlBase()
{
    m_model = NULL;
    m_expander_column = 0;
    m_indent = 8;
}

wxDataViewCtrlBase::~wxDataViewCtrlBase()
{
    if (m_model)
    {
        m_model->DecRef();
        m_model = NULL;
    }
}

bool wxDataViewCtrlBase::AssociateModel( wxDataViewModel *model )
{
    if (m_model)
    {
        m_model->DecRef();   // discard old model, if any
    }

    // add our own reference to the new model:
    m_model = model;
    if (m_model)
    {
        m_model->IncRef();
    }

    return true;
}

wxDataViewModel* wxDataViewCtrlBase::GetModel()
{
    return m_model;
}

const wxDataViewModel* wxDataViewCtrlBase::GetModel() const
{
    return m_model;
}

void wxDataViewCtrlBase::ExpandAncestors( const wxDataViewItem & item )
{
    if (!m_model) return;

    if (!item.IsOk()) return;

    wxVector<wxDataViewItem> parentChain;

    // at first we get all the parents of the selected item
    wxDataViewItem parent = m_model->GetParent(item);
    while (parent.IsOk())
    {
        parentChain.push_back(parent);
        parent = m_model->GetParent(parent);
    }

    // then we expand the parents, starting at the root
    while (!parentChain.empty())
    {
         Expand(parentChain.back());
         parentChain.pop_back();
    }
}

wxDataViewItem wxDataViewCtrlBase::GetCurrentItem() const
{
    return HasFlag(wxDV_MULTIPLE) ? DoGetCurrentItem()
                                  : GetSelection();
}

void wxDataViewCtrlBase::SetCurrentItem(const wxDataViewItem& item)
{
    wxCHECK_RET( item.IsOk(), "Can't make current an invalid item." );

    if ( HasFlag(wxDV_MULTIPLE) )
        DoSetCurrentItem(item);
    else
        Select(item);
}

wxDataViewItem wxDataViewCtrlBase::GetSelection() const
{
    if ( GetSelectedItemsCount() != 1 )
        return wxDataViewItem();

    wxDataViewItemArray selections;
    GetSelections(selections);
    return selections[0];
}

wxDataViewColumn *
wxDataViewCtrlBase::AppendTextColumn( const wxString &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewTextRenderer( wxT("string"), mode ),
        model_column, width, align, flags );
    AppendColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::AppendIconTextColumn( const wxString &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewIconTextRenderer( wxT("wxDataViewIconText"), mode ),
        model_column, width, align, flags );
    AppendColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::AppendToggleColumn( const wxString &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewToggleRenderer( wxT("bool"), mode ),
        model_column, width, align, flags );
    AppendColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::AppendProgressColumn( const wxString &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewProgressRenderer( wxEmptyString, wxT("long"), mode ),
        model_column, width, align, flags );
    AppendColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::AppendDateColumn( const wxString &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewDateRenderer( wxT("datetime"), mode ),
        model_column, width, align, flags );
    AppendColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::AppendBitmapColumn( const wxString &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewBitmapRenderer( wxT("wxBitmap"), mode ),
        model_column, width, align, flags );
    AppendColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::AppendTextColumn( const wxBitmap &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewTextRenderer( wxT("string"), mode ),
        model_column, width, align, flags );
    AppendColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::AppendIconTextColumn( const wxBitmap &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewIconTextRenderer( wxT("wxDataViewIconText"), mode ),
        model_column, width, align, flags );
    AppendColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::AppendToggleColumn( const wxBitmap &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewToggleRenderer( wxT("bool"), mode ),
        model_column, width, align, flags );
    AppendColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::AppendProgressColumn( const wxBitmap &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewProgressRenderer( wxEmptyString, wxT("long"), mode ),
        model_column, width, align, flags );
    AppendColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::AppendDateColumn( const wxBitmap &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewDateRenderer( wxT("datetime"), mode ),
        model_column, width, align, flags );
    AppendColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::AppendBitmapColumn( const wxBitmap &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewBitmapRenderer( wxT("wxBitmap"), mode ),
        model_column, width, align, flags );
    AppendColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::PrependTextColumn( const wxString &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewTextRenderer( wxT("string"), mode ),
        model_column, width, align, flags );
    PrependColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::PrependIconTextColumn( const wxString &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewIconTextRenderer( wxT("wxDataViewIconText"), mode ),
        model_column, width, align, flags );
    PrependColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::PrependToggleColumn( const wxString &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{

    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewToggleRenderer( wxT("bool"), mode ),
        model_column, width, align, flags );
    PrependColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::PrependProgressColumn( const wxString &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewProgressRenderer( wxEmptyString, wxT("long"), mode ),
        model_column, width, align, flags );
    PrependColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::PrependDateColumn( const wxString &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewDateRenderer( wxT("datetime"), mode ),
        model_column, width, align, flags );
    PrependColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::PrependBitmapColumn( const wxString &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewBitmapRenderer( wxT("wxBitmap"), mode ),
        model_column, width, align, flags );
    PrependColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::PrependTextColumn( const wxBitmap &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewTextRenderer( wxT("string"), mode ),
        model_column, width, align, flags );
    PrependColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::PrependIconTextColumn( const wxBitmap &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewIconTextRenderer( wxT("wxDataViewIconText"), mode ),
        model_column, width, align, flags );
    PrependColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::PrependToggleColumn( const wxBitmap &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewToggleRenderer( wxT("bool"), mode ),
        model_column, width, align, flags );
    PrependColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::PrependProgressColumn( const wxBitmap &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewProgressRenderer( wxEmptyString, wxT("long"), mode ),
        model_column, width, align, flags );
    PrependColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::PrependDateColumn( const wxBitmap &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewDateRenderer( wxT("datetime"), mode ),
        model_column, width, align, flags );
    PrependColumn( ret );
    return ret;
}

wxDataViewColumn *
wxDataViewCtrlBase::PrependBitmapColumn( const wxBitmap &label, unsigned int model_column,
                            wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewBitmapRenderer( wxT("wxBitmap"), mode ),
        model_column, width, align, flags );
    PrependColumn( ret );
    return ret;
}

bool
wxDataViewCtrlBase::AppendColumn( wxDataViewColumn *col )
{
    col->SetOwner( (wxDataViewCtrl*) this );
    return true;
}

bool
wxDataViewCtrlBase::PrependColumn( wxDataViewColumn *col )
{
    col->SetOwner( (wxDataViewCtrl*) this );
    return true;
}

bool
wxDataViewCtrlBase::InsertColumn( unsigned int WXUNUSED(pos), wxDataViewColumn *col )
{
    col->SetOwner( (wxDataViewCtrl*) this );
    return true;
}

void wxDataViewCtrlBase::StartEditor(const wxDataViewItem& item, unsigned int column)
{
    EditItem(item, GetColumn(column));
}

// ---------------------------------------------------------
// wxDataViewEvent
// ---------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxDataViewEvent,wxNotifyEvent)

wxDEFINE_EVENT( wxEVT_DATAVIEW_SELECTION_CHANGED, wxDataViewEvent );

wxDEFINE_EVENT( wxEVT_DATAVIEW_ITEM_ACTIVATED, wxDataViewEvent );
wxDEFINE_EVENT( wxEVT_DATAVIEW_ITEM_COLLAPSING, wxDataViewEvent );
wxDEFINE_EVENT( wxEVT_DATAVIEW_ITEM_COLLAPSED, wxDataViewEvent );
wxDEFINE_EVENT( wxEVT_DATAVIEW_ITEM_EXPANDING, wxDataViewEvent );
wxDEFINE_EVENT( wxEVT_DATAVIEW_ITEM_EXPANDED, wxDataViewEvent );
wxDEFINE_EVENT( wxEVT_DATAVIEW_ITEM_EDITING_STARTED, wxDataViewEvent );
wxDEFINE_EVENT( wxEVT_DATAVIEW_ITEM_START_EDITING, wxDataViewEvent );
wxDEFINE_EVENT( wxEVT_DATAVIEW_ITEM_EDITING_DONE, wxDataViewEvent );
wxDEFINE_EVENT( wxEVT_DATAVIEW_ITEM_VALUE_CHANGED, wxDataViewEvent );

wxDEFINE_EVENT( wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, wxDataViewEvent );

wxDEFINE_EVENT( wxEVT_DATAVIEW_COLUMN_HEADER_CLICK, wxDataViewEvent );
wxDEFINE_EVENT( wxEVT_DATAVIEW_COLUMN_HEADER_RIGHT_CLICK, wxDataViewEvent );
wxDEFINE_EVENT( wxEVT_DATAVIEW_COLUMN_SORTED, wxDataViewEvent );
wxDEFINE_EVENT( wxEVT_DATAVIEW_COLUMN_REORDERED, wxDataViewEvent );

wxDEFINE_EVENT( wxEVT_DATAVIEW_CACHE_HINT, wxDataViewEvent );

wxDEFINE_EVENT( wxEVT_DATAVIEW_ITEM_BEGIN_DRAG, wxDataViewEvent );
wxDEFINE_EVENT( wxEVT_DATAVIEW_ITEM_DROP_POSSIBLE, wxDataViewEvent );
wxDEFINE_EVENT( wxEVT_DATAVIEW_ITEM_DROP, wxDataViewEvent );



// -------------------------------------
// wxDataViewSpinRenderer
// -------------------------------------

wxDataViewSpinRenderer::wxDataViewSpinRenderer( int min, int max, wxDataViewCellMode mode, int alignment ) :
   wxDataViewCustomRenderer(wxT("long"), mode, alignment )
{
    m_min = min;
    m_max = max;
}

wxWindow* wxDataViewSpinRenderer::CreateEditorCtrl( wxWindow *parent, wxRect labelRect, const wxVariant &value )
{
    long l = value;
    wxString str;
    str.Printf( wxT("%d"), (int) l );
    wxSpinCtrl *sc = new wxSpinCtrl( parent, wxID_ANY, str,
               labelRect.GetTopLeft(), labelRect.GetSize(), wxSP_ARROW_KEYS|wxTE_PROCESS_ENTER, m_min, m_max, l );
#ifdef __WXMAC__
    const wxSize size = sc->GetSize();
    wxPoint pt = sc->GetPosition();
    sc->SetSize( pt.x - 4, pt.y - 4, size.x, size.y );
#endif

    return sc;
}

bool wxDataViewSpinRenderer::GetValueFromEditorCtrl( wxWindow* editor, wxVariant &value )
{
    wxSpinCtrl *sc = (wxSpinCtrl*) editor;
    long l = sc->GetValue();
    value = l;
    return true;
}

bool wxDataViewSpinRenderer::Render( wxRect rect, wxDC *dc, int state )
{
    wxString str;
    str.Printf(wxT("%d"), (int) m_data );
    RenderText( str, 0, rect, dc, state );
    return true;
}

wxSize wxDataViewSpinRenderer::GetSize() const
{
    wxSize sz = GetTextExtent(wxString::Format("%d", (int)m_data));

    // Allow some space for the spin buttons, which is approximately the size
    // of a scrollbar (and getting pixel-exact value would be complicated).
    // Also add some whitespace between the text and the button:
    sz.x += wxSystemSettings::GetMetric(wxSYS_VSCROLL_X);
    sz.x += GetTextExtent("M").x;

    return sz;
}

bool wxDataViewSpinRenderer::SetValue( const wxVariant &value )
{
    m_data = value.GetLong();
    return true;
}

bool wxDataViewSpinRenderer::GetValue( wxVariant &value ) const
{
    value = m_data;
    return true;
}

// -------------------------------------
// wxDataViewChoiceRenderer
// -------------------------------------

#if defined(wxHAS_GENERIC_DATAVIEWCTRL) || defined(__WXOSX_CARBON__)

wxDataViewChoiceRenderer::wxDataViewChoiceRenderer( const wxArrayString& choices, wxDataViewCellMode mode, int alignment ) :
   wxDataViewCustomRenderer(wxT("string"), mode, alignment )
{
    m_choices = choices;
}

wxWindow* wxDataViewChoiceRenderer::CreateEditorCtrl( wxWindow *parent, wxRect labelRect, const wxVariant &value )
{
    wxChoice* c = new wxChoice
                      (
                          parent,
                          wxID_ANY,
                          labelRect.GetTopLeft(),
                          wxSize(labelRect.GetWidth(), -1),
                          m_choices
                      );
    c->Move(labelRect.GetRight() - c->GetRect().width, wxDefaultCoord);
    c->SetStringSelection( value.GetString() );
    return c;
}

bool wxDataViewChoiceRenderer::GetValueFromEditorCtrl( wxWindow* editor, wxVariant &value )
{
    wxChoice *c = (wxChoice*) editor;
    wxString s = c->GetStringSelection();
    value = s;
    return true;
}

bool wxDataViewChoiceRenderer::Render( wxRect rect, wxDC *dc, int state )
{
    RenderText( m_data, 0, rect, dc, state );
    return true;
}

wxSize wxDataViewChoiceRenderer::GetSize() const
{
    wxSize sz;

    for ( wxArrayString::const_iterator i = m_choices.begin(); i != m_choices.end(); ++i )
        sz.IncTo(GetTextExtent(*i));

    // Allow some space for the right-side button, which is approximately the
    // size of a scrollbar (and getting pixel-exact value would be complicated).
    // Also add some whitespace between the text and the button:
    sz.x += wxSystemSettings::GetMetric(wxSYS_VSCROLL_X);
    sz.x += GetTextExtent("M").x;

    return sz;
}

bool wxDataViewChoiceRenderer::SetValue( const wxVariant &value )
{
    m_data = value.GetString();
    return true;
}

bool wxDataViewChoiceRenderer::GetValue( wxVariant &value ) const
{
    value = m_data;
    return true;
}

// ----------------------------------------------------------------------------
// wxDataViewChoiceByIndexRenderer
// ----------------------------------------------------------------------------

wxDataViewChoiceByIndexRenderer::wxDataViewChoiceByIndexRenderer( const wxArrayString &choices,
                                  wxDataViewCellMode mode, int alignment ) :
      wxDataViewChoiceRenderer( choices, mode, alignment )
{
}

wxWindow* wxDataViewChoiceByIndexRenderer::CreateEditorCtrl( wxWindow *parent, wxRect labelRect, const wxVariant &value )
{
    wxVariant string_value = GetChoice( value.GetLong() );

    return wxDataViewChoiceRenderer::CreateEditorCtrl( parent, labelRect, string_value );
}

bool wxDataViewChoiceByIndexRenderer::GetValueFromEditorCtrl( wxWindow* editor, wxVariant &value )
{
    wxVariant string_value;
    if (!wxDataViewChoiceRenderer::GetValueFromEditorCtrl( editor, string_value ))
        return false;

    value = (long) GetChoices().Index( string_value.GetString() );
    return true;
}

bool wxDataViewChoiceByIndexRenderer::SetValue( const wxVariant &value )
{
    wxVariant string_value = GetChoice( value.GetLong() );
    return wxDataViewChoiceRenderer::SetValue( string_value );
}

bool wxDataViewChoiceByIndexRenderer::GetValue( wxVariant &value ) const
{
    wxVariant string_value;
    if (!wxDataViewChoiceRenderer::GetValue( string_value ))
        return false;

    value = (long) GetChoices().Index( string_value.GetString() );
    return true;
}

#endif

// ---------------------------------------------------------
// wxDataViewDateRenderer
// ---------------------------------------------------------

#if (defined(wxHAS_GENERIC_DATAVIEWCTRL) || defined(__WXGTK__)) && wxUSE_DATEPICKCTRL

wxDataViewDateRenderer::wxDataViewDateRenderer(const wxString& varianttype,
                                              wxDataViewCellMode mode, int align)
    : wxDataViewCustomRenderer(varianttype, mode, align)
{
}

wxWindow *
wxDataViewDateRenderer::CreateEditorCtrl(wxWindow *parent, wxRect labelRect, const wxVariant& value)
{
    return new wxDatePickerCtrl
               (
                   parent,
                   wxID_ANY,
                   value.GetDateTime(),
                   labelRect.GetTopLeft(),
                   labelRect.GetSize()
               );
}

bool wxDataViewDateRenderer::GetValueFromEditorCtrl(wxWindow *editor, wxVariant& value)
{
    wxDatePickerCtrl *ctrl = static_cast<wxDatePickerCtrl*>(editor);
    value = ctrl->GetValue();
    return true;
}

bool wxDataViewDateRenderer::SetValue(const wxVariant& value)
{
    m_date = value.GetDateTime();
    return true;
}

bool wxDataViewDateRenderer::GetValue(wxVariant& value) const
{
    value = m_date;
    return true;
}

bool wxDataViewDateRenderer::Render(wxRect cell, wxDC* dc, int state)
{
    wxString tmp = m_date.FormatDate();
    RenderText( tmp, 0, cell, dc, state );
    return true;
}

wxSize wxDataViewDateRenderer::GetSize() const
{
    return GetTextExtent(m_date.FormatDate());
}

#endif // (defined(wxHAS_GENERIC_DATAVIEWCTRL) || defined(__WXGTK__)) && wxUSE_DATEPICKCTRL

//-----------------------------------------------------------------------------
// wxDataViewListStore
//-----------------------------------------------------------------------------

wxDataViewListStore::wxDataViewListStore()
{
}

wxDataViewListStore::~wxDataViewListStore()
{
    wxVector<wxDataViewListStoreLine*>::iterator it;
    for (it = m_data.begin(); it != m_data.end(); ++it)
    {
        wxDataViewListStoreLine* line = *it;
        delete line;
    }
}

void wxDataViewListStore::PrependColumn( const wxString &varianttype )
{
    m_cols.Insert( varianttype, 0 );
}

void wxDataViewListStore::InsertColumn( unsigned int pos, const wxString &varianttype )
{
    m_cols.Insert( varianttype, pos );
}

void wxDataViewListStore::AppendColumn( const wxString &varianttype )
{
    m_cols.Add( varianttype );
}

unsigned int wxDataViewListStore::GetColumnCount() const
{
    return m_cols.GetCount();
}

unsigned int wxDataViewListStore::GetItemCount() const
{
    return m_data.size();
}

wxString wxDataViewListStore::GetColumnType( unsigned int pos ) const
{
    return m_cols[pos];
}

void wxDataViewListStore::AppendItem( const wxVector<wxVariant> &values, wxUIntPtr data )
{
    wxDataViewListStoreLine *line = new wxDataViewListStoreLine( data );
    line->m_values = values;
    m_data.push_back( line );

    RowAppended();
}

void wxDataViewListStore::PrependItem( const wxVector<wxVariant> &values, wxUIntPtr data )
{
    wxDataViewListStoreLine *line = new wxDataViewListStoreLine( data );
    line->m_values = values;
    m_data.insert( m_data.begin(), line );

    RowPrepended();
}

void wxDataViewListStore::InsertItem(  unsigned int row, const wxVector<wxVariant> &values,
                                       wxUIntPtr data )
{
    wxDataViewListStoreLine *line = new wxDataViewListStoreLine( data );
    line->m_values = values;
    m_data.insert( m_data.begin()+row, line );

    RowInserted( row );
}

void wxDataViewListStore::DeleteItem( unsigned int row )
{
    wxVector<wxDataViewListStoreLine*>::iterator it = m_data.begin() + row;
    delete *it;
    m_data.erase( it );

    RowDeleted( row );
}

void wxDataViewListStore::DeleteAllItems()
{
    wxVector<wxDataViewListStoreLine*>::iterator it;
    for (it = m_data.begin(); it != m_data.end(); ++it)
    {
        wxDataViewListStoreLine* line = *it;
        delete line;
    }

    m_data.clear();

    Reset( 0 );
}

void wxDataViewListStore::ClearColumns()
{
    m_cols.clear();
}

void wxDataViewListStore::SetItemData( const wxDataViewItem& item, wxUIntPtr data )
{
    wxDataViewListStoreLine* line = m_data[GetRow(item)];
    if (!line) return;

    line->SetData( data );
}

wxUIntPtr wxDataViewListStore::GetItemData( const wxDataViewItem& item ) const
{
    wxDataViewListStoreLine* line = m_data[GetRow(item)];
    if (!line) return 0;

    return line->GetData();
}

void wxDataViewListStore::GetValueByRow( wxVariant &value, unsigned int row, unsigned int col ) const
{
    wxDataViewListStoreLine *line = m_data[row];
    value = line->m_values[col];
}

bool wxDataViewListStore::SetValueByRow( const wxVariant &value, unsigned int row, unsigned int col )
{
    wxDataViewListStoreLine *line = m_data[row];
    line->m_values[col] = value;

    return true;
}

//-----------------------------------------------------------------------------
// wxDataViewListCtrl
//-----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxDataViewListCtrl,wxDataViewCtrl)

BEGIN_EVENT_TABLE(wxDataViewListCtrl,wxDataViewCtrl)
   EVT_SIZE( wxDataViewListCtrl::OnSize )
END_EVENT_TABLE()

wxDataViewListCtrl::wxDataViewListCtrl()
{
}

wxDataViewListCtrl::wxDataViewListCtrl( wxWindow *parent, wxWindowID id,
           const wxPoint& pos, const wxSize& size, long style,
           const wxValidator& validator )
{
    Create( parent, id, pos, size, style, validator );
}

wxDataViewListCtrl::~wxDataViewListCtrl()
{
}


bool wxDataViewListCtrl::Create( wxWindow *parent, wxWindowID id,
           const wxPoint& pos, const wxSize& size, long style,
           const wxValidator& validator )
{
    if ( !wxDataViewCtrl::Create( parent, id, pos, size, style, validator ) )
        return false;

    wxDataViewListStore *store = new wxDataViewListStore;
    AssociateModel( store );
    store->DecRef();

    return true;
}

bool wxDataViewListCtrl::AppendColumn( wxDataViewColumn *column, const wxString &varianttype )
{
    GetStore()->AppendColumn( varianttype );
    return wxDataViewCtrl::AppendColumn( column );
}

bool wxDataViewListCtrl::PrependColumn( wxDataViewColumn *column, const wxString &varianttype )
{
    GetStore()->PrependColumn( varianttype );
    return wxDataViewCtrl::PrependColumn( column );
}

bool wxDataViewListCtrl::InsertColumn( unsigned int pos, wxDataViewColumn *column, const wxString &varianttype )
{
    GetStore()->InsertColumn( pos, varianttype );
    return wxDataViewCtrl::InsertColumn( pos, column );
}

bool wxDataViewListCtrl::PrependColumn( wxDataViewColumn *col )
{
    return PrependColumn( col, col->GetRenderer()->GetVariantType() );
}

bool wxDataViewListCtrl::InsertColumn( unsigned int pos, wxDataViewColumn *col )
{
    return InsertColumn( pos, col, col->GetRenderer()->GetVariantType() );
}

bool wxDataViewListCtrl::AppendColumn( wxDataViewColumn *col )
{
    return AppendColumn( col, col->GetRenderer()->GetVariantType() );
}

bool wxDataViewListCtrl::ClearColumns()
{
    GetStore()->ClearColumns();
    return wxDataViewCtrl::ClearColumns();
}

wxDataViewColumn *wxDataViewListCtrl::AppendTextColumn( const wxString &label,
          wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    GetStore()->AppendColumn( wxT("string") );

    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewTextRenderer( wxT("string"), mode ),
        GetStore()->GetColumnCount()-1, width, align, flags );

    wxDataViewCtrl::AppendColumn( ret );

    return ret;
}

wxDataViewColumn *wxDataViewListCtrl::AppendToggleColumn( const wxString &label,
          wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    GetStore()->AppendColumn( wxT("bool") );

    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewToggleRenderer( wxT("bool"), mode ),
        GetStore()->GetColumnCount()-1, width, align, flags );

    wxDataViewCtrl::AppendColumn( ret );

    return ret;
}

wxDataViewColumn *wxDataViewListCtrl::AppendProgressColumn( const wxString &label,
          wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    GetStore()->AppendColumn( wxT("long") );

    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewProgressRenderer( wxEmptyString, wxT("long"), mode ),
        GetStore()->GetColumnCount()-1, width, align, flags );

    wxDataViewCtrl::AppendColumn( ret );

    return ret;
}

wxDataViewColumn *wxDataViewListCtrl::AppendIconTextColumn( const wxString &label,
          wxDataViewCellMode mode, int width, wxAlignment align, int flags )
{
    GetStore()->AppendColumn( wxT("wxDataViewIconText") );

    wxDataViewColumn *ret = new wxDataViewColumn( label,
        new wxDataViewIconTextRenderer( wxT("wxDataViewIconText"), mode ),
        GetStore()->GetColumnCount()-1, width, align, flags );

    wxDataViewCtrl::AppendColumn( ret );

    return ret;
}

void wxDataViewListCtrl::OnSize( wxSizeEvent &event )
{
    event.Skip( true );
}

//-----------------------------------------------------------------------------
// wxDataViewTreeStore
//-----------------------------------------------------------------------------

wxDataViewTreeStoreNode::wxDataViewTreeStoreNode(
        wxDataViewTreeStoreNode *parent,
        const wxString &text, const wxIcon &icon, wxClientData *data )
{
    m_parent = parent;
    m_text = text;
    m_icon = icon;
    m_data = data;
}

wxDataViewTreeStoreNode::~wxDataViewTreeStoreNode()
{
    if (m_data)
        delete m_data;
}

#include "wx/listimpl.cpp"
WX_DEFINE_LIST(wxDataViewTreeStoreNodeList)

wxDataViewTreeStoreContainerNode::wxDataViewTreeStoreContainerNode(
        wxDataViewTreeStoreNode *parent, const wxString &text,
        const wxIcon &icon, const wxIcon &expanded, wxClientData *data ) :
    wxDataViewTreeStoreNode( parent, text, icon, data )
{
    m_iconExpanded = expanded;
    m_isExpanded = false;
    m_children.DeleteContents(true);
}

wxDataViewTreeStoreContainerNode::~wxDataViewTreeStoreContainerNode()
{
}

//-----------------------------------------------------------------------------

wxDataViewTreeStore::wxDataViewTreeStore()
{
    m_root = new wxDataViewTreeStoreContainerNode( NULL, wxEmptyString );
}

wxDataViewTreeStore::~wxDataViewTreeStore()
{
    delete m_root;
}

wxDataViewItem wxDataViewTreeStore::AppendItem( const wxDataViewItem& parent,
        const wxString &text, const wxIcon &icon, wxClientData *data )
{
    wxDataViewTreeStoreContainerNode *parent_node = FindContainerNode( parent );
    if (!parent_node) return wxDataViewItem(0);

    wxDataViewTreeStoreNode *node =
        new wxDataViewTreeStoreNode( parent_node, text, icon, data );
    parent_node->GetChildren().Append( node );

    return node->GetItem();
}

wxDataViewItem wxDataViewTreeStore::PrependItem( const wxDataViewItem& parent,
        const wxString &text, const wxIcon &icon, wxClientData *data )
{
    wxDataViewTreeStoreContainerNode *parent_node = FindContainerNode( parent );
    if (!parent_node) return wxDataViewItem(0);

    wxDataViewTreeStoreNode *node =
        new wxDataViewTreeStoreNode( parent_node, text, icon, data );
    parent_node->GetChildren().Insert( node );

    return node->GetItem();
}

wxDataViewItem
wxDataViewTreeStore::InsertItem(const wxDataViewItem& parent,
                                const wxDataViewItem& previous,
                                const wxString& text,
                                const wxIcon& icon,
                                wxClientData *data)
{
    wxDataViewTreeStoreContainerNode *parent_node = FindContainerNode( parent );
    if (!parent_node) return wxDataViewItem(0);

    wxDataViewTreeStoreNode *previous_node = FindNode( previous );
    int pos = parent_node->GetChildren().IndexOf( previous_node );
    if (pos == wxNOT_FOUND) return wxDataViewItem(0);

    wxDataViewTreeStoreNode *node =
        new wxDataViewTreeStoreNode( parent_node, text, icon, data );
    parent_node->GetChildren().Insert( (size_t) pos, node );

    return node->GetItem();
}

wxDataViewItem wxDataViewTreeStore::PrependContainer( const wxDataViewItem& parent,
        const wxString &text, const wxIcon &icon, const wxIcon &expanded,
        wxClientData *data )
{
    wxDataViewTreeStoreContainerNode *parent_node = FindContainerNode( parent );
    if (!parent_node) return wxDataViewItem(0);

    wxDataViewTreeStoreContainerNode *node =
        new wxDataViewTreeStoreContainerNode( parent_node, text, icon, expanded, data );
    parent_node->GetChildren().Insert( node );

    return node->GetItem();
}

wxDataViewItem
wxDataViewTreeStore::AppendContainer(const wxDataViewItem& parent,
                                     const wxString &text,
                                     const wxIcon& icon,
                                     const wxIcon& expanded,
                                     wxClientData * data)
{
    wxDataViewTreeStoreContainerNode *parent_node = FindContainerNode( parent );
    if (!parent_node) return wxDataViewItem(0);

    wxDataViewTreeStoreContainerNode *node =
        new wxDataViewTreeStoreContainerNode( parent_node, text, icon, expanded, data );
    parent_node->GetChildren().Append( node );

    return node->GetItem();
}

wxDataViewItem
wxDataViewTreeStore::InsertContainer(const wxDataViewItem& parent,
                                     const wxDataViewItem& previous,
                                     const wxString& text,
                                     const wxIcon& icon,
                                     const wxIcon& expanded,
                                     wxClientData * data)
{
    wxDataViewTreeStoreContainerNode *parent_node = FindContainerNode( parent );
    if (!parent_node) return wxDataViewItem(0);

    wxDataViewTreeStoreNode *previous_node = FindNode( previous );
    int pos = parent_node->GetChildren().IndexOf( previous_node );
    if (pos == wxNOT_FOUND) return wxDataViewItem(0);

    wxDataViewTreeStoreContainerNode *node =
        new wxDataViewTreeStoreContainerNode( parent_node, text, icon, expanded, data );
    parent_node->GetChildren().Insert( (size_t) pos, node );

    return node->GetItem();
}

bool wxDataViewTreeStore::IsContainer( const wxDataViewItem& item ) const
{
    wxDataViewTreeStoreNode *node = FindNode( item );
    if (!node) return false;

    return node->IsContainer();
}

wxDataViewItem wxDataViewTreeStore::GetNthChild( const wxDataViewItem& parent, unsigned int pos ) const
{
    wxDataViewTreeStoreContainerNode *parent_node = FindContainerNode( parent );
    if (!parent_node) return wxDataViewItem(0);

    wxDataViewTreeStoreNodeList::compatibility_iterator node = parent_node->GetChildren().Item( pos );
    if (node)
        return wxDataViewItem(node->GetData());

    return wxDataViewItem(0);
}

int wxDataViewTreeStore::GetChildCount( const wxDataViewItem& parent ) const
{
    wxDataViewTreeStoreNode *node = FindNode( parent );
    if (!node) return -1;

    if (!node->IsContainer())
        return 0;

    wxDataViewTreeStoreContainerNode *container_node = (wxDataViewTreeStoreContainerNode*) node;
    return (int) container_node->GetChildren().GetCount();
}

void wxDataViewTreeStore::SetItemText( const wxDataViewItem& item, const wxString &text )
{
    wxDataViewTreeStoreNode *node = FindNode( item );
    if (!node) return;

    node->SetText( text );
}

wxString wxDataViewTreeStore::GetItemText( const wxDataViewItem& item ) const
{
    wxDataViewTreeStoreNode *node = FindNode( item );
    if (!node) return wxEmptyString;

    return node->GetText();
}

void wxDataViewTreeStore::SetItemIcon( const wxDataViewItem& item, const wxIcon &icon )
{
    wxDataViewTreeStoreNode *node = FindNode( item );
    if (!node) return;

    node->SetIcon( icon );
}

const wxIcon &wxDataViewTreeStore::GetItemIcon( const wxDataViewItem& item ) const
{
    wxDataViewTreeStoreNode *node = FindNode( item );
    if (!node) return wxNullIcon;

    return node->GetIcon();
}

void wxDataViewTreeStore::SetItemExpandedIcon( const wxDataViewItem& item, const wxIcon &icon )
{
    wxDataViewTreeStoreContainerNode *node = FindContainerNode( item );
    if (!node) return;

    node->SetExpandedIcon( icon );
}

const wxIcon &wxDataViewTreeStore::GetItemExpandedIcon( const wxDataViewItem& item ) const
{
    wxDataViewTreeStoreContainerNode *node = FindContainerNode( item );
    if (!node) return wxNullIcon;

    return node->GetExpandedIcon();
}

void wxDataViewTreeStore::SetItemData( const wxDataViewItem& item, wxClientData *data )
{
    wxDataViewTreeStoreNode *node = FindNode( item );
    if (!node) return;

    node->SetData( data );
}

wxClientData *wxDataViewTreeStore::GetItemData( const wxDataViewItem& item ) const
{
    wxDataViewTreeStoreNode *node = FindNode( item );
    if (!node) return NULL;

    return node->GetData();
}

void wxDataViewTreeStore::DeleteItem( const wxDataViewItem& item )
{
    if (!item.IsOk()) return;

    wxDataViewItem parent_item = GetParent( item );

    wxDataViewTreeStoreContainerNode *parent_node = FindContainerNode( parent_item );
    if (!parent_node) return;

    parent_node->GetChildren().DeleteObject( FindNode(item) );
}

void wxDataViewTreeStore::DeleteChildren( const wxDataViewItem& item )
{
    wxDataViewTreeStoreContainerNode *node = FindContainerNode( item );
    if (!node) return;

    node->GetChildren().clear();
}

void wxDataViewTreeStore::DeleteAllItems()
{
    DeleteChildren(wxDataViewItem(m_root));
}

void
wxDataViewTreeStore::GetValue(wxVariant &variant,
                              const wxDataViewItem &item,
                              unsigned int WXUNUSED(col)) const
{
    // if (col != 0) return;

    wxDataViewTreeStoreNode *node = FindNode( item );
    if (!node) return;

    wxIcon icon( node->GetIcon());
    if (node->IsContainer())
    {
        wxDataViewTreeStoreContainerNode *container = (wxDataViewTreeStoreContainerNode*) node;
        if (container->IsExpanded() && container->GetExpandedIcon().IsOk())
           icon = container->GetExpandedIcon();
    }

    wxDataViewIconText data( node->GetText(), icon );

    variant << data;
}

bool
wxDataViewTreeStore::SetValue(const wxVariant& variant,
                              const wxDataViewItem& item,
                              unsigned int WXUNUSED(col))
{
    // if (col != 0) return false;

    wxDataViewTreeStoreNode *node = FindNode( item );
    if (!node) return false;

    wxDataViewIconText data;

    data << variant;

    node->SetText( data.GetText() );
    node->SetIcon( data.GetIcon() );

    return true;
}

wxDataViewItem wxDataViewTreeStore::GetParent( const wxDataViewItem &item ) const
{
    wxDataViewTreeStoreNode *node = FindNode( item );
    if (!node) return wxDataViewItem(0);

    wxDataViewTreeStoreNode *parent = node->GetParent();
    if (!parent) return wxDataViewItem(0);

    if (parent == m_root)
        return wxDataViewItem(0);

    return parent->GetItem();
}

unsigned int wxDataViewTreeStore::GetChildren( const wxDataViewItem &item, wxDataViewItemArray &children ) const
{
    wxDataViewTreeStoreContainerNode *node = FindContainerNode( item );
    if (!node) return 0;

    wxDataViewTreeStoreNodeList::iterator iter;
    for (iter = node->GetChildren().begin(); iter != node->GetChildren().end(); iter++)
    {
        wxDataViewTreeStoreNode* child = *iter;
        children.Add( child->GetItem() );
    }

    return node->GetChildren().GetCount();
}

int wxDataViewTreeStore::Compare( const wxDataViewItem &item1, const wxDataViewItem &item2,
                         unsigned int WXUNUSED(column), bool WXUNUSED(ascending) ) const
{
    wxDataViewTreeStoreNode *node1 = FindNode( item1 );
    wxDataViewTreeStoreNode *node2 = FindNode( item2 );

    if (!node1 || !node2)
        return 0;

    wxDataViewTreeStoreContainerNode* parent1 =
        (wxDataViewTreeStoreContainerNode*) node1->GetParent();
    wxDataViewTreeStoreContainerNode* parent2 =
        (wxDataViewTreeStoreContainerNode*) node2->GetParent();

    if (parent1 != parent2)
    {
        wxLogError( wxT("Comparing items with different parent.") );
        return 0;
    }

    if (node1->IsContainer() && !node2->IsContainer())
        return -1;

    if (node2->IsContainer() && !node1->IsContainer())
        return 1;

    return parent1->GetChildren().IndexOf( node1 ) - parent2->GetChildren().IndexOf( node2 );
}

wxDataViewTreeStoreNode *wxDataViewTreeStore::FindNode( const wxDataViewItem &item ) const
{
    if (!item.IsOk())
        return m_root;

    return (wxDataViewTreeStoreNode*) item.GetID();
}

wxDataViewTreeStoreContainerNode *wxDataViewTreeStore::FindContainerNode( const wxDataViewItem &item ) const
{
    if (!item.IsOk())
        return (wxDataViewTreeStoreContainerNode*) m_root;

    wxDataViewTreeStoreNode* node = (wxDataViewTreeStoreNode*) item.GetID();

    if (!node->IsContainer())
        return NULL;

    return (wxDataViewTreeStoreContainerNode*) node;
}

//-----------------------------------------------------------------------------
// wxDataViewTreeCtrl
//-----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxDataViewTreeCtrl,wxDataViewCtrl)

BEGIN_EVENT_TABLE(wxDataViewTreeCtrl,wxDataViewCtrl)
   EVT_DATAVIEW_ITEM_EXPANDED(-1, wxDataViewTreeCtrl::OnExpanded)
   EVT_DATAVIEW_ITEM_COLLAPSED(-1, wxDataViewTreeCtrl::OnCollapsed)
   EVT_SIZE( wxDataViewTreeCtrl::OnSize )
END_EVENT_TABLE()

bool wxDataViewTreeCtrl::Create( wxWindow *parent, wxWindowID id,
           const wxPoint& pos, const wxSize& size, long style, const wxValidator& validator )
{
    if ( !wxDataViewCtrl::Create( parent, id, pos, size, style, validator ) )
        return false;

    // create the standard model and a column in the tree
    wxDataViewTreeStore *store = new wxDataViewTreeStore;
    AssociateModel( store );
    store->DecRef();

    AppendIconTextColumn
    (
        wxString(),                 // no label (header is not shown anyhow)
        0,                          // the only model column
        wxDATAVIEW_CELL_EDITABLE,
        -1,                         // default width
        wxALIGN_NOT,                //  and alignment
        0                           // not resizable
    );

    return true;
}

wxDataViewItem wxDataViewTreeCtrl::AppendItem( const wxDataViewItem& parent,
        const wxString &text, int iconIndex, wxClientData *data )
{
    wxDataViewItem res = GetStore()->
        AppendItem( parent, text, GetImage(iconIndex), data );

    GetStore()->ItemAdded( parent, res );

    return res;
}

wxDataViewItem wxDataViewTreeCtrl::PrependItem( const wxDataViewItem& parent,
        const wxString &text, int iconIndex, wxClientData *data )
{
    wxDataViewItem res = GetStore()->
        PrependItem( parent, text, GetImage(iconIndex), data );

    GetStore()->ItemAdded( parent, res );

    return res;
}

wxDataViewItem wxDataViewTreeCtrl::InsertItem( const wxDataViewItem& parent, const wxDataViewItem& previous,
        const wxString &text, int iconIndex, wxClientData *data )
{
    wxDataViewItem res = GetStore()->
        InsertItem( parent, previous, text, GetImage(iconIndex), data );

    GetStore()->ItemAdded( parent, res );

    return res;
}

wxDataViewItem wxDataViewTreeCtrl::PrependContainer( const wxDataViewItem& parent,
        const wxString &text, int iconIndex, int expandedIndex, wxClientData *data )
{
    wxDataViewItem res = GetStore()->
        PrependContainer( parent, text,
                          GetImage(iconIndex), GetImage(expandedIndex), data );

    GetStore()->ItemAdded( parent, res );

    return res;
}

wxDataViewItem wxDataViewTreeCtrl::AppendContainer( const wxDataViewItem& parent,
        const wxString &text, int iconIndex, int expandedIndex, wxClientData *data )
{
    wxDataViewItem res = GetStore()->
        AppendContainer( parent, text,
                         GetImage(iconIndex), GetImage(expandedIndex), data );

    GetStore()->ItemAdded( parent, res );

    return res;
}

wxDataViewItem wxDataViewTreeCtrl::InsertContainer( const wxDataViewItem& parent, const wxDataViewItem& previous,
        const wxString &text, int iconIndex, int expandedIndex, wxClientData *data )
{
    wxDataViewItem res = GetStore()->
        InsertContainer( parent, previous, text,
                         GetImage(iconIndex), GetImage(expandedIndex), data );

    GetStore()->ItemAdded( parent, res );

    return res;
}

void wxDataViewTreeCtrl::SetItemText( const wxDataViewItem& item, const wxString &text )
{
    GetStore()->SetItemText(item,text);

    // notify control
    GetStore()->ValueChanged( item, 0 );
}

void wxDataViewTreeCtrl::SetItemIcon( const wxDataViewItem& item, const wxIcon &icon )
{
    GetStore()->SetItemIcon(item,icon);

    // notify control
    GetStore()->ValueChanged( item, 0 );
}

void wxDataViewTreeCtrl::SetItemExpandedIcon( const wxDataViewItem& item, const wxIcon &icon )
{
    GetStore()->SetItemExpandedIcon(item,icon);

    // notify control
    GetStore()->ValueChanged( item, 0 );
}

void wxDataViewTreeCtrl::DeleteItem( const wxDataViewItem& item )
{
    wxDataViewItem parent_item = GetStore()->GetParent( item );

    GetStore()->DeleteItem(item);

    // notify control
    GetStore()->ItemDeleted( parent_item, item );
}

void wxDataViewTreeCtrl::DeleteChildren( const wxDataViewItem& item )
{
    wxDataViewTreeStoreContainerNode *node = GetStore()->FindContainerNode( item );
    if (!node) return;

    wxDataViewItemArray array;
    wxDataViewTreeStoreNodeList::iterator iter;
    for (iter = node->GetChildren().begin(); iter != node->GetChildren().end(); iter++)
    {
        wxDataViewTreeStoreNode* child = *iter;
        array.Add( child->GetItem() );
    }

    GetStore()->DeleteChildren( item );

    // notify control
    GetStore()->ItemsDeleted( item, array );
}

void  wxDataViewTreeCtrl::DeleteAllItems()
{
    GetStore()->DeleteAllItems();

    GetStore()->Cleared();
}

void wxDataViewTreeCtrl::OnExpanded( wxDataViewEvent &event )
{
    if (HasImageList()) return;

    wxDataViewTreeStoreContainerNode* container = GetStore()->FindContainerNode( event.GetItem() );
    if (!container) return;

    container->SetExpanded( true );

    GetStore()->ItemChanged( event.GetItem() );
}

void wxDataViewTreeCtrl::OnCollapsed( wxDataViewEvent &event )
{
    if (HasImageList()) return;

    wxDataViewTreeStoreContainerNode* container = GetStore()->FindContainerNode( event.GetItem() );
    if (!container) return;

    container->SetExpanded( false );

    GetStore()->ItemChanged( event.GetItem() );
}

void wxDataViewTreeCtrl::OnSize( wxSizeEvent &event )
{
#if defined(wxUSE_GENERICDATAVIEWCTRL)
    // automatically resize our only column to take the entire control width
    if ( GetColumnCount() )
    {
        wxSize size = GetClientSize();
        GetColumn(0)->SetWidth(size.x);
    }
#endif
    event.Skip( true );
}

#endif // wxUSE_DATAVIEWCTRL

