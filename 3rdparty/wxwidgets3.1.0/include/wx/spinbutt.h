/////////////////////////////////////////////////////////////////////////////
// Name:        wx/spinbutt.h
// Purpose:     wxSpinButtonBase class
// Author:      Julian Smart, Vadim Zeitlin
// Modified by:
// Created:     23.07.99
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SPINBUTT_H_BASE_
#define _WX_SPINBUTT_H_BASE_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/defs.h"

#if wxUSE_SPINBTN

#include "wx/control.h"
#include "wx/event.h"
#include "wx/range.h"

#define wxSPIN_BUTTON_NAME wxT("wxSpinButton")

// ----------------------------------------------------------------------------
//  The wxSpinButton is like a small scrollbar than is often placed next
//  to a text control.
//
//  Styles:
//  wxSP_HORIZONTAL:   horizontal spin button
//  wxSP_VERTICAL:     vertical spin button (the default)
//  wxSP_ARROW_KEYS:   arrow keys increment/decrement value
//  wxSP_WRAP:         value wraps at either end
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxSpinButtonBase : public wxControl
{
public:
    // ctor initializes the range with the default (0..100) values
    wxSpinButtonBase() { m_min = 0; m_max = 100; }

    // accessors
    virtual int GetValue() const = 0;
    virtual int GetMin() const { return m_min; }
    virtual int GetMax() const { return m_max; }
    wxRange GetRange() const { return wxRange( GetMin(), GetMax() );}

    // operations
    virtual void SetValue(int val) = 0;
    virtual void SetMin(int minVal) { SetRange ( minVal , m_max ) ; }
    virtual void SetMax(int maxVal) { SetRange ( m_min , maxVal ) ; }
    virtual void SetRange(int minVal, int maxVal)
    {
        m_min = minVal;
        m_max = maxVal;
    }
    void SetRange( const wxRange& range) { SetRange( range.GetMin(), range.GetMax()); }

    // is this spin button vertically oriented?
    bool IsVertical() const { return (m_windowStyle & wxSP_VERTICAL) != 0; }

protected:
    // the range value
    int   m_min;
    int   m_max;

    wxDECLARE_NO_COPY_CLASS(wxSpinButtonBase);
};

// ----------------------------------------------------------------------------
// include the declaration of the real class
// ----------------------------------------------------------------------------

#if defined(__WXUNIVERSAL__)
    #include "wx/univ/spinbutt.h"
#elif defined(__WXMSW__)
    #include "wx/msw/spinbutt.h"
#elif defined(__WXMOTIF__)
    #include "wx/motif/spinbutt.h"
#elif defined(__WXGTK20__)
    #include "wx/gtk/spinbutt.h"
#elif defined(__WXGTK__)
    #include "wx/gtk1/spinbutt.h"
#elif defined(__WXMAC__)
    #include "wx/osx/spinbutt.h"
#elif defined(__WXQT__)
    #include "wx/qt/spinbutt.h"
#endif

// ----------------------------------------------------------------------------
// the wxSpinButton event
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxSpinEvent : public wxNotifyEvent
{
public:
    wxSpinEvent(wxEventType commandType = wxEVT_NULL, int winid = 0)
           : wxNotifyEvent(commandType, winid)
    {
    }

    wxSpinEvent(const wxSpinEvent& event) : wxNotifyEvent(event) {}

    // get the current value of the control
    int GetValue() const { return m_commandInt; }
    void SetValue(int value) { m_commandInt = value; }

    int GetPosition() const { return m_commandInt; }
    void SetPosition(int pos) { m_commandInt = pos; }

    virtual wxEvent *Clone() const wxOVERRIDE { return new wxSpinEvent(*this); }

private:
    wxDECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxSpinEvent);
};

typedef void (wxEvtHandler::*wxSpinEventFunction)(wxSpinEvent&);

#define wxSpinEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxSpinEventFunction, func)

// macros for handling spin events: notice that we must use the real values of
// the event type constants and not their references (wxEVT_SPIN[_UP/DOWN])
// here as otherwise the event tables could end up with non-initialized
// (because of undefined initialization order of the globals defined in
// different translation units) references in them
#define EVT_SPIN_UP(winid, func) \
    wx__DECLARE_EVT1(wxEVT_SPIN_UP, winid, wxSpinEventHandler(func))
#define EVT_SPIN_DOWN(winid, func) \
    wx__DECLARE_EVT1(wxEVT_SPIN_DOWN, winid, wxSpinEventHandler(func))
#define EVT_SPIN(winid, func) \
    wx__DECLARE_EVT1(wxEVT_SPIN, winid, wxSpinEventHandler(func))

#endif // wxUSE_SPINBTN

#endif
    // _WX_SPINBUTT_H_BASE_
