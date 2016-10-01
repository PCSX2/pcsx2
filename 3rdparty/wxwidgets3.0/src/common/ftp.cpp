/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/ftp.cpp
// Purpose:     FTP protocol
// Author:      Guilhem Lavaux
// Modified by: Mark Johnson, wxWindows@mj10777.de
//              20000917 : RmDir, GetLastResult, GetList
//              Vadim Zeitlin (numerous fixes and rewrites to all part of the
//              code, support ASCII/Binary modes, better error reporting, more
//              robust Abort(), support for arbitrary FTP commands, ...)
//              Randall Fox (support for active mode)
// Created:     07/07/1997
// Copyright:   (c) 1997, 1998 Guilhem Lavaux
//              (c) 1998-2004 wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

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

#if wxUSE_PROTOCOL_FTP

#ifndef WX_PRECOMP
    #include <stdlib.h>
    #include "wx/string.h"
    #include "wx/utils.h"
    #include "wx/log.h"
    #include "wx/intl.h"
    #include "wx/wxcrtvararg.h"
#endif // WX_PRECOMP

#include "wx/sckaddr.h"
#include "wx/socket.h"
#include "wx/url.h"
#include "wx/sckstrm.h"
#include "wx/protocol/protocol.h"
#include "wx/protocol/ftp.h"

#include <memory.h>

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// the length of FTP status code (3 digits)
static const size_t LEN_CODE = 3;

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxFTP, wxProtocol)
IMPLEMENT_PROTOCOL(wxFTP, wxT("ftp"), wxT("ftp"), true)

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxFTP constructor and destructor
// ----------------------------------------------------------------------------

wxFTP::wxFTP()
{
    m_streaming = false;
    m_currentTransfermode = NONE;

    m_username = wxT("anonymous");
    m_password << wxGetUserId() << wxT('@') << wxGetFullHostName();

    m_bPassive = true;
    m_bEncounteredError = false;
}

wxFTP::~wxFTP()
{
    if ( m_streaming )
    {
        // if we are streaming, this will issue
        // an FTP ABORT command, to tell the server we are aborting
        (void)Abort();
    }

    // now this issues a "QUIT" command to tell the server we are
    Close();
}

// ----------------------------------------------------------------------------
// wxFTP connect and login methods
// ----------------------------------------------------------------------------

bool wxFTP::Connect(const wxSockAddress& addr, bool WXUNUSED(wait))
{
    if ( !wxProtocol::Connect(addr) )
    {
        m_lastError = wxPROTO_NETERR;
        return false;
    }

    if ( !m_username )
    {
        m_lastError = wxPROTO_CONNERR;
        return false;
    }

    // we should have 220 welcome message
    if ( !CheckResult('2') )
    {
        Close();
        return false;
    }

    wxString command;
    command.Printf(wxT("USER %s"), m_username.c_str());
    char rc = SendCommand(command);
    if ( rc == '2' )
    {
        // 230 return: user accepted without password
        m_lastError = wxPROTO_NOERR;
        return true;
    }

    if ( rc != '3' )
    {
        m_lastError = wxPROTO_CONNERR;
        Close();
        return false;
    }

    command.Printf(wxT("PASS %s"), m_password.c_str());
    if ( !CheckCommand(command, '2') )
    {
        m_lastError = wxPROTO_CONNERR;
        Close();
        return false;
    }

    m_lastError = wxPROTO_NOERR;
    return true;
}

bool wxFTP::Connect(const wxString& host, unsigned short port)
{
    wxIPV4address addr;
    addr.Hostname(host);

    if ( port )
        addr.Service(port);
    else if (!addr.Service(wxT("ftp")))
        addr.Service(21);

    return Connect(addr);
}

bool wxFTP::Close()
{
    if ( m_streaming )
    {
        m_lastError = wxPROTO_STREAMING;
        return false;
    }

    if ( IsConnected() )
    {
        if ( !CheckCommand(wxT("QUIT"), '2') )
        {
            m_lastError = wxPROTO_CONNERR;
            wxLogDebug(wxT("Failed to close connection gracefully."));
        }
    }

    return wxSocketClient::Close();
}

// ============================================================================
// low level methods
// ============================================================================

