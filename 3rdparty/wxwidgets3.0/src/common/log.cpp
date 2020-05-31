/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/log.cpp
// Purpose:     Assorted wxLogXXX functions, and wxLog (sink for logs)
// Author:      Vadim Zeitlin
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
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

#if wxUSE_LOG

// wxWidgets
#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/app.h"
    #include "wx/arrstr.h"
    #include "wx/intl.h"
    #include "wx/string.h"
    #include "wx/utils.h"
#endif //WX_PRECOMP

#include "wx/apptrait.h"
#include "wx/datetime.h"
#include "wx/file.h"
#include "wx/msgout.h"
#include "wx/textfile.h"
#include "wx/thread.h"
#include "wx/private/threadinfo.h"
#include "wx/crt.h"
#include "wx/vector.h"

// other standard headers
#ifndef __WXWINCE__
#include <errno.h>
#endif

#include <stdlib.h>

#ifndef __WXWINCE__
#include <time.h>
#else
#include "wx/msw/wince/time.h"
#endif

#if defined(__WINDOWS__)
    #include "wx/msw/private.h" // includes windows.h
#endif

#undef wxLOG_COMPONENT
const char *wxLOG_COMPONENT = "";

// this macro allows to define an object which will be initialized before any
// other function in this file is called: this is necessary to allow log
// functions to be used during static initialization (this is not advisable
// anyhow but we should at least try to not crash) and to also ensure that they
// are initialized by the time static initialization is done, i.e. before any
// threads are created hopefully
//
// the net effect of all this is that you can use Get##name() function to
// access the object without worrying about it being not initialized
//
// see also WX_DEFINE_GLOBAL_CONV2() in src/common/strconv.cpp
#define WX_DEFINE_GLOBAL_VAR(type, name)                                      \
    inline type& Get##name()                                                  \
    {                                                                         \
        static type s_##name;                                                 \
        return s_##name;                                                      \
    }                                                                         \
                                                                              \
    type *gs_##name##Ptr = &Get##name()

#if wxUSE_THREADS

namespace
{

// contains messages logged by the other threads and waiting to be shown until
// Flush() is called in the main one
typedef wxVector<wxLogRecord> wxLogRecords;
wxLogRecords gs_bufferedLogRecords;

#define WX_DEFINE_LOG_CS(name) WX_DEFINE_GLOBAL_VAR(wxCriticalSection, name##CS)

// this critical section is used for buffering the messages from threads other
// than main, i.e. it protects all accesses to gs_bufferedLogRecords above
WX_DEFINE_LOG_CS(BackgroundLog);

// this one is used for protecting TraceMasks() from concurrent access
WX_DEFINE_LOG_CS(TraceMask);

// and this one is used for GetComponentLevels()
WX_DEFINE_LOG_CS(Levels);

} // anonymous namespace

#endif // wxUSE_THREADS

// ----------------------------------------------------------------------------
// non member functions
// ----------------------------------------------------------------------------

// define this to enable wrapping of log messages
//#define LOG_PRETTY_WRAP

#ifdef  LOG_PRETTY_WRAP
  static void wxLogWrap(FILE *f, const char *pszPrefix, const char *psz);
#endif

// ----------------------------------------------------------------------------
// module globals
// ----------------------------------------------------------------------------

namespace
{

// this struct is used to store information about the previous log message used
// by OnLog() to (optionally) avoid logging multiple copies of the same message
struct PreviousLogInfo
{
    PreviousLogInfo()
    {
        numRepeated = 0;
    }


    // previous message itself
    wxString msg;

    // its level
    wxLogLevel level;

    // other information about it
    wxLogRecordInfo info;

    // the number of times it was already repeated
    unsigned numRepeated;
};

PreviousLogInfo gs_prevLog;


// map containing all components for which log level was explicitly set
//
// NB: all accesses to it must be protected by GetLevelsCS() critical section
WX_DEFINE_GLOBAL_VAR(wxStringToNumHashMap, ComponentLevels);

// ----------------------------------------------------------------------------
// wxLogOutputBest: wxLog wrapper around wxMessageOutputBest
// ----------------------------------------------------------------------------

class wxLogOutputBest : public wxLog
{
public:
    wxLogOutputBest() { }

protected:
    virtual void DoLogText(const wxString& msg)
    {
        wxMessageOutputBest().Output(msg);
    }

private:
    wxDECLARE_NO_COPY_CLASS(wxLogOutputBest);
};

} // anonymous namespace

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// helper global functions
// ----------------------------------------------------------------------------

