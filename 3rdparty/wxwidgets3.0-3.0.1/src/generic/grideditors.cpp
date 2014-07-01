///////////////////////////////////////////////////////////////////////////
// Name:        src/generic/grideditors.cpp
// Purpose:     wxGridCellEditorEvtHandler and wxGrid editors
// Author:      Michael Bedward (based on code by Julian Smart, Robin Dunn)
// Modified by: Robin Dunn, Vadim Zeitlin, Santiago Palacios
// Created:     1/08/1999
// Copyright:   (c) Michael Bedward (mbedward@ozemail.com.au)
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_GRID

#include "wx/grid.h"

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/dcclient.h"
    #include "wx/settings.h"
    #include "wx/log.h"
    #include "wx/textctrl.h"
    #include "wx/checkbox.h"
    #include "wx/combobox.h"
    #include "wx/intl.h"
    #include "wx/math.h"
    #include "wx/listbox.h"
#endif

#include "wx/valnum.h"
#include "wx/textfile.h"
#include "wx/spinctrl.h"
#include "wx/tokenzr.h"
#include "wx/renderer.h"
#include "wx/headerctrl.h"

#include "wx/generic/gridsel.h"
#include "wx/generic/grideditors.h"
#include "wx/generic/private/grid.h"

#if defined(__WXMOTIF__)
    #define WXUNUSED_MOTIF(identifier)  WXUNUSED(identifier)
#else
    #define WXUNUSED_MOTIF(identifier)  identifier
#endif

#if defined(__WXGTK__)
    #define WXUNUSED_GTK(identifier)    WXUNUSED(identifier)
#else
    #define WXUNUSED_GTK(identifier)    identifier
#endif

#ifdef __WXOSX__
#include "wx/osx/private.h"
#endif

// Required for wxIs... functions
#include <ctype.h>

// ============================================================================
// implementation
// ============================================================================

wxDEFINE_EVENT( wxEVT_GRID_HIDE_EDITOR, wxCommandEvent );

// ----------------------------------------------------------------------------
// wxGridCellEditorEvtHandler
// ----------------------------------------------------------------------------

void wxGridCellEditorEvtHandler::OnKillFocus(wxFocusEvent& event)
{
    // We must let the native control have this event so in any case don't mark
    // it as handled, otherwise various weird problems can happen (see #11681).
    event.Skip();

    // Don't disable the cell if we're just starting to edit it
    if (m_inSetFocus)
        return;

    // Tell the grid to dismiss the control but don't do it immediately as it
    // could result in the editor being destroyed right now and a crash in the
    // code searching for the next event handler, so post an event asking the
    // grid to do it slightly later instead.

    // FIXME-VC6: Once we drop support for VC6, we should use a simpler
    //            m_grid->CallAfter(&wxGrid::DisableCellEditControl) and get
    //            rid of wxEVT_GRID_HIDE_EDITOR entirely.
    m_grid->GetEventHandler()->
        AddPendingEvent(wxCommandEvent(wxEVT_GRID_HIDE_EDITOR));
}

void wxGridCellEditorEvtHandler::OnKeyDown(wxKeyEvent& event)
{
    switch ( event.GetKeyCode() )
    {
        case WXK_ESCAPE:
            m_editor->Reset();
            m_grid->DisableCellEditControl();
            break;

        case WXK_TAB:
            m_grid->GetEventHandler()->ProcessEvent( event );
            break;

        case WXK_RETURN:
        case WXK_NUMPAD_ENTER:
            if (!m_grid->GetEventHandler()->ProcessEvent(event))
                m_editor->HandleReturn(event);
            break;

        default:
            event.Skip();
            break;
    }
}

void wxGridCellEditorEvtHandler::OnChar(wxKeyEvent& event)
{
    int row = m_grid->GetGridCursorRow();
    int col = m_grid->GetGridCursorCol();
    wxRect rect = m_grid->CellToRect( row, col );
    int cw, ch;
    m_grid->GetGridWindow()->GetClientSize( &cw, &ch );

    // if cell width is smaller than grid client area, cell is wholly visible
    bool wholeCellVisible = (rect.GetWidth() < cw);

    switch ( event.GetKeyCode() )
    {
        case WXK_ESCAPE:
        case WXK_TAB:
        case WXK_RETURN:
        case WXK_NUMPAD_ENTER:
            break;

        case WXK_HOME:
        {
            if ( wholeCellVisible )
            {
                // no special processing needed...
                event.Skip();
                break;
            }

            // do special processing for partly visible cell...

            // get the widths of all cells previous to this one
            int colXPos = 0;
            for ( int i = 0; i < col; i++ )
            {
                colXPos += m_grid->GetColSize(i);
            }

            int xUnit = 1, yUnit = 1;
            m_grid->GetScrollPixelsPerUnit(&xUnit, &yUnit);
            if (col != 0)
            {
                m_grid->Scroll(colXPos / xUnit - 1, m_grid->GetScrollPos(wxVERTICAL));
            }
            else
            {
                m_grid->Scroll(colXPos / xUnit, m_grid->GetScrollPos(wxVERTICAL));
            }
            event.Skip();
            break;
        }

        case WXK_END:
        {
            if ( wholeCellVisible )
            {
                // no special processing needed...
                event.Skip();
                break;
            }

            // do special processing for partly visible cell...

            int textWidth = 0;
            wxString value = m_grid->GetCellValue(row, col);
            if ( wxEmptyString != value )
            {
                // get width of cell CONTENTS (text)
                int y;
                wxFont font = m_grid->GetCellFont(row, col);
                m_grid->GetTextExtent(value, &textWidth, &y, NULL, NULL, &font);

                // try to RIGHT align the text by scrolling
                int client_right = m_grid->GetGridWindow()->GetClientSize().GetWidth();

                // (m_grid->GetScrollLineX()*2) is a factor for not scrolling to far,
                // otherwise the last part of the cell content might be hidden below the scroll bar
                // FIXME: maybe there is a more suitable correction?
                textWidth -= (client_right - (m_grid->GetScrollLineX() * 2));
                if ( textWidth < 0 )
                {
                    textWidth = 0;
                }
            }

            // get the widths of all cells previous to this one
            int colXPos = 0;
            for ( int i = 0; i < col; i++ )
            {
                colXPos += m_grid->GetColSize(i);
            }

            // and add the (modified) text width of the cell contents
            // as we'd like to see the last part of the cell contents
            colXPos += textWidth;

            int xUnit = 1, yUnit = 1;
            m_grid->GetScrollPixelsPerUnit(&xUnit, &yUnit);
            m_grid->Scroll(colXPos / xUnit - 1, m_grid->GetScrollPos(wxVERTICAL));
            event.Skip();
            break;
        }

        default:
            event.Skip();
            break;
    }
}

