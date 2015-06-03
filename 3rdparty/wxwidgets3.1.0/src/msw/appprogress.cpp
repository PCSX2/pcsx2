/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/appprog.cpp
// Purpose:     Implementation of wxAppProgressIndicator.
// Author:      Chaobin Zhang <zhchbin@gmail.com>
// Created:     2014-09-05
// Copyright:   (c) 2014 wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_TASKBARBUTTON

#ifndef WX_PRECOMP
    #include "wx/toplevel.h"
#endif

#include "wx/appprogress.h"
#include "wx/msw/taskbarbutton.h"

// ----------------------------------------------------------------------------
// wxAppProgressIndicator Implementation.
// ----------------------------------------------------------------------------
wxAppProgressIndicator::wxAppProgressIndicator(wxWindow* parent, int maxValue)
    : m_maxValue(maxValue)
{
    if ( parent == NULL )
    {
        for ( wxWindowList::const_iterator it = wxTopLevelWindows.begin();
              it != wxTopLevelWindows.end();
              ++it )
        {
            wxTaskBarButton* const button = wxTaskBarButton::New(*it);
            if ( button )
                m_taskBarButtons.push_back(button);
        }
    }
    else
    {
        wxTaskBarButton* const button = wxTaskBarButton::New(parent);
        if ( button )
            m_taskBarButtons.push_back(button);
    }

    Reset();
    SetRange(m_maxValue);
}

wxAppProgressIndicator::~wxAppProgressIndicator()
{
    Reset();

    for ( size_t i = 0; i < m_taskBarButtons.size(); ++i )
    {
        delete m_taskBarButtons[i];
    }
}

bool wxAppProgressIndicator::IsAvailable() const
{
    return !m_taskBarButtons.empty();
}

void wxAppProgressIndicator::SetValue(int value)
{
    wxASSERT_MSG( value <= m_maxValue, wxT("invalid progress value") );

    for ( size_t i = 0; i < m_taskBarButtons.size(); ++i )
    {
        m_taskBarButtons[i]->SetProgressValue(value);
    }
}

void wxAppProgressIndicator::SetRange(int range)
{
    m_maxValue = range;

    for ( size_t i = 0; i < m_taskBarButtons.size(); ++i )
    {
        m_taskBarButtons[i]->SetProgressRange(range);
    }
}

void wxAppProgressIndicator::Pulse()
{
    for ( size_t i = 0; i < m_taskBarButtons.size(); ++i )
    {
        m_taskBarButtons[i]->PulseProgress();
    }
}

void wxAppProgressIndicator::Reset()
{
    for ( size_t i = 0; i < m_taskBarButtons.size(); ++i )
    {
        m_taskBarButtons[i]->SetProgressState(wxTASKBAR_BUTTON_NO_PROGRESS);
    }
}

#endif // wxUSE_TASKBARBUTTON