wxSocketBase *wxFTP::AcceptIfActive(wxSocketBase *sock)
{
    if ( m_bPassive )
        return sock;

    // now wait for a connection from server
    wxSocketServer *sockSrv = (wxSocketServer *)sock;
    if ( !sockSrv->WaitForAccept() )
    {
        m_lastError = wxPROTO_CONNERR;
        wxLogError(_("Timeout while waiting for FTP server to connect, try passive mode."));
        wxDELETE(sock);
    }
    else
    {
        m_lastError = wxPROTO_NOERR;
        sock = sockSrv->Accept(true);
        delete sockSrv;
    }

    return sock;
}

bool wxFTP::Abort()
{
    if ( !m_streaming )
        return true;

    m_streaming = false;
    if ( !CheckCommand(wxT("ABOR"), '4') )
        return false;

    return CheckResult('2');
}


// ----------------------------------------------------------------------------
// Send command to FTP server
// ----------------------------------------------------------------------------

char wxFTP::SendCommand(const wxString& command)
{
    if ( m_streaming )
    {
        m_lastError = wxPROTO_STREAMING;
        return 0;
    }

    wxString tmp_str = command + wxT("\r\n");
    const wxWX2MBbuf tmp_buf = tmp_str.mb_str();
    if ( Write(wxMBSTRINGCAST tmp_buf, strlen(tmp_buf)).Error())
    {
        m_lastError = wxPROTO_NETERR;
        return 0;
    }

    // don't show the passwords in the logs (even in debug ones)
    wxString cmd, password;
    if ( command.Upper().StartsWith(wxT("PASS "), &password) )
    {
        cmd << wxT("PASS ") << wxString(wxT('*'), password.length());
    }
    else
    {
        cmd = command;
    }

    LogRequest(cmd);

    m_lastError = wxPROTO_NOERR;
    return GetResult();
}

// ----------------------------------------------------------------------------
// Receive servers reply
// ----------------------------------------------------------------------------

char wxFTP::GetResult()
{
    // if we've already had a read or write timeout error, the connection is
    // probably toast, so don't bother, it just wastes the users time
    if ( m_bEncounteredError )
        return 0;

    wxString code;

    // m_lastResult will contain the entire server response, possibly on
    // multiple lines
    m_lastResult.clear();

    // we handle multiline replies here according to RFC 959: it says that a
    // reply may either be on 1 line of the form "xyz ..." or on several lines
    // in whuch case it looks like
    //      xyz-...
    //      ...
    //      xyz ...
    // and the intermeidate lines may start with xyz or not
    bool badReply = false;
    bool firstLine = true;
    bool endOfReply = false;
    while ( !endOfReply && !badReply )
    {
        wxString line;
        m_lastError = ReadLine(this,line);
        if ( m_lastError )
        {
            m_bEncounteredError = true;
            return 0;
        }

        LogResponse(line);

        if ( !m_lastResult.empty() )
        {
            // separate from last line
            m_lastResult += wxT('\n');
        }

        m_lastResult += line;

        // unless this is an intermediate line of a multiline reply, it must
        // contain the code in the beginning and '-' or ' ' following it
        if ( line.Len() < LEN_CODE + 1 )
        {
            if ( firstLine )
            {
                badReply = true;
            }
        }
        else // line has at least 4 chars
        {
            // this is the char which tells us what we're dealing with
            wxChar chMarker = line.GetChar(LEN_CODE);

            if ( firstLine )
            {
                code = wxString(line, LEN_CODE);

                switch ( chMarker )
                {
                    case wxT(' '):
                        endOfReply = true;
                        break;

                    case wxT('-'):
                        firstLine = false;
                        break;

                    default:
                        // unexpected
                        badReply = true;
                }
            }
            else // subsequent line of multiline reply
            {
                if ( line.compare(0, LEN_CODE, code) == 0 )
                {
                    if ( chMarker == wxT(' ') )
                    {
                        endOfReply = true;
                    }
                }
            }
        }
    }

    if ( badReply )
    {
        wxLogDebug(wxT("Broken FTP server: '%s' is not a valid reply."),
                   m_lastResult.c_str());

        m_lastError = wxPROTO_PROTERR;

        return 0;
    }
    else
        m_lastError = wxPROTO_NOERR;

    // if we got here we must have a non empty code string
    return (char)code[0u];
}

// ----------------------------------------------------------------------------
// wxFTP simple commands
// ----------------------------------------------------------------------------

