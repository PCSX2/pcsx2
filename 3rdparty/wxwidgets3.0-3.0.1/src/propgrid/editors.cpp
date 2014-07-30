/////////////////////////////////////////////////////////////////////////////
// Name:        src/propgrid/editors.cpp
// Purpose:     wxPropertyGrid editors
// Author:      Jaakko Salli
// Modified by:
// Created:     2007-04-14
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
    #include "wx/dcclient.h"
    #include "wx/dcmemory.h"
    #include "wx/button.h"
    #include "wx/pen.h"
    #include "wx/brush.h"
    #include "wx/cursor.h"
    #include "wx/dialog.h"
    #include "wx/settings.h"
    #include "wx/msgdlg.h"
    #include "wx/choice.h"
    #include "wx/stattext.h"
    #include "wx/scrolwin.h"
    #include "wx/dirdlg.h"
    #include "wx/sizer.h"
    #include "wx/textdlg.h"
    #include "wx/filedlg.h"
    #include "wx/statusbr.h"
    #include "wx/intl.h"
    #include "wx/frame.h"
#endif


#include "wx/timer.h"
#include "wx/dcbuffer.h"
#include "wx/bmpbuttn.h"


// This define is necessary to prevent macro clearing
#define __wxPG_SOURCE_FILE__

#include "wx/propgrid/propgrid.h"
#include "wx/propgrid/editors.h"
#include "wx/propgrid/props.h"

#if wxPG_USE_RENDERER_NATIVE
    #include "wx/renderer.h"
#endif

// How many pixels between textctrl and button
#ifdef __WXMAC__
    #define wxPG_TEXTCTRL_AND_BUTTON_SPACING        4
#else
    #define wxPG_TEXTCTRL_AND_BUTTON_SPACING        2
#endif

#define wxPG_BUTTON_SIZEDEC                         0

#include "wx/odcombo.h"

// -----------------------------------------------------------------------

#if defined(__WXMSW__)
    // tested
    #define wxPG_NAT_BUTTON_BORDER_ANY          1
    #define wxPG_NAT_BUTTON_BORDER_X            1
    #define wxPG_NAT_BUTTON_BORDER_Y            1

    #define wxPG_CHECKMARK_XADJ                 1
    #define wxPG_CHECKMARK_YADJ                 (-1)
    #define wxPG_CHECKMARK_WADJ                 0
    #define wxPG_CHECKMARK_HADJ                 0
    #define wxPG_CHECKMARK_DEFLATE              0

    #define wxPG_TEXTCTRLYADJUST                (m_spacingy+0)

#elif defined(__WXGTK__)
    // tested
    #define wxPG_CHECKMARK_XADJ                 1
    #define wxPG_CHECKMARK_YADJ                 1
    #define wxPG_CHECKMARK_WADJ                 (-2)
    #define wxPG_CHECKMARK_HADJ                 (-2)
    #define wxPG_CHECKMARK_DEFLATE              3

    #define wxPG_NAT_BUTTON_BORDER_ANY      1
    #define wxPG_NAT_BUTTON_BORDER_X        1
    #define wxPG_NAT_BUTTON_BORDER_Y        1

    #define wxPG_TEXTCTRLYADJUST            0

#elif defined(__WXMAC__)
    // *not* tested
    #define wxPG_CHECKMARK_XADJ                 4
    #define wxPG_CHECKMARK_YADJ                 4
    #define wxPG_CHECKMARK_WADJ                 -6
    #define wxPG_CHECKMARK_HADJ                 -6
    #define wxPG_CHECKMARK_DEFLATE              0

    #define wxPG_NAT_BUTTON_BORDER_ANY      0
    #define wxPG_NAT_BUTTON_BORDER_X        0
    #define wxPG_NAT_BUTTON_BORDER_Y        0

    #define wxPG_TEXTCTRLYADJUST            0

#else
    // defaults
    #define wxPG_CHECKMARK_XADJ                 0
    #define wxPG_CHECKMARK_YADJ                 0
    #define wxPG_CHECKMARK_WADJ                 0
    #define wxPG_CHECKMARK_HADJ                 0
    #define wxPG_CHECKMARK_DEFLATE              0

    #define wxPG_NAT_BUTTON_BORDER_ANY      0
    #define wxPG_NAT_BUTTON_BORDER_X        0
    #define wxPG_NAT_BUTTON_BORDER_Y        0

    #define wxPG_TEXTCTRLYADJUST            0

#endif

// for odcombo
#ifdef __WXMAC__
#define wxPG_CHOICEXADJUST           -3 // required because wxComboCtrl reserves 3pixels for wxTextCtrl's focus ring
#define wxPG_CHOICEYADJUST           -3
#else
#define wxPG_CHOICEXADJUST           0
#define wxPG_CHOICEYADJUST           0
#endif

// Number added to image width for SetCustomPaintWidth
#define ODCB_CUST_PAINT_MARGIN               6

// Milliseconds to wait for two mouse-ups after focus in order
// to trigger a double-click.
#define DOUBLE_CLICK_CONVERSION_TRESHOLD        500

// -----------------------------------------------------------------------
// wxPGEditor
// -----------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxPGEditor, wxObject)


wxPGEditor::~wxPGEditor()
{
}

wxString wxPGEditor::GetName() const
{
    return GetClassInfo()->GetClassName();
}

void wxPGEditor::DrawValue( wxDC& dc, const wxRect& rect,
                            wxPGProperty* WXUNUSED(property),
                            const wxString& text ) const
{
    dc.DrawText( text, rect.x+wxPG_XBEFORETEXT, rect.y );
}

bool wxPGEditor::GetValueFromControl( wxVariant&, wxPGProperty*, wxWindow* ) const
{
    return false;
}

void wxPGEditor::SetControlStringValue( wxPGProperty* WXUNUSED(property), wxWindow*, const wxString& ) const
{
}


void wxPGEditor::SetControlIntValue( wxPGProperty* WXUNUSED(property), wxWindow*, int ) const
{
}


int wxPGEditor::InsertItem( wxWindow*, const wxString&, int ) const
{
    return -1;
}


void wxPGEditor::DeleteItem( wxWindow*, int ) const
{
    return;
}


void wxPGEditor::OnFocus( wxPGProperty*, wxWindow* ) const
{
}

void wxPGEditor::SetControlAppearance( wxPropertyGrid* pg,
                                       wxPGProperty* property,
                                       wxWindow* ctrl,
                                       const wxPGCell& cell,
                                       const wxPGCell& oCell,
                                       bool unspecified ) const
{
    // Get old editor appearance
    wxTextCtrl* tc = NULL;
    wxComboCtrl* cb = NULL;
    if ( wxDynamicCast(ctrl, wxTextCtrl) )
    {
        tc = (wxTextCtrl*) ctrl;
    }
    else
    {
        if ( wxDynamicCast(ctrl, wxComboCtrl) )
        {
            cb = (wxComboCtrl*) ctrl;
            tc = cb->GetTextCtrl();
        }
    }

    if ( tc || cb )
    {
        wxString tcText;
        bool changeText = false;

        if ( cell.HasText() && !pg->IsEditorFocused() )
        {
            tcText = cell.GetText();
            changeText = true;
        }
        else if ( oCell.HasText() )
        {
            tcText = property->GetValueAsString(
                property->HasFlag(wxPG_PROP_READONLY)?0:wxPG_EDITABLE_VALUE);
            changeText = true;
        }

        if ( changeText )
        {
            // This prevents value from being modified
            if ( tc )
            {
                pg->SetupTextCtrlValue(tcText);
                tc->SetValue(tcText);
            }
            else
            {
                cb->SetText(tcText);
            }
        }
    }

    // Do not make the mistake of calling GetClassDefaultAttributes()
    // here. It is static, while GetDefaultAttributes() is virtual
    // and the correct one to use.
    wxVisualAttributes vattrs = ctrl->GetDefaultAttributes();

    // Foreground colour
    const wxColour& fgCol = cell.GetFgCol();
    if ( fgCol.IsOk() )
    {
        ctrl->SetForegroundColour(fgCol);
    }
    else if ( oCell.GetFgCol().IsOk() )
    {
        ctrl->SetForegroundColour(vattrs.colFg);
    }

    // Background colour
    const wxColour& bgCol = cell.GetBgCol();
    if ( bgCol.IsOk() )
    {
        ctrl->SetBackgroundColour(bgCol);
    }
    else if ( oCell.GetBgCol().IsOk() )
    {
        ctrl->SetBackgroundColour(vattrs.colBg);
    }

    // Font
    const wxFont& font = cell.GetFont();
    if ( font.IsOk() )
    {
        ctrl->SetFont(font);
    }
    else if ( oCell.GetFont().IsOk() )
    {
        ctrl->SetFont(vattrs.font);
    }

    // Also call the old SetValueToUnspecified()
    if ( unspecified )
        SetValueToUnspecified(property, ctrl);
}

