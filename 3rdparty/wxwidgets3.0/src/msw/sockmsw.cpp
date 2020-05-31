/////////////////////////////////////////////////////////////////////////////
// Name:       src/msw/sockmsw.cpp
// Purpose:    MSW-specific socket code
// Authors:    Guilhem Lavaux, Guillermo Rodriguez Garcia
// Created:    April 1997
// Copyright:  (C) 1999-1997, Guilhem Lavaux
//             (C) 1999-2000, Guillermo Rodriguez Garcia
//             (C) 2008 Vadim Zeitlin
// Licence:    wxWindows licence
/////////////////////////////////////////////////////////////////////////////


// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_SOCKETS

/* including rasasync.h (included from windows.h itself included from
 * wx/setup.h and/or winsock.h results in this warning for
 * RPCNOTIFICATION_ROUTINE
 */
#ifdef _MSC_VER
#   pragma warning(disable:4115) /* named type definition in parentheses */
#endif

#include "wx/private/socket.h"
#include "wx/msw/private.h"     // for wxGetInstance()
#include "wx/private/fd.h"
#include "wx/apptrait.h"
#include "wx/thread.h"
#include "wx/dynlib.h"
#include "wx/link.h"

#ifdef __WXWINCE__
/*
 * As WSAAsyncSelect is not present on WinCE, it now uses WSACreateEvent,
 * WSAEventSelect, WSAWaitForMultipleEvents and WSAEnumNetworkEvents. When
 * enabling eventhandling for a socket a new thread it created that keeps track
 * of the events and posts a messageto the hidden window to use the standard
 * message loop.
 */
#include "wx/msw/wince/net.h"
#include "wx/hashmap.h"
WX_DECLARE_HASH_MAP(int,bool,wxIntegerHash,wxIntegerEqual,SocketHash);

#ifndef isdigit
#define isdigit(x) (x > 47 && x < 58)
#endif
#include "wx/msw/wince/net.h"

#endif // __WXWINCE__

#ifdef _MSC_VER
#  pragma warning(default:4115) /* named type definition in parentheses */
#endif

#include "wx/msw/private/hiddenwin.h"

#define CLASSNAME  TEXT("_wxSocket_Internal_Window_Class")

/* Maximum number of different wxSocket objects at a given time.
 * This value can be modified at will, but it CANNOT be greater
 * than (0x7FFF - WM_USER + 1)
 */
#define MAXSOCKETS 1024

#if (MAXSOCKETS > (0x7FFF - WM_USER + 1))
#error "MAXSOCKETS is too big!"
#endif

#ifndef __WXWINCE__
typedef int (PASCAL *WSAAsyncSelect_t)(SOCKET,HWND,u_int,long);
#else
/* Typedef the needed function prototypes and the WSANETWORKEVENTS structure
*/
typedef struct _WSANETWORKEVENTS {
       long lNetworkEvents;
       int iErrorCode[10];
} WSANETWORKEVENTS, FAR * LPWSANETWORKEVENTS;
typedef HANDLE (PASCAL *WSACreateEvent_t)();
typedef int (PASCAL *WSAEventSelect_t)(SOCKET,HANDLE,long);
typedef int (PASCAL *WSAWaitForMultipleEvents_t)(long,HANDLE,BOOL,long,BOOL);
typedef int (PASCAL *WSAEnumNetworkEvents_t)(SOCKET,HANDLE,LPWSANETWORKEVENTS);
#endif //__WXWINCE__

LRESULT CALLBACK wxSocket_Internal_WinProc(HWND, UINT, WPARAM, LPARAM);

/* Global variables */

static HWND hWin;
wxCRIT_SECT_DECLARE_MEMBER(gs_critical);
static wxSocketImplMSW *socketList[MAXSOCKETS];
static int firstAvailable;

#ifndef __WXWINCE__
static WSAAsyncSelect_t gs_WSAAsyncSelect = NULL;
#else
static SocketHash socketHash;
static unsigned int currSocket;
HANDLE hThread[MAXSOCKETS];
static WSACreateEvent_t gs_WSACreateEvent = NULL;
static WSAEventSelect_t gs_WSAEventSelect = NULL;
static WSAWaitForMultipleEvents_t gs_WSAWaitForMultipleEvents = NULL;
static WSAEnumNetworkEvents_t gs_WSAEnumNetworkEvents = NULL;
/* This structure will be used to pass data on to the thread that handles socket events.
*/
typedef struct thread_data{
    HWND hEvtWin;
    unsigned long msgnumber;
    unsigned long fd;
    unsigned long lEvent;
}thread_data;
#endif