void wxSafeShowMessage(const wxString& title, const wxString& text)
{
#ifdef __WINDOWS__
    ::MessageBox(NULL, text.t_str(), title.t_str(), MB_OK | MB_ICONSTOP);
#else
    wxFprintf(stderr, wxS("%s: %s\n"), title.c_str(), text.c_str());
    fflush(stderr);
#endif
}

// ----------------------------------------------------------------------------
// wxLogFormatter class implementation
// ----------------------------------------------------------------------------

wxString
wxLogFormatter::Format(wxLogLevel level,
                       const wxString& msg,
                       const wxLogRecordInfo& info) const
{
    wxString prefix;

    // don't time stamp debug messages under MSW as debug viewers usually
    // already have an option to do it
#ifdef __WINDOWS__
    if ( level != wxLOG_Debug && level != wxLOG_Trace )
#endif // __WINDOWS__
        prefix = FormatTime(info.timestamp);

    switch ( level )
    {
    case wxLOG_Error:
        prefix += _("Error: ");
        break;

    case wxLOG_Warning:
        prefix += _("Warning: ");
        break;

        // don't prepend "debug/trace" prefix under MSW as it goes to the debug
        // window anyhow and so can't be confused with something else
#ifndef __WINDOWS__
    case wxLOG_Debug:
        // this prefix (as well as the one below) is intentionally not
        // translated as nobody translates debug messages anyhow
        prefix += "Debug: ";
        break;

    case wxLOG_Trace:
        prefix += "Trace: ";
        break;
#endif // !__WINDOWS__
    }

    return prefix + msg;
}

wxString
wxLogFormatter::FormatTime(time_t t) const
{
    wxString str;
    wxLog::TimeStamp(&str, t);

    return str;
}


// ----------------------------------------------------------------------------
// wxLog class implementation
// ----------------------------------------------------------------------------

unsigned wxLog::LogLastRepeatIfNeeded()
{
    const unsigned count = gs_prevLog.numRepeated;

    if ( gs_prevLog.numRepeated )
    {
        wxString msg;
#if wxUSE_INTL
        if ( gs_prevLog.numRepeated == 1 )
        {
            // We use a separate message for this case as "repeated 1 time"
            // looks somewhat strange.
            msg = _("The previous message repeated once.");
        }
        else
        {
            // Notice that we still use wxPLURAL() to ensure that multiple
            // numbers of times are correctly formatted, even though we never
            // actually use the singular string.
            msg.Printf(wxPLURAL("The previous message repeated %u time.",
                                "The previous message repeated %u times.",
                                gs_prevLog.numRepeated),
                       gs_prevLog.numRepeated);
        }
#else
        msg.Printf(wxS("The previous message was repeated %u time(s)."),
                   gs_prevLog.numRepeated);
#endif
        gs_prevLog.numRepeated = 0;
        gs_prevLog.msg.clear();
        DoLogRecord(gs_prevLog.level, msg, gs_prevLog.info);
    }

    return count;
}

wxLog::~wxLog()
{
    // Flush() must be called before destroying the object as otherwise some
    // messages could be lost
    if ( gs_prevLog.numRepeated )
    {
        wxMessageOutputDebug().Printf
        (
#if wxUSE_INTL
            wxPLURAL
            (
                "Last repeated message (\"%s\", %u time) wasn't output",
                "Last repeated message (\"%s\", %u times) wasn't output",
                gs_prevLog.numRepeated
            ),
#else
            wxS("Last repeated message (\"%s\", %u time(s)) wasn't output"),
#endif
            gs_prevLog.msg,
            gs_prevLog.numRepeated
        );
    }

    delete m_formatter;
}

// ----------------------------------------------------------------------------
// wxLog logging functions
// ----------------------------------------------------------------------------

/* static */
void
wxLog::OnLog(wxLogLevel level, const wxString& msg, time_t t)
{
    wxLogRecordInfo info;
    info.timestamp = t;
#if wxUSE_THREADS
    info.threadId = wxThread::GetCurrentId();
#endif // wxUSE_THREADS

    OnLog(level, msg, info);
}