// ----------------------------------------------------------------------------
// wxGridCellEditor
// ----------------------------------------------------------------------------

wxGridCellEditor::wxGridCellEditor()
{
    m_control = NULL;
    m_attr = NULL;
}

wxGridCellEditor::~wxGridCellEditor()
{
    Destroy();
}

void wxGridCellEditor::Create(wxWindow* WXUNUSED(parent),
                              wxWindowID WXUNUSED(id),
                              wxEvtHandler* evtHandler)
{
    if ( evtHandler )
        m_control->PushEventHandler(evtHandler);
}

void wxGridCellEditor::PaintBackground(wxDC& dc,
                                       const wxRect& rectCell,
                                       const wxGridCellAttr& attr)
{
    // erase the background because we might not fill the cell
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(attr.GetBackgroundColour()));
    dc.DrawRectangle(rectCell);
}

void wxGridCellEditor::Destroy()
{
    if (m_control)
    {
        m_control->PopEventHandler( true /* delete it*/ );

        m_control->Destroy();
        m_control = NULL;
    }
}

void wxGridCellEditor::Show(bool show, wxGridCellAttr *attr)
{
    wxASSERT_MSG(m_control, wxT("The wxGridCellEditor must be created first!"));

    m_control->Show(show);

    if ( show )
    {
        // set the colours/fonts if we have any
        if ( attr )
        {
            m_colFgOld = m_control->GetForegroundColour();
            m_control->SetForegroundColour(attr->GetTextColour());

            m_colBgOld = m_control->GetBackgroundColour();
            m_control->SetBackgroundColour(attr->GetBackgroundColour());

// Workaround for GTK+1 font setting problem on some platforms
#if !defined(__WXGTK__) || defined(__WXGTK20__)
            m_fontOld = m_control->GetFont();
            m_control->SetFont(attr->GetFont());
#endif

            // can't do anything more in the base class version, the other
            // attributes may only be used by the derived classes
        }
    }
    else
    {
        // restore the standard colours fonts
        if ( m_colFgOld.IsOk() )
        {
            m_control->SetForegroundColour(m_colFgOld);
            m_colFgOld = wxNullColour;
        }

        if ( m_colBgOld.IsOk() )
        {
            m_control->SetBackgroundColour(m_colBgOld);
            m_colBgOld = wxNullColour;
        }

// Workaround for GTK+1 font setting problem on some platforms
#if !defined(__WXGTK__) || defined(__WXGTK20__)
        if ( m_fontOld.IsOk() )
        {
            m_control->SetFont(m_fontOld);
            m_fontOld = wxNullFont;
        }
#endif
    }
}

void wxGridCellEditor::SetSize(const wxRect& rect)
{
    wxASSERT_MSG(m_control, wxT("The wxGridCellEditor must be created first!"));

    m_control->SetSize(rect, wxSIZE_ALLOW_MINUS_ONE);
}

void wxGridCellEditor::HandleReturn(wxKeyEvent& event)
{
    event.Skip();
}

bool wxGridCellEditor::IsAcceptedKey(wxKeyEvent& event)
{
    bool ctrl = event.ControlDown();
    bool alt;

#ifdef __WXMAC__
    // On the Mac the Alt key is more like shift and is used for entry of
    // valid characters, so check for Ctrl and Meta instead.
    alt = event.MetaDown();
#else // !__WXMAC__
    alt = event.AltDown();
#endif // __WXMAC__/!__WXMAC__

    // Assume it's not a valid char if ctrl or alt is down, but if both are
    // down then it may be because of an AltGr key combination, so let them
    // through in that case.
    if ((ctrl || alt) && !(ctrl && alt))
        return false;

#if wxUSE_UNICODE
    if ( static_cast<int>(event.GetUnicodeKey()) == WXK_NONE )
        return false;
#else
    if ( event.GetKeyCode() > WXK_START )
        return false;
#endif

    return true;
}

void wxGridCellEditor::StartingKey(wxKeyEvent& event)
{
    event.Skip();
}

void wxGridCellEditor::StartingClick()
{
}

#if wxUSE_TEXTCTRL

// ----------------------------------------------------------------------------
// wxGridCellTextEditor
// ----------------------------------------------------------------------------

wxGridCellTextEditor::wxGridCellTextEditor(size_t maxChars)
{
    m_maxChars = maxChars;
}

void wxGridCellTextEditor::Create(wxWindow* parent,
                                  wxWindowID id,
                                  wxEvtHandler* evtHandler)
{
    DoCreate(parent, id, evtHandler);
}

void wxGridCellTextEditor::DoCreate(wxWindow* parent,
                                    wxWindowID id,
                                    wxEvtHandler* evtHandler,
                                    long style)
{
    style |= wxTE_PROCESS_ENTER | wxTE_PROCESS_TAB | wxNO_BORDER;

    wxTextCtrl* const text = new wxTextCtrl(parent, id, wxEmptyString,
                                            wxDefaultPosition, wxDefaultSize,
                                            style);
    text->SetMargins(0, 0);
    m_control = text;

#ifdef __WXOSX__
    wxWidgetImpl* impl = m_control->GetPeer();
    impl->SetNeedsFocusRect(false);
#endif
    // set max length allowed in the textctrl, if the parameter was set
    if ( m_maxChars != 0 )
    {
        Text()->SetMaxLength(m_maxChars);
    }
#if wxUSE_VALIDATORS
    // validate text in textctrl, if validator is set
    if ( m_validator )
    {
        Text()->SetValidator(*m_validator);
    }
#endif

    wxGridCellEditor::Create(parent, id, evtHandler);
}