void wxPGEditor::SetValueToUnspecified( wxPGProperty* WXUNUSED(property),
                                        wxWindow* WXUNUSED(ctrl) ) const
{
}

bool wxPGEditor::CanContainCustomImage() const
{
    return false;
}

// -----------------------------------------------------------------------
// wxPGTextCtrlEditor
// -----------------------------------------------------------------------

WX_PG_IMPLEMENT_INTERNAL_EDITOR_CLASS(TextCtrl,wxPGTextCtrlEditor,wxPGEditor)


wxPGWindowList wxPGTextCtrlEditor::CreateControls( wxPropertyGrid* propGrid,
                                                   wxPGProperty* property,
                                                   const wxPoint& pos,
                                                   const wxSize& sz ) const
{
    wxString text;

    //
    // If has children, and limited editing is specified, then don't create.
    if ( (property->GetFlags() & wxPG_PROP_NOEDITOR) &&
         property->GetChildCount() )
        return NULL;

    int argFlags = 0;
    if ( !property->HasFlag(wxPG_PROP_READONLY) &&
         !property->IsValueUnspecified() )
        argFlags |= wxPG_EDITABLE_VALUE;
    text = property->GetValueAsString(argFlags);

    int flags = 0;
    if ( (property->GetFlags() & wxPG_PROP_PASSWORD) &&
         wxDynamicCast(property, wxStringProperty) )
        flags |= wxTE_PASSWORD;

    wxWindow* wnd = propGrid->GenerateEditorTextCtrl(pos,sz,text,NULL,flags,
                                                     property->GetMaxLength());

    return wnd;
}

#if 0
void wxPGTextCtrlEditor::DrawValue( wxDC& dc, wxPGProperty* property, const wxRect& rect ) const
{
    if ( !property->IsValueUnspecified() )
    {
        wxString drawStr = property->GetDisplayedString();

        // Code below should no longer be needed, as the obfuscation
        // is now done in GetValueAsString.
        /*if ( (property->GetFlags() & wxPG_PROP_PASSWORD) &&
             property->IsKindOf(WX_PG_CLASSINFO(wxStringProperty)) )
        {
            size_t a = drawStr.length();
            drawStr.Empty();
            drawStr.Append(wxS('*'),a);
        }*/
        dc.DrawText( drawStr, rect.x+wxPG_XBEFORETEXT, rect.y );
    }
}
#endif

void wxPGTextCtrlEditor::UpdateControl( wxPGProperty* property, wxWindow* ctrl ) const
{
    wxTextCtrl* tc = wxDynamicCast(ctrl, wxTextCtrl);
    if (!tc) return;

    wxString s;

    if ( tc->HasFlag(wxTE_PASSWORD) )
        s = property->GetValueAsString(wxPG_FULL_VALUE);
    else
        s = property->GetDisplayedString();

    wxPropertyGrid* pg = property->GetGrid();

    pg->SetupTextCtrlValue(s);
    tc->SetValue(s);

    //
    // Fix indentation, just in case (change in font boldness is one good
    // reason).
    tc->SetMargins(0);
}

// Provided so that, for example, ComboBox editor can use the same code
// (multiple inheritance would get way too messy).
bool wxPGTextCtrlEditor::OnTextCtrlEvent( wxPropertyGrid* propGrid,
                                          wxPGProperty* WXUNUSED(property),
                                          wxWindow* ctrl,
                                          wxEvent& event )
{
    if ( !ctrl )
        return false;

    if ( event.GetEventType() == wxEVT_TEXT_ENTER )
    {
        if ( propGrid->IsEditorsValueModified() )
        {
            return true;
        }
    }
    else if ( event.GetEventType() == wxEVT_TEXT )
    {
        //
        // Pass this event outside wxPropertyGrid so that,
        // if necessary, program can tell when user is editing
        // a textctrl.
        // FIXME: Is it safe to change event id in the middle of event
        //        processing (seems to work, but...)?
        event.Skip();
        event.SetId(propGrid->GetId());

        propGrid->EditorsValueWasModified();
    }
    return false;
}


bool wxPGTextCtrlEditor::OnEvent( wxPropertyGrid* propGrid,
                                  wxPGProperty* property,
                                  wxWindow* ctrl,
                                  wxEvent& event ) const
{
    return wxPGTextCtrlEditor::OnTextCtrlEvent(propGrid,property,ctrl,event);
}


bool wxPGTextCtrlEditor::GetTextCtrlValueFromControl( wxVariant& variant, wxPGProperty* property, wxWindow* ctrl )
{
    wxTextCtrl* tc = wxStaticCast(ctrl, wxTextCtrl);
    wxString textVal = tc->GetValue();

    if ( property->UsesAutoUnspecified() && textVal.empty() )
    {
        variant.MakeNull();
        return true;
    }

    bool res = property->StringToValue(variant, textVal, wxPG_EDITABLE_VALUE);

    // Changing unspecified always causes event (returning
    // true here should be enough to trigger it).
    // TODO: Move to propgrid.cpp
    if ( !res && variant.IsNull() )
        res = true;

    return res;
}


bool wxPGTextCtrlEditor::GetValueFromControl( wxVariant& variant, wxPGProperty* property, wxWindow* ctrl ) const
{
    return wxPGTextCtrlEditor::GetTextCtrlValueFromControl(variant, property, ctrl);
}


void wxPGTextCtrlEditor::SetControlStringValue( wxPGProperty* property, wxWindow* ctrl, const wxString& txt ) const
{
    wxTextCtrl* tc = wxStaticCast(ctrl, wxTextCtrl);

    wxPropertyGrid* pg = property->GetGrid();
    wxASSERT(pg);  // Really, property grid should exist if editor does
    if ( pg )
    {
        pg->SetupTextCtrlValue(txt);
        tc->SetValue(txt);
    }
}


void wxPGTextCtrlEditor_OnFocus( wxPGProperty* property,
                                 wxTextCtrl* tc )
{
    // Make sure there is correct text (instead of unspecified value
    // indicator or hint text)
    int flags = property->HasFlag(wxPG_PROP_READONLY) ?
        0 : wxPG_EDITABLE_VALUE;
    wxString correctText = property->GetValueAsString(flags);

    if ( tc->GetValue() != correctText )
    {
        property->GetGrid()->SetupTextCtrlValue(correctText);
        tc->SetValue(correctText);
    }

    tc->SelectAll();
}

void wxPGTextCtrlEditor::OnFocus( wxPGProperty* property,
                                  wxWindow* wnd ) const
{
    wxTextCtrl* tc = wxStaticCast(wnd, wxTextCtrl);
    wxPGTextCtrlEditor_OnFocus(property, tc);
}

wxPGTextCtrlEditor::~wxPGTextCtrlEditor()
{
    // Reset the global pointer. Useful when wxPropertyGrid is accessed
    // from an external main loop.
    wxPG_EDITOR(TextCtrl) = NULL;
}


// -----------------------------------------------------------------------
// wxPGChoiceEditor
// -----------------------------------------------------------------------


WX_PG_IMPLEMENT_INTERNAL_EDITOR_CLASS(Choice,wxPGChoiceEditor,wxPGEditor)


// This is a special enhanced double-click processor class.
// In essence, it allows for double-clicks for which the
// first click "created" the control.
class wxPGDoubleClickProcessor : public wxEvtHandler
{
public:

    wxPGDoubleClickProcessor( wxOwnerDrawnComboBox* combo, wxPGProperty* property )
        : wxEvtHandler()
    {
        m_timeLastMouseUp = 0;
        m_combo = combo;
        m_property = property;
        m_downReceived = false;
    }

protected:

    void OnMouseEvent( wxMouseEvent& event )
    {
        wxLongLong t = ::wxGetLocalTimeMillis();
        int evtType = event.GetEventType();

        if ( m_property->HasFlag(wxPG_PROP_USE_DCC) &&
             wxDynamicCast(m_property, wxBoolProperty) &&
             !m_combo->IsPopupShown() )
        {
            // Just check that it is in the text area
            wxPoint pt = event.GetPosition();
            if ( m_combo->GetTextRect().Contains(pt) )
            {
                if ( evtType == wxEVT_LEFT_DOWN )
                {
                    // Set value to avoid up-events without corresponding downs
                    m_downReceived = true;
                }
                else if ( evtType == wxEVT_LEFT_DCLICK )
                {
                    // We'll make our own double-clicks
                    event.SetEventType(0);
                    return;
                }
                else if ( evtType == wxEVT_LEFT_UP )
                {
                    if ( m_downReceived || m_timeLastMouseUp == 1 )
                    {
                        wxLongLong timeFromLastUp = (t-m_timeLastMouseUp);

                        if ( timeFromLastUp < DOUBLE_CLICK_CONVERSION_TRESHOLD )
                        {
                            event.SetEventType(wxEVT_LEFT_DCLICK);
                            m_timeLastMouseUp = 1;
                        }
                        else
                        {
                            m_timeLastMouseUp = t;
                        }
                    }
                }
            }
        }

        event.Skip();
    }