bool wxFTP::SetTransferMode(TransferMode transferMode)
{
    if ( transferMode == m_currentTransfermode )
    {
        // nothing to do
        return true;
    }

    wxString mode;
    switch ( transferMode )
    {
        default:
            wxFAIL_MSG(wxT("unknown FTP transfer mode"));
            // fall through

        case BINARY:
            mode = wxT('I');
            break;

        case ASCII:
            mode = wxT('A');
            break;
    }

    if ( !DoSimpleCommand(wxT("TYPE"), mode) )
    {
        wxLogError(_("Failed to set FTP transfer mode to %s."),
                   (transferMode == ASCII ? _("ASCII") : _("binary")));

        return false;
    }

    // If we get here the operation has been successfully completed
    // Set the status-member
    m_currentTransfermode = transferMode;

    return true;
}

bool wxFTP::DoSimpleCommand(const wxChar *command, const wxString& arg)
{
    wxString fullcmd = command;
    if ( !arg.empty() )
    {
        fullcmd << wxT(' ') << arg;
    }

    if ( !CheckCommand(fullcmd, '2') )
    {
        wxLogDebug(wxT("FTP command '%s' failed."), fullcmd.c_str());
        m_lastError = wxPROTO_NETERR;

        return false;
    }

    m_lastError = wxPROTO_NOERR;
    return true;
}

bool wxFTP::ChDir(const wxString& dir)
{
    // some servers might not understand ".." if they use different directory
    // tree conventions, but they always understand CDUP - should we use it if
    // dir == ".."? OTOH, do such servers (still) exist?

    return DoSimpleCommand(wxT("CWD"), dir);
}

bool wxFTP::MkDir(const wxString& dir)
{
    return DoSimpleCommand(wxT("MKD"), dir);
}

bool wxFTP::RmDir(const wxString& dir)
{
    return DoSimpleCommand(wxT("RMD"), dir);
}

wxString wxFTP::Pwd()
{
    wxString path;

    if ( CheckCommand(wxT("PWD"), '2') )
    {
        // the result is at least that long if CheckCommand() succeeded
        wxString::const_iterator p = m_lastResult.begin() + LEN_CODE + 1;
        const wxString::const_iterator end = m_lastResult.end();
        if ( p == end || *p != wxT('"') )
        {
            wxLogDebug(wxT("Missing starting quote in reply for PWD: %s"),
                       wxString(p, end));
        }
        else
        {
            for ( ++p; p != end; ++p )
            {
                if ( *p == wxT('"') )
                {
                    // check if the quote is doubled
                    ++p;
                    if ( p == end || *p != wxT('"') )
                    {
                        // no, this is the end
                        break;
                    }
                    //else: yes, it is: this is an embedded quote in the
                    //      filename, treat as normal char
                }

                path += *p;
            }

            if ( p != end )
            {
                wxLogDebug(wxT("Missing ending quote in reply for PWD: %s"),
                           m_lastResult.c_str() + LEN_CODE + 1);
            }
        }
    }
    else
    {
        m_lastError = wxPROTO_PROTERR;
        wxLogDebug(wxT("FTP PWD command failed."));
    }

    return path;
}

bool wxFTP::Rename(const wxString& src, const wxString& dst)
{
    wxString str;

    str = wxT("RNFR ") + src;
    if ( !CheckCommand(str, '3') )
        return false;

    str = wxT("RNTO ") + dst;

    return CheckCommand(str, '2');
}

bool wxFTP::RmFile(const wxString& path)
{
    wxString str;
    str = wxT("DELE ") + path;

    return CheckCommand(str, '2');
}

// ----------------------------------------------------------------------------
// wxFTP port methods
// ----------------------------------------------------------------------------

wxSocketBase *wxFTP::GetPort()
{
    /*
    PASSIVE:    Client sends a "PASV" to the server.  The server responds with
                an address and port number which it will be listening on. Then
                the client connects to the server at the specified address and
                port.

    ACTIVE:     Client sends the server a PORT command which includes an
                address and port number which the client will be listening on.
                The server then connects to the client at that address and
                port.
    */

    wxSocketBase *socket = m_bPassive ? GetPassivePort() : GetActivePort();
    if ( !socket )
    {
        m_bEncounteredError = true;
        return NULL;
    }

    // Now set the time for the new socket to the default or user selected
    // timeout period
    socket->SetTimeout(m_uiDefaultTimeout);

    return socket;
}