/* static */
void
wxLog::OnLog(wxLogLevel level,
             const wxString& msg,
             const wxLogRecordInfo& info)
{
    // fatal errors can't be suppressed nor handled by the custom log target
    // and always terminate the program
    if ( level == wxLOG_FatalError )
    {
        wxSafeShowMessage(wxS("Fatal Error"), msg);

        wxAbort();
    }

    wxLog *logger;

#if wxUSE_THREADS
    if ( !wxThread::IsMain() )
    {
        logger = wxThreadInfo.logger;
        if ( !logger )
        {
            if ( ms_pLogger )
            {
                // buffer the messages until they can be shown from the main
                // thread
                wxCriticalSectionLocker lock(GetBackgroundLogCS());

                gs_bufferedLogRecords.push_back(wxLogRecord(level, msg, info));

                // ensure that our Flush() will be called soon
                wxWakeUpIdle();
            }
            //else: we don't have any logger at all, there is no need to log
            //      anything

            return;
        }
        //else: we have a thread-specific logger, we can send messages to it
        //      directly
    }
    else
#endif // wxUSE_THREADS
    {
        logger = GetMainThreadActiveTarget();
        if ( !logger )
            return;
    }

    logger->CallDoLogNow(level, msg, info);
}

void
wxLog::CallDoLogNow(wxLogLevel level,
                    const wxString& msg,
                    const wxLogRecordInfo& info)
{
    if ( GetRepetitionCounting() )
    {
        if ( msg == gs_prevLog.msg )
        {
            gs_prevLog.numRepeated++;

            // nothing else to do, in particular, don't log the
            // repeated message
            return;
        }

        LogLastRepeatIfNeeded();

        // reset repetition counter for a new message
        gs_prevLog.msg = msg;
        gs_prevLog.level = level;
        gs_prevLog.info = info;
    }

    // handle extra data which may be passed to us by wxLogXXX()
    wxString prefix, suffix;
    wxUIntPtr num = 0;
    if ( info.GetNumValue(wxLOG_KEY_SYS_ERROR_CODE, &num) )
    {
        const long err = static_cast<long>(num);

        suffix.Printf(_(" (error %ld: %s)"), err, wxSysErrorMsg(err));
    }

#if wxUSE_LOG_TRACE
    wxString str;
    if ( level == wxLOG_Trace && info.GetStrValue(wxLOG_KEY_TRACE_MASK, &str) )
    {
        prefix = "(" + str + ") ";
    }
#endif // wxUSE_LOG_TRACE

    DoLogRecord(level, prefix + msg + suffix, info);
}

void wxLog::DoLogRecord(wxLogLevel level,
                             const wxString& msg,
                             const wxLogRecordInfo& info)
{
#if WXWIN_COMPATIBILITY_2_8
    // call the old DoLog() to ensure that existing custom log classes still
    // work
    //
    // as the user code could have defined it as either taking "const char *"
    // (in ANSI build) or "const wxChar *" (in ANSI/Unicode), we have no choice
    // but to call both of them
    DoLog(level, (const char*)msg.mb_str(), info.timestamp);
    DoLog(level, (const wchar_t*)msg.wc_str(), info.timestamp);
#else // !WXWIN_COMPATIBILITY_2_8
    wxUnusedVar(info);
#endif // WXWIN_COMPATIBILITY_2_8/!WXWIN_COMPATIBILITY_2_8

    // Use wxLogFormatter to format the message
    DoLogTextAtLevel(level, m_formatter->Format (level, msg, info));
}

void wxLog::DoLogTextAtLevel(wxLogLevel level, const wxString& msg)
{
    // we know about debug messages (because using wxMessageOutputDebug is the
    // right thing to do in 99% of all cases and also for compatibility) but
    // anything else needs to be handled in the derived class
    if ( level == wxLOG_Debug || level == wxLOG_Trace )
    {
        wxMessageOutputDebug().Output(msg + wxS('\n'));
    }
    else
    {
        DoLogText(msg);
    }
}

void wxLog::DoLogText(const wxString& WXUNUSED(msg))
{
    // in 2.8-compatible build the derived class might override DoLog() or
    // DoLogString() instead so we can't have this assert there
#if !WXWIN_COMPATIBILITY_2_8
    wxFAIL_MSG( "must be overridden if it is called" );
#endif // WXWIN_COMPATIBILITY_2_8
}

#if WXWIN_COMPATIBILITY_2_8

void wxLog::DoLog(wxLogLevel WXUNUSED(level), const char *szString, time_t t)
{
    DoLogString(szString, t);
}