#ifdef __WXWINCE__
/* This thread handles socket events on WinCE using WSAEventSelect() as
 * WSAAsyncSelect is not supported. When an event occurs for the socket, it is
 * checked what kind of event happened and the correct message gets posted so
 * that the hidden window can handle it as it would in other MSW builds.
*/
DWORD WINAPI SocketThread(LPVOID data)
{
    WSANETWORKEVENTS NetworkEvents;
    thread_data* d = (thread_data *)data;

    HANDLE NetworkEvent = gs_WSACreateEvent();
    gs_WSAEventSelect(d->fd, NetworkEvent, d->lEvent);

    while(socketHash[d->fd] == true)
    {
        if ((gs_WSAWaitForMultipleEvents(1, &NetworkEvent, FALSE,INFINITE, FALSE)) == WAIT_FAILED)
        {
            printf("WSAWaitForMultipleEvents failed with error %d\n", WSAGetLastError());
            return 0;
        }
        if (gs_WSAEnumNetworkEvents(d->fd ,NetworkEvent, &NetworkEvents) == SOCKET_ERROR)
        {
            printf("WSAEnumNetworkEvents failed with error %d\n", WSAGetLastError());
            return 0;
        }

        long flags = NetworkEvents.lNetworkEvents;
        if (flags & FD_READ)
            ::PostMessage(d->hEvtWin, d->msgnumber,d->fd, FD_READ);
        if (flags & FD_WRITE)
            ::PostMessage(d->hEvtWin, d->msgnumber,d->fd, FD_WRITE);
        if (flags & FD_OOB)
            ::PostMessage(d->hEvtWin, d->msgnumber,d->fd, FD_OOB);
        if (flags & FD_ACCEPT)
            ::PostMessage(d->hEvtWin, d->msgnumber,d->fd, FD_ACCEPT);
        if (flags & FD_CONNECT)
            ::PostMessage(d->hEvtWin, d->msgnumber,d->fd, FD_CONNECT);
        if (flags & FD_CLOSE)
            ::PostMessage(d->hEvtWin, d->msgnumber,d->fd, FD_CLOSE);

    }
    gs_WSAEventSelect(d->fd, NetworkEvent, 0);
    ExitThread(0);
    return 0;
}
#endif

// ----------------------------------------------------------------------------
// MSW implementation of wxSocketManager
// ----------------------------------------------------------------------------

class wxSocketMSWManager : public wxSocketManager
{
public:
    virtual bool OnInit();
    virtual void OnExit();

    virtual wxSocketImpl *CreateSocket(wxSocketBase& wxsocket)
    {
        return new wxSocketImplMSW(wxsocket);
    }
    virtual void Install_Callback(wxSocketImpl *socket,
                                  wxSocketNotify event = wxSOCKET_LOST);
    virtual void Uninstall_Callback(wxSocketImpl *socket,
                                    wxSocketNotify event = wxSOCKET_LOST);

private:
    static wxDynamicLibrary gs_wsock32dll;
};

wxDynamicLibrary wxSocketMSWManager::gs_wsock32dll;