    void OnSetFocus( wxFocusEvent& event )
    {
        m_timeLastMouseUp = ::wxGetLocalTimeMillis();
        event.Skip();
    }

private:
    wxLongLong                  m_timeLastMouseUp;
    wxOwnerDrawnComboBox*       m_combo;
    wxPGProperty*               m_property;  // Selected property
    bool                        m_downReceived;

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(wxPGDoubleClickProcessor, wxEvtHandler)
    EVT_MOUSE_EVENTS(wxPGDoubleClickProcessor::OnMouseEvent)
    EVT_SET_FOCUS(wxPGDoubleClickProcessor::OnSetFocus)
END_EVENT_TABLE()



class wxPGComboBox : public wxOwnerDrawnComboBox
{
public:

    wxPGComboBox()
        : wxOwnerDrawnComboBox()
    {
        m_dclickProcessor = NULL;
        m_sizeEventCalled = false;
    }

    ~wxPGComboBox()
    {
        if ( m_dclickProcessor )
        {
            RemoveEventHandler(m_dclickProcessor);
            delete m_dclickProcessor;
        }
    }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& value,
                const wxPoint& pos,
                const wxSize& size,
                const wxArrayString& choices,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxS("wxOwnerDrawnComboBox"))
    {
        if ( !wxOwnerDrawnComboBox::Create( parent,
                                            id,
                                            value,
                                            pos,
                                            size,
                                            choices,
                                            style,
                                            validator,
                                            name ) )
            return false;

        m_dclickProcessor = new
            wxPGDoubleClickProcessor( this, GetGrid()->GetSelection() );

        PushEventHandler(m_dclickProcessor);

        return true;
    }

    virtual void OnDrawItem( wxDC& dc,
                             const wxRect& rect,
                             int item,
                             int flags ) const
    {
        wxPropertyGrid* pg = GetGrid();

        // Handle hint text via super class
        if ( (flags & wxODCB_PAINTING_CONTROL) &&
             ShouldUseHintText(flags) )
        {
            wxOwnerDrawnComboBox::OnDrawItem(dc, rect, item, flags);
        }
        else
        {
            pg->OnComboItemPaint( this, item, &dc, (wxRect&)rect, flags );
        }
    }

    virtual wxCoord OnMeasureItem( size_t item ) const
    {
        wxPropertyGrid* pg = GetGrid();
        wxRect rect;
        rect.x = -1;
        rect.width = 0;
        pg->OnComboItemPaint( this, item, NULL, rect, 0 );
        return rect.height;
    }

    wxPropertyGrid* GetGrid() const
    {
        wxPropertyGrid* pg = wxDynamicCast(GetParent(),
                                           wxPropertyGrid);
        wxASSERT(pg);
        return pg;
    }

    virtual wxCoord OnMeasureItemWidth( size_t item ) const
    {
        wxPropertyGrid* pg = GetGrid();
        wxRect rect;
        rect.x = -1;
        rect.width = -1;
        pg->OnComboItemPaint( this, item, NULL, rect, 0 );
        return rect.width;
    }

    virtual void PositionTextCtrl( int textCtrlXAdjust,
                                   int WXUNUSED(textCtrlYAdjust) )
    {
        wxPropertyGrid* pg = GetGrid();
    #ifdef wxPG_TEXTCTRLXADJUST
        textCtrlXAdjust = wxPG_TEXTCTRLXADJUST -
                          (wxPG_XBEFOREWIDGET+wxPG_CONTROL_MARGIN+1) - 1,
    #endif
        wxOwnerDrawnComboBox::PositionTextCtrl(
            textCtrlXAdjust,
            pg->GetSpacingY() + 2
        );
    }

private:
    wxPGDoubleClickProcessor*   m_dclickProcessor;
    bool                        m_sizeEventCalled;
};


void wxPropertyGrid::OnComboItemPaint( const wxPGComboBox* pCb,
                                       int item,
                                       wxDC* pDc,
                                       wxRect& rect,
                                       int flags )
{
    wxPGProperty* p = GetSelection();
    wxString text;

    const wxPGChoices& choices = p->GetChoices();
    const wxPGCommonValue* comVal = NULL;
    int comVals = p->GetDisplayedCommonValueCount();
    int comValIndex = -1;

    int choiceCount = 0;
    if ( choices.IsOk() )
        choiceCount = choices.GetCount();

    if ( item >= choiceCount && comVals > 0 )
    {
        comValIndex = item - choiceCount;
        comVal = GetCommonValue(comValIndex);
        if ( !p->IsValueUnspecified() )
            text = comVal->GetLabel();
    }
    else
    {
        if ( !(flags & wxODCB_PAINTING_CONTROL) )
        {
            text = pCb->GetString(item);
        }
        else
        {
            if ( !p->IsValueUnspecified() )
                text = p->GetValueAsString(0);
        }
    }

    if ( item < 0 )
        return;

    wxSize cis;

    const wxBitmap* itemBitmap = NULL;

    if ( item >= 0 && choices.IsOk() && choices.Item(item).GetBitmap().IsOk() && comValIndex == -1 )
        itemBitmap = &choices.Item(item).GetBitmap();

    //
    // Decide what custom image size to use
    if ( itemBitmap )
    {
        cis.x = itemBitmap->GetWidth();
        cis.y = itemBitmap->GetHeight();
    }
    else
    {
        cis = GetImageSize(p, item);
    }

    if ( rect.x < 0 )
    {
        // Default measure behaviour (no flexible, custom paint image only)
        if ( rect.width < 0 )
        {
            wxCoord x, y;
            pCb->GetTextExtent(text, &x, &y, 0, 0);
            rect.width = cis.x + wxCC_CUSTOM_IMAGE_MARGIN1 + wxCC_CUSTOM_IMAGE_MARGIN2 + 9 + x;
        }

        rect.height = cis.y + 2;
        return;
    }

    wxPGPaintData paintdata;
    paintdata.m_parent = NULL;
    paintdata.m_choiceItem = item;

    // This is by the current (1.0.0b) spec - if painting control, item is -1
    if ( (flags & wxODCB_PAINTING_CONTROL) )
        paintdata.m_choiceItem = -1;

    if ( pDc )
        pDc->SetBrush(*wxWHITE_BRUSH);

    wxPGCellRenderer* renderer = NULL;
    const wxPGChoiceEntry* cell = NULL;

    if ( rect.x >= 0 )
    {
        //
        // DrawItem call
        wxDC& dc = *pDc;

        wxPoint pt(rect.x + wxPG_CONTROL_MARGIN - wxPG_CHOICEXADJUST - 1,
                   rect.y + 1);

        int renderFlags = wxPGCellRenderer::DontUseCellColours;
        bool useCustomPaintProcedure;

        // If custom image had some size, we will start from the assumption
        // that custom paint procedure is required
        if ( cis.x > 0 )
            useCustomPaintProcedure = true;
        else
            useCustomPaintProcedure = false;

        if ( flags & wxODCB_PAINTING_SELECTED )
            renderFlags |= wxPGCellRenderer::Selected;

        if ( flags & wxODCB_PAINTING_CONTROL )
        {
            renderFlags |= wxPGCellRenderer::Control;

            // If wxPG_PROP_CUSTOMIMAGE was set, then that means any custom
            // image will not appear on the control row (it may be too
            // large to fit, for instance). Also do not draw custom image
            // if no choice was selected.
            if ( !p->HasFlag(wxPG_PROP_CUSTOMIMAGE) || item < 0 )
                useCustomPaintProcedure = false;
        }
        else
        {
            renderFlags |= wxPGCellRenderer::ChoicePopup;

            // For consistency, always use normal font when drawing drop down
            // items
            dc.SetFont(GetFont());
        }

        // If not drawing a selected popup item, then give property's
        // m_valueBitmap a chance.
        if ( p->m_valueBitmap && item != pCb->GetSelection() )
            useCustomPaintProcedure = false;
        // If current choice had a bitmap set by the application, then
        // use it instead of any custom paint procedure.
        else if ( itemBitmap )
            useCustomPaintProcedure = false;

        if ( useCustomPaintProcedure )
        {
            pt.x += wxCC_CUSTOM_IMAGE_MARGIN1;
            wxRect r(pt.x,pt.y,cis.x,cis.y);

            if ( flags & wxODCB_PAINTING_CONTROL )
            {
                //r.width = cis.x;
                r.height = wxPG_STD_CUST_IMAGE_HEIGHT(m_lineHeight);
            }

            paintdata.m_drawnWidth = r.width;

            dc.SetPen(m_colPropFore);
            if ( comValIndex >= 0 )
            {
                const wxPGCommonValue* cv = GetCommonValue(comValIndex);
                renderer = cv->GetRenderer();
                r.width = rect.width;
                renderer->Render( dc, r, this, p, m_selColumn, comValIndex, renderFlags );
                return;
            }
            else if ( item >= 0 )
            {
                p->OnCustomPaint( dc, r, paintdata );
            }
            else
            {
                dc.DrawRectangle( r );
            }

            pt.x += paintdata.m_drawnWidth + wxCC_CUSTOM_IMAGE_MARGIN2 - 1;
        }
        else
        {
            // TODO: This aligns text so that it seems to be horizontally
            //       on the same line as property values. Not really
            //       sure if its needed, but seems to not cause any harm.
            pt.x -= 1;

            if ( item < 0 && (flags & wxODCB_PAINTING_CONTROL) )
                item = pCb->GetSelection();

            if ( choices.IsOk() && item >= 0 && comValIndex < 0 )
            {
                cell = &choices.Item(item);
                renderer = wxPGGlobalVars->m_defaultRenderer;
                int imageOffset = renderer->PreDrawCell(dc, rect, *cell,
                                                        renderFlags );
                if ( imageOffset )
                    imageOffset += wxCC_CUSTOM_IMAGE_MARGIN1 +
                                   wxCC_CUSTOM_IMAGE_MARGIN2;
                pt.x += imageOffset;
            }
        }

        //
        // Draw text
        //

        pt.y += (rect.height-m_fontHeight)/2 - 1;

        pt.x += 1;

        dc.DrawText( text, pt.x + wxPG_XBEFORETEXT, pt.y );

        if ( renderer )
            renderer->PostDrawCell(dc, this, *cell, renderFlags);
    }
    else
    {
        //
        // MeasureItem call
        wxDC& dc = *pDc;

        p->OnCustomPaint( dc, rect, paintdata );
        rect.height = paintdata.m_drawnHeight + 2;
        rect.width = cis.x + wxCC_CUSTOM_IMAGE_MARGIN1 + wxCC_CUSTOM_IMAGE_MARGIN2 + 9;
    }
}