void wxGridCellTextEditor::PaintBackground(wxDC& WXUNUSED(dc),
                                           const wxRect& WXUNUSED(rectCell),
                                           const wxGridCellAttr& WXUNUSED(attr))
{
    // as we fill the entire client area,
    // don't do anything here to minimize flicker
}

void wxGridCellTextEditor::SetSize(const wxRect& rectOrig)
{
    wxRect rect(rectOrig);

    // Make the edit control large enough to allow for internal margins
    //
    // TODO: remove this if the text ctrl sizing is improved esp. for unix
    //
#if defined(__WXGTK__)
    if (rect.x != 0)
    {
        rect.x += 1;
        rect.y += 1;
        rect.width -= 1;
        rect.height -= 1;
    }
#elif defined(__WXMSW__)
    if ( rect.x == 0 )
        rect.x += 2;
    else
        rect.x += 3;

    if ( rect.y == 0 )
        rect.y += 2;
    else
        rect.y += 3;

    rect.width -= 2;
    rect.height -= 2;
#elif defined(__WXOSX__)
    rect.x += 1;
    rect.y += 1;
    
    rect.width -= 1;
    rect.height -= 1;
#else
    int extra_x = ( rect.x > 2 ) ? 2 : 1;
    int extra_y = ( rect.y > 2 ) ? 2 : 1;

    #if defined(__WXMOTIF__)
        extra_x *= 2;
        extra_y *= 2;
    #endif

    rect.SetLeft( wxMax(0, rect.x - extra_x) );
    rect.SetTop( wxMax(0, rect.y - extra_y) );
    rect.SetRight( rect.GetRight() + 2 * extra_x );
    rect.SetBottom( rect.GetBottom() + 2 * extra_y );
#endif

    wxGridCellEditor::SetSize(rect);
}

void wxGridCellTextEditor::BeginEdit(int row, int col, wxGrid* grid)
{
    wxASSERT_MSG(m_control, wxT("The wxGridCellEditor must be created first!"));

    m_value = grid->GetTable()->GetValue(row, col);

    DoBeginEdit(m_value);
}

void wxGridCellTextEditor::DoBeginEdit(const wxString& startValue)
{
    Text()->SetValue(startValue);
    Text()->SetInsertionPointEnd();
    Text()->SelectAll();
    Text()->SetFocus();
}

bool wxGridCellTextEditor::EndEdit(int WXUNUSED(row),
                                   int WXUNUSED(col),
                                   const wxGrid* WXUNUSED(grid),
                                   const wxString& WXUNUSED(oldval),
                                   wxString *newval)
{
    wxCHECK_MSG( m_control, false,
                 "wxGridCellTextEditor must be created first!" );

    const wxString value = Text()->GetValue();
    if ( value == m_value )
        return false;

    m_value = value;

    if ( newval )
        *newval = m_value;

    return true;
}

void wxGridCellTextEditor::ApplyEdit(int row, int col, wxGrid* grid)
{
    grid->GetTable()->SetValue(row, col, m_value);
    m_value.clear();
}

void wxGridCellTextEditor::Reset()
{
    wxASSERT_MSG( m_control, "wxGridCellTextEditor must be created first!" );

    DoReset(m_value);
}

void wxGridCellTextEditor::DoReset(const wxString& startValue)
{
    Text()->SetValue(startValue);
    Text()->SetInsertionPointEnd();
}

bool wxGridCellTextEditor::IsAcceptedKey(wxKeyEvent& event)
{
    switch ( event.GetKeyCode() )
    {
        case WXK_DELETE:
        case WXK_BACK:
            return true;

        default:
            return wxGridCellEditor::IsAcceptedKey(event);
    }
}

void wxGridCellTextEditor::StartingKey(wxKeyEvent& event)
{
    // Since this is now happening in the EVT_CHAR event EmulateKeyPress is no
    // longer an appropriate way to get the character into the text control.
    // Do it ourselves instead.  We know that if we get this far that we have
    // a valid character, so not a whole lot of testing needs to be done.

    wxTextCtrl* tc = Text();
    int ch;

    bool isPrintable;

#if wxUSE_UNICODE
    ch = event.GetUnicodeKey();
    if ( ch != WXK_NONE )
        isPrintable = true;
    else
#endif // wxUSE_UNICODE
    {
        ch = event.GetKeyCode();
        isPrintable = ch >= WXK_SPACE && ch < WXK_START;
    }

    switch (ch)
    {
        case WXK_DELETE:
            // Delete the initial character when starting to edit with DELETE.
            tc->Remove(0, 1);
            break;

        case WXK_BACK:
            // Delete the last character when starting to edit with BACKSPACE.
            {
                const long pos = tc->GetLastPosition();
                tc->Remove(pos - 1, pos);
            }
            break;

        default:
            if ( isPrintable )
                tc->WriteText(static_cast<wxChar>(ch));
            break;
    }
}

void wxGridCellTextEditor::HandleReturn( wxKeyEvent&
                                         WXUNUSED_GTK(WXUNUSED_MOTIF(event)) )
{
#if defined(__WXMOTIF__) || defined(__WXGTK__)
    // wxMotif needs a little extra help...
    size_t pos = (size_t)( Text()->GetInsertionPoint() );
    wxString s( Text()->GetValue() );
    s = s.Left(pos) + wxT("\n") + s.Mid(pos);
    Text()->SetValue(s);
    Text()->SetInsertionPoint( pos );
#else
    // the other ports can handle a Return key press
    //
    event.Skip();
#endif
}

