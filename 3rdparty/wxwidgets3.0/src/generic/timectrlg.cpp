///////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/timectrl.cpp
// Purpose:     Generic implementation of wxTimePickerCtrl.
// Author:      Paul Breen, Vadim Zeitlin
// Created:     2011-09-22
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
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

#if wxUSE_TIMEPICKCTRL

#ifndef WX_PRECOMP
    #include "wx/textctrl.h"
    #include "wx/utils.h"           // wxMax()
#endif // WX_PRECOMP

#include "wx/timectrl.h"

// This class is only compiled if there is no native version or if we
// explicitly want to use both the native and generic one (this is useful for
// testing but not much otherwise and so by default we don't use the generic
// implementation if a native one is available).
#if !defined(wxHAS_NATIVE_TIMEPICKERCTRL) || wxUSE_TIMEPICKCTRL_GENERIC

#include "wx/generic/timectrl.h"

#include "wx/dateevt.h"
#include "wx/spinbutt.h"

#ifndef wxHAS_NATIVE_TIMEPICKERCTRL
    IMPLEMENT_DYNAMIC_CLASS(wxTimePickerCtrl, wxControl)
#endif

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

enum
{
    // Horizontal margin between the text and spin control.
    HMARGIN_TEXT_SPIN = 2
};

// ----------------------------------------------------------------------------
// wxTimePickerGenericImpl: used to implement wxTimePickerCtrlGeneric
// ----------------------------------------------------------------------------

class wxTimePickerGenericImpl : public wxEvtHandler
{
public:
    wxTimePickerGenericImpl(wxTimePickerCtrlGeneric* ctrl)
    {
        m_text = new wxTextCtrl(ctrl, wxID_ANY, wxString());

        // As this text can't be edited, don't use the standard cursor for it
        // to avoid misleading the user. Ideally we'd also hide the caret but
        // this is not currently supported by wxTextCtrl.
        m_text->SetCursor(wxCURSOR_ARROW);

        m_btn = new wxSpinButton(ctrl, wxID_ANY,
                                 wxDefaultPosition, wxDefaultSize,
                                 wxSP_VERTICAL | wxSP_WRAP);

        m_currentField = Field_Hour;
        m_isFirstDigit = true;

        // We don't support arbitrary formats currently as this requires
        // significantly more work both here and also in wxLocale::GetInfo().
        //
        // For now just use either "%H:%M:%S" or "%I:%M:%S %p". It would be
        // nice to add support to "%k" and "%l" (hours with leading blanks
        // instead of zeros) too as this is the most common unsupported case in
        // practice.
        m_useAMPM = wxLocale::GetInfo(wxLOCALE_TIME_FMT).Contains("%p");

        m_text->Connect
                (
                    wxEVT_SET_FOCUS,
                    wxFocusEventHandler(wxTimePickerGenericImpl::OnTextSetFocus),
                    NULL,
                    this
                );
        m_text->Connect
                (
                    wxEVT_KEY_DOWN,
                    wxKeyEventHandler(wxTimePickerGenericImpl::OnTextKeyDown),
                    NULL,
                    this
                );
        m_text->Connect
                (
                    wxEVT_LEFT_DOWN,
                    wxMouseEventHandler(wxTimePickerGenericImpl::OnTextClick),
                    NULL,
                    this
                );

        m_btn->Connect
               (
                    wxEVT_SPIN_UP,
                    wxSpinEventHandler(wxTimePickerGenericImpl::OnArrowUp),
                    NULL,
                    this
               );
        m_btn->Connect
               (
                    wxEVT_SPIN_DOWN,
                    wxSpinEventHandler(wxTimePickerGenericImpl::OnArrowDown),
                    NULL,
                    this
               );
    }

    // Set the new value.
    void SetValue(const wxDateTime& time)
    {
        m_time = time.IsValid() ? time : wxDateTime::Now();

        // Ensure that the date part doesn't correspond to a DST change date as
        // time is discontinuous then resulting in many problems, e.g. it's
        // impossible to even enter 2:00:00 at the beginning of summer time
        // date as this time doesn't exist. By using Jan 1, on which nobody
        // changes DST, we avoid all such problems.
        wxDateTime::Tm tm = m_time.GetTm();
        tm.mday =
        tm.yday = 1;
        tm.mon = wxDateTime::Jan;
        m_time.Set(tm);

        UpdateTextWithoutEvent();
    }


    // The text part of the control.
    wxTextCtrl* m_text;

    // The spin button used to change the text fields.
    wxSpinButton* m_btn;