void wxLog::DoLog(wxLogLevel WXUNUSED(level), const wchar_t *wzString, time_t t)
{
    DoLogString(wzString, t);
}

#endif // WXWIN_COMPATIBILITY_2_8

// ----------------------------------------------------------------------------
// wxLog active target management
// ----------------------------------------------------------------------------

wxLog *wxLog::GetActiveTarget()
{
#if wxUSE_THREADS
    if ( !wxThread::IsMain() )
    {
        // check if we have a thread-specific log target
        wxLog * const logger = wxThreadInfo.logger;

        // the code below should be only executed for the main thread as
        // CreateLogTarget() is not meant for auto-creating log targets for
        // worker threads so skip it in any case
        return logger ? logger : ms_pLogger;
    }
#endif // wxUSE_THREADS

    return GetMainThreadActiveTarget();
}

/* static */
wxLog *wxLog::GetMainThreadActiveTarget()
{
    if ( ms_bAutoCreate && ms_pLogger == NULL ) {
        // prevent infinite recursion if someone calls wxLogXXX() from
        // wxApp::CreateLogTarget()
        static bool s_bInGetActiveTarget = false;
        if ( !s_bInGetActiveTarget ) {
            s_bInGetActiveTarget = true;

            // ask the application to create a log target for us
            if ( wxTheApp != NULL )
                ms_pLogger = wxTheApp->GetTraits()->CreateLogTarget();
            else
                ms_pLogger = new wxLogOutputBest;

            s_bInGetActiveTarget = false;

            // do nothing if it fails - what can we do?
        }
    }

    return ms_pLogger;
}

wxLog *wxLog::SetActiveTarget(wxLog *pLogger)
{
    if ( ms_pLogger != NULL ) {
        // flush the old messages before changing because otherwise they might
        // get lost later if this target is not restored
        ms_pLogger->Flush();
    }

    wxLog *pOldLogger = ms_pLogger;
    ms_pLogger = pLogger;

    return pOldLogger;
}

#if wxUSE_THREADS
/* static */
wxLog *wxLog::SetThreadActiveTarget(wxLog *logger)
{
    wxASSERT_MSG( !wxThread::IsMain(), "use SetActiveTarget() for main thread" );

    wxLog * const oldLogger = wxThreadInfo.logger;
    if ( oldLogger )
        oldLogger->Flush();

    wxThreadInfo.logger = logger;

    return oldLogger;
}
#endif // wxUSE_THREADS

void wxLog::DontCreateOnDemand()
{
    ms_bAutoCreate = false;

    // this is usually called at the end of the program and we assume that it
    // is *always* called at the end - so we free memory here to avoid false
    // memory leak reports from wxWin  memory tracking code
    ClearTraceMasks();
}

void wxLog::DoCreateOnDemand()
{
    ms_bAutoCreate = true;
}

// ----------------------------------------------------------------------------
// wxLog components levels
// ----------------------------------------------------------------------------

/* static */
void wxLog::SetComponentLevel(const wxString& component, wxLogLevel level)
{
    if ( component.empty() )
    {
        SetLogLevel(level);
    }
    else
    {
        wxCRIT_SECT_LOCKER(lock, GetLevelsCS());

        GetComponentLevels()[component] = level;
    }
}

/* static */
wxLogLevel wxLog::GetComponentLevel(wxString component)
{
    wxCRIT_SECT_LOCKER(lock, GetLevelsCS());

    const wxStringToNumHashMap& componentLevels = GetComponentLevels();
    while ( !component.empty() )
    {
        wxStringToNumHashMap::const_iterator
            it = componentLevels.find(component);
        if ( it != componentLevels.end() )
            return static_cast<wxLogLevel>(it->second);

        component = component.BeforeLast('/');
    }

    return GetLogLevel();
}

// ----------------------------------------------------------------------------
// wxLog trace masks
// ----------------------------------------------------------------------------

namespace
{

// because IsAllowedTraceMask() may be called during static initialization
// (this is not recommended but it may still happen, see #11592) we can't use a
// simple static variable which might be not initialized itself just yet to
// store the trace masks, but need this accessor function which will ensure
// that the variable is always correctly initialized before being accessed
//
// notice that this doesn't make accessing it MT-safe, of course, you need to
// serialize accesses to it using GetTraceMaskCS() for this
wxArrayString& TraceMasks()
{
    static wxArrayString s_traceMasks;

    return s_traceMasks;
}

} // anonymous namespace

