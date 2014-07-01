/////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xh_slidr.cpp
// Purpose:     XRC resource for wxSlider
// Author:      Bob Mitchell
// Created:     2000/03/21
// Copyright:   (c) 2000 Bob Mitchell and Verant Interactive
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC && wxUSE_SLIDER

#include "wx/xrc/xh_slidr.h"

#ifndef WX_PRECOMP
    #include "wx/slider.h"
#endif

static const long DEFAULT_VALUE = 0;
static const long DEFAULT_MIN = 0;
static const long DEFAULT_MAX = 100;


IMPLEMENT_DYNAMIC_CLASS(wxSliderXmlHandler, wxXmlResourceHandler)

wxSliderXmlHandler::wxSliderXmlHandler()
                   :wxXmlResourceHandler()
{
    XRC_ADD_STYLE(wxSL_HORIZONTAL);
    XRC_ADD_STYLE(wxSL_VERTICAL);
    XRC_ADD_STYLE(wxSL_AUTOTICKS);
    XRC_ADD_STYLE(wxSL_LABELS);
    XRC_ADD_STYLE(wxSL_LEFT);
    XRC_ADD_STYLE(wxSL_TOP);
    XRC_ADD_STYLE(wxSL_RIGHT);
    XRC_ADD_STYLE(wxSL_BOTTOM);
    XRC_ADD_STYLE(wxSL_BOTH);
    XRC_ADD_STYLE(wxSL_SELRANGE);
    XRC_ADD_STYLE(wxSL_INVERSE);
    AddWindowStyles();
}

wxObject *wxSliderXmlHandler::DoCreateResource()
{
    XRC_MAKE_INSTANCE(control, wxSlider)

    control->Create(m_parentAsWindow,
                    GetID(),
                    GetLong(wxT("value"), DEFAULT_VALUE),
                    GetLong(wxT("min"), DEFAULT_MIN),
                    GetLong(wxT("max"), DEFAULT_MAX),
                    GetPosition(), GetSize(),
                    GetStyle(),
                    wxDefaultValidator,
                    GetName());

    if( HasParam(wxT("tickfreq")))
    {
        control->SetTickFreq(GetLong(wxT("tickfreq")));
    }
    if( HasParam(wxT("pagesize")))
    {
        control->SetPageSize(GetLong(wxT("pagesize")));
    }
    if( HasParam(wxT("linesize")))
    {
        control->SetLineSize(GetLong(wxT("linesize")));
    }
    if( HasParam(wxT("thumb")))
    {
        control->SetThumbLength(GetLong(wxT("thumb")));
    }
    if( HasParam(wxT("tick")))
    {
        control->SetTick(GetLong(wxT("tick")));
    }
    if( HasParam(wxT("selmin")) && HasParam(wxT("selmax")))
    {
        control->SetSelection(GetLong(wxT("selmin")), GetLong(wxT("selmax")));
    }

    SetupWindow(control);

    return control;
}

bool wxSliderXmlHandler::CanHandle(wxXmlNode *node)
{
    return IsOfClass(node, wxT("wxSlider"));
}

#endif // wxUSE_XRC && wxUSE_SLIDER