wxString wxFTP::GetPortCmdArgument(const wxIPV4address& addrLocal,
                                   const wxIPV4address& addrNew)
{
    // Just fills in the return value with the local IP
    // address of the current socket.  Also it fill in the
    // PORT which the client will be listening on

    wxString addrIP = addrLocal.IPAddress();
    int portNew = addrNew.Service();

    // We need to break the PORT number in bytes
    addrIP.Replace(wxT("."), wxT(","));
    addrIP << wxT(',')
           << wxString::Format(wxT("%d"), portNew >> 8) << wxT(',')
           << wxString::Format(wxT("%d"), portNew & 0xff);

    // Now we have a value like "10,0,0,1,5,23"
    return addrIP;
}

wxSocketBase *wxFTP::GetActivePort()
{
    // we need an address to listen on
    wxIPV4address addrNew, addrLocal;
    GetLocal(addrLocal);
    addrNew.AnyAddress();
    addrNew.Service(0); // pick an open port number.

    wxSocketServer *sockSrv = new wxSocketServer(addrNew);
    if (!sockSrv->IsOk())
    {
        // We use IsOk() here to see if everything is ok
        m_lastError = wxPROTO_PROTERR;
        delete sockSrv;
        return NULL;
    }

    //gets the new address, actually it is just the port number
    sockSrv->GetLocal(addrNew);

    // Now we create the argument of the PORT command, we send in both
    // addresses because the addrNew has an IP of "0.0.0.0", so we need the
    // value in addrLocal
    wxString port = GetPortCmdArgument(addrLocal, addrNew);
    if ( !DoSimpleCommand(wxT("PORT"), port) )
    {
        m_lastError = wxPROTO_PROTERR;
        delete sockSrv;
        wxLogError(_("The FTP server doesn't support the PORT command."));
        return NULL;
    }

    m_lastError = wxPROTO_NOERR;
    sockSrv->Notify(false); // Don't send any events
    return sockSrv;
}

wxSocketBase *wxFTP::GetPassivePort()
{
    if ( !DoSimpleCommand(wxT("PASV")) )
    {
        m_lastError = wxPROTO_PROTERR;
        wxLogError(_("The FTP server doesn't support passive mode."));
        return NULL;
    }

    size_t addrStart = m_lastResult.find(wxT('('));
    size_t addrEnd = (addrStart == wxString::npos)
                     ? wxString::npos
                     : m_lastResult.find(wxT(')'), addrStart);

    if ( addrEnd == wxString::npos )
    {
        m_lastError = wxPROTO_PROTERR;
        return NULL;
    }

    // get the port number and address
    int a[6];
    wxString straddr(m_lastResult, addrStart + 1, addrEnd - (addrStart + 1));
    wxSscanf(straddr, wxT("%d,%d,%d,%d,%d,%d"),
             &a[2],&a[3],&a[4],&a[5],&a[0],&a[1]);

    wxUint32 hostaddr = (wxUint16)a[2] << 24 |
                        (wxUint16)a[3] << 16 |
                        (wxUint16)a[4] << 8 |
                        a[5];
    wxUint16 port = (wxUint16)(a[0] << 8 | a[1]);

    wxIPV4address addr;
    addr.Hostname(hostaddr);
    addr.Service(port);

    wxSocketClient *client = new wxSocketClient();
    if ( !client->Connect(addr) )
    {
        m_lastError = wxPROTO_CONNERR;
        delete client;
        return NULL;
    }

    client->Notify(false);

    m_lastError = wxPROTO_NOERR;
    return client;
}


// ----------------------------------------------------------------------------
// wxFTP download and upload
// ----------------------------------------------------------------------------

class wxInputFTPStream : public wxSocketInputStream
{
public:
    wxInputFTPStream(wxFTP *ftp, wxSocketBase *sock)
        : wxSocketInputStream(*sock)
    {
        m_ftp = ftp;
        // socket timeout automatically set in GetPort function
    }

    virtual ~wxInputFTPStream()
    {
        delete m_i_socket;   // keep at top

        // when checking the result, the stream will
        // almost always show an error, even if the file was
        // properly transferred, thus, let's just grab the result

        // we are looking for "226 transfer completed"
        char code = m_ftp->GetResult();
        if ('2' == code)
        {
            // it was a good transfer.
            // we're done!
             m_ftp->m_streaming = false;
            return;
        }
        // did we timeout?
        if (0 == code)
        {
            // the connection is probably toast. issue an abort, and
            // then a close. there won't be any more waiting
            // for this connection
            m_ftp->Abort();
            m_ftp->Close();
            return;
        }
        // There was a problem with the transfer and the server
        // has acknowledged it.  If we issue an "ABORT" now, the user
        // would get the "226" for the abort and think the xfer was
        // complete, thus, don't do anything here, just return
    }