/* static */ const wxArrayString& wxLog::GetTraceMasks()
{
    // because of this function signature (it returns a reference, not the
    // object), it is inherently MT-unsafe so there is no need to acquire the
    // lock here anyhow

    return TraceMasks();
}

void wxLog::AddTraceMask(const wxString& str)
{
    wxCRIT_SECT_LOCKER(lock, GetTraceMaskCS());

    TraceMasks().push_back(str);
}

void wxLog::RemoveTraceMask(const wxString& str)
{
    wxCRIT_SECT_LOCKER(lock, GetTraceMaskCS());

    int index = TraceMasks().Index(str);
    if ( index != wxNOT_FOUND )
        TraceMasks().RemoveAt((size_t)index);
}

void wxLog::ClearTraceMasks()
{
    wxCRIT_SECT_LOCKER(lock, GetTraceMaskCS());

    TraceMasks().Clear();
}

/*static*/ bool wxLog::IsAllowedTraceMask(const wxString& mask)
{
    wxCRIT_SECT_LOCKER(lock, GetTraceMaskCS());

    const wxArrayString& masks = GetTraceMasks();
    for ( wxArrayString::const_iterator it = masks.begin(),
                                        en = masks.end();
          it != en;
          ++it )
    {
        if ( *it == mask)
            return true;
    }

    return false;
}

// ----------------------------------------------------------------------------
// wxLog miscellaneous other methods
// ----------------------------------------------------------------------------

#if wxUSE_DATETIME

void wxLog::TimeStamp(wxString *str)
{
    if ( !ms_timestamp.empty() )
    {
        *str = wxDateTime::UNow().Format(ms_timestamp);
        *str += wxS(": ");
    }
}

void wxLog::TimeStamp(wxString *str, time_t t)
{
    if ( !ms_timestamp.empty() )
    {
        *str = wxDateTime(t).Format(ms_timestamp);
        *str += wxS(": ");
    }
}

#else // !wxUSE_DATETIME

void wxLog::TimeStamp(wxString*)
{
}

void wxLog::TimeStamp(wxString*, time_t)
{
}

#endif // wxUSE_DATETIME/!wxUSE_DATETIME

#if wxUSE_THREADS

void wxLog::FlushThreadMessages()
{
    // check if we have queued messages from other threads
    wxLogRecords bufferedLogRecords;

    {
        wxCriticalSectionLocker lock(GetBackgroundLogCS());
        bufferedLogRecords.swap(gs_bufferedLogRecords);

        // release the lock now to not keep it while we are logging the
        // messages below, allowing background threads to run
    }

    if ( !bufferedLogRecords.empty() )
    {
        for ( wxLogRecords::const_iterator it = bufferedLogRecords.begin();
              it != bufferedLogRecords.end();
              ++it )
        {
            CallDoLogNow(it->level, it->msg, it->info);
        }
    }
}

/* static */
bool wxLog::IsThreadLoggingEnabled()
{
    return !wxThreadInfo.loggingDisabled;
}

/* static */
bool wxLog::EnableThreadLogging(bool enable)
{
    const bool wasEnabled = !wxThreadInfo.loggingDisabled;
    wxThreadInfo.loggingDisabled = !enable;
    return wasEnabled;
}

#endif // wxUSE_THREADS

wxLogFormatter *wxLog::SetFormatter(wxLogFormatter* formatter)
{
    wxLogFormatter* formatterOld = m_formatter;
    m_formatter = formatter ? formatter : new wxLogFormatter;

    return formatterOld;
}

void wxLog::Flush()
{
    LogLastRepeatIfNeeded();
}

/* static */
void wxLog::FlushActive()
{
    if ( ms_suspendCount )
        return;

    wxLog * const log = GetActiveTarget();
    if ( log )
    {
#if wxUSE_THREADS
        if ( wxThread::IsMain() )
            log->FlushThreadMessages();
#endif // wxUSE_THREADS

        log->Flush();
    }
}

// ----------------------------------------------------------------------------
// wxLogBuffer implementation
// ----------------------------------------------------------------------------

void wxLogBuffer::Flush()
{
    wxLog::Flush();

    if ( !m_str.empty() )
    {
        wxMessageOutputBest out;
        out.Printf(wxS("%s"), m_str.c_str());
        m_str.clear();
    }
}