bool wxPGChoiceEditor_SetCustomPaintWidth( wxPropertyGrid* propGrid, wxPGComboBox* cb, int cmnVal )
{
    wxPGProperty* property = propGrid->GetSelectedProperty();
    wxASSERT( property );

    wxSize imageSize;
    bool res;

    // TODO: Do this always when cell has custom text.
    if ( property->IsValueUnspecified() )
    {
        cb->SetCustomPaintWidth( 0 );
        return true;
    }

    if ( cmnVal >= 0 )
    {
        // Yes, a common value is being selected
        property->SetCommonValue( cmnVal );
        imageSize = propGrid->GetCommonValue(cmnVal)->
                            GetRenderer()->GetImageSize(property, 1, cmnVal);
        res = false;
    }
    else
    {
        imageSize = propGrid->GetImageSize(property, -1);
        res = true;
    }

    if ( imageSize.x )
        imageSize.x += ODCB_CUST_PAINT_MARGIN;
    cb->SetCustomPaintWidth( imageSize.x );

    return res;
}

// CreateControls calls this with CB_READONLY in extraStyle
wxWindow* wxPGChoiceEditor::CreateControlsBase( wxPropertyGrid* propGrid,
                                                wxPGProperty* property,
                                                const wxPoint& pos,
                                                const wxSize& sz,
                                                long extraStyle ) const
{
    // Since it is not possible (yet) to create a read-only combo box in
    // the same sense that wxTextCtrl is read-only, simply do not create
    // the control in this case.
    if ( property->HasFlag(wxPG_PROP_READONLY) )
        return NULL;

    const wxPGChoices& choices = property->GetChoices();
    wxString defString;
    int index = property->GetChoiceSelection();

    int argFlags = 0;
    if ( !property->HasFlag(wxPG_PROP_READONLY) &&
         !property->IsValueUnspecified() )
        argFlags |= wxPG_EDITABLE_VALUE;
    defString = property->GetValueAsString(argFlags);

    wxArrayString labels = choices.GetLabels();

    wxPGComboBox* cb;

    wxPoint po(pos);
    wxSize si(sz);
    po.y += wxPG_CHOICEYADJUST;
    si.y -= (wxPG_CHOICEYADJUST*2);

    po.x += wxPG_CHOICEXADJUST;
    si.x -= wxPG_CHOICEXADJUST;
    wxWindow* ctrlParent = propGrid->GetPanel();

    int odcbFlags = extraStyle | wxBORDER_NONE | wxTE_PROCESS_ENTER;

    if ( (property->GetFlags() & wxPG_PROP_USE_DCC) &&
         wxDynamicCast(property, wxBoolProperty) )
        odcbFlags |= wxODCB_DCLICK_CYCLES;

    //
    // If common value specified, use appropriate index
    unsigned int cmnVals = property->GetDisplayedCommonValueCount();
    if ( cmnVals )
    {
        if ( !property->IsValueUnspecified() )
        {
            int cmnVal = property->GetCommonValue();
            if ( cmnVal >= 0 )
            {
                index = labels.size() + cmnVal;
            }
        }

        unsigned int i;
        for ( i=0; i<cmnVals; i++ )
            labels.Add(propGrid->GetCommonValueLabel(i));
    }

    cb = new wxPGComboBox();
#ifdef __WXMSW__
    cb->Hide();
#endif
    cb->Create(ctrlParent,
               wxPG_SUBID1,
               wxString(),
               po,
               si,
               labels,
               odcbFlags);

    cb->SetButtonPosition(si.y,0,wxRIGHT);
    cb->SetMargins(wxPG_XBEFORETEXT-1);

    // Set hint text
    cb->SetHint(property->GetHintText());

    wxPGChoiceEditor_SetCustomPaintWidth( propGrid, cb,
                                          property->GetCommonValue() );

    if ( index >= 0 && index < (int)cb->GetCount() )
    {
        cb->SetSelection( index );
        if ( !defString.empty() )
            cb->SetText( defString );
    }
    else if ( !(extraStyle & wxCB_READONLY) && !defString.empty() )
    {
        propGrid->SetupTextCtrlValue(defString);
        cb->SetValue( defString );
    }
    else
    {
        cb->SetSelection( -1 );
    }

#ifdef __WXMSW__
    cb->Show();
#endif

    return (wxWindow*) cb;
}


void wxPGChoiceEditor::UpdateControl( wxPGProperty* property, wxWindow* ctrl ) const
{
    wxASSERT( ctrl );
    wxOwnerDrawnComboBox* cb = (wxOwnerDrawnComboBox*)ctrl;
    wxASSERT( wxDynamicCast(cb, wxOwnerDrawnComboBox));
    int ind = property->GetChoiceSelection();
    cb->SetSelection(ind);
}

wxPGWindowList wxPGChoiceEditor::CreateControls( wxPropertyGrid* propGrid, wxPGProperty* property,
        const wxPoint& pos, const wxSize& sz ) const
{
    return CreateControlsBase(propGrid,property,pos,sz,wxCB_READONLY);
}


int wxPGChoiceEditor::InsertItem( wxWindow* ctrl, const wxString& label, int index ) const
{
    wxASSERT( ctrl );
    wxOwnerDrawnComboBox* cb = (wxOwnerDrawnComboBox*)ctrl;
    wxASSERT( wxDynamicCast(cb, wxOwnerDrawnComboBox));

    if (index < 0)
        index = cb->GetCount();

    return cb->Insert(label,index);
}


void wxPGChoiceEditor::DeleteItem( wxWindow* ctrl, int index ) const
{
    wxASSERT( ctrl );
    wxOwnerDrawnComboBox* cb = (wxOwnerDrawnComboBox*)ctrl;
    wxASSERT( wxDynamicCast(cb, wxOwnerDrawnComboBox));

    cb->Delete(index);
}

