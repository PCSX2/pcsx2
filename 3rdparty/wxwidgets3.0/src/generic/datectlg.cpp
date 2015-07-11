/////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/datectlg.cpp
// Purpose:     generic wxDatePickerCtrlGeneric implementation
// Author:      Andreas Pflug
// Modified by:
// Created:     2005-01-19
// Copyright:   (c) 2005 Andreas Pflug <pgadmin@pse-consulting.de>
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

#if wxUSE_DATEPICKCTRL

#ifndef WX_PRECOMP
    #include "wx/dialog.h"
    #include "wx/dcmemory.h"
    #include "wx/intl.h"
    #include "wx/panel.h"
    #include "wx/textctrl.h"
    #include "wx/valtext.h"
#endif

#include "wx/calctrl.h"
#include "wx/combo.h"

#include "wx/datectrl.h"
#include "wx/generic/datectrl.h"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// global variables
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// local classes
// ----------------------------------------------------------------------------

class wxCalendarComboPopup : public wxCalendarCtrl,
                             public wxComboPopup
{
public:

    wxCalendarComboPopup() : wxCalendarCtrl(),
                             wxComboPopup()
    {
    }

    virtual void Init()
    {
    }

    // NB: Don't create lazily since it didn't work that way before
    //     wxComboCtrl was used, and changing behaviour would almost
    //     certainly introduce new bugs.
    virtual bool Create(wxWindow* parent)
    {
        if ( !wxCalendarCtrl::Create(parent, wxID_ANY, wxDefaultDateTime,
                              wxPoint(0, 0), wxDefaultSize,
                              wxCAL_SEQUENTIAL_MONTH_SELECTION
                              | wxCAL_SHOW_HOLIDAYS | wxBORDER_SUNKEN) )
            return false;

        SetFormat(GetLocaleDateFormat());

        m_useSize  = wxCalendarCtrl::GetBestSize();

        wxWindow* tx = m_combo->GetTextCtrl();
        if ( !tx )
            tx = m_combo;

        tx->Connect(wxEVT_KILL_FOCUS,
                    wxFocusEventHandler(wxCalendarComboPopup::OnKillTextFocus),
                    NULL, this);

        return true;
    }

    virtual wxSize GetAdjustedSize(int WXUNUSED(minWidth),
                                   int WXUNUSED(prefHeight),
                                   int WXUNUSED(maxHeight))
    {
        return m_useSize;
    }

    virtual wxWindow *GetControl() { return this; }

    void SetDateValue(const wxDateTime& date)
    {
        if ( date.IsValid() )
        {
            m_combo->SetText(date.Format(m_format));
            SetDate(date);
        }
        else // invalid date
        {
            wxASSERT_MSG( HasDPFlag(wxDP_ALLOWNONE),
                            wxT("this control must have a valid date") );

            m_combo->SetText(wxEmptyString);
        }
    }

    bool IsTextEmpty() const
    {
        return m_combo->GetTextCtrl()->IsEmpty();
    }

    bool ParseDateTime(const wxString& s, wxDateTime* pDt)
    {
        wxASSERT(pDt);

        if ( !s.empty() )
        {
            pDt->ParseFormat(s, m_format);
            if ( !pDt->IsValid() )
                return false;
        }

        return true;
    }

    void SendDateEvent(const wxDateTime& dt)
    {
        // Sends both wxCalendarEvent and wxDateEvent
        wxWindow* datePicker = m_combo->GetParent();

        wxCalendarEvent cev(datePicker, dt, wxEVT_CALENDAR_SEL_CHANGED);
        datePicker->GetEventHandler()->ProcessEvent(cev);

        wxDateEvent event(datePicker, dt, wxEVT_DATE_CHANGED);
        datePicker->GetEventHandler()->ProcessEvent(event);
    }

private:

    void OnCalKey(wxKeyEvent & ev)
    {
        if (ev.GetKeyCode() == WXK_ESCAPE && !ev.HasModifiers())
            Dismiss();
        else
            ev.Skip();
    }

    void OnSelChange(wxCalendarEvent &ev)
    {
        m_combo->SetText(GetDate().Format(m_format));

        if ( ev.GetEventType() == wxEVT_CALENDAR_DOUBLECLICKED )
        {
            Dismiss();
        }

        SendDateEvent(GetDate());
    }

    void OnKillTextFocus(wxFocusEvent &ev)
    {
        ev.Skip();

        const wxDateTime& dtOld = GetDate();

        wxDateTime dt;
        wxString value = m_combo->GetValue();
        if ( !ParseDateTime(value, &dt) )
        {
            if ( !HasDPFlag(wxDP_ALLOWNONE) )
                dt = dtOld;
        }

        m_combo->SetText(GetStringValueFor(dt));

        if ( !dt.IsValid() && HasDPFlag(wxDP_ALLOWNONE) )
            return;

        // notify that we had to change the date after validation
        if ( (dt.IsValid() && (!dtOld.IsValid() || dt != dtOld)) ||
                (!dt.IsValid() && dtOld.IsValid()) )
        {
            SetDate(dt);
            SendDateEvent(dt);
        }
    }

    bool HasDPFlag(int flag) const
    {
        return m_combo->GetParent()->HasFlag(flag);
    }

    // Return the format to be used for the dates shown by the control. This
    // functions honours wxDP_SHOWCENTURY flag.
    wxString GetLocaleDateFormat() const
    {
#if wxUSE_INTL
        wxString fmt = wxLocale::GetInfo(wxLOCALE_SHORT_DATE_FMT);
        if ( HasDPFlag(wxDP_SHOWCENTURY) )
            fmt.Replace("%y", "%Y");

        return fmt;
#else // !wxUSE_INTL
        return wxT("x");
#endif // wxUSE_INTL/!wxUSE_INTL
    }

    bool SetFormat(const wxString& fmt)
    {
        m_format = fmt;

        if ( m_combo )
        {
            wxArrayString allowedChars;
            for ( wxChar c = wxT('0'); c <= wxT('9'); c++ )
                allowedChars.Add(wxString(c, 1));

            const wxChar *p2 = m_format.c_str();
            while ( *p2 )
            {
                if ( *p2 == '%')
                    p2 += 2;
                else
                    allowedChars.Add(wxString(*p2++, 1));
            }

    #if wxUSE_VALIDATORS
            wxTextValidator tv(wxFILTER_INCLUDE_CHAR_LIST);
            tv.SetIncludes(allowedChars);
            m_combo->SetValidator(tv);
    #endif

            if ( GetDate().IsValid() )
                m_combo->SetText(GetDate().Format(m_format));
        }

        return true;
    }

    virtual void SetStringValue(const wxString& s)
    {
        wxDateTime dt;
        if ( !s.empty() && ParseDateTime(s, &dt) )
            SetDate(dt);
        //else: keep the old value
    }

    virtual wxString GetStringValue() const
    {
        return GetStringValueFor(GetDate());
    }

private:
    // returns either the given date representation using the current format or
    // an empty string if it's invalid
    wxString GetStringValueFor(const wxDateTime& dt) const
    {
        wxString val;
        if ( dt.IsValid() )
            val = dt.Format(m_format);

        return val;
    }

    wxSize          m_useSize;
    wxString        m_format;

    DECLARE_EVENT_TABLE()
};