bool wxSocketMSWManager::OnInit()
{
  LPCTSTR pclassname = NULL;
  int i;

  /* Create internal window for event notifications */
  hWin = wxCreateHiddenWindow(&pclassname, CLASSNAME, wxSocket_Internal_WinProc);
  if (!hWin)
      return false;

  /* Initialize socket list */
  for (i = 0; i < MAXSOCKETS; i++)
  {
    socketList[i] = NULL;
  }
  firstAvailable = 0;

  // we don't link with wsock32.dll (or ws2 in CE case) statically to avoid
  // dependencies on it for all the application using wx even if they don't use
  // sockets
#ifdef __WXWINCE__
    #define WINSOCK_DLL_NAME wxT("ws2.dll")
#else
    #define WINSOCK_DLL_NAME wxT("wsock32.dll")
#endif

    gs_wsock32dll.Load(WINSOCK_DLL_NAME, wxDL_VERBATIM | wxDL_QUIET);
    if ( !gs_wsock32dll.IsLoaded() )
        return false;

#ifndef __WXWINCE__
    wxDL_INIT_FUNC(gs_, WSAAsyncSelect, gs_wsock32dll);
    if ( !gs_WSAAsyncSelect )
        return false;
#else
    wxDL_INIT_FUNC(gs_, WSAEventSelect, gs_wsock32dll);
    if ( !gs_WSAEventSelect )
        return false;

    wxDL_INIT_FUNC(gs_, WSACreateEvent, gs_wsock32dll);
    if ( !gs_WSACreateEvent )
        return false;

    wxDL_INIT_FUNC(gs_, WSAWaitForMultipleEvents, gs_wsock32dll);
    if ( !gs_WSAWaitForMultipleEvents )
        return false;

    wxDL_INIT_FUNC(gs_, WSAEnumNetworkEvents, gs_wsock32dll);
    if ( !gs_WSAEnumNetworkEvents )
        return false;

    currSocket = 0;
#endif // !__WXWINCE__/__WXWINCE__

  // finally initialize WinSock
  WSADATA wsaData;
  return WSAStartup((1 << 8) | 1, &wsaData) == 0;
}

void wxSocketMSWManager::OnExit()
{
#ifdef __WXWINCE__
/* Delete the threads here */
    for(unsigned int i=0; i < currSocket; i++)
        CloseHandle(hThread[i]);
#endif
  /* Destroy internal window */
  DestroyWindow(hWin);
  UnregisterClass(CLASSNAME, wxGetInstance());

  WSACleanup();

  gs_wsock32dll.Unload();
}

/* Per-socket GUI initialization / cleanup */

wxSocketImplMSW::wxSocketImplMSW(wxSocketBase& wxsocket)
    : wxSocketImpl(wxsocket)
{
  /* Allocate a new message number for this socket */
  wxCRIT_SECT_LOCKER(lock, gs_critical);

  int i = firstAvailable;
  while (socketList[i] != NULL)
  {
    i = (i + 1) % MAXSOCKETS;

    if (i == firstAvailable)    /* abort! */
    {
      m_msgnumber = 0; // invalid
      return;
    }
  }
  socketList[i] = this;
  firstAvailable = (i + 1) % MAXSOCKETS;
  m_msgnumber = (i + WM_USER);
}

wxSocketImplMSW::~wxSocketImplMSW()
{
  /* Remove the socket from the list */
  wxCRIT_SECT_LOCKER(lock, gs_critical);

  if ( m_msgnumber )
  {
      // we need to remove any pending messages for this socket to avoid having
      // them sent to a new socket which could reuse the same message number as
      // soon as we destroy this one
      MSG msg;
      while ( ::PeekMessage(&msg, hWin, m_msgnumber, m_msgnumber, PM_REMOVE) )
          ;

      socketList[m_msgnumber - WM_USER] = NULL;
  }
  //else: the socket has never been created successfully
}

/* Windows proc for asynchronous event handling */

