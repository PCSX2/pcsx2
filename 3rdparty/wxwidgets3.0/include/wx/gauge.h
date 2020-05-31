///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gauge.h
// Purpose:     wxGauge interface
// Author:      Vadim Zeitlin
// Modified by:
// Created:     20.02.01
// Copyright:   (c) 1996-2001 wxWidgets team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GAUGE_H_BASE_
#define _WX_GAUGE_H_BASE_

#include "wx/defs.h"

#if wxUSE_GAUGE

#include "wx/control.h"

// ----------------------------------------------------------------------------
// wxGauge style flags
// ----------------------------------------------------------------------------

#define wxGA_HORIZONTAL      wxHORIZONTAL
#define wxGA_VERTICAL        wxVERTICAL

// Win32 only, is default (and only) on some other platforms
#define wxGA_SMOOTH          0x0020

#if WXWIN_COMPATIBILITY_2_6
    // obsolete style
    #define wxGA_PROGRESSBAR     0
#endif // WXWIN_COMPATIBILITY_2_6

// GTK and Mac always have native implementation of the indeterminate mode
// wxMSW has native implementation only if comctl32.dll >= 6.00
#if !defined(__WXGTK20__) && !defined(__WXMAC__) && !defined(__WXCOCOA__)
    #define wxGAUGE_EMULATE_INDETERMINATE_MODE 1
#else
    #define wxGAUGE_EMULATE_INDETERMINATE_MODE 0
#endif

extern WXDLLIMPEXP_DATA_CORE(const char) wxGaugeNameStr[];

// ----------------------------------------------------------------------------
// wxGauge: a progress bar
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGaugeBase : public wxControl
{
public:
    wxGaugeBase() { m_rangeMax = m_gaugePos = 0; }
    virtual ~wxGaugeBase();

    bool Create(wxWindow *parent,
                wxWindowID id,
                int range,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxGA_HORIZONTAL,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxGaugeNameStr);

    // determinate mode API

    // set/get the control range
    virtual void SetRange(int range);
    virtual int GetRange() const;

    virtual void SetValue(int pos);
    virtual int GetValue() const;

    // indeterminate mode API
    virtual void Pulse();

    // simple accessors
    bool IsVertical() const { return HasFlag(wxGA_VERTICAL); }

    // appearance params (not implemented for most ports)
    virtual void SetShadowWidth(int w);
    virtual int GetShadowWidth() const;

    virtual void SetBezelFace(int w);
    virtual int GetBezelFace() const;

    // overridden base class virtuals
    virtual bool AcceptsFocus() const { return false; }

protected:
    virtual wxBorder GetDefaultBorder() const { return wxBORDER_NONE; }

    // the max position
    int m_rangeMax;

    // the current position
    int m_gaugePos;

#if wxGAUGE_EMULATE_INDETERMINATE_MODE
    int m_nDirection;       // can be wxRIGHT or wxLEFT
#endif

    wxDECLARE_NO_COPY_CLASS(wxGaugeBase);
};

#if defined(__WXUNIVERSAL__)
    #include "wx/univ/gauge.h"
#elif defined(__WXMSW__)
    #include "wx/msw/gauge.h"
#elif defined(__WXMOTIF__)
    #include "wx/motif/gauge.h"
#elif defined(__WXGTK20__)
    #include "wx/gtk/gauge.h"
#elif defined(__WXGTK__)
    #include "wx/gtk1/gauge.h"
#elif defined(__WXMAC__)
    #include "wx/osx/gauge.h"
#elif defined(__WXCOCOA__)
    #include "wx/cocoa/gauge.h"
#elif defined(__WXPM__)
    #include "wx/os2/gauge.h"
#endif

#endif // wxUSE_GAUGE

#endif
    // _WX_GAUGE_H_BASE_