void wxGridCellTextEditor::SetParameters(const wxString& params)
{
    if ( !params )
    {
        // reset to default
        m_maxChars = 0;
    }
    else
    {
        long tmp;
        if ( params.ToLong(&tmp) )
        {
            m_maxChars = (size_t)tmp;
        }
        else
        {
            wxLogDebug( wxT("Invalid wxGridCellTextEditor parameter string '%s' ignored"), params.c_str() );
        }
    }
}

#if wxUSE_VALIDATORS
void wxGridCellTextEditor::SetValidator(const wxValidator& validator)
{
    m_validator.reset(static_cast<wxValidator*>(validator.Clone()));
}
#endif

wxGridCellEditor *wxGridCellTextEditor::Clone() const
{
    wxGridCellTextEditor* editor = new wxGridCellTextEditor(m_maxChars);
#if wxUSE_VALIDATORS
    if ( m_validator )
    {
        editor->SetValidator(*m_validator);
    }
#endif
    return editor;
}

// return the value in the text control
wxString wxGridCellTextEditor::GetValue() const
{
    return Text()->GetValue();
}

// ----------------------------------------------------------------------------
// wxGridCellNumberEditor
// ----------------------------------------------------------------------------

wxGridCellNumberEditor::wxGridCellNumberEditor(int min, int max)
{
    m_min = min;
    m_max = max;
}

void wxGridCellNumberEditor::Create(wxWindow* parent,
                                    wxWindowID id,
                                    wxEvtHandler* evtHandler)
{
#if wxUSE_SPINCTRL
    if ( HasRange() )
    {
        // create a spin ctrl
        m_control = new wxSpinCtrl(parent, wxID_ANY, wxEmptyString,
                                   wxDefaultPosition, wxDefaultSize,
                                   wxSP_ARROW_KEYS,
                                   m_min, m_max);

        wxGridCellEditor::Create(parent, id, evtHandler);
    }
    else
#endif
    {
        // just a text control
        wxGridCellTextEditor::Create(parent, id, evtHandler);

#if wxUSE_VALIDATORS
        Text()->SetValidator(wxIntegerValidator<int>());
#endif
    }
}

void wxGridCellNumberEditor::BeginEdit(int row, int col, wxGrid* grid)
{
    // first get the value
    wxGridTableBase *table = grid->GetTable();
    if ( table->CanGetValueAs(row, col, wxGRID_VALUE_NUMBER) )
    {
        m_value = table->GetValueAsLong(row, col);
    }
    else
    {
        m_value = 0;
        wxString sValue = table->GetValue(row, col);
        if (! sValue.ToLong(&m_value) && ! sValue.empty())
        {
            wxFAIL_MSG( wxT("this cell doesn't have numeric value") );
            return;
        }
    }

#if wxUSE_SPINCTRL
    if ( HasRange() )
    {
        Spin()->SetValue((int)m_value);
        Spin()->SetFocus();
    }
    else
#endif
    {
        DoBeginEdit(GetString());
    }
}

bool wxGridCellNumberEditor::EndEdit(int WXUNUSED(row),
                                     int WXUNUSED(col),
                                     const wxGrid* WXUNUSED(grid),
                                     const wxString& oldval, wxString *newval)
{
    long value = 0;
    wxString text;

#if wxUSE_SPINCTRL
    if ( HasRange() )
    {
        value = Spin()->GetValue();
        if ( value == m_value )
            return false;

        text.Printf(wxT("%ld"), value);
    }
    else // using unconstrained input
#endif // wxUSE_SPINCTRL
    {
        text = Text()->GetValue();
        if ( text.empty() )
        {
            if ( oldval.empty() )
                return false;
        }
        else // non-empty text now (maybe 0)
        {
            if ( !text.ToLong(&value) )
                return false;

            // if value == m_value == 0 but old text was "" and new one is
            // "0" something still did change
            if ( value == m_value && (value || !oldval.empty()) )
                return false;
        }
    }

    m_value = value;

    if ( newval )
        *newval = text;

    return true;
}

void wxGridCellNumberEditor::ApplyEdit(int row, int col, wxGrid* grid)
{
    wxGridTableBase * const table = grid->GetTable();
    if ( table->CanSetValueAs(row, col, wxGRID_VALUE_NUMBER) )
        table->SetValueAsLong(row, col, m_value);
    else
        table->SetValue(row, col, wxString::Format("%ld", m_value));
}

void wxGridCellNumberEditor::Reset()
{
#if wxUSE_SPINCTRL
    if ( HasRange() )
    {
        Spin()->SetValue((int)m_value);
    }
    else
#endif
    {
        DoReset(GetString());
    }
}

bool wxGridCellNumberEditor::IsAcceptedKey(wxKeyEvent& event)
{
    if ( wxGridCellEditor::IsAcceptedKey(event) )
    {
        int keycode = event.GetKeyCode();
        if ( (keycode < 128) &&
             (wxIsdigit(keycode) || keycode == '+' || keycode == '-'))
        {
            return true;
        }
    }

    return false;
}

void wxGridCellNumberEditor::StartingKey(wxKeyEvent& event)
{
    int keycode = event.GetKeyCode();
    if ( !HasRange() )
    {
        if ( wxIsdigit(keycode) || keycode == '+' || keycode == '-')
        {
            wxGridCellTextEditor::StartingKey(event);

            // skip Skip() below
            return;
        }
    }
#if wxUSE_SPINCTRL
    else
    {
        if ( wxIsdigit(keycode) )
        {
            wxSpinCtrl* spin = (wxSpinCtrl*)m_control;
            spin->SetValue(keycode - '0');
            spin->SetSelection(1,1);
            return;
        }
    }
#endif

    event.Skip();
}

void wxGridCellNumberEditor::SetParameters(const wxString& params)
{
    if ( !params )
    {
        // reset to default
        m_min =
        m_max = -1;
    }
    else
    {
        long tmp;
        if ( params.BeforeFirst(wxT(',')).ToLong(&tmp) )
        {
            m_min = (int)tmp;

            if ( params.AfterFirst(wxT(',')).ToLong(&tmp) )
            {
                m_max = (int)tmp;

                // skip the error message below
                return;
            }
        }

        wxLogDebug(wxT("Invalid wxGridCellNumberEditor parameter string '%s' ignored"), params.c_str());
    }
}