    wxFTP *m_ftp;

    wxDECLARE_NO_COPY_CLASS(wxInputFTPStream);
};

class wxOutputFTPStream : public wxSocketOutputStream
{
public:
    wxOutputFTPStream(wxFTP *ftp_clt, wxSocketBase *sock)
        : wxSocketOutputStream(*sock), m_ftp(ftp_clt)
    {
    }

    virtual ~wxOutputFTPStream(void)
    {
        if ( IsOk() )
        {
            // close data connection first, this will generate "transfer
            // completed" reply
            delete m_o_socket;

            // read this reply
            m_ftp->GetResult(); // save result so user can get to it

            m_ftp->m_streaming = false;
        }
        else
        {
            // abort data connection first
            m_ftp->Abort();

            // and close it after
            delete m_o_socket;
        }
    }

    wxFTP *m_ftp;

    wxDECLARE_NO_COPY_CLASS(wxOutputFTPStream);
};

wxInputStream *wxFTP::GetInputStream(const wxString& path)
{
    if ( ( m_currentTransfermode == NONE ) && !SetTransferMode(BINARY) )
    {
        m_lastError = wxPROTO_CONNERR;
        return NULL;
    }

    wxSocketBase *sock = GetPort();

    if ( !sock )
    {
        m_lastError = wxPROTO_NETERR;
        return NULL;
    }

    wxString tmp_str = wxT("RETR ") + wxURI::Unescape(path);
    if ( !CheckCommand(tmp_str, '1') )
    {
        delete sock;

        return NULL;
    }

    sock = AcceptIfActive(sock);
    if ( !sock )
    {
        m_lastError = wxPROTO_CONNERR;
        return NULL;
    }

    m_streaming = true;

    wxInputFTPStream *in_stream = new wxInputFTPStream(this, sock);

    m_lastError = wxPROTO_NOERR;
    return in_stream;
}

wxOutputStream *wxFTP::GetOutputStream(const wxString& path)
{
    if ( ( m_currentTransfermode == NONE ) && !SetTransferMode(BINARY) )
    {
        m_lastError = wxPROTO_CONNERR;
        return NULL;
    }

    wxSocketBase *sock = GetPort();

    wxString tmp_str = wxT("STOR ") + path;
    if ( !CheckCommand(tmp_str, '1') )
    {
        delete sock;

        return NULL;
    }

    sock = AcceptIfActive(sock);

    m_streaming = true;

    m_lastError = wxPROTO_NOERR;
    return new wxOutputFTPStream(this, sock);
}

// ----------------------------------------------------------------------------
// FTP directory listing
// ----------------------------------------------------------------------------

bool wxFTP::GetList(wxArrayString& files,
                    const wxString& wildcard,
                    bool details)
{
    wxSocketBase *sock = GetPort();
    if (!sock) {
        m_lastError = wxPROTO_NETERR;
        return false;
    }

    // NLST : List of Filenames (including Directory's !)
    // LIST : depending on BS of FTP-Server
    //        - Unix    : result like "ls" command
    //        - Windows : like "dir" command
    //        - others  : ?
    wxString line(details ? wxT("LIST") : wxT("NLST"));
    if ( !wildcard.empty() )
    {
        line << wxT(' ') << wildcard;
    }

    if ( !CheckCommand(line, '1') )
    {
        m_lastError = wxPROTO_PROTERR;
        wxLogDebug(wxT("FTP 'LIST' command returned unexpected result from server"));
        delete sock;
        return false;
    }

    sock = AcceptIfActive(sock);
    if ( !sock ) {
        m_lastError = wxPROTO_CONNERR;
        return false;
    }

    files.Empty();
    while (ReadLine(sock, line) == wxPROTO_NOERR )
    {
        files.Add(line);
    }

    delete sock;

    // the file list should be terminated by "226 Transfer complete""
    m_lastError = wxPROTO_NOERR;
    return CheckResult('2');
}