LRESULT CALLBACK wxSocket_Internal_WinProc(HWND hWnd,
                                           UINT uMsg,
                                           WPARAM wParam,
                                           LPARAM lParam)
{
    if ( uMsg < WM_USER || uMsg > (WM_USER + MAXSOCKETS - 1))
        return DefWindowProc(hWnd, uMsg, wParam, lParam);

    wxSocketImplMSW *socket;
    wxSocketNotify event = (wxSocketNotify)-1;
    {
        wxCRIT_SECT_LOCKER(lock, gs_critical);

        socket = socketList[(uMsg - WM_USER)];
        if ( !socket )
            return 0;

        // the socket may be already closed but we could still receive
        // notifications for it sent (asynchronously) before it got closed
        if ( socket->m_fd == INVALID_SOCKET )
            return 0;

        wxASSERT_MSG( socket->m_fd == (SOCKET)wParam,
                      "mismatch between message and socket?" );

        switch ( WSAGETSELECTEVENT(lParam) )
        {
            case FD_READ:
                // We may get a FD_READ notification even when there is no data
                // to read on the socket, in particular this happens on socket
                // creation when we seem to always get FD_CONNECT, FD_WRITE and
                // FD_READ notifications all at once (but it doesn't happen
                // only then). Ignore such dummy notifications.
                {
                    fd_set fds;
                    timeval tv = { 0, 0 };

                    wxFD_ZERO(&fds);
                    wxFD_SET(socket->m_fd, &fds);

                    if ( select(socket->m_fd + 1, &fds, NULL, NULL, &tv) != 1 )
                        return 0;
                }

                event = wxSOCKET_INPUT;
                break;

            case FD_WRITE:
                event = wxSOCKET_OUTPUT;
                break;

            case FD_ACCEPT:
                event = wxSOCKET_CONNECTION;
                break;

            case FD_CONNECT:
                event = WSAGETSELECTERROR(lParam) ? wxSOCKET_LOST
                                                  : wxSOCKET_CONNECTION;
                break;

            case FD_CLOSE:
                event = wxSOCKET_LOST;
                break;

            default:
                wxFAIL_MSG( "unexpected socket notification" );
                return 0;
        }
    } // unlock gs_critical

    socket->NotifyOnStateChange(event);

    return 0;
}

/*
 *  Enable all event notifications; we need to be notified of all
 *  events for internal processing, but we will only notify users
 *  when an appropriate callback function has been installed.
 */
void wxSocketMSWManager::Install_Callback(wxSocketImpl *socket_,
                                         wxSocketNotify WXUNUSED(event))
{
    wxSocketImplMSW * const socket = static_cast<wxSocketImplMSW *>(socket_);

  if (socket->m_fd != INVALID_SOCKET)
  {
    /* We could probably just subscribe to all events regardless
     * of the socket type, but MS recommends to do it this way.
     */
    long lEvent = socket->m_server?
                  FD_ACCEPT : (FD_READ | FD_WRITE | FD_CONNECT | FD_CLOSE);
#ifndef __WXWINCE__
    gs_WSAAsyncSelect(socket->m_fd, hWin, socket->m_msgnumber, lEvent);
#else
/*
*  WinCE creates a thread for socket event handling.
*  All needed parameters get passed through the thread_data structure.
*/

    thread_data* d = new thread_data;
    d->lEvent = lEvent;
    d->hEvtWin = hWin;
    d->msgnumber = socket->m_msgnumber;
    d->fd = socket->m_fd;
    socketHash[socket->m_fd] = true;
    hThread[currSocket++] = CreateThread(NULL, 0, &SocketThread,(LPVOID)d, 0, NULL);
#endif
  }
}

/*
 *  Disable event notifications (used when shutting down the socket)
 */
void wxSocketMSWManager::Uninstall_Callback(wxSocketImpl *socket_,
                                            wxSocketNotify WXUNUSED(event))
{
    wxSocketImplMSW * const socket = static_cast<wxSocketImplMSW *>(socket_);

  if (socket->m_fd != INVALID_SOCKET)
  {
#ifndef __WXWINCE__
    gs_WSAAsyncSelect(socket->m_fd, hWin, socket->m_msgnumber, 0);
#else
    //Destroy the thread
    socketHash[socket->m_fd] = false;
#endif
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
        static wxSocketMSWManager s_manager;
        wxAppTraits::SetDefaultSocketManager(&s_manager);
    }
} gs_managerSetter;

// see the relative linker macro in socket.cpp
wxFORCE_LINK_THIS_MODULE( mswsocket );

// ============================================================================
// wxSocketImpl implementation
// ============================================================================

void wxSocketImplMSW::DoClose()
{
    wxSocketManager::Get()->Uninstall_Callback(this);

    closesocket(m_fd);
}

wxSocketError wxSocketImplMSW::GetLastError() const
{
    switch ( WSAGetLastError() )
    {
        case 0:
            return wxSOCKET_NOERROR;

        case WSAENOTSOCK:
            return wxSOCKET_INVSOCK;

        case WSAEWOULDBLOCK:
            return wxSOCKET_WOULDBLOCK;

        default:
            return wxSOCKET_IOERR;
    }
}

#endif  // wxUSE_SOCKETS