// return the value in the spin control if it is there (the text control otherwise)
wxString wxGridCellNumberEditor::GetValue() const
{
    wxString s;

#if wxUSE_SPINCTRL
    if ( HasRange() )
    {
        long value = Spin()->GetValue();
        s.Printf(wxT("%ld"), value);
    }
    else
#endif
    {
        s = Text()->GetValue();
    }

    return s;
}

// ----------------------------------------------------------------------------
// wxGridCellFloatEditor
// ----------------------------------------------------------------------------

wxGridCellFloatEditor::wxGridCellFloatEditor(int width,
                                             int precision,
                                             int format)
{
    m_width = width;
    m_precision = precision;
    m_style = format;
}

void wxGridCellFloatEditor::Create(wxWindow* parent,
                                   wxWindowID id,
                                   wxEvtHandler* evtHandler)
{
    wxGridCellTextEditor::Create(parent, id, evtHandler);

#if wxUSE_VALIDATORS
    Text()->SetValidator(wxFloatingPointValidator<double>(m_precision));
#endif
}

void wxGridCellFloatEditor::BeginEdit(int row, int col, wxGrid* grid)
{
    // first get the value
    wxGridTableBase * const table = grid->GetTable();
    if ( table->CanGetValueAs(row, col, wxGRID_VALUE_FLOAT) )
    {
        m_value = table->GetValueAsDouble(row, col);
    }
    else
    {
        m_value = 0.0;

        const wxString value = table->GetValue(row, col);
        if ( !value.empty() )
        {
            if ( !value.ToDouble(&m_value) )
            {
                wxFAIL_MSG( wxT("this cell doesn't have float value") );
                return;
            }
        }
    }

    DoBeginEdit(GetString());
}

bool wxGridCellFloatEditor::EndEdit(int WXUNUSED(row),
                                    int WXUNUSED(col),
                                    const wxGrid* WXUNUSED(grid),
                                    const wxString& oldval, wxString *newval)
{
    const wxString text(Text()->GetValue());

    double value;
    if ( !text.empty() )
    {
        if ( !text.ToDouble(&value) )
            return false;
    }
    else // new value is empty string
    {
        if ( oldval.empty() )
            return false;           // nothing changed

        value = 0.;
    }

    // the test for empty strings ensures that we don't skip the value setting
    // when "" is replaced by "0" or vice versa as "" numeric value is also 0.
    if ( wxIsSameDouble(value, m_value) && !text.empty() && !oldval.empty() )
        return false;           // nothing changed

    m_value = value;

    if ( newval )
        *newval = text;

    return true;
}

void wxGridCellFloatEditor::ApplyEdit(int row, int col, wxGrid* grid)
{
    wxGridTableBase * const table = grid->GetTable();

    if ( table->CanSetValueAs(row, col, wxGRID_VALUE_FLOAT) )
        table->SetValueAsDouble(row, col, m_value);
    else
        table->SetValue(row, col, Text()->GetValue());
}

void wxGridCellFloatEditor::Reset()
{
    DoReset(GetString());
}

void wxGridCellFloatEditor::StartingKey(wxKeyEvent& event)
{
    int keycode = event.GetKeyCode();
    char tmpbuf[2];
    tmpbuf[0] = (char) keycode;
    tmpbuf[1] = '\0';
    wxString strbuf(tmpbuf, *wxConvCurrent);

#if wxUSE_INTL
    bool is_decimal_point = ( strbuf ==
       wxLocale::GetInfo(wxLOCALE_DECIMAL_POINT, wxLOCALE_CAT_NUMBER) );
#else
    bool is_decimal_point = ( strbuf == wxT(".") );
#endif

    if ( wxIsdigit(keycode) || keycode == '+' || keycode == '-'
         || is_decimal_point )
    {
        wxGridCellTextEditor::StartingKey(event);

        // skip Skip() below
        return;
    }

    event.Skip();
}

void wxGridCellFloatEditor::SetParameters(const wxString& params)
{
    if ( !params )
    {
        // reset to default
        m_width =
        m_precision = -1;
        m_style = wxGRID_FLOAT_FORMAT_DEFAULT;
        m_format.clear();
    }
    else
    {
        wxString rest;
        wxString tmp = params.BeforeFirst(wxT(','), &rest);
        if ( !tmp.empty() )
        {
            long width;
            if ( tmp.ToLong(&width) )
            {
                m_width = (int)width;
            }
            else
            {
                wxLogDebug(wxT("Invalid wxGridCellFloatRenderer width parameter string '%s ignored"), params.c_str());
            }
        }

        tmp = rest.BeforeFirst(wxT(','));
        if ( !tmp.empty() )
        {
            long precision;
            if ( tmp.ToLong(&precision) )
            {
                m_precision = (int)precision;
            }
            else
            {
                wxLogDebug(wxT("Invalid wxGridCellFloatRenderer precision parameter string '%s ignored"), params.c_str());
            }
        }

        tmp = rest.AfterFirst(wxT(','));
        if ( !tmp.empty() )
        {
            if ( tmp[0] == wxT('f') )
            {
                m_style = wxGRID_FLOAT_FORMAT_FIXED;
            }
            else if ( tmp[0] == wxT('e') )
            {
                m_style = wxGRID_FLOAT_FORMAT_SCIENTIFIC;
            }
            else if ( tmp[0] == wxT('g') )
            {
                m_style = wxGRID_FLOAT_FORMAT_COMPACT;
            }
            else if ( tmp[0] == wxT('E') )
            {
                m_style = wxGRID_FLOAT_FORMAT_SCIENTIFIC |
                          wxGRID_FLOAT_FORMAT_UPPER;
            }
            else if ( tmp[0] == wxT('F') )
            {
                m_style = wxGRID_FLOAT_FORMAT_FIXED |
                          wxGRID_FLOAT_FORMAT_UPPER;
            }
            else if ( tmp[0] == wxT('G') )
            {
                m_style = wxGRID_FLOAT_FORMAT_COMPACT |
                          wxGRID_FLOAT_FORMAT_UPPER;
            }
            else
            {
                wxLogDebug("Invalid wxGridCellFloatRenderer format "
                           "parameter string '%s ignored", params);
            }
        }
    }
}