bool wxFTP::FileExists(const wxString& fileName)
{
    // This function checks if the file specified in fileName exists in the
    // current dir. It does so by simply doing an NLST (via GetList).
    // If this succeeds (and the list is not empty) the file exists.

    bool retval = false;
    wxArrayString fileList;

    if ( GetList(fileList, fileName, false) )
    {
        // Some ftp-servers (Ipswitch WS_FTP Server 1.0.5 does this)
        // displays this behaviour when queried on a nonexistent file:
        // NLST this_file_does_not_exist
        // 150 Opening ASCII data connection for directory listing
        // (no data transferred)
        // 226 Transfer complete
        // Here wxFTP::GetList(...) will succeed but it will return an empty
        // list.
        retval = !fileList.IsEmpty();
    }

    return retval;
}

// ----------------------------------------------------------------------------
// FTP GetSize
// ----------------------------------------------------------------------------

int wxFTP::GetFileSize(const wxString& fileName)
{
    // return the filesize of the given file if possible
    // return -1 otherwise (predominantly if file doesn't exist
    // in current dir)

    int filesize = -1;

    // Check for existence of file via wxFTP::FileExists(...)
    if ( FileExists(fileName) )
    {
        wxString command;

        // First try "SIZE" command using BINARY(IMAGE) transfermode
        // Especially UNIX ftp-servers distinguish between the different
        // transfermodes and reports different filesizes accordingly.
        // The BINARY size is the interesting one: How much memory
        // will we need to hold this file?
        TransferMode oldTransfermode = m_currentTransfermode;
        SetTransferMode(BINARY);
        command << wxT("SIZE ") << fileName;

        bool ok = CheckCommand(command, '2');

        if ( ok )
        {
            // The answer should be one line: "213 <filesize>\n"
            // 213 is File Status (STD9)
            // "SIZE" is not described anywhere..? It works on most servers
            int statuscode;
            if ( wxSscanf(GetLastResult().c_str(), wxT("%i %i"),
                          &statuscode, &filesize) == 2 )
            {
                // We've gotten a good reply.
                ok = true;
            }
            else
            {
                // Something bad happened.. A "2yz" reply with no size
                // Fallback
                ok = false;
            }
        }

        // Set transfermode back to the original. Only the "SIZE"-command
        // is dependent on transfermode
        if ( oldTransfermode != NONE )
        {
            SetTransferMode(oldTransfermode);
        }

        // this is not a direct else clause.. The size command might return an
        // invalid "2yz" reply
        if ( !ok )
        {
            // The server didn't understand the "SIZE"-command or it
            // returned an invalid reply.
            // We now try to get details for the file with a "LIST"-command
            // and then parse the output from there..
            wxArrayString fileList;
            if ( GetList(fileList, fileName, true) )
            {
                if ( !fileList.IsEmpty() )
                {
                    // We _should_ only get one line in return, but just to be
                    // safe we run through the line(s) returned and look for a
                    // substring containing the name we are looking for. We
                    // stop the iteration at the first occurrence of the
                    // filename. The search is not case-sensitive.
                    const size_t numFiles = fileList.size();
                    size_t i;
                    for ( i = 0; i < fileList.GetCount(); i++ )
                    {
                        if ( fileList[i].Upper().Contains(fileName.Upper()) )
                            break;
                    }

                    if ( i != numFiles )
                    {
                        // The index i points to the first occurrence of
                        // fileName in the array Now we have to find out what
                        // format the LIST has returned. There are two
                        // "schools": Unix-like
                        //
                        // '-rw-rw-rw- owner group size month day time filename'
                        //
                        // or Windows-like
                        //
                        // 'date size filename'

                        // check if the first character is '-'. This would
                        // indicate Unix-style (this also limits this function
                        // to searching for files, not directories)
                        if ( fileList[i].Mid(0, 1) == wxT("-") )
                        {

                            if ( wxSscanf(fileList[i].c_str(),
                                          wxT("%*s %*s %*s %*s %i %*s %*s %*s %*s"),
                                          &filesize) != 9 )
                            {
                                // Hmm... Invalid response
                                wxLogDebug(wxT("Invalid LIST response"));
                            }
                        }
                        else // Windows-style response (?)
                        {
                            if ( wxSscanf(fileList[i].c_str(),
                                          wxT("%*s %*s %i %*s"),
                                          &filesize) != 4 )
                            {
                                // something bad happened..?
                                wxLogDebug(wxT("Invalid or unknown LIST response"));
                            }
                        }
                    }
                }
            }
        }
    }

    // filesize might still be -1 when exiting
    return filesize;
}

#endif // wxUSE_PROTOCOL_FTP