void wxLogBuffer::DoLogTextAtLevel(wxLogLevel level, const wxString& msg)
{
    // don't put debug messages in the buffer, we don't want to show
    // them to the user in a msg box, log them immediately
    switch ( level )
    {
        case wxLOG_Debug:
        case wxLOG_Trace:
            wxLog::DoLogTextAtLevel(level, msg);
            break;

        default:
            m_str << msg << wxS("\n");
    }
}

// ----------------------------------------------------------------------------
// wxLogStderr class implementation
// ----------------------------------------------------------------------------

wxLogStderr::wxLogStderr(FILE *fp)
{
    if ( fp == NULL )
        m_fp = stderr;
    else
        m_fp = fp;
}

void wxLogStderr::DoLogText(const wxString& msg)
{
    // First send it to stderr, even if we don't have it (e.g. in a Windows GUI
    // application under) it's not a problem to try to use it and it's easier
    // than determining whether we do have it or not.
    wxMessageOutputStderr(m_fp).Output(msg);

    // under GUI systems such as Windows or Mac, programs usually don't have
    // stderr at all, so show the messages also somewhere else, typically in
    // the debugger window so that they go at least somewhere instead of being
    // simply lost
    if ( m_fp == stderr )
    {
        wxAppTraits *traits = wxTheApp ? wxTheApp->GetTraits() : NULL;
        if ( traits && !traits->HasStderr() )
        {
            wxMessageOutputDebug().Output(msg + wxS('\n'));
        }
    }
}

// ----------------------------------------------------------------------------
// wxLogStream implementation
// ----------------------------------------------------------------------------

#if wxUSE_STD_IOSTREAM
#include "wx/ioswrap.h"
wxLogStream::wxLogStream(wxSTD ostream *ostr)
{
    if ( ostr == NULL )
        m_ostr = &wxSTD cerr;
    else
        m_ostr = ostr;
}

void wxLogStream::DoLogText(const wxString& msg)
{
    (*m_ostr) << msg << wxSTD endl;
}
#endif // wxUSE_STD_IOSTREAM

// ----------------------------------------------------------------------------
// wxLogChain
// ----------------------------------------------------------------------------

wxLogChain::wxLogChain(wxLog *logger)
{
    m_bPassMessages = true;

    m_logNew = logger;

    // Notice that we use GetActiveTarget() here instead of directly calling
    // SetActiveTarget() to trigger wxLog auto-creation: if we're created as
    // the first logger, we should still chain with the standard, implicit and
    // possibly still not created standard logger instead of disabling normal
    // logging entirely.
    m_logOld = wxLog::GetActiveTarget();
    wxLog::SetActiveTarget(this);
}

wxLogChain::~wxLogChain()
{
    wxLog::SetActiveTarget(m_logOld);

    if ( m_logNew != this )
        delete m_logNew;
}

void wxLogChain::SetLog(wxLog *logger)
{
    if ( m_logNew != this )
        delete m_logNew;

    m_logNew = logger;
}

void wxLogChain::Flush()
{
    if ( m_logOld )
        m_logOld->Flush();

    // be careful to avoid infinite recursion
    if ( m_logNew && m_logNew != this )
        m_logNew->Flush();
}

void wxLogChain::DoLogRecord(wxLogLevel level,
                             const wxString& msg,
                             const wxLogRecordInfo& info)
{
    // let the previous logger show it
    if ( m_logOld && IsPassingMessages() )
        m_logOld->LogRecord(level, msg, info);

    // and also send it to the new one
    if ( m_logNew )
    {
        // don't call m_logNew->LogRecord() to avoid infinite recursion when
        // m_logNew is this object itself
        if ( m_logNew != this )
            m_logNew->LogRecord(level, msg, info);
        else
            wxLog::DoLogRecord(level, msg, info);
    }
}

#ifdef __VISUALC__
    // "'this' : used in base member initializer list" - so what?
    #pragma warning(disable:4355)
#endif // VC++

// ----------------------------------------------------------------------------
// wxLogInterposer
// ----------------------------------------------------------------------------

wxLogInterposer::wxLogInterposer()
                : wxLogChain(this)
{
}

// ----------------------------------------------------------------------------
// wxLogInterposerTemp
// ----------------------------------------------------------------------------

wxLogInterposerTemp::wxLogInterposerTemp()
                : wxLogChain(this)
{
    DetachOldLog();
}

#ifdef __VISUALC__
    #pragma warning(default:4355)
#endif // VC++

