///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/gaugecmn.cpp
// Purpose:     wxGaugeBase: common to all ports methods of wxGauge
// Author:      Vadim Zeitlin
// Modified by:
// Created:     20.02.01
// Copyright:   (c) 2001 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
#endif //WX_PRECOMP

#if wxUSE_GAUGE

#include "wx/gauge.h"
#include "wx/appprogress.h"

const char wxGaugeNameStr[] = "gauge";

// ============================================================================
// implementation
// ============================================================================

wxGaugeBase::~wxGaugeBase()
{
    // this destructor is required for Darwin
    delete m_appProgressIndicator;
}

// ----------------------------------------------------------------------------
// XTI
// ----------------------------------------------------------------------------

wxDEFINE_FLAGS( wxGaugeStyle )
wxBEGIN_FLAGS( wxGaugeStyle )
// new style border flags, we put them first to
// use them for streaming out
wxFLAGS_MEMBER(wxBORDER_SIMPLE)
wxFLAGS_MEMBER(wxBORDER_SUNKEN)
wxFLAGS_MEMBER(wxBORDER_DOUBLE)
wxFLAGS_MEMBER(wxBORDER_RAISED)
wxFLAGS_MEMBER(wxBORDER_STATIC)
wxFLAGS_MEMBER(wxBORDER_NONE)

// old style border flags
wxFLAGS_MEMBER(wxSIMPLE_BORDER)
wxFLAGS_MEMBER(wxSUNKEN_BORDER)
wxFLAGS_MEMBER(wxDOUBLE_BORDER)
wxFLAGS_MEMBER(wxRAISED_BORDER)
wxFLAGS_MEMBER(wxSTATIC_BORDER)
wxFLAGS_MEMBER(wxBORDER)

// standard window styles
wxFLAGS_MEMBER(wxTAB_TRAVERSAL)
wxFLAGS_MEMBER(wxCLIP_CHILDREN)
wxFLAGS_MEMBER(wxTRANSPARENT_WINDOW)
wxFLAGS_MEMBER(wxWANTS_CHARS)
wxFLAGS_MEMBER(wxFULL_REPAINT_ON_RESIZE)
wxFLAGS_MEMBER(wxALWAYS_SHOW_SB )
wxFLAGS_MEMBER(wxVSCROLL)
wxFLAGS_MEMBER(wxHSCROLL)

wxFLAGS_MEMBER(wxGA_HORIZONTAL)
wxFLAGS_MEMBER(wxGA_VERTICAL)
wxFLAGS_MEMBER(wxGA_SMOOTH)
wxFLAGS_MEMBER(wxGA_PROGRESS)
wxEND_FLAGS( wxGaugeStyle )

wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxGauge, wxControl, "wx/gauge.h");

wxBEGIN_PROPERTIES_TABLE(wxGauge)
wxPROPERTY( Value, int, SetValue, GetValue, 0, 0 /*flags*/, \
           wxT("Helpstring"), wxT("group"))
wxPROPERTY( Range, int, SetRange, GetRange, 0, 0 /*flags*/, \
           wxT("Helpstring"), wxT("group"))

wxPROPERTY_FLAGS( WindowStyle, wxGaugeStyle, long, SetWindowStyleFlag, \
                 GetWindowStyleFlag, wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, \
                 wxT("Helpstring"), wxT("group")) // style
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxGauge)

wxCONSTRUCTOR_6( wxGauge, wxWindow*, Parent, wxWindowID, Id, int, Range, \
                wxPoint, Position, wxSize, Size, long, WindowStyle )

// ----------------------------------------------------------------------------
// wxGauge creation
// ----------------------------------------------------------------------------

bool wxGaugeBase::Create(wxWindow *parent,
                         wxWindowID id,
                         int range,
                         const wxPoint& pos,
                         const wxSize& size,
                         long style,
                         const wxValidator& validator,
                         const wxString& name)
{
    if ( !wxControl::Create(parent, id, pos, size, style, validator, name) )
        return false;

    SetName(name);

#if wxUSE_VALIDATORS
    SetValidator(validator);
#endif // wxUSE_VALIDATORS

    m_appProgressIndicator = NULL;
    if ( (style & wxGA_PROGRESS) != 0 )
    {
        wxWindow* topParent = wxGetTopLevelParent(this);
        if ( topParent != NULL )
        {
            m_appProgressIndicator =
                new wxAppProgressIndicator(topParent, range);
        }
    }

    SetRange(range);
    SetValue(0);
#if wxGAUGE_EMULATE_INDETERMINATE_MODE
    m_nDirection = wxRIGHT;
#endif

    return true;
}

// ----------------------------------------------------------------------------
// wxGauge determinate mode range/position
// ----------------------------------------------------------------------------

void wxGaugeBase::SetRange(int range)
{
    m_rangeMax = range;
}

int wxGaugeBase::GetRange() const
{
    return m_rangeMax;
}

void wxGaugeBase::SetValue(int pos)
{
    m_gaugePos = pos;
}

int wxGaugeBase::GetValue() const
{
    return m_gaugePos;
}

// ----------------------------------------------------------------------------
// wxGauge indeterminate mode
// ----------------------------------------------------------------------------

void wxGaugeBase::Pulse()
{
#if wxGAUGE_EMULATE_INDETERMINATE_MODE
    // simulate indeterminate mode
    int curr = GetValue(), max = GetRange();

    if (m_nDirection == wxRIGHT)
    {
        if (curr < max)
            SetValue(curr + 1);
        else
        {
            SetValue(max - 1);
            m_nDirection = wxLEFT;
        }
    }
    else
    {
        if (curr > 0)
            SetValue(curr - 1);
        else
        {
            SetValue(1);
            m_nDirection = wxRIGHT;
        }
    }
#endif
}

#endif // wxUSE_GAUGE