BEGIN_EVENT_TABLE(wxCalendarComboPopup, wxCalendarCtrl)
    EVT_KEY_DOWN(wxCalendarComboPopup::OnCalKey)
    EVT_CALENDAR_SEL_CHANGED(wxID_ANY, wxCalendarComboPopup::OnSelChange)
    EVT_CALENDAR_PAGE_CHANGED(wxID_ANY, wxCalendarComboPopup::OnSelChange)
    EVT_CALENDAR(wxID_ANY, wxCalendarComboPopup::OnSelChange)
END_EVENT_TABLE()


// ============================================================================
// wxDatePickerCtrlGeneric implementation
// ============================================================================

BEGIN_EVENT_TABLE(wxDatePickerCtrlGeneric, wxDatePickerCtrlBase)
    EVT_TEXT(wxID_ANY, wxDatePickerCtrlGeneric::OnText)
    EVT_SIZE(wxDatePickerCtrlGeneric::OnSize)
    EVT_SET_FOCUS(wxDatePickerCtrlGeneric::OnFocus)
END_EVENT_TABLE()

#ifndef wxHAS_NATIVE_DATEPICKCTRL
    IMPLEMENT_DYNAMIC_CLASS(wxDatePickerCtrl, wxControl)
#endif

// ----------------------------------------------------------------------------
// creation
// ----------------------------------------------------------------------------

