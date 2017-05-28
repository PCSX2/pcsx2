///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/socketiohandler.cpp
// Purpose:     implementation of wxFDIOHandler for wxSocket
// Author:      Angel Vidal, Lukasz Michalski
// Created:     08.24.06
// Copyright:   (c) 2006 Angel vidal
//              (c) 2007 Vadim Zeitlin <vadim@wxwidgets.org>
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

#if wxUSE_SOCKETS

#include "wx/app.h"
#include "wx/apptrait.h"
#include "wx/private/socket.h"
#include "wx/link.h"

// ============================================================================
// wxSocketFDBasedManager implementation
// ============================================================================

bool wxSocketFDBasedManager::OnInit()
{
    wxAppTraits * const traits = wxApp::GetTraitsIfExists();
    if ( !traits )
        return false;

    m_fdioManager = traits->GetFDIOManager();
    return m_fdioManager != NULL;
}

void wxSocketFDBasedManager::Install_Callback(wxSocketImpl *socket_,
                                              wxSocketNotify event)
{
    wxSocketImplUnix * const
        socket = static_cast<wxSocketImplUnix *>(socket_);

    wxCHECK_RET( socket->m_fd != -1,
                    "shouldn't be called on invalid socket" );

    const wxFDIOManager::Direction d = GetDirForEvent(socket, event);

    int& fd = FD(socket, d);
    if ( fd != -1 )
        m_fdioManager->RemoveInput(socket, fd, d);

    fd = m_fdioManager->AddInput(socket, socket->m_fd, d);
}

void wxSocketFDBasedManager::Uninstall_Callback(wxSocketImpl *socket_,
                                                wxSocketNotify event)
{
    wxSocketImplUnix * const
        socket = static_cast<wxSocketImplUnix *>(socket_);

    const wxFDIOManager::Direction d = GetDirForEvent(socket, event);

    int& fd = FD(socket, d);
    if ( fd != -1 )
    {
        m_fdioManager->RemoveInput(socket, fd, d);
        fd = -1;
    }
}

wxFDIOManager::Direction
wxSocketFDBasedManager::GetDirForEvent(wxSocketImpl *socket,
                                       wxSocketNotify event)
{
    switch ( event )
    {
        default:
            wxFAIL_MSG( "unknown socket event" );
            return wxFDIOManager::INPUT; // we must return something

        case wxSOCKET_LOST:
            wxFAIL_MSG( "unexpected socket event" );
            return wxFDIOManager::INPUT; // as above

        case wxSOCKET_INPUT:
            return wxFDIOManager::INPUT;

        case wxSOCKET_OUTPUT:
            return wxFDIOManager::OUTPUT;

        case wxSOCKET_CONNECTION:
            // for server sockets we're interested in events indicating
            // that a new connection is pending, i.e. that accept() will
            // succeed and this is indicated by socket becoming ready for
            // reading, while for the other ones we're interested in the
            // completion of non-blocking connect() which is indicated by
            // the socket becoming ready for writing
            return socket->IsServer() ? wxFDIOManager::INPUT
                                      : wxFDIOManager::OUTPUT;
    }
}

// set the wxBase variable to point to our wxSocketManager implementation
//
// see comments in wx/apptrait.h for the explanation of why do we do it
// like this
static struct ManagerSetter
{
    ManagerSetter()
    {
        static wxSocketFDBasedManager s_manager;
        wxAppTraits::SetDefaultSocketManager(&s_manager);
    }
} gs_managerSetter;


// see the relative linker macro in socket.cpp
wxFORCE_LINK_THIS_MODULE( socketiohandler );

#endif // wxUSE_SOCKETS
