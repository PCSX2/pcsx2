///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/textentrycmn.cpp
// Purpose:     wxTextEntryBase implementation
// Author:      Vadim Zeitlin
// Created:     2007-09-26
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_TEXTCTRL || wxUSE_COMBOBOX

#ifndef WX_PRECOMP
    #include "wx/window.h"
    #include "wx/dataobj.h"
#endif //WX_PRECOMP

#include "wx/textentry.h"
#include "wx/textcompleter.h"
#include "wx/clipbrd.h"

// ----------------------------------------------------------------------------
// wxTextEntryHintData
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxTextEntryHintData wxBIND_OR_CONNECT_HACK_ONLY_BASE_CLASS
{
public:
    wxTextEntryHintData(wxTextEntryBase *entry, wxWindow *win)
        : m_entry(entry),
          m_win(win),
          m_text(m_entry->GetValue())
    {
        wxBIND_OR_CONNECT_HACK(win, wxEVT_SET_FOCUS, wxFocusEventHandler,
                                wxTextEntryHintData::OnSetFocus, this);
        wxBIND_OR_CONNECT_HACK(win, wxEVT_KILL_FOCUS, wxFocusEventHandler,
                                wxTextEntryHintData::OnKillFocus, this);
        wxBIND_OR_CONNECT_HACK(win, wxEVT_TEXT,
                                wxCommandEventHandler,
                                wxTextEntryHintData::OnTextChanged, this);
    }

    // default dtor is ok

    // Get the real text of the control such as it was before we replaced it
    // with the hint.
    const wxString& GetText() const { return m_text; }

    // Set the hint to show, shouldn't be empty normally.
    //
    // This should be called after creating a new wxTextEntryHintData object
    // and may be called more times in the future.
    void SetHintString(const wxString& hint)
    {
        m_hint = hint;

        if ( !m_win->HasFocus() )
            ShowHintIfAppropriate();
        //else: The new hint will be shown later when we lose focus.
    }

    const wxString& GetHintString() const { return m_hint; }

    // This is called whenever the text control contents changes.
    //
    // We call it ourselves when this change generates an event but it's also
    // necessary to call it explicitly from wxTextEntry::ChangeValue() as it,
    // by design, does not generate any events.
    void HandleTextUpdate(const wxString& text)
    {
        m_text = text;

        // If we're called because of a call to Set or ChangeValue(), the
        // control may still have the hint text colour, reset it in this case.
        RestoreTextColourIfNecessary();
    }

private:
    // Show the hint in the window if we should do it, i.e. if the window
    // doesn't have any text of its own.
    void ShowHintIfAppropriate()
    {
        // Never overwrite existing window text.
        if ( !m_text.empty() )
            return;

        // Save the old text colour and set a more inconspicuous one for the
        // hint.
        m_colFg = m_win->GetForegroundColour();
        m_win->SetForegroundColour(*wxLIGHT_GREY);

        m_entry->DoSetValue(m_hint, wxTextEntryBase::SetValue_NoEvent);
    }

    // Restore the original text colour if we had changed it to show the hint
    // and not restored it yet.
    void RestoreTextColourIfNecessary()
    {
        if ( m_colFg.IsOk() )
        {
            m_win->SetForegroundColour(m_colFg);
            m_colFg = wxColour();
        }
    }

    void OnSetFocus(wxFocusEvent& event)
    {
        // If we had been showing the hint before, remove it now and restore
        // the normal colour.
        if ( m_text.empty() )
        {
            RestoreTextColourIfNecessary();

            m_entry->DoSetValue(wxString(), wxTextEntryBase::SetValue_NoEvent);
        }

        event.Skip();
    }

    void OnKillFocus(wxFocusEvent& event)
    {
        // Restore the hint if the user didn't enter anything.
        ShowHintIfAppropriate();

        event.Skip();
    }

    void OnTextChanged(wxCommandEvent& event)
    {
        // Update the stored window text.
        //
        // Notice that we can't use GetValue() nor wxCommandEvent::GetString()
        // which uses it internally because this would just forward back to us
        // so go directly to the private method which returns the real control
        // contents.
        HandleTextUpdate(m_entry->DoGetValue());

        event.Skip();
    }


    // the text control we're associated with (as its interface and its window)
    wxTextEntryBase * const m_entry;
    wxWindow * const m_win;

    // the original foreground colour of m_win before we changed it
    wxColour m_colFg;

    // The hint passed to wxTextEntry::SetHint(), never empty.
    wxString m_hint;

    // The real text of the window.
    wxString m_text;


    wxDECLARE_NO_COPY_CLASS(wxTextEntryHintData);
};

// ============================================================================
// wxTextEntryBase implementation
// ============================================================================

wxTextEntryBase::~wxTextEntryBase()
{
    delete m_hintData;
}

// ----------------------------------------------------------------------------
// text accessors
// ----------------------------------------------------------------------------

wxString wxTextEntryBase::GetValue() const
{
    return m_hintData ? m_hintData->GetText() : DoGetValue();
}

wxString wxTextEntryBase::GetRange(long from, long to) const
{
    wxString sel;
    wxString value = GetValue();

    if ( from < to && (long)value.length() >= to )
    {
        sel = value.substr(from, to - from);
    }

    return sel;
}