// ============================================================================
// Global functions/variables
// ============================================================================

// ----------------------------------------------------------------------------
// static variables
// ----------------------------------------------------------------------------

bool            wxLog::ms_bRepetCounting = false;

wxLog          *wxLog::ms_pLogger      = NULL;
bool            wxLog::ms_doLog        = true;
bool            wxLog::ms_bAutoCreate  = true;
bool            wxLog::ms_bVerbose     = false;

wxLogLevel      wxLog::ms_logLevel     = wxLOG_Max;  // log everything by default

size_t          wxLog::ms_suspendCount = 0;

wxString        wxLog::ms_timestamp(wxS("%X"));  // time only, no date

#if WXWIN_COMPATIBILITY_2_8
wxTraceMask     wxLog::ms_ulTraceMask  = (wxTraceMask)0;
#endif // wxDEBUG_LEVEL

// ----------------------------------------------------------------------------
// stdout error logging helper
// ----------------------------------------------------------------------------

// helper function: wraps the message and justifies it under given position
// (looks more pretty on the terminal). Also adds newline at the end.
//
// TODO this is now disabled until I find a portable way of determining the
//      terminal window size (ok, I found it but does anybody really cares?)
#ifdef LOG_PRETTY_WRAP
static void wxLogWrap(FILE *f, const char *pszPrefix, const char *psz)
{
    size_t nMax = 80; // FIXME
    size_t nStart = strlen(pszPrefix);
    fputs(pszPrefix, f);

    size_t n;
    while ( *psz != '\0' ) {
        for ( n = nStart; (n < nMax) && (*psz != '\0'); n++ )
            putc(*psz++, f);

        // wrapped?
        if ( *psz != '\0' ) {
            /*putc('\n', f);*/
            for ( n = 0; n < nStart; n++ )
                putc(' ', f);

            // as we wrapped, squeeze all white space
            while ( isspace(*psz) )
                psz++;
        }
    }

    putc('\n', f);
}
#endif  //LOG_PRETTY_WRAP

// ----------------------------------------------------------------------------
// error code/error message retrieval functions
// ----------------------------------------------------------------------------

// get error code from syste
unsigned long wxSysErrorCode()
{
#if defined(__WINDOWS__) && !defined(__WXMICROWIN__)
    return ::GetLastError();
#else   //Unix
    return errno;
#endif  //Win/Unix
}

// get error message from system
const wxChar *wxSysErrorMsg(unsigned long nErrCode)
{
    if ( nErrCode == 0 )
        nErrCode = wxSysErrorCode();

#if defined(__WINDOWS__) && !defined(__WXMICROWIN__)
    static wxChar s_szBuf[1024];

    // get error message from system
    LPVOID lpMsgBuf;
    if ( ::FormatMessage
         (
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            nErrCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&lpMsgBuf,
            0,
            NULL
         ) == 0 )
    {
        // if this happens, something is seriously wrong, so don't use _() here
        // for safety
        wxSprintf(s_szBuf, wxS("unknown error %lx"), nErrCode);
        return s_szBuf;
    }


    // copy it to our buffer and free memory
    // Crashes on SmartPhone (FIXME)
#if !defined(__SMARTPHONE__) /* of WinCE */
    if( lpMsgBuf != 0 )
    {
        wxStrlcpy(s_szBuf, (const wxChar *)lpMsgBuf, WXSIZEOF(s_szBuf));

        LocalFree(lpMsgBuf);

        // returned string is capitalized and ended with '\r\n' - bad
        s_szBuf[0] = (wxChar)wxTolower(s_szBuf[0]);
        size_t len = wxStrlen(s_szBuf);
        if ( len > 0 ) {
            // truncate string
            if ( s_szBuf[len - 2] == wxS('\r') )
                s_szBuf[len - 2] = wxS('\0');
        }
    }
    else
#endif // !__SMARTPHONE__
    {
        s_szBuf[0] = wxS('\0');
    }

    return s_szBuf;
#else // !__WINDOWS__
    #if wxUSE_UNICODE
        static wchar_t s_wzBuf[1024];
        wxConvCurrent->MB2WC(s_wzBuf, strerror((int)nErrCode),
                             WXSIZEOF(s_wzBuf) - 1);
        return s_wzBuf;
    #else
        return strerror((int)nErrCode);
    #endif
#endif  // __WINDOWS__/!__WINDOWS__
}

#endif // wxUSE_LOG
