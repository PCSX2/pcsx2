///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/power.cpp
// Purpose:     power management functions for MSW
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2006-05-27
// Copyright:   (c) 2006 Vadim Zeitlin <vadim@wxwindows.org>
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

#ifndef WX_PRECOMP
#endif //WX_PRECOMP

#include "wx/power.h"
#include "wx/atomic.h"
#include "wx/msw/private.h"

#if !defined(__WINCE_STANDARDSDK__)

#ifdef __WXWINCE__
    typedef SYSTEM_POWER_STATUS_EX SYSTEM_POWER_STATUS;
    BOOL GetSystemPowerStatus(SYSTEM_POWER_STATUS *status)
    {
        return GetSystemPowerStatusEx(status, TRUE);
    }
#endif

// ----------------------------------------------------------------------------
// wxPowerResource
// ----------------------------------------------------------------------------

namespace
{

wxAtomicInt g_powerResourceScreenRefCount = 0;
wxAtomicInt g_powerResourceSystemRefCount = 0;

bool UpdatePowerResourceExecutionState()
{
    EXECUTION_STATE executionState = ES_CONTINUOUS;
    if ( g_powerResourceScreenRefCount > 0 )
        executionState |= ES_DISPLAY_REQUIRED;

    if ( g_powerResourceSystemRefCount > 0 )
        executionState |= ES_SYSTEM_REQUIRED;

    if ( ::SetThreadExecutionState(executionState) == 0 )
    {
        wxLogLastError(wxT("SetThreadExecutionState()"));
        return false;
    }

    return true;
}

} // anonymous namespace

bool
wxPowerResource::Acquire(wxPowerResourceKind kind,
                         const wxString& WXUNUSED(reason))
{
    switch ( kind )
    {
        case wxPOWER_RESOURCE_SCREEN:
            wxAtomicInc(g_powerResourceScreenRefCount);
            break;

        case wxPOWER_RESOURCE_SYSTEM:
            wxAtomicInc(g_powerResourceSystemRefCount);
            break;
    }

    return UpdatePowerResourceExecutionState();
}

void wxPowerResource::Release(wxPowerResourceKind kind)
{
    switch ( kind )
    {
        case wxPOWER_RESOURCE_SCREEN:
            if ( g_powerResourceScreenRefCount > 0 )
            {
                wxAtomicDec(g_powerResourceScreenRefCount);
            }
            else
            {
                wxFAIL_MSG( "Screen power resource was not acquired" );
            }
            break;

        case wxPOWER_RESOURCE_SYSTEM:
            if ( g_powerResourceSystemRefCount > 0 )
            {
                wxAtomicDec(g_powerResourceSystemRefCount);
            }
            else
            {
                wxFAIL_MSG( "System power resource was not acquired" );
            }
            break;
    }

    UpdatePowerResourceExecutionState();
}


// ----------------------------------------------------------------------------
// helper functions
// ----------------------------------------------------------------------------

static inline bool wxGetPowerStatus(SYSTEM_POWER_STATUS *sps)
{
    if ( !::GetSystemPowerStatus(sps) )
    {
        wxLogLastError(wxT("GetSystemPowerStatus()"));
        return false;
    }

    return true;
}

#endif

// ============================================================================
// implementation
// ============================================================================

wxPowerType wxGetPowerType()
{
#if !defined(__WINCE_STANDARDSDK__)
    SYSTEM_POWER_STATUS sps;
    if ( wxGetPowerStatus(&sps) )
    {
        switch ( sps.ACLineStatus )
        {
            case 0:
                return wxPOWER_BATTERY;

            case 1:
                return wxPOWER_SOCKET;

            default:
                wxLogDebug(wxT("Unknown ACLineStatus=%u"), sps.ACLineStatus);
            case 255:
                break;
        }
    }
#endif

    return wxPOWER_UNKNOWN;
}

wxBatteryState wxGetBatteryState()
{
#if !defined(__WINCE_STANDARDSDK__)
    SYSTEM_POWER_STATUS sps;
    if ( wxGetPowerStatus(&sps) )
    {
        // there can be other bits set in the flag field ("charging" and "no
        // battery"), extract only those which we need here
        switch ( sps.BatteryFlag & 7 )
        {
            case 1:
                return wxBATTERY_NORMAL_STATE;

            case 2:
                return wxBATTERY_LOW_STATE;

            case 3:
                return wxBATTERY_CRITICAL_STATE;
        }
    }
#endif

    return wxBATTERY_UNKNOWN_STATE;
}