bool wxPGChoiceEditor::OnEvent( wxPropertyGrid* propGrid, wxPGProperty* property,
    wxWindow* ctrl, wxEvent& event ) const
{
    if ( event.GetEventType() == wxEVT_COMBOBOX )
    {
        wxPGComboBox* cb = (wxPGComboBox*)ctrl;
        int index = cb->GetSelection();
        int cmnValIndex = -1;
        int cmnVals = property->GetDisplayedCommonValueCount();
        int items = cb->GetCount();

        if ( index >= (items-cmnVals) )
        {
            // Yes, a common value is being selected
            cmnValIndex = index - (items-cmnVals);
            property->SetCommonValue( cmnValIndex );

            // Truly set value to unspecified?
            if ( propGrid->GetUnspecifiedCommonValue() == cmnValIndex )
            {
                if ( !property->IsValueUnspecified() )
                    propGrid->SetInternalFlag(wxPG_FL_VALUE_CHANGE_IN_EVENT);
                property->SetValueToUnspecified();
                if ( !cb->HasFlag(wxCB_READONLY) )
                {
                    wxString unspecValueText;
                    unspecValueText = propGrid->GetUnspecifiedValueText();
                    propGrid->SetupTextCtrlValue(unspecValueText);
                    cb->GetTextCtrl()->SetValue(unspecValueText);
                }
                return false;
            }
        }
        return wxPGChoiceEditor_SetCustomPaintWidth( propGrid, cb, cmnValIndex );
    }
    return false;
}


bool wxPGChoiceEditor::GetValueFromControl( wxVariant& variant, wxPGProperty* property, wxWindow* ctrl ) const
{
    wxOwnerDrawnComboBox* cb = (wxOwnerDrawnComboBox*)ctrl;

    int index = cb->GetSelection();

    if ( index != property->GetChoiceSelection() ||
        // Changing unspecified always causes event (returning
        // true here should be enough to trigger it).
         property->IsValueUnspecified()
       )
    {
        return property->IntToValue(variant, index, wxPG_PROPERTY_SPECIFIC);
    }
    return false;
}


void wxPGChoiceEditor::SetControlStringValue( wxPGProperty* property,
                                              wxWindow* ctrl,
                                              const wxString& txt ) const
{
    wxOwnerDrawnComboBox* cb = (wxOwnerDrawnComboBox*)ctrl;
    wxASSERT( cb );
    property->GetGrid()->SetupTextCtrlValue(txt);
    cb->SetValue(txt);
}


void wxPGChoiceEditor::SetControlIntValue( wxPGProperty* WXUNUSED(property), wxWindow* ctrl, int value ) const
{
    wxOwnerDrawnComboBox* cb = (wxOwnerDrawnComboBox*)ctrl;
    wxASSERT( cb );
    cb->SetSelection(value);
}


void wxPGChoiceEditor::SetValueToUnspecified( wxPGProperty* WXUNUSED(property),
                                              wxWindow* ctrl ) const
{
    wxOwnerDrawnComboBox* cb = (wxOwnerDrawnComboBox*)ctrl;

    if ( cb->HasFlag(wxCB_READONLY) )
        cb->SetSelection(-1);
}


bool wxPGChoiceEditor::CanContainCustomImage() const
{
    return true;
}


wxPGChoiceEditor::~wxPGChoiceEditor()
{
    wxPG_EDITOR(Choice) = NULL;
}


// -----------------------------------------------------------------------
// wxPGComboBoxEditor
// -----------------------------------------------------------------------


WX_PG_IMPLEMENT_INTERNAL_EDITOR_CLASS(ComboBox,
                                      wxPGComboBoxEditor,
                                      wxPGChoiceEditor)


void wxPGComboBoxEditor::UpdateControl( wxPGProperty* property, wxWindow* ctrl ) const
{
    wxOwnerDrawnComboBox* cb = (wxOwnerDrawnComboBox*)ctrl;
    wxString s = property->GetValueAsString(wxPG_EDITABLE_VALUE);
    property->GetGrid()->SetupTextCtrlValue(s);
    cb->SetValue(s);

    // TODO: If string matches any selection, then select that.
}


wxPGWindowList wxPGComboBoxEditor::CreateControls( wxPropertyGrid* propGrid,
                                                   wxPGProperty* property,
                                                   const wxPoint& pos,
                                                   const wxSize& sz ) const
{
    return CreateControlsBase(propGrid,property,pos,sz,0);
}


bool wxPGComboBoxEditor::OnEvent( wxPropertyGrid* propGrid,
                                  wxPGProperty* property,
                                  wxWindow* ctrl,
                                  wxEvent& event ) const
{
    wxOwnerDrawnComboBox* cb = NULL;
    wxWindow* textCtrl = NULL;

    if ( ctrl )
    {
        cb = (wxOwnerDrawnComboBox*)ctrl;
        textCtrl = cb->GetTextCtrl();
    }

    if ( wxPGTextCtrlEditor::OnTextCtrlEvent(propGrid,property,textCtrl,event) )
        return true;

    return wxPGChoiceEditor::OnEvent(propGrid,property,ctrl,event);
}


bool wxPGComboBoxEditor::GetValueFromControl( wxVariant& variant, wxPGProperty* property, wxWindow* ctrl ) const
{
    wxOwnerDrawnComboBox* cb = (wxOwnerDrawnComboBox*)ctrl;
    wxString textVal = cb->GetValue();

    if ( property->UsesAutoUnspecified() && textVal.empty() )
    {
        variant.MakeNull();
        return true;
    }

    bool res = property->StringToValue(variant, textVal, wxPG_EDITABLE_VALUE|wxPG_PROPERTY_SPECIFIC);

    // Changing unspecified always causes event (returning
    // true here should be enough to trigger it).
    if ( !res && variant.IsNull() )
        res = true;

    return res;
}


void wxPGComboBoxEditor::OnFocus( wxPGProperty* property,
                                  wxWindow* ctrl ) const
{
    wxOwnerDrawnComboBox* cb = (wxOwnerDrawnComboBox*)ctrl;
    wxPGTextCtrlEditor_OnFocus(property, cb->GetTextCtrl());
}


wxPGComboBoxEditor::~wxPGComboBoxEditor()
{
    wxPG_EDITOR(ComboBox) = NULL;
}



// -----------------------------------------------------------------------
// wxPGChoiceAndButtonEditor
// -----------------------------------------------------------------------


WX_PG_IMPLEMENT_INTERNAL_EDITOR_CLASS(ChoiceAndButton,
                                      wxPGChoiceAndButtonEditor,
                                      wxPGChoiceEditor)


wxPGWindowList wxPGChoiceAndButtonEditor::CreateControls( wxPropertyGrid* propGrid,
                                                          wxPGProperty* property,
                                                          const wxPoint& pos,
                                                          const wxSize& sz ) const
{
    // Use one two units smaller to match size of the combo's dropbutton.
    // (normally a bigger button is used because it looks better)
    int bt_wid = sz.y;
    bt_wid -= 2;
    wxSize bt_sz(bt_wid,bt_wid);

    // Position of button.
    wxPoint bt_pos(pos.x+sz.x-bt_sz.x,pos.y);
#ifdef __WXMAC__
    bt_pos.y -= 1;
#else
    bt_pos.y += 1;
#endif

    wxWindow* bt = propGrid->GenerateEditorButton( bt_pos, bt_sz );

    // Size of choice.
    wxSize ch_sz(sz.x-bt->GetSize().x,sz.y);

#ifdef __WXMAC__
    ch_sz.x -= wxPG_TEXTCTRL_AND_BUTTON_SPACING;
#endif

    wxWindow* ch = wxPGEditor_Choice->CreateControls(propGrid,property,
        pos,ch_sz).m_primary;

#ifdef __WXMSW__
    bt->Show();
#endif

    return wxPGWindowList(ch, bt);
}


wxPGChoiceAndButtonEditor::~wxPGChoiceAndButtonEditor()
{
    wxPG_EDITOR(ChoiceAndButton) = NULL;
}

// -----------------------------------------------------------------------
// wxPGTextCtrlAndButtonEditor
// -----------------------------------------------------------------------

WX_PG_IMPLEMENT_INTERNAL_EDITOR_CLASS(TextCtrlAndButton,
                                      wxPGTextCtrlAndButtonEditor,
                                      wxPGTextCtrlEditor)


wxPGWindowList wxPGTextCtrlAndButtonEditor::CreateControls( wxPropertyGrid* propGrid,
                                                            wxPGProperty* property,
                                                            const wxPoint& pos,
                                                            const wxSize& sz ) const
{
    wxWindow* wnd2;
    wxWindow* wnd = propGrid->GenerateEditorTextCtrlAndButton( pos, sz, &wnd2,
        property->GetFlags() & wxPG_PROP_NOEDITOR, property);

    return wxPGWindowList(wnd, wnd2);
}


wxPGTextCtrlAndButtonEditor::~wxPGTextCtrlAndButtonEditor()
{
    wxPG_EDITOR(TextCtrlAndButton) = NULL;
}