    // The current time (date part is ignored).
    wxDateTime m_time;

private:
    // The logical fields of the text control (AM/PM one may not be present).
    enum Field
    {
        Field_Hour,
        Field_Min,
        Field_Sec,
        Field_AMPM,
        Field_Max
    };

    // Direction of change of time fields.
    enum Direction
    {
        // Notice that the enum elements values matter.
        Dir_Down = -1,
        Dir_Up   = +1
    };

    // A range of character positions ("from" is inclusive, "to" -- exclusive).
    struct CharRange
    {
        int from,
            to;
    };

    // Event handlers for various events in our controls.
    void OnTextSetFocus(wxFocusEvent& event)
    {
        HighlightCurrentField();

        event.Skip();
    }

    // Keyboard interface here is modelled over MSW native control and may need
    // adjustments for other platforms.
    void OnTextKeyDown(wxKeyEvent& event)
    {
        const int key = event.GetKeyCode();

        switch ( key )
        {
            case WXK_DOWN:
                ChangeCurrentFieldBy1(Dir_Down);
                break;

            case WXK_UP:
                ChangeCurrentFieldBy1(Dir_Up);
                break;

            case WXK_LEFT:
                CycleCurrentField(Dir_Down);
                break;

            case WXK_RIGHT:
                CycleCurrentField(Dir_Up);
                break;

            case WXK_HOME:
                ResetCurrentField(Dir_Down);
                break;

            case WXK_END:
                ResetCurrentField(Dir_Up);
                break;

            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                // The digits work in all keys except AM/PM.
                if ( m_currentField != Field_AMPM )
                {
                    AppendDigitToCurrentField(key - '0');
                }
                break;

            case 'A':
            case 'P':
                // These keys only work to toggle AM/PM field.
                if ( m_currentField == Field_AMPM )
                {
                    unsigned hour = m_time.GetHour();
                    if ( key == 'A' )
                    {
                        if ( hour >= 12 )
                            hour -= 12;
                    }
                    else // PM
                    {
                        if ( hour < 12 )
                            hour += 12;
                    }

                    if ( hour != m_time.GetHour() )
                    {
                        m_time.SetHour(hour);
                        UpdateText();
                    }
                }
                break;

            // Do not skip the other events, just consume them to prevent the
            // user from editing the text directly.
        }
    }

    void OnTextClick(wxMouseEvent& event)
    {
        Field field = Field_Max; // Initialize just to suppress warnings.
        long pos;
        switch ( m_text->HitTest(event.GetPosition(), &pos) )
        {
            case wxTE_HT_UNKNOWN:
                // Don't do anything, it's better than doing something wrong.
                return;

            case wxTE_HT_BEFORE:
                // Select the first field.
                field = Field_Hour;
                break;

            case wxTE_HT_ON_TEXT:
                // Find the field containing this position.
                for ( field = Field_Hour; field <= GetLastField(); )
                {
                    const CharRange range = GetFieldRange(field);

                    // Normally the "to" end is exclusive but we want to give
                    // focus to some field when the user clicks between them so
                    // count it as part of the preceding field here.
                    if ( range.from <= pos && pos <= range.to )
                        break;

                    field = static_cast<Field>(field + 1);
                }
                break;

            case wxTE_HT_BELOW:
                // This shouldn't happen for single line control.
                wxFAIL_MSG( "Unreachable" );
                // fall through

            case wxTE_HT_BEYOND:
                // Select the last field.
                field = GetLastField();
                break;
        }

        ChangeCurrentField(field);
    }

    void OnArrowUp(wxSpinEvent& WXUNUSED(event))
    {
        ChangeCurrentFieldBy1(Dir_Up);
    }

    void OnArrowDown(wxSpinEvent& WXUNUSED(event))
    {
        ChangeCurrentFieldBy1(Dir_Down);
    }


    // Get the range of the given field in character positions ("from" is
    // inclusive, "to" exclusive).
    static CharRange GetFieldRange(Field field)
    {
        // Currently we can just hard code the ranges as they are the same for
        // both supported formats, if we want to support arbitrary formats in
        // the future, we'd need to determine them dynamically by examining the
        // format here.
        static const CharRange ranges[] =
        {
            { 0, 2 },
            { 3, 5 },
            { 6, 8 },
            { 9, 11},
        };

        wxCOMPILE_TIME_ASSERT( WXSIZEOF(ranges) == Field_Max,
                               FieldRangesMismatch );

        return ranges[field];
    }