wxString wxGridCellFloatEditor::GetString()
{
    if ( !m_format )
    {
        if ( m_precision == -1 && m_width != -1)
        {
            // default precision
            m_format.Printf(wxT("%%%d."), m_width);
        }
        else if ( m_precision != -1 && m_width == -1)
        {
            // default width
            m_format.Printf(wxT("%%.%d"), m_precision);
        }
        else if ( m_precision != -1 && m_width != -1 )
        {
            m_format.Printf(wxT("%%%d.%d"), m_width, m_precision);
        }
        else
        {
            // default width/precision
            m_format = wxT("%");
        }

        bool isUpper = (m_style & wxGRID_FLOAT_FORMAT_UPPER) != 0;
        if ( m_style & wxGRID_FLOAT_FORMAT_SCIENTIFIC )
            m_format += isUpper ? wxT('E') : wxT('e');
        else if ( m_style & wxGRID_FLOAT_FORMAT_COMPACT )
            m_format += isUpper ? wxT('G') : wxT('g');
        else
            m_format += wxT('f');
    }

    return wxString::Format(m_format, m_value);
}

bool wxGridCellFloatEditor::IsAcceptedKey(wxKeyEvent& event)
{
    if ( wxGridCellEditor::IsAcceptedKey(event) )
    {
        const int keycode = event.GetKeyCode();
        if ( wxIsascii(keycode) )
        {
#if wxUSE_INTL
            const wxString decimalPoint =
                wxLocale::GetInfo(wxLOCALE_DECIMAL_POINT, wxLOCALE_CAT_NUMBER);
#else
            const wxString decimalPoint(wxT('.'));
#endif

            // accept digits, 'e' as in '1e+6', also '-', '+', and '.'
            if ( wxIsdigit(keycode) ||
                    tolower(keycode) == 'e' ||
                        keycode == decimalPoint ||
                            keycode == '+' ||
                                keycode == '-' )
            {
                return true;
            }
        }
    }

    return false;
}

#endif // wxUSE_TEXTCTRL

#if wxUSE_CHECKBOX

// ----------------------------------------------------------------------------
// wxGridCellBoolEditor
// ----------------------------------------------------------------------------

// the default values for GetValue()
wxString wxGridCellBoolEditor::ms_stringValues[2] = { wxT(""), wxT("1") };

void wxGridCellBoolEditor::Create(wxWindow* parent,
                                  wxWindowID id,
                                  wxEvtHandler* evtHandler)
{
    m_control = new wxCheckBox(parent, id, wxEmptyString,
                               wxDefaultPosition, wxDefaultSize,
                               wxNO_BORDER);

    wxGridCellEditor::Create(parent, id, evtHandler);
}

void wxGridCellBoolEditor::SetSize(const wxRect& r)
{
    bool resize = false;
    wxSize size = m_control->GetSize();
    wxCoord minSize = wxMin(r.width, r.height);

    // check if the checkbox is not too big/small for this cell
    wxSize sizeBest = m_control->GetBestSize();
    if ( !(size == sizeBest) )
    {
        // reset to default size if it had been made smaller
        size = sizeBest;

        resize = true;
    }

    if ( size.x >= minSize || size.y >= minSize )
    {
        // leave 1 pixel margin
        size.x = size.y = minSize - 2;

        resize = true;
    }

    if ( resize )
    {
        m_control->SetSize(size);
    }

    // position it in the centre of the rectangle (TODO: support alignment?)

#if defined(__WXGTK__) || defined (__WXMOTIF__)
    // the checkbox without label still has some space to the right in wxGTK,
    // so shift it to the right
    size.x -= 8;
#elif defined(__WXMSW__)
    // here too, but in other way
    size.x += 1;
    size.y -= 2;
#endif

    int hAlign = wxALIGN_CENTRE;
    int vAlign = wxALIGN_CENTRE;
    if (GetCellAttr())
        GetCellAttr()->GetAlignment(& hAlign, & vAlign);

    int x = 0, y = 0;
    if (hAlign == wxALIGN_LEFT)
    {
        x = r.x + 2;

#ifdef __WXMSW__
        x += 2;
#endif

        y = r.y + r.height / 2 - size.y / 2;
    }
    else if (hAlign == wxALIGN_RIGHT)
    {
        x = r.x + r.width - size.x - 2;
        y = r.y + r.height / 2 - size.y / 2;
    }
    else if (hAlign == wxALIGN_CENTRE)
    {
        x = r.x + r.width / 2 - size.x / 2;
        y = r.y + r.height / 2 - size.y / 2;
    }

    m_control->Move(x, y);
}

void wxGridCellBoolEditor::Show(bool show, wxGridCellAttr *attr)
{
    m_control->Show(show);

    if ( show )
    {
        wxColour colBg = attr ? attr->GetBackgroundColour() : *wxLIGHT_GREY;
        CBox()->SetBackgroundColour(colBg);
    }
}

void wxGridCellBoolEditor::BeginEdit(int row, int col, wxGrid* grid)
{
    wxASSERT_MSG(m_control,
                 wxT("The wxGridCellEditor must be created first!"));

    if (grid->GetTable()->CanGetValueAs(row, col, wxGRID_VALUE_BOOL))
    {
        m_value = grid->GetTable()->GetValueAsBool(row, col);
    }
    else
    {
        wxString cellval( grid->GetTable()->GetValue(row, col) );

        if ( cellval == ms_stringValues[false] )
            m_value = false;
        else if ( cellval == ms_stringValues[true] )
            m_value = true;
        else
        {
            // do not try to be smart here and convert it to true or false
            // because we'll still overwrite it with something different and
            // this risks to be very surprising for the user code, let them
            // know about it
            wxFAIL_MSG( wxT("invalid value for a cell with bool editor!") );
        }
    }

    CBox()->SetValue(m_value);
    CBox()->SetFocus();
}