// -----------------------------------------------------------------------
// wxPGCheckBoxEditor
// -----------------------------------------------------------------------

#if wxPG_INCLUDE_CHECKBOX

WX_PG_IMPLEMENT_INTERNAL_EDITOR_CLASS(CheckBox,
                                      wxPGCheckBoxEditor,
                                      wxPGEditor)


// Check box state flags
enum
{
    wxSCB_STATE_UNCHECKED   = 0,
    wxSCB_STATE_CHECKED     = 1,
    wxSCB_STATE_BOLD        = 2,
    wxSCB_STATE_UNSPECIFIED = 4
};

const int wxSCB_SETVALUE_CYCLE = 2;


static void DrawSimpleCheckBox( wxDC& dc, const wxRect& rect, int box_hei,
                                int state )
{
    // Box rectangle.
    wxRect r(rect.x+wxPG_XBEFORETEXT,rect.y+((rect.height-box_hei)/2),
             box_hei,box_hei);
    wxColour useCol = dc.GetTextForeground();

    if ( state & wxSCB_STATE_UNSPECIFIED )
    {
        useCol = wxColour(220, 220, 220);
    }

    // Draw check mark first because it is likely to overdraw the
    // surrounding rectangle.
    if ( state & wxSCB_STATE_CHECKED )
    {
        wxRect r2(r.x+wxPG_CHECKMARK_XADJ,
                  r.y+wxPG_CHECKMARK_YADJ,
                  r.width+wxPG_CHECKMARK_WADJ,
                  r.height+wxPG_CHECKMARK_HADJ);
    #if wxPG_CHECKMARK_DEFLATE
        r2.Deflate(wxPG_CHECKMARK_DEFLATE);
    #endif
        dc.DrawCheckMark(r2);

        // This would draw a simple cross check mark.
        // dc.DrawLine(r.x,r.y,r.x+r.width-1,r.y+r.height-1);
        // dc.DrawLine(r.x,r.y+r.height-1,r.x+r.width-1,r.y);
    }

    if ( !(state & wxSCB_STATE_BOLD) )
    {
        // Pen for thin rectangle.
        dc.SetPen(useCol);
    }
    else
    {
        // Pen for bold rectangle.
        wxPen linepen(useCol,2,wxSOLID);
        linepen.SetJoin(wxJOIN_MITER); // This prevents round edges.
        dc.SetPen(linepen);
        r.x++;
        r.y++;
        r.width--;
        r.height--;
    }

    dc.SetBrush(*wxTRANSPARENT_BRUSH);

    dc.DrawRectangle(r);
    dc.SetPen(*wxTRANSPARENT_PEN);
}

//
// Real simple custom-drawn checkbox-without-label class.
//
class wxSimpleCheckBox : public wxControl
{
public:

    void SetValue( int value );

    wxSimpleCheckBox( wxWindow* parent,
                      wxWindowID id,
                      const wxPoint& pos = wxDefaultPosition,
                      const wxSize& size = wxDefaultSize )
        : wxControl(parent,id,pos,size,wxBORDER_NONE|wxWANTS_CHARS)
    {
        // Due to SetOwnFont stuff necessary for GTK+ 1.2, we need to have this
        SetFont( parent->GetFont() );

        m_state = 0;
        m_boxHeight = 12;

        SetBackgroundStyle( wxBG_STYLE_CUSTOM );
    }

    virtual ~wxSimpleCheckBox();

    int m_state;
    int m_boxHeight;

private:
    void OnPaint( wxPaintEvent& event );
    void OnLeftClick( wxMouseEvent& event );
    void OnKeyDown( wxKeyEvent& event );

    void OnResize( wxSizeEvent& event )
    {
        Refresh();
        event.Skip();
    }