    // Get the last field used depending on m_useAMPM.
    Field GetLastField() const
    {
        return m_useAMPM ? Field_AMPM : Field_Sec;
    }

    // Change the current field. For convenience, accept int field here as this
    // allows us to use arithmetic operations in the caller.
    void ChangeCurrentField(int field)
    {
        if ( field == m_currentField )
            return;

        wxCHECK_RET( field <= GetLastField(), "Invalid field" );

        m_currentField = static_cast<Field>(field);
        m_isFirstDigit = true;

        HighlightCurrentField();
    }

    // Go to the next (Dir_Up) or previous (Dir_Down) field, wrapping if
    // necessary.
    void CycleCurrentField(Direction dir)
    {
        const unsigned numFields = GetLastField() + 1;

        ChangeCurrentField((m_currentField + numFields + dir) % numFields);
    }

    // Select the currently actively field.
    void HighlightCurrentField()
    {
        m_text->SetFocus();

        const CharRange range = GetFieldRange(m_currentField);

        m_text->SetSelection(range.from, range.to);
    }

    // Decrement or increment the value of the current field (wrapping if
    // necessary).
    void ChangeCurrentFieldBy1(Direction dir)
    {
        switch ( m_currentField )
        {
            case Field_Hour:
                m_time.SetHour((m_time.GetHour() + 24 + dir) % 24);
                break;

            case Field_Min:
                m_time.SetMinute((m_time.GetMinute() + 60 + dir) % 60);
                break;

            case Field_Sec:
                m_time.SetSecond((m_time.GetSecond() + 60 + dir) % 60);
                break;

            case Field_AMPM:
                m_time.SetHour((m_time.GetHour() + 12) % 24);
                break;

            case Field_Max:
                wxFAIL_MSG( "Invalid field" );
        }

        UpdateText();
    }

    // Set the current field to its minimal or maximal value.
    void ResetCurrentField(Direction dir)
    {
        switch ( m_currentField )
        {
            case Field_Hour:
            case Field_AMPM:
                // In 12-hour mode setting the hour to the minimal value
                // also changes the suffix to AM and, correspondingly,
                // setting it to the maximal one changes the suffix to PM.
                // And, for consistency with the native MSW behaviour, we
                // also do the same thing when changing AM/PM field itself,
                // so change hours in any case.
                m_time.SetHour(dir == Dir_Down ? 0 : 23);
                break;

            case Field_Min:
                m_time.SetMinute(dir == Dir_Down ? 0 : 59);
                break;

            case Field_Sec:
                m_time.SetSecond(dir == Dir_Down ? 0 : 59);
                break;

            case Field_Max:
                wxFAIL_MSG( "Invalid field" );
        }

        UpdateText();
    }

    // Append the given digit (from 0 to 9) to the current value of the current
    // field.
    void AppendDigitToCurrentField(int n)
    {
        bool moveToNextField = false;

        if ( !m_isFirstDigit )
        {
            // The first digit simply replaces the existing field contents,
            // but the second one should be combined with the previous one,
            // otherwise entering 2-digit numbers would be impossible.
            int currentValue = 0,
                maxValue  = 0;

            switch ( m_currentField )
            {
                case Field_Hour:
                    currentValue = m_time.GetHour();
                    maxValue = 23;
                    break;

                case Field_Min:
                    currentValue = m_time.GetMinute();
                    maxValue = 59;
                    break;

                case Field_Sec:
                    currentValue = m_time.GetSecond();
                    maxValue = 59;
                    break;

                case Field_AMPM:
                case Field_Max:
                    wxFAIL_MSG( "Invalid field" );
                    return;
            }

            // Check if the new value is acceptable. If not, we just handle
            // this digit as if it were the first one.
            int newValue = currentValue*10 + n;
            if ( newValue < maxValue )
            {
                n = newValue;

                // If we're not on the seconds field, advance to the next one.
                // This makes it more convenient to enter times as you can just
                // press all digits one after one without touching the cursor
                // arrow keys at all.
                //
                // Notice that MSW native control doesn't do this but it seems
                // so useful that we intentionally diverge from it here.
                moveToNextField = true;

                // We entered both digits so the next one will be "first" again.
                m_isFirstDigit = true;
            }
        }
        else // First digit entered.
        {
            // The next one won't be first any more.
            m_isFirstDigit = false;
        }

        switch ( m_currentField )
        {
            case Field_Hour:
                m_time.SetHour(n);
                break;

            case Field_Min:
                m_time.SetMinute(n);
                break;

            case Field_Sec:
                m_time.SetSecond(n);
                break;

            case Field_AMPM:
            case Field_Max:
                wxFAIL_MSG( "Invalid field" );
        }

        if ( moveToNextField && m_currentField < Field_Sec )
            CycleCurrentField(Dir_Up);

        UpdateText();
    }