bool wxGridCellBoolEditor::EndEdit(int WXUNUSED(row),
                                   int WXUNUSED(col),
                                   const wxGrid* WXUNUSED(grid),
                                   const wxString& WXUNUSED(oldval),
                                   wxString *newval)
{
    bool value = CBox()->GetValue();
    if ( value == m_value )
        return false;

    m_value = value;

    if ( newval )
        *newval = GetValue();

    return true;
}

void wxGridCellBoolEditor::ApplyEdit(int row, int col, wxGrid* grid)
{
    wxGridTableBase * const table = grid->GetTable();
    if ( table->CanSetValueAs(row, col, wxGRID_VALUE_BOOL) )
        table->SetValueAsBool(row, col, m_value);
    else
        table->SetValue(row, col, GetValue());
}

void wxGridCellBoolEditor::Reset()
{
    wxASSERT_MSG(m_control,
                 wxT("The wxGridCellEditor must be created first!"));

    CBox()->SetValue(m_value);
}

void wxGridCellBoolEditor::StartingClick()
{
    CBox()->SetValue(!CBox()->GetValue());
}

bool wxGridCellBoolEditor::IsAcceptedKey(wxKeyEvent& event)
{
    if ( wxGridCellEditor::IsAcceptedKey(event) )
    {
        int keycode = event.GetKeyCode();
        switch ( keycode )
        {
            case WXK_SPACE:
            case '+':
            case '-':
                return true;
        }
    }

    return false;
}

void wxGridCellBoolEditor::StartingKey(wxKeyEvent& event)
{
    int keycode = event.GetKeyCode();
    switch ( keycode )
    {
        case WXK_SPACE:
            CBox()->SetValue(!CBox()->GetValue());
            break;

        case '+':
            CBox()->SetValue(true);
            break;

        case '-':
            CBox()->SetValue(false);
            break;
    }
}

wxString wxGridCellBoolEditor::GetValue() const
{
  return ms_stringValues[CBox()->GetValue()];
}

/* static */ void
wxGridCellBoolEditor::UseStringValues(const wxString& valueTrue,
                                      const wxString& valueFalse)
{
    ms_stringValues[false] = valueFalse;
    ms_stringValues[true] = valueTrue;
}

/* static */ bool
wxGridCellBoolEditor::IsTrueValue(const wxString& value)
{
    return value == ms_stringValues[true];
}

#endif // wxUSE_CHECKBOX

#if wxUSE_COMBOBOX

// ----------------------------------------------------------------------------
// wxGridCellChoiceEditor
// ----------------------------------------------------------------------------

wxGridCellChoiceEditor::wxGridCellChoiceEditor(const wxArrayString& choices,
                                               bool allowOthers)
    : m_choices(choices),
      m_allowOthers(allowOthers) { }

wxGridCellChoiceEditor::wxGridCellChoiceEditor(size_t count,
                                               const wxString choices[],
                                               bool allowOthers)
                      : m_allowOthers(allowOthers)
{
    if ( count )
    {
        m_choices.Alloc(count);
        for ( size_t n = 0; n < count; n++ )
        {
            m_choices.Add(choices[n]);
        }
    }
}

wxGridCellEditor *wxGridCellChoiceEditor::Clone() const
{
    wxGridCellChoiceEditor *editor = new wxGridCellChoiceEditor;
    editor->m_allowOthers = m_allowOthers;
    editor->m_choices = m_choices;

    return editor;
}

void wxGridCellChoiceEditor::Create(wxWindow* parent,
                                    wxWindowID id,
                                    wxEvtHandler* evtHandler)
{
    int style = wxTE_PROCESS_ENTER |
                wxTE_PROCESS_TAB |
                wxBORDER_NONE;

    if ( !m_allowOthers )
        style |= wxCB_READONLY;
    m_control = new wxComboBox(parent, id, wxEmptyString,
                               wxDefaultPosition, wxDefaultSize,
                               m_choices,
                               style);

    wxGridCellEditor::Create(parent, id, evtHandler);
}

void wxGridCellChoiceEditor::SetSize(const wxRect& rect)
{
    wxASSERT_MSG(m_control,
                 wxT("The wxGridCellChoiceEditor must be created first!"));

    // Check that the height is not too small to fit the combobox.
    wxRect rectTallEnough = rect;
    const wxSize bestSize = m_control->GetBestSize();
    const wxCoord diffY = bestSize.GetHeight() - rectTallEnough.GetHeight();
    if ( diffY > 0 )
    {
        // Do make it tall enough.
        rectTallEnough.height += diffY;

        // Also centre the effective rectangle vertically with respect to the
        // original one.
        rectTallEnough.y -= diffY/2;
    }
    //else: The rectangle provided is already tall enough.

    wxGridCellEditor::SetSize(rectTallEnough);
}

void wxGridCellChoiceEditor::PaintBackground(wxDC& dc,
                                             const wxRect& rectCell,
                                             const wxGridCellAttr& attr)
{
    // as we fill the entire client area, don't do anything here to minimize
    // flicker

    // TODO: It doesn't actually fill the client area since the height of a
    // combo always defaults to the standard.  Until someone has time to
    // figure out the right rectangle to paint, just do it the normal way.
    wxGridCellEditor::PaintBackground(dc, rectCell, attr);
}