    static wxBitmap* ms_doubleBuffer;

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(wxSimpleCheckBox, wxControl)
    EVT_PAINT(wxSimpleCheckBox::OnPaint)
    EVT_LEFT_DOWN(wxSimpleCheckBox::OnLeftClick)
    EVT_LEFT_DCLICK(wxSimpleCheckBox::OnLeftClick)
    EVT_KEY_DOWN(wxSimpleCheckBox::OnKeyDown)
    EVT_SIZE(wxSimpleCheckBox::OnResize)
END_EVENT_TABLE()

wxSimpleCheckBox::~wxSimpleCheckBox()
{
    wxDELETE(ms_doubleBuffer);
}

wxBitmap* wxSimpleCheckBox::ms_doubleBuffer = NULL;

void wxSimpleCheckBox::OnPaint( wxPaintEvent& WXUNUSED(event) )
{
    wxSize clientSize = GetClientSize();
    wxAutoBufferedPaintDC dc(this);

    dc.Clear();
    wxRect rect(0,0,clientSize.x,clientSize.y);
    rect.y += 1;
    rect.width += 1;

    wxColour bgcol = GetBackgroundColour();
    dc.SetBrush( bgcol );
    dc.SetPen( bgcol );
    dc.DrawRectangle( rect );

    dc.SetTextForeground(GetForegroundColour());

    int state = m_state;
    if ( !(state & wxSCB_STATE_UNSPECIFIED) &&
         GetFont().GetWeight() == wxBOLD )
        state |= wxSCB_STATE_BOLD;

    DrawSimpleCheckBox(dc, rect, m_boxHeight, state);
}

void wxSimpleCheckBox::OnLeftClick( wxMouseEvent& event )
{
    if ( (event.m_x > (wxPG_XBEFORETEXT-2)) &&
         (event.m_x <= (wxPG_XBEFORETEXT-2+m_boxHeight)) )
    {
        SetValue(wxSCB_SETVALUE_CYCLE);
    }
}

void wxSimpleCheckBox::OnKeyDown( wxKeyEvent& event )
{
    if ( event.GetKeyCode() == WXK_SPACE )
    {
        SetValue(wxSCB_SETVALUE_CYCLE);
    }
}

void wxSimpleCheckBox::SetValue( int value )
{
    if ( value == wxSCB_SETVALUE_CYCLE )
    {
        if ( m_state & wxSCB_STATE_CHECKED )
            m_state &= ~wxSCB_STATE_CHECKED;
        else
            m_state |= wxSCB_STATE_CHECKED;
    }
    else
    {
        m_state = value;
    }
    Refresh();

    wxCommandEvent evt(wxEVT_CHECKBOX,GetParent()->GetId());

    wxPropertyGrid* propGrid = (wxPropertyGrid*) GetParent();
    wxASSERT( wxDynamicCast(propGrid, wxPropertyGrid) );
    propGrid->HandleCustomEditorEvent(evt);
}

wxPGWindowList wxPGCheckBoxEditor::CreateControls( wxPropertyGrid* propGrid,
                                                   wxPGProperty* property,
                                                   const wxPoint& pos,
                                                   const wxSize& size ) const
{
    if ( property->HasFlag(wxPG_PROP_READONLY) )
        return NULL;

    wxPoint pt = pos;
    pt.x -= wxPG_XBEFOREWIDGET;
    wxSize sz = size;
    sz.x = propGrid->GetFontHeight() + (wxPG_XBEFOREWIDGET*2) + 4;

    wxSimpleCheckBox* cb = new wxSimpleCheckBox(propGrid->GetPanel(),
                                                wxPG_SUBID1, pt, sz);

    cb->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

    UpdateControl(property, cb);

    if ( !property->IsValueUnspecified() )
    {
        // If mouse cursor was on the item, toggle the value now.
        if ( propGrid->GetInternalFlags() & wxPG_FL_ACTIVATION_BY_CLICK )
        {
            wxPoint point = cb->ScreenToClient(::wxGetMousePosition());
            if ( point.x <= (wxPG_XBEFORETEXT-2+cb->m_boxHeight) )
            {
                if ( cb->m_state & wxSCB_STATE_CHECKED )
                    cb->m_state &= ~wxSCB_STATE_CHECKED;
                else
                    cb->m_state |= wxSCB_STATE_CHECKED;

                // Makes sure wxPG_EVT_CHANGING etc. is sent for this initial
                // click
                propGrid->ChangePropertyValue(property,
                                              wxPGVariant_Bool(cb->m_state));
            }
        }
    }

    propGrid->SetInternalFlag( wxPG_FL_FIXED_WIDTH_EDITOR );

    return cb;
}

void wxPGCheckBoxEditor::DrawValue( wxDC& dc, const wxRect& rect,
                                    wxPGProperty* property,
                                    const wxString& WXUNUSED(text) ) const
{
    int state = wxSCB_STATE_UNCHECKED;

    if ( !property->IsValueUnspecified() )
    {
        state = property->GetChoiceSelection();
        if ( dc.GetFont().GetWeight() == wxBOLD )
            state |= wxSCB_STATE_BOLD;
    }
    else
    {
        state |= wxSCB_STATE_UNSPECIFIED;
    }

    DrawSimpleCheckBox(dc, rect, dc.GetCharHeight(), state);
}

void wxPGCheckBoxEditor::UpdateControl( wxPGProperty* property,
                                        wxWindow* ctrl ) const
{
    wxSimpleCheckBox* cb = (wxSimpleCheckBox*) ctrl;
    wxASSERT( cb );

    if ( !property->IsValueUnspecified() )
        cb->m_state = property->GetChoiceSelection();
    else
        cb->m_state = wxSCB_STATE_UNSPECIFIED;

    wxPropertyGrid* propGrid = property->GetGrid();
    cb->m_boxHeight = propGrid->GetFontHeight();

    cb->Refresh();
}

bool wxPGCheckBoxEditor::OnEvent( wxPropertyGrid* WXUNUSED(propGrid), wxPGProperty* WXUNUSED(property),
    wxWindow* WXUNUSED(ctrl), wxEvent& event ) const
{
    if ( event.GetEventType() == wxEVT_CHECKBOX )
    {
        return true;
    }
    return false;
}


bool wxPGCheckBoxEditor::GetValueFromControl( wxVariant& variant, wxPGProperty* property, wxWindow* ctrl ) const
{
    wxSimpleCheckBox* cb = (wxSimpleCheckBox*)ctrl;

    int index = cb->m_state;

    if ( index != property->GetChoiceSelection() ||
         // Changing unspecified always causes event (returning
         // true here should be enough to trigger it).
         property->IsValueUnspecified()
       )
    {
        return property->IntToValue(variant, index, wxPG_PROPERTY_SPECIFIC);
    }
    return false;
}


void wxPGCheckBoxEditor::SetControlIntValue( wxPGProperty* WXUNUSED(property), wxWindow* ctrl, int value ) const
{
    if ( value != 0 ) value = 1;
    ((wxSimpleCheckBox*)ctrl)->m_state = value;
    ctrl->Refresh();
}


void wxPGCheckBoxEditor::SetValueToUnspecified( wxPGProperty* WXUNUSED(property), wxWindow* ctrl ) const
{
    ((wxSimpleCheckBox*)ctrl)->m_state = wxSCB_STATE_UNSPECIFIED;
    ctrl->Refresh();
}


wxPGCheckBoxEditor::~wxPGCheckBoxEditor()
{
    wxPG_EDITOR(CheckBox) = NULL;
}

#endif // wxPG_INCLUDE_CHECKBOX

// -----------------------------------------------------------------------

wxWindow* wxPropertyGrid::GetEditorControl() const
{
    wxWindow* ctrl = m_wndEditor;

    if ( !ctrl )
        return ctrl;

    return ctrl;
}

// -----------------------------------------------------------------------

void wxPropertyGrid::CorrectEditorWidgetSizeX()
{
    int secWid = 0;

    // Use fixed selColumn 1 for main editor widgets
    int newSplitterx = m_pState->DoGetSplitterPosition(0);
    int newWidth = newSplitterx + m_pState->m_colWidths[1];

    if ( m_wndEditor2 )
    {
        // if width change occurred, move secondary wnd by that amount
        wxRect r = m_wndEditor2->GetRect();
        secWid = r.width;
        r.x = newWidth - secWid;

        m_wndEditor2->SetSize( r );

        // if primary is textctrl, then we have to add some extra space
#ifdef __WXMAC__
        if ( m_wndEditor )
#else
        if ( wxDynamicCast(m_wndEditor, wxTextCtrl) )
#endif
            secWid += wxPG_TEXTCTRL_AND_BUTTON_SPACING;
    }

    if ( m_wndEditor )
    {
        wxRect r = m_wndEditor->GetRect();

        r.x = newSplitterx+m_ctrlXAdjust;

        if ( !(m_iFlags & wxPG_FL_FIXED_WIDTH_EDITOR) )
            r.width = newWidth - r.x - secWid;

        m_wndEditor->SetSize(r);
    }

    if ( m_wndEditor2 )
        m_wndEditor2->Refresh();
}

// -----------------------------------------------------------------------

void wxPropertyGrid::CorrectEditorWidgetPosY()
{
    wxPGProperty* selected = GetSelection();

    if ( selected )
    {
        if ( m_labelEditor )
        {
            wxRect r = GetEditorWidgetRect(selected, m_selColumn);
            wxPoint pos = m_labelEditor->GetPosition();

            // Calculate y offset
            int offset = pos.y % m_lineHeight;

            m_labelEditor->Move(pos.x, r.y + offset);
        }

        if ( m_wndEditor || m_wndEditor2 )
        {
            wxRect r = GetEditorWidgetRect(selected, 1);

            if ( m_wndEditor )
            {
                wxPoint pos = m_wndEditor->GetPosition();

                // Calculate y offset
                int offset = pos.y % m_lineHeight;

                m_wndEditor->Move(pos.x, r.y + offset);
            }

            if ( m_wndEditor2 )
            {
                wxPoint pos = m_wndEditor2->GetPosition();

                m_wndEditor2->Move(pos.x, r.y);
            }
        }
    }
}

// -----------------------------------------------------------------------

// Fixes position of wxTextCtrl-like control (wxSpinCtrl usually
// fits into that category as well).
void wxPropertyGrid::FixPosForTextCtrl( wxWindow* ctrl,
                                        unsigned int WXUNUSED(forColumn),
                                        const wxPoint& offset )
{
    // Center the control vertically
    wxRect finalPos = ctrl->GetRect();
    int y_adj = (m_lineHeight - finalPos.height)/2 + wxPG_TEXTCTRLYADJUST;

    // Prevent over-sized control
    int sz_dec = (y_adj + finalPos.height) - m_lineHeight;
    if ( sz_dec < 0 ) sz_dec = 0;

    finalPos.y += y_adj;
    finalPos.height -= (y_adj+sz_dec);

#ifndef wxPG_TEXTCTRLXADJUST
    int textCtrlXAdjust = wxPG_XBEFORETEXT - 1;

    wxTextCtrl* tc = static_cast<wxTextCtrl*>(ctrl);
    tc->SetMargins(0);
#else
    int textCtrlXAdjust = wxPG_TEXTCTRLXADJUST;
#endif

    finalPos.x += textCtrlXAdjust;
    finalPos.width -= textCtrlXAdjust;

    finalPos.x += offset.x;
    finalPos.y += offset.y;

    ctrl->SetSize(finalPos);
}

// -----------------------------------------------------------------------

wxWindow* wxPropertyGrid::GenerateEditorTextCtrl( const wxPoint& pos,
                                                  const wxSize& sz,
                                                  const wxString& value,
                                                  wxWindow* secondary,
                                                  int extraStyle,
                                                  int maxLen,
                                                  unsigned int forColumn )
{
    wxWindowID id = wxPG_SUBID1;
    wxPGProperty* prop = GetSelection();
    wxASSERT(prop);

    int tcFlags = wxTE_PROCESS_ENTER | extraStyle;

    if ( prop->HasFlag(wxPG_PROP_READONLY) && forColumn == 1 )
        tcFlags |= wxTE_READONLY;

    wxPoint p(pos.x,pos.y);
    wxSize s(sz.x,sz.y);

   // Need to reduce width of text control on Mac
#if defined(__WXMAC__)
    s.x -= 8;
#endif

    // For label editors, trim the size to allow better splitter grabbing
    if ( forColumn != 1 )
        s.x -= 2;

    // Take button into acccount
    if ( secondary )
    {
        s.x -= (secondary->GetSize().x + wxPG_TEXTCTRL_AND_BUTTON_SPACING);
        m_iFlags &= ~(wxPG_FL_PRIMARY_FILLS_ENTIRE);
    }

    // If the height is significantly higher, then use border, and fill the rect exactly.
    bool hasSpecialSize = false;

    if ( (sz.y - m_lineHeight) > 5 )
        hasSpecialSize = true;

    wxWindow* ctrlParent = GetPanel();

    if ( !hasSpecialSize )
        tcFlags |= wxBORDER_NONE;

    wxTextCtrl* tc = new wxTextCtrl();

#if defined(__WXMSW__)
    tc->Hide();
#endif
    SetupTextCtrlValue(value);
    tc->Create(ctrlParent,id,value, p, s,tcFlags);

#if defined(__WXMSW__)
    // On Windows, we need to override read-only text ctrl's background
    // colour to white. One problem with native 'grey' background is that
    // tc->GetBackgroundColour() doesn't seem to return correct value
    // for it.
    if ( tcFlags & wxTE_READONLY )
    {
        wxVisualAttributes vattrs = tc->GetDefaultAttributes();
        tc->SetBackgroundColour(vattrs.colBg);
    }
#endif

    // This code is repeated from DoSelectProperty(). However, font boldness
    // must be set before margin is set up below in FixPosForTextCtrl().
    if ( forColumn == 1 &&
         prop->HasFlag(wxPG_PROP_MODIFIED) &&
         HasFlag(wxPG_BOLD_MODIFIED) )
         tc->SetFont( m_captionFont );

    // Center the control vertically
    if ( !hasSpecialSize )
        FixPosForTextCtrl(tc, forColumn);

    if ( forColumn != 1 )
    {
        tc->SetBackgroundColour(m_colSelBack);
        tc->SetForegroundColour(m_colSelFore);
    }

#ifdef __WXMSW__
    tc->Show();
    if ( secondary )
        secondary->Show();
#endif

    // Set maximum length
    if ( maxLen > 0 )
        tc->SetMaxLength( maxLen );

    wxVariant attrVal = prop->GetAttribute(wxPG_ATTR_AUTOCOMPLETE);
    if ( !attrVal.IsNull() )
    {
        wxASSERT(attrVal.GetType() == wxS("arrstring"));
        tc->AutoComplete(attrVal.GetArrayString());
    }

    // Set hint text
    tc->SetHint(prop->GetHintText());

    return tc;
}

// -----------------------------------------------------------------------

wxWindow* wxPropertyGrid::GenerateEditorButton( const wxPoint& pos, const wxSize& sz )
{
    wxWindowID id = wxPG_SUBID2;
    wxPGProperty* selected = GetSelection();
    wxASSERT(selected);

#ifdef __WXMAC__
   // Decorations are chunky on Mac, and we can't make the button square, so
   // do things a bit differently on this platform.

   wxPoint p(pos.x+sz.x,
             pos.y+wxPG_BUTTON_SIZEDEC-wxPG_NAT_BUTTON_BORDER_Y);
   wxSize s(25, -1);

   wxButton* but = new wxButton();
   but->Create(GetPanel(),id,wxS("..."),p,s,wxWANTS_CHARS);

   // Now that we know the size, move to the correct position
   p.x = pos.x + sz.x - but->GetSize().x - 2;
   but->Move(p);

#else
    wxSize s(sz.y-(wxPG_BUTTON_SIZEDEC*2)+(wxPG_NAT_BUTTON_BORDER_Y*2),
        sz.y-(wxPG_BUTTON_SIZEDEC*2)+(wxPG_NAT_BUTTON_BORDER_Y*2));

    // Reduce button width to lineheight
    if ( s.x > m_lineHeight )
        s.x = m_lineHeight;

#ifdef __WXGTK__
    // On wxGTK, take fixed button margins into account
    if ( s.x < 25 )
        s.x = 25;
#endif

    wxPoint p(pos.x+sz.x-s.x,
        pos.y+wxPG_BUTTON_SIZEDEC-wxPG_NAT_BUTTON_BORDER_Y);

    wxButton* but = new wxButton();
  #ifdef __WXMSW__
    but->Hide();
  #endif
    but->Create(GetPanel(),id,wxS("..."),p,s,wxWANTS_CHARS);

  #ifdef __WXGTK__
    wxFont font = GetFont();
    font.SetPointSize(font.GetPointSize()-2);
    but->SetFont(font);
  #else
    but->SetFont(GetFont());
  #endif
#endif

    if ( selected->HasFlag(wxPG_PROP_READONLY) )
        but->Disable();

    return but;
}

// -----------------------------------------------------------------------

wxWindow* wxPropertyGrid::GenerateEditorTextCtrlAndButton( const wxPoint& pos,
                                                           const wxSize& sz,
                                                           wxWindow** psecondary,
                                                           int limitedEditing,
                                                           wxPGProperty* property )
{
    wxButton* but = (wxButton*)GenerateEditorButton(pos,sz);
    *psecondary = (wxWindow*)but;

    if ( limitedEditing )
    {
    #ifdef __WXMSW__
        // There is button Show in GenerateEditorTextCtrl as well
        but->Show();
    #endif
        return NULL;
    }

    wxString text;

    if ( !property->IsValueUnspecified() )
        text = property->GetValueAsString(property->HasFlag(wxPG_PROP_READONLY)?0:wxPG_EDITABLE_VALUE);

    return GenerateEditorTextCtrl(pos,sz,text,but,property->m_maxLen);
}

// -----------------------------------------------------------------------

void wxPropertyGrid::SetEditorAppearance( const wxPGCell& cell,
                                          bool unspecified )
{
    wxPGProperty* property = GetSelection();
    if ( !property )
        return;
    wxWindow* ctrl = GetEditorControl();
    if ( !ctrl )
        return;

    property->GetEditorClass()->SetControlAppearance( this,
                                                      property,
                                                      ctrl,
                                                      cell,
                                                      m_editorAppearance,
                                                      unspecified );

    m_editorAppearance = cell;
}

// -----------------------------------------------------------------------

wxTextCtrl* wxPropertyGrid::GetEditorTextCtrl() const
{
    wxWindow* wnd = GetEditorControl();

    if ( !wnd )
        return NULL;

    if ( wxDynamicCast(wnd, wxTextCtrl) )
        return wxStaticCast(wnd, wxTextCtrl);

    if ( wxDynamicCast(wnd, wxOwnerDrawnComboBox) )
    {
        wxOwnerDrawnComboBox* cb = wxStaticCast(wnd, wxOwnerDrawnComboBox);
        return cb->GetTextCtrl();
    }

    return NULL;
}

// -----------------------------------------------------------------------

wxPGEditor* wxPropertyGridInterface::GetEditorByName( const wxString& editorName )
{
    wxPGHashMapS2P::const_iterator it;

    it = wxPGGlobalVars->m_mapEditorClasses.find(editorName);
    if ( it == wxPGGlobalVars->m_mapEditorClasses.end() )
        return NULL;
    return (wxPGEditor*) it->second;
}

// -----------------------------------------------------------------------
// wxPGEditorDialogAdapter
// -----------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxPGEditorDialogAdapter, wxObject)

bool wxPGEditorDialogAdapter::ShowDialog( wxPropertyGrid* propGrid, wxPGProperty* property )
{
    if ( !propGrid->EditorValidate() )
        return false;

    bool res = DoShowDialog( propGrid, property );

    if ( res )
    {
        propGrid->ValueChangeInEvent( m_value );
        return true;
    }

    return false;
}

// -----------------------------------------------------------------------
// wxPGMultiButton
// -----------------------------------------------------------------------

wxPGMultiButton::wxPGMultiButton( wxPropertyGrid* pg, const wxSize& sz )
    : wxWindow( pg->GetPanel(), wxPG_SUBID2, wxPoint(-100,-100), wxSize(0, sz.y) ),
      m_fullEditorSize(sz), m_buttonsWidth(0)
{
    SetBackgroundColour(pg->GetCellBackgroundColour());
}

void wxPGMultiButton::Finalize( wxPropertyGrid* WXUNUSED(propGrid),
                                const wxPoint& pos )
{
    Move( pos.x + m_fullEditorSize.x - m_buttonsWidth, pos.y );
}

int wxPGMultiButton::GenId( int itemid ) const
{
    if ( itemid < -1 )
    {
        if ( m_buttons.size() )
            itemid = GetButton(m_buttons.size()-1)->GetId() + 1;
        else
            itemid = wxPG_SUBID2;
    }
    return itemid;
}

#if wxUSE_BMPBUTTON
void wxPGMultiButton::Add( const wxBitmap& bitmap, int itemid )
{
    itemid = GenId(itemid);
    wxSize sz = GetSize();
    wxButton* button = new wxBitmapButton( this, itemid, bitmap,
                                           wxPoint(sz.x, 0),
                                           wxSize(sz.y, sz.y) );
    DoAddButton( button, sz );
}
#endif

void wxPGMultiButton::Add( const wxString& label, int itemid )
{
    itemid = GenId(itemid);
    wxSize sz = GetSize();
    wxButton* button = new wxButton( this, itemid, label, wxPoint(sz.x, 0),
                                     wxSize(sz.y, sz.y) );
    DoAddButton( button, sz );
}

void wxPGMultiButton::DoAddButton( wxWindow* button,
                                   const wxSize& sz )
{
    m_buttons.push_back(button);
    int bw = button->GetSize().x;
    SetSize(wxSize(sz.x+bw,sz.y));
    m_buttonsWidth += bw;
}

// -----------------------------------------------------------------------

#endif  // wxUSE_PROPGRID