bool wxDatePickerCtrlGeneric::Create(wxWindow *parent,
                                     wxWindowID id,
                                     const wxDateTime& date,
                                     const wxPoint& pos,
                                     const wxSize& size,
                                     long style,
                                     const wxValidator& validator,
                                     const wxString& name)
{
    wxASSERT_MSG( !(style & wxDP_SPIN),
                  wxT("wxDP_SPIN style not supported, use wxDP_DEFAULT") );

    if ( !wxControl::Create(parent, id, pos, size,
                            style | wxCLIP_CHILDREN | wxWANTS_CHARS | wxBORDER_NONE,
                            validator, name) )
    {
        return false;
    }

    InheritAttributes();

    m_combo = new wxComboCtrl(this, -1, wxEmptyString,
                              wxDefaultPosition, wxDefaultSize);

    m_combo->SetCtrlMainWnd(this);

    m_popup = new wxCalendarComboPopup();

#if defined(__WXMSW__)
    // without this keyboard navigation in month control doesn't work
    m_combo->UseAltPopupWindow();
#endif
    m_combo->SetPopupControl(m_popup);

    m_popup->SetDateValue(date.IsValid() ? date : wxDateTime::Today());

    SetInitialSize(size);

    return true;
}


void wxDatePickerCtrlGeneric::Init()
{
    m_combo = NULL;
    m_popup = NULL;
}

wxDatePickerCtrlGeneric::~wxDatePickerCtrlGeneric()
{
}

bool wxDatePickerCtrlGeneric::Destroy()
{
    if ( m_combo )
        m_combo->Destroy();

    m_combo = NULL;
    m_popup = NULL;

    return wxControl::Destroy();
}

// ----------------------------------------------------------------------------
// overridden base class methods
// ----------------------------------------------------------------------------

wxSize wxDatePickerCtrlGeneric::DoGetBestSize() const
{
    // A better solution would be to use a custom text control that would have
    // the best size determined by the current date format and let m_combo take
    // care of the best size computation, but this isn't easily possible with
    // wxComboCtrl currently, so we compute our own best size here instead even
    // if this means adding some extra margins to account for text control
    // borders, space between it and the button and so on.
    wxSize size = m_combo->GetButtonSize();

    wxTextCtrl* const text = m_combo->GetTextCtrl();
    size.x += text->GetTextExtent(text->GetValue()).x;
    size.x += 2*text->GetCharWidth(); // This is the margin mentioned above.

    return size;
}

wxWindowList wxDatePickerCtrlGeneric::GetCompositeWindowParts() const
{
    wxWindowList parts;
    parts.push_back(m_combo);
    parts.push_back(m_popup);
    return parts;
}

// ----------------------------------------------------------------------------
// wxDatePickerCtrlGeneric API
// ----------------------------------------------------------------------------

bool
wxDatePickerCtrlGeneric::SetDateRange(const wxDateTime& lowerdate,
                                      const wxDateTime& upperdate)
{
    return m_popup->SetDateRange(lowerdate, upperdate);
}


wxDateTime wxDatePickerCtrlGeneric::GetValue() const
{
    if ( HasFlag(wxDP_ALLOWNONE) && m_popup->IsTextEmpty() )
        return wxInvalidDateTime;
    return m_popup->GetDate();
}


void wxDatePickerCtrlGeneric::SetValue(const wxDateTime& date)
{
    m_popup->SetDateValue(date);
}


bool wxDatePickerCtrlGeneric::GetRange(wxDateTime *dt1, wxDateTime *dt2) const
{
    return m_popup->GetDateRange(dt1, dt2);
}


void
wxDatePickerCtrlGeneric::SetRange(const wxDateTime &dt1, const wxDateTime &dt2)
{
    m_popup->SetDateRange(dt1, dt2);
}

wxCalendarCtrl *wxDatePickerCtrlGeneric::GetCalendar() const
{
    return m_popup;
}

// ----------------------------------------------------------------------------
// event handlers
// ----------------------------------------------------------------------------


void wxDatePickerCtrlGeneric::OnSize(wxSizeEvent& event)
{
    if ( m_combo )
        m_combo->SetSize(GetClientSize());

    event.Skip();
}


void wxDatePickerCtrlGeneric::OnText(wxCommandEvent &ev)
{
    ev.SetEventObject(this);
    ev.SetId(GetId());
    GetParent()->GetEventHandler()->ProcessEvent(ev);

    // We'll create an additional event if the date is valid.
    // If the date isn't valid, the user's probably in the middle of typing
    wxDateTime dt;
    if ( !m_popup || !m_popup->ParseDateTime(m_combo->GetValue(), &dt) )
        return;

    m_popup->SendDateEvent(dt);
}


void wxDatePickerCtrlGeneric::OnFocus(wxFocusEvent& WXUNUSED(event))
{
    m_combo->SetFocus();
}


#endif // wxUSE_DATEPICKCTRL