    // Update the text value to correspond to the current time. By default also
    // generate an event but this can be avoided by calling the "WithoutEvent"
    // variant.
    void UpdateText()
    {
        UpdateTextWithoutEvent();

        wxWindow* const ctrl = m_text->GetParent();

        wxDateEvent event(ctrl, m_time, wxEVT_TIME_CHANGED);
        ctrl->HandleWindowEvent(event);
    }

    void UpdateTextWithoutEvent()
    {
        m_text->SetValue(m_time.Format(m_useAMPM ? "%I:%M:%S %p" : "%H:%M:%S"));

        HighlightCurrentField();
    }


    // The current field of the text control: this is the one affected by
    // pressing arrow keys or spin button.
    Field m_currentField;

    // Flag indicating whether we use AM/PM indicator or not.
    bool m_useAMPM;

    // Flag indicating whether the next digit pressed by user will be the first
    // digit of the current field or the second one. This is necessary because
    // the first digit replaces the current field contents while the second one
    // is appended to it (if possible, e.g. pressing '7' in a field already
    // containing '8' will still replace it as "78" would be invalid).
    bool m_isFirstDigit;

    wxDECLARE_NO_COPY_CLASS(wxTimePickerGenericImpl);
};

// ============================================================================
// wxTimePickerCtrlGeneric implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxTimePickerCtrlGeneric creation
// ----------------------------------------------------------------------------

void wxTimePickerCtrlGeneric::Init()
{
    m_impl = NULL;
}

bool
wxTimePickerCtrlGeneric::Create(wxWindow *parent,
                                wxWindowID id,
                                const wxDateTime& date,
                                const wxPoint& pos,
                                const wxSize& size,
                                long style,
                                const wxValidator& validator,
                                const wxString& name)
{
    // The text control we use already has a border, so we don't need one
    // ourselves.
    style &= ~wxBORDER_MASK;
    style |= wxBORDER_NONE;

    if ( !Base::Create(parent, id, pos, size, style, validator, name) )
        return false;

    m_impl = new wxTimePickerGenericImpl(this);
    m_impl->SetValue(date);

    InvalidateBestSize();
    SetInitialSize(size);

    return true;
}

wxTimePickerCtrlGeneric::~wxTimePickerCtrlGeneric()
{
    delete m_impl;
}

wxWindowList wxTimePickerCtrlGeneric::GetCompositeWindowParts() const
{
    wxWindowList parts;
    if ( m_impl )
    {
        parts.push_back(m_impl->m_text);
        parts.push_back(m_impl->m_btn);
    }
    return parts;
}

// ----------------------------------------------------------------------------
// wxTimePickerCtrlGeneric value
// ----------------------------------------------------------------------------

void wxTimePickerCtrlGeneric::SetValue(const wxDateTime& date)
{
    wxCHECK_RET( m_impl, "Must create first" );

    m_impl->SetValue(date);
}

wxDateTime wxTimePickerCtrlGeneric::GetValue() const
{
    wxCHECK_MSG( m_impl, wxDateTime(), "Must create first" );

    return m_impl->m_time;
}

// ----------------------------------------------------------------------------
// wxTimePickerCtrlGeneric geometry
// ----------------------------------------------------------------------------

void wxTimePickerCtrlGeneric::DoMoveWindow(int x, int y, int width, int height)
{
    Base::DoMoveWindow(x, y, width, height);

    if ( !m_impl )
        return;

    const int widthBtn = m_impl->m_btn->GetSize().x;
    const int widthText = wxMax(width - widthBtn - HMARGIN_TEXT_SPIN, 0);

    m_impl->m_text->SetSize(0, 0, widthText, height);
    m_impl->m_btn->SetSize(widthText + HMARGIN_TEXT_SPIN, 0, widthBtn, height);
}

wxSize wxTimePickerCtrlGeneric::DoGetBestSize() const
{
    if ( !m_impl )
        return Base::DoGetBestSize();

    wxSize size = m_impl->m_text->GetBestSize();
    size.x += m_impl->m_btn->GetBestSize().x + HMARGIN_TEXT_SPIN;

    return size;
}

#endif // !wxHAS_NATIVE_TIMEPICKERCTRL || wxUSE_TIMEPICKCTRL_GENERIC

#endif // wxUSE_TIMEPICKCTRL
