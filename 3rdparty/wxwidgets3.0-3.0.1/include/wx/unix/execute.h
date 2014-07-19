/////////////////////////////////////////////////////////////////////////////
// Name:        wx/unix/execute.h
// Purpose:     private details of wxExecute() implementation
// Author:      Vadim Zeitlin
// Copyright:   (c) 1998 Robert Roebling, Julian Smart, Vadim Zeitlin
//              (c) 2013 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIX_EXECUTE_H
#define _WX_UNIX_EXECUTE_H

#include "wx/app.h"
#include "wx/hashmap.h"
#include "wx/process.h"

#if wxUSE_STREAMS
    #include "wx/unix/pipe.h"
    #include "wx/private/streamtempinput.h"
#endif

class wxEventLoopBase;

// Information associated with a running child process.
class wxExecuteData
{
public:
    wxExecuteData()
    {
        flags =
        pid = 0;
        exitcode = -1;

        process = NULL;

        syncEventLoop = NULL;

#if wxUSE_STREAMS
        fdOut =
        fdErr = wxPipe::INVALID_FD;
#endif // wxUSE_STREAMS
    }

    // This must be called in the parent process as soon as fork() returns to
    // update us with the effective child PID. It also ensures that we handle
    // SIGCHLD to be able to detect when this PID exits, so wxTheApp must be
    // available.
    void OnStart(int pid);

    // Called when the child process exits.
    void OnExit(int exitcode);

    // Return true if we should (or already did) redirect the child IO.
    bool IsRedirected() const { return process && process->IsRedirected(); }


    // wxExecute() flags
    int flags;

    // the pid of the child process
    int pid;

    // The exit code of the process, set once the child terminates.
    int exitcode;

    // the associated process object or NULL
    wxProcess *process;

    // Local event loop used to wait for the child process termination in
    // synchronous execution case. We can't create it ourselves as its exact
    // type depends on the application kind (console/GUI), so we rely on
    // wxAppTraits setting up this pointer to point to the appropriate object.
    wxEventLoopBase *syncEventLoop;

#if wxUSE_STREAMS
    // the input buffer bufOut is connected to stdout, this is why it is
    // called bufOut and not bufIn
    wxStreamTempInputBuffer bufOut,
                            bufErr;

    // the corresponding FDs, -1 if not redirected
    int fdOut,
        fdErr;
#endif // wxUSE_STREAMS


private:
    // SIGCHLD signal handler that checks whether any of the currently running
    // children have exited.
    static void OnSomeChildExited(int sig);

    // All currently running child processes indexed by their PID.
    //
    // Notice that the container doesn't own its elements.
    WX_DECLARE_HASH_MAP(int, wxExecuteData*, wxIntegerHash, wxIntegerEqual,
                        ChildProcessesData);
    static ChildProcessesData ms_childProcesses;

    wxDECLARE_NO_COPY_CLASS(wxExecuteData);
};

#endif // _WX_UNIX_EXECUTE_H