void wxGridCellChoiceEditor::BeginEdit(int row, int col, wxGrid* grid)
{
    wxASSERT_MSG(m_control,
                 wxT("The wxGridCellEditor must be created first!"));

    wxGridCellEditorEvtHandler* evtHandler = NULL;
    if (m_control)
        evtHandler = wxDynamicCast(m_control->GetEventHandler(), wxGridCellEditorEvtHandler);

    // Don't immediately end if we get a kill focus event within BeginEdit
    if (evtHandler)
        evtHandler->SetInSetFocus(true);

    m_value = grid->GetTable()->GetValue(row, col);

    Reset(); // this updates combo box to correspond to m_value

    Combo()->SetFocus();

#ifdef __WXOSX_COCOA__
    // This is a work around for the combobox being simply dismissed when a
    // choice is made in it under OS X. The bug is almost certainly due to a
    // problem in focus events generation logic but it's not obvious to fix and
    // for now this at least allows to use wxGrid.
    Combo()->Popup();
#endif

    if (evtHandler)
    {
        // When dropping down the menu, a kill focus event
        // happens after this point, so we can't reset the flag yet.
#if !defined(__WXGTK20__)
        evtHandler->SetInSetFocus(false);
#endif
    }
}

bool wxGridCellChoiceEditor::EndEdit(int WXUNUSED(row),
                                     int WXUNUSED(col),
                                     const wxGrid* WXUNUSED(grid),
                                     const wxString& WXUNUSED(oldval),
                                     wxString *newval)
{
    const wxString value = Combo()->GetValue();
    if ( value == m_value )
        return false;

    m_value = value;

    if ( newval )
        *newval = value;

    return true;
}

void wxGridCellChoiceEditor::ApplyEdit(int row, int col, wxGrid* grid)
{
    grid->GetTable()->SetValue(row, col, m_value);
}

void wxGridCellChoiceEditor::Reset()
{
    if (m_allowOthers)
    {
        Combo()->SetValue(m_value);
        Combo()->SetInsertionPointEnd();
    }
    else // the combobox is read-only
    {
        // find the right position, or default to the first if not found
        int pos = Combo()->FindString(m_value);
        if (pos == wxNOT_FOUND)
            pos = 0;
        Combo()->SetSelection(pos);
    }
}

void wxGridCellChoiceEditor::SetParameters(const wxString& params)
{
    if ( !params )
    {
        // what can we do?
        return;
    }

    m_choices.Empty();

    wxStringTokenizer tk(params, wxT(','));
    while ( tk.HasMoreTokens() )
    {
        m_choices.Add(tk.GetNextToken());
    }
}

// return the value in the text control
wxString wxGridCellChoiceEditor::GetValue() const
{
  return Combo()->GetValue();
}

#endif // wxUSE_COMBOBOX

#if wxUSE_COMBOBOX

// ----------------------------------------------------------------------------
// wxGridCellEnumEditor
// ----------------------------------------------------------------------------

// A cell editor which displays an enum number as a textual equivalent. eg
// data in cell is 0,1,2 ... n the cell could be displayed as
// "John","Fred"..."Bob" in the combo choice box

wxGridCellEnumEditor::wxGridCellEnumEditor(const wxString& choices)
                     :wxGridCellChoiceEditor()
{
    m_index = -1;

    if (!choices.empty())
        SetParameters(choices);
}

wxGridCellEditor *wxGridCellEnumEditor::Clone() const
{
    wxGridCellEnumEditor *editor = new wxGridCellEnumEditor();
    editor->m_index = m_index;
    return editor;
}

void wxGridCellEnumEditor::BeginEdit(int row, int col, wxGrid* grid)
{
    wxASSERT_MSG(m_control,
                 wxT("The wxGridCellEnumEditor must be Created first!"));

    wxGridCellEditorEvtHandler* evtHandler = NULL;
    if (m_control)
        evtHandler = wxDynamicCast(m_control->GetEventHandler(), wxGridCellEditorEvtHandler);

    // Don't immediately end if we get a kill focus event within BeginEdit
    if (evtHandler)
        evtHandler->SetInSetFocus(true);

    wxGridTableBase *table = grid->GetTable();

    if ( table->CanGetValueAs(row, col, wxGRID_VALUE_NUMBER) )
    {
        m_index = table->GetValueAsLong(row, col);
    }
    else
    {
        wxString startValue = table->GetValue(row, col);
        if (startValue.IsNumber() && !startValue.empty())
        {
            startValue.ToLong(&m_index);
        }
        else
        {
            m_index = -1;
        }
    }

    Combo()->SetSelection(m_index);
    Combo()->SetFocus();

#ifdef __WXOSX_COCOA__
    // This is a work around for the combobox being simply dismissed when a
    // choice is made in it under OS X. The bug is almost certainly due to a
    // problem in focus events generation logic but it's not obvious to fix and
    // for now this at least allows to use wxGrid.
    Combo()->Popup();
#endif

    if (evtHandler)
    {
        // When dropping down the menu, a kill focus event
        // happens after this point, so we can't reset the flag yet.
#if !defined(__WXGTK20__)
        evtHandler->SetInSetFocus(false);
#endif
    }
}

bool wxGridCellEnumEditor::EndEdit(int WXUNUSED(row),
                                   int WXUNUSED(col),
                                   const wxGrid* WXUNUSED(grid),
                                   const wxString& WXUNUSED(oldval),
                                   wxString *newval)
{
    long idx = Combo()->GetSelection();
    if ( idx == m_index )
        return false;

    m_index = idx;

    if ( newval )
        newval->Printf("%ld", m_index);

    return true;
}

void wxGridCellEnumEditor::ApplyEdit(int row, int col, wxGrid* grid)
{
    wxGridTableBase * const table = grid->GetTable();
    if ( table->CanSetValueAs(row, col, wxGRID_VALUE_NUMBER) )
        table->SetValueAsLong(row, col, m_index);
    else
        table->SetValue(row, col, wxString::Format("%ld", m_index));
}

#endif // wxUSE_COMBOBOX

// ----------------------------------------------------------------------------
// wxGridCellAutoWrapStringEditor
// ----------------------------------------------------------------------------

void
wxGridCellAutoWrapStringEditor::Create(wxWindow* parent,
                                       wxWindowID id,
                                       wxEvtHandler* evtHandler)
{
    wxGridCellTextEditor::DoCreate(parent, id, evtHandler,
                                    wxTE_MULTILINE | wxTE_RICH);
}


#endif // wxUSE_GRID