// ----------------------------------------------------------------------------
// text operations
// ----------------------------------------------------------------------------

void wxTextEntryBase::ChangeValue(const wxString& value)
{
    DoSetValue(value, SetValue_NoEvent);

    // As we didn't generate any events for wxTextEntryHintData to catch,
    // notify it explicitly about our changed contents.
    if ( m_hintData )
        m_hintData->HandleTextUpdate(value);
}

void wxTextEntryBase::AppendText(const wxString& text)
{
    SetInsertionPointEnd();
    WriteText(text);
}

void wxTextEntryBase::DoSetValue(const wxString& value, int flags)
{
    if ( value != DoGetValue() )
    {
        EventsSuppressor noeventsIf(this, !(flags & SetValue_SendEvent));

        SelectAll();
        WriteText(value);

        SetInsertionPoint(0);
    }
    else // Same value, no need to do anything.
    {
        // Except that we still need to generate the event for consistency with
        // the normal case when the text does change.
        if ( flags & SetValue_SendEvent )
            SendTextUpdatedEvent(GetEditableWindow());
    }
}

void wxTextEntryBase::Replace(long from, long to, const wxString& value)
{
    {
        EventsSuppressor noevents(this);
        Remove(from, to);
    }

    SetInsertionPoint(from);
    WriteText(value);
}

// ----------------------------------------------------------------------------
// selection
// ----------------------------------------------------------------------------

bool wxTextEntryBase::HasSelection() const
{
    long from, to;
    GetSelection(&from, &to);

    return from < to;
}

void wxTextEntryBase::RemoveSelection()
{
    long from, to;
    GetSelection(& from, & to);
    if (from != -1 && to != -1)
        Remove(from, to);
}

wxString wxTextEntryBase::GetStringSelection() const
{
    long from, to;
    GetSelection(&from, &to);

    return GetRange(from, to);
}

// ----------------------------------------------------------------------------
// clipboard
// ----------------------------------------------------------------------------

bool wxTextEntryBase::CanCopy() const
{
    return HasSelection();
}

bool wxTextEntryBase::CanCut() const
{
    return CanCopy() && IsEditable();
}

bool wxTextEntryBase::CanPaste() const
{
    if ( IsEditable() )
    {
#if wxUSE_CLIPBOARD
        // check if there is any text on the clipboard
        if ( wxTheClipboard->IsSupported(wxDF_TEXT)
#if wxUSE_UNICODE
                || wxTheClipboard->IsSupported(wxDF_UNICODETEXT)
#endif // wxUSE_UNICODE
           )
        {
            return true;
        }
#endif // wxUSE_CLIPBOARD
    }

    return false;
}

// ----------------------------------------------------------------------------
// hints support
// ----------------------------------------------------------------------------

bool wxTextEntryBase::SetHint(const wxString& hint)
{
    if ( !hint.empty() )
    {
        if ( !m_hintData )
            m_hintData = new wxTextEntryHintData(this, GetEditableWindow());

        m_hintData->SetHintString(hint);
    }
    else if ( m_hintData )
    {
        // Setting empty hint removes any currently set one.
        delete m_hintData;
        m_hintData = NULL;
    }
    //else: Setting empty hint when we don't have any doesn't do anything.

    return true;
}

wxString wxTextEntryBase::GetHint() const
{
    return m_hintData ? m_hintData->GetHintString() : wxString();
}

// ----------------------------------------------------------------------------
// margins support
// ----------------------------------------------------------------------------

bool wxTextEntryBase::DoSetMargins(const wxPoint& WXUNUSED(pt))
{
    return false;
}

wxPoint wxTextEntryBase::DoGetMargins() const
{
    return wxPoint(-1, -1);
}

// ----------------------------------------------------------------------------
// events
// ----------------------------------------------------------------------------

/* static */
bool wxTextEntryBase::SendTextUpdatedEvent(wxWindow *win)
{
    wxCHECK_MSG( win, false, "can't send an event without a window" );

    wxCommandEvent event(wxEVT_TEXT, win->GetId());

    // do not do this as it could be very inefficient if the text control
    // contains a lot of text and we're not using ref-counted wxString
    // implementation -- instead, event.GetString() will query the control for
    // its current text if needed
    //event.SetString(win->GetValue());

    event.SetEventObject(win);
    return win->HandleWindowEvent(event);
}

// ----------------------------------------------------------------------------
// auto-completion stubs
// ----------------------------------------------------------------------------

wxTextCompleter::~wxTextCompleter()
{
}

bool wxTextCompleterSimple::Start(const wxString& prefix)
{
    m_index = 0;
    m_completions.clear();
    GetCompletions(prefix, m_completions);

    return !m_completions.empty();
}

wxString wxTextCompleterSimple::GetNext()
{
    if ( m_index == m_completions.size() )
        return wxString();

    return m_completions[m_index++];
}

bool wxTextEntryBase::DoAutoCompleteCustom(wxTextCompleter *completer)
{
    // We don't do anything here but we still need to delete the completer for
    // consistency with the ports that do implement this method and take
    // ownership of the pointer.
    delete completer;

    return false;
}

#endif // wxUSE_TEXTCTRL || wxUSE_COMBOBOX
