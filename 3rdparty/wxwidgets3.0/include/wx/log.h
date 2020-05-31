/////////////////////////////////////////////////////////////////////////////
// Name:        wx/log.h
// Purpose:     Assorted wxLogXXX functions, and wxLog (sink for logs)
// Author:      Vadim Zeitlin
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_LOG_H_
#define _WX_LOG_H_

#include "wx/defs.h"
#include "wx/cpp.h"

// ----------------------------------------------------------------------------
// types
// ----------------------------------------------------------------------------

// NB: this is needed even if wxUSE_LOG == 0
typedef unsigned long wxLogLevel;

// the trace masks have been superseded by symbolic trace constants, they're
// for compatibility only and will be removed soon - do NOT use them
#if WXWIN_COMPATIBILITY_2_8
    #define wxTraceMemAlloc 0x0001  // trace memory allocation (new/delete)
    #define wxTraceMessages 0x0002  // trace window messages/X callbacks
    #define wxTraceResAlloc 0x0004  // trace GDI resource allocation
    #define wxTraceRefCount 0x0008  // trace various ref counting operations

    #ifdef  __WINDOWS__
        #define wxTraceOleCalls 0x0100  // OLE interface calls
    #endif

    typedef unsigned long wxTraceMask;
#endif // WXWIN_COMPATIBILITY_2_8

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/string.h"
#include "wx/strvararg.h"

// ----------------------------------------------------------------------------
// forward declarations
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_BASE wxObject;

#if wxUSE_GUI
    class WXDLLIMPEXP_FWD_CORE wxFrame;
#endif // wxUSE_GUI

#if wxUSE_LOG

#include "wx/arrstr.h"

#ifndef __WXWINCE__
    #include <time.h>   // for time_t
#endif

#include "wx/dynarray.h"
#include "wx/hashmap.h"

#if wxUSE_THREADS
    #include "wx/thread.h"
#endif // wxUSE_THREADS

// wxUSE_LOG_DEBUG enables the debug log messages
#ifndef wxUSE_LOG_DEBUG
    #if wxDEBUG_LEVEL
        #define wxUSE_LOG_DEBUG 1
    #else // !wxDEBUG_LEVEL
        #define wxUSE_LOG_DEBUG 0
    #endif
#endif

// wxUSE_LOG_TRACE enables the trace messages, they are disabled by default
#ifndef wxUSE_LOG_TRACE
    #if wxDEBUG_LEVEL
        #define wxUSE_LOG_TRACE 1
    #else // !wxDEBUG_LEVEL
        #define wxUSE_LOG_TRACE 0
    #endif
#endif // wxUSE_LOG_TRACE

// wxLOG_COMPONENT identifies the component which generated the log record and
// can be #define'd to a user-defined value when compiling the user code to use
// component-based filtering (see wxLog::SetComponentLevel())
#ifndef wxLOG_COMPONENT
    // this is a variable and not a macro in order to allow the user code to
    // just #define wxLOG_COMPONENT without #undef'ining it first
    extern WXDLLIMPEXP_DATA_BASE(const char *) wxLOG_COMPONENT;

    #ifdef WXBUILDING
        #define wxLOG_COMPONENT "wx"
    #endif
#endif

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// different standard log levels (you may also define your own)
enum wxLogLevelValues
{
    wxLOG_FatalError, // program can't continue, abort immediately
    wxLOG_Error,      // a serious error, user must be informed about it
    wxLOG_Warning,    // user is normally informed about it but may be ignored
    wxLOG_Message,    // normal message (i.e. normal output of a non GUI app)
    wxLOG_Status,     // informational: might go to the status line of GUI app
    wxLOG_Info,       // informational message (a.k.a. 'Verbose')
    wxLOG_Debug,      // never shown to the user, disabled in release mode
    wxLOG_Trace,      // trace messages are also only enabled in debug mode
    wxLOG_Progress,   // used for progress indicator (not yet)
    wxLOG_User = 100, // user defined levels start here
    wxLOG_Max = 10000
};

// symbolic trace masks - wxLogTrace("foo", "some trace message...") will be
// discarded unless the string "foo" has been added to the list of allowed
// ones with AddTraceMask()

#define wxTRACE_MemAlloc wxT("memalloc") // trace memory allocation (new/delete)
#define wxTRACE_Messages wxT("messages") // trace window messages/X callbacks
#define wxTRACE_ResAlloc wxT("resalloc") // trace GDI resource allocation
#define wxTRACE_RefCount wxT("refcount") // trace various ref counting operations

#ifdef  __WINDOWS__
    #define wxTRACE_OleCalls wxT("ole")  // OLE interface calls
#endif

#include "wx/iosfwrap.h"

// ----------------------------------------------------------------------------
// information about a log record, i.e. unit of log output
// ----------------------------------------------------------------------------

class wxLogRecordInfo
{
public:
    // default ctor creates an uninitialized object
    wxLogRecordInfo()
    {
        memset(this, 0, sizeof(*this));
    }

    // normal ctor, used by wxLogger specifies the location of the log
    // statement; its time stamp and thread id are set up here
    wxLogRecordInfo(const char *filename_,
                    int line_,
                    const char *func_,
                    const char *component_)
    {
        filename = filename_;
        func = func_;
        line = line_;
        component = component_;

        timestamp = time(NULL);

#if wxUSE_THREADS
        threadId = wxThread::GetCurrentId();
#endif // wxUSE_THREADS

        m_data = NULL;
    }

    // we need to define copy ctor and assignment operator because of m_data
    wxLogRecordInfo(const wxLogRecordInfo& other)
    {
        Copy(other);
    }

    wxLogRecordInfo& operator=(const wxLogRecordInfo& other)
    {
        if ( &other != this )
        {
            delete m_data;
            Copy(other);
        }

        return *this;
    }

    // dtor is non-virtual, this class is not meant to be derived from
    ~wxLogRecordInfo()
    {
        delete m_data;
    }


    // the file name and line number of the file where the log record was
    // generated, if available or NULL and 0 otherwise
    const char *filename;
    int line;

    // the name of the function where the log record was generated (may be NULL
    // if the compiler doesn't support __FUNCTION__)
    const char *func;

    // the name of the component which generated this message, may be NULL if
    // not set (i.e. wxLOG_COMPONENT not defined)
    const char *component;

    // time of record generation
    time_t timestamp;

#if wxUSE_THREADS
    // id of the thread which logged this record
    wxThreadIdType threadId;
#endif // wxUSE_THREADS


    // store an arbitrary value in this record context
    //
    // wxWidgets always uses keys starting with "wx.", e.g. "wx.sys_error"
    void StoreValue(const wxString& key, wxUIntPtr val)
    {
        if ( !m_data )
            m_data = new ExtraData;

        m_data->numValues[key] = val;
    }

    void StoreValue(const wxString& key, const wxString& val)
    {
        if ( !m_data )
            m_data = new ExtraData;

        m_data->strValues[key] = val;
    }


    // these functions retrieve the value of either numeric or string key,
    // return false if not found
    bool GetNumValue(const wxString& key, wxUIntPtr *val) const
    {
        if ( !m_data )
            return false;

        wxStringToNumHashMap::const_iterator it = m_data->numValues.find(key);
        if ( it == m_data->numValues.end() )
            return false;

        *val = it->second;

        return true;
    }

    bool GetStrValue(const wxString& key, wxString *val) const
    {
        if ( !m_data )
            return false;

        wxStringToStringHashMap::const_iterator it = m_data->strValues.find(key);
        if ( it == m_data->strValues.end() )
            return false;

        *val = it->second;

        return true;
    }

private:
    void Copy(const wxLogRecordInfo& other)
    {
        memcpy(this, &other, sizeof(*this));
        if ( other.m_data )
           m_data = new ExtraData(*other.m_data);
    }

    // extra data associated with the log record: this is completely optional
    // and can be used to pass information from the log function to the log
    // sink (e.g. wxLogSysError() uses this to pass the error code)
    struct ExtraData
    {
        wxStringToNumHashMap numValues;
        wxStringToStringHashMap strValues;
    };

    // NULL if not used
    ExtraData *m_data;
};

#define wxLOG_KEY_TRACE_MASK "wx.trace_mask"

// ----------------------------------------------------------------------------
// log record: a unit of log output
// ----------------------------------------------------------------------------

struct wxLogRecord
{
    wxLogRecord(wxLogLevel level_,
                const wxString& msg_,
                const wxLogRecordInfo& info_)
        : level(level_),
          msg(msg_),
          info(info_)
    {
    }

    wxLogLevel level;
    wxString msg;
    wxLogRecordInfo info;
};

// ----------------------------------------------------------------------------
// Derive from this class to customize format of log messages.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxLogFormatter
{
public:
    // Default constructor.
    wxLogFormatter() { }

    // Trivial but virtual destructor for the base class.
    virtual ~wxLogFormatter() { }


    // Override this method to implement custom formatting of the given log
    // record. The default implementation simply prepends a level-dependent
    // prefix to the message and optionally adds a time stamp.
    virtual wxString Format(wxLogLevel level,
                            const wxString& msg,
                            const wxLogRecordInfo& info) const;

protected:
    // Override this method to change just the time stamp formatting. It is
    // called by default Format() implementation.
    virtual wxString FormatTime(time_t t) const;
};


// ----------------------------------------------------------------------------
// derive from this class to redirect (or suppress, or ...) log messages
// normally, only a single instance of this class exists but it's not enforced
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxLog
{
public:
    // ctor
    wxLog() : m_formatter(new wxLogFormatter) { }

    // make dtor virtual for all derived classes
    virtual ~wxLog();


    // log messages selection
    // ----------------------

    // these functions allow to completely disable all log messages or disable
    // log messages at level less important than specified for the current
    // thread

    // is logging enabled at all now?
    static bool IsEnabled()
    {
#if wxUSE_THREADS
        if ( !wxThread::IsMain() )
            return IsThreadLoggingEnabled();
#endif // wxUSE_THREADS

        return ms_doLog;
    }

    // change the flag state, return the previous one
    static bool EnableLogging(bool enable = true)
    {
#if wxUSE_THREADS
        if ( !wxThread::IsMain() )
            return EnableThreadLogging(enable);
#endif // wxUSE_THREADS

        bool doLogOld = ms_doLog;
        ms_doLog = enable;
        return doLogOld;
    }

    // return the current global log level
    static wxLogLevel GetLogLevel() { return ms_logLevel; }

    // set global log level: messages with level > logLevel will not be logged
    static void SetLogLevel(wxLogLevel logLevel) { ms_logLevel = logLevel; }

    // set the log level for the given component
    static void SetComponentLevel(const wxString& component, wxLogLevel level);

    // return the effective log level for this component, falling back to
    // parent component and to the default global log level if necessary
    //
    // NB: component argument is passed by value and not const reference in an
    //     attempt to encourage compiler to avoid an extra copy: as we modify
    //     the component internally, we'd create one anyhow and like this it
    //     can be avoided if the string is a temporary anyhow
    static wxLogLevel GetComponentLevel(wxString component);


    // is logging of messages from this component enabled at this level?
    //
    // usually always called with wxLOG_COMPONENT as second argument
    static bool IsLevelEnabled(wxLogLevel level, wxString component)
    {
        return IsEnabled() && level <= GetComponentLevel(component);
    }


    // enable/disable messages at wxLOG_Verbose level (only relevant if the
    // current log level is greater or equal to it)
    //
    // notice that verbose mode can be activated by the standard command-line
    // '--verbose' option
    static void SetVerbose(bool bVerbose = true) { ms_bVerbose = bVerbose; }

    // check if verbose messages are enabled
    static bool GetVerbose() { return ms_bVerbose; }


    // message buffering
    // -----------------

    // flush shows all messages if they're not logged immediately (FILE
    // and iostream logs don't need it, but wxLogGui does to avoid showing
    // 17 modal dialogs one after another)
    virtual void Flush();

    // flush the active target if any and also output any pending messages from
    // background threads
    static void FlushActive();

    // only one sink is active at each moment get current log target, will call
    // wxAppTraits::CreateLogTarget() to create one if none exists
    static wxLog *GetActiveTarget();

    // change log target, logger may be NULL
    static wxLog *SetActiveTarget(wxLog *logger);

#if wxUSE_THREADS
    // change log target for the current thread only, shouldn't be called from
    // the main thread as it doesn't use thread-specific log target
    static wxLog *SetThreadActiveTarget(wxLog *logger);
#endif // wxUSE_THREADS

    // suspend the message flushing of the main target until the next call
    // to Resume() - this is mainly for internal use (to prevent wxYield()
    // from flashing the messages)
    static void Suspend() { ms_suspendCount++; }

    // must be called for each Suspend()!
    static void Resume() { ms_suspendCount--; }

    // should GetActiveTarget() try to create a new log object if the
    // current is NULL?
    static void DontCreateOnDemand();

    // Make GetActiveTarget() create a new log object again.
    static void DoCreateOnDemand();

    // log the count of repeating messages instead of logging the messages
    // multiple times
    static void SetRepetitionCounting(bool bRepetCounting = true)
        { ms_bRepetCounting = bRepetCounting; }

    // gets duplicate counting status
    static bool GetRepetitionCounting() { return ms_bRepetCounting; }

    // add string trace mask
    static void AddTraceMask(const wxString& str);

    // add string trace mask
    static void RemoveTraceMask(const wxString& str);

    // remove all string trace masks
    static void ClearTraceMasks();

    // get string trace masks: note that this is MT-unsafe if other threads can
    // call AddTraceMask() concurrently
    static const wxArrayString& GetTraceMasks();

    // is this trace mask in the list?
    static bool IsAllowedTraceMask(const wxString& mask);


    // log formatting
    // -----------------

    // Change wxLogFormatter object used by wxLog to format the log messages.
    //
    // wxLog takes ownership of the pointer passed in but the caller is
    // responsible for deleting the returned pointer.
    wxLogFormatter* SetFormatter(wxLogFormatter* formatter);


    // All the time stamp related functions below only work when the default
    // wxLogFormatter is being used. Defining a custom formatter overrides them
    // as it could use its own time stamp format or format messages without
    // using time stamp at all.


    // sets the time stamp string format: this is used as strftime() format
    // string for the log targets which add time stamps to the messages; set
    // it to empty string to disable time stamping completely.
    static void SetTimestamp(const wxString& ts) { ms_timestamp = ts; }

    // disable time stamping of log messages
    static void DisableTimestamp() { SetTimestamp(wxEmptyString); }


    // get the current timestamp format string (maybe empty)
    static const wxString& GetTimestamp() { return ms_timestamp; }



    // helpers: all functions in this section are mostly for internal use only,
    // don't call them from your code even if they are not formally deprecated

    // put the time stamp into the string if ms_timestamp is not empty (don't
    // change it otherwise); the first overload uses the current time.
    static void TimeStamp(wxString *str);
    static void TimeStamp(wxString *str, time_t t);

    // these methods should only be called from derived classes DoLogRecord(),
    // DoLogTextAtLevel() and DoLogText() implementations respectively and
    // shouldn't be called directly, use logging functions instead
    void LogRecord(wxLogLevel level,
                   const wxString& msg,
                   const wxLogRecordInfo& info)
    {
        DoLogRecord(level, msg, info);
    }

    void LogTextAtLevel(wxLogLevel level, const wxString& msg)
    {
        DoLogTextAtLevel(level, msg);
    }

    void LogText(const wxString& msg)
    {
        DoLogText(msg);
    }

    // this is a helper used by wxLogXXX() functions, don't call it directly
    // and see DoLog() for function to overload in the derived classes
    static void OnLog(wxLogLevel level,
                      const wxString& msg,
                      const wxLogRecordInfo& info);

    // version called when no information about the location of the log record
    // generation is available (but the time stamp is), it mainly exists for
    // backwards compatibility, don't use it in new code
    static void OnLog(wxLogLevel level, const wxString& msg, time_t t);

    // a helper calling the above overload with current time
    static void OnLog(wxLogLevel level, const wxString& msg)
    {
        OnLog(level, msg, time(NULL));
    }


    // this method exists for backwards compatibility only, don't use
    bool HasPendingMessages() const { return true; }

#if WXWIN_COMPATIBILITY_2_6
    // this function doesn't do anything any more, don't call it
    static wxDEPRECATED_INLINE(
        wxChar *SetLogBuffer(wxChar *, size_t = 0), return NULL;
    );
#endif // WXWIN_COMPATIBILITY_2_6

    // don't use integer masks any more, use string trace masks instead
#if WXWIN_COMPATIBILITY_2_8
    static wxDEPRECATED_INLINE( void SetTraceMask(wxTraceMask ulMask),
        ms_ulTraceMask = ulMask; )

    // this one can't be marked deprecated as it's used in our own wxLogger
    // below but it still is deprecated and shouldn't be used
    static wxTraceMask GetTraceMask() { return ms_ulTraceMask; }
#endif // WXWIN_COMPATIBILITY_2_8

protected:
    // the logging functions that can be overridden: DoLogRecord() is called
    // for every "record", i.e. a unit of log output, to be logged and by
    // default formats the message and passes it to DoLogTextAtLevel() which in
    // turn passes it to DoLogText() by default

    // override this method if you want to change message formatting or do
    // dynamic filtering
    virtual void DoLogRecord(wxLogLevel level,
                             const wxString& msg,
                             const wxLogRecordInfo& info);

    // override this method to redirect output to different channels depending
    // on its level only; if even the level doesn't matter, override
    // DoLogText() instead
    virtual void DoLogTextAtLevel(wxLogLevel level, const wxString& msg);

    // this function is not pure virtual as it might not be needed if you do
    // the logging in overridden DoLogRecord() or DoLogTextAtLevel() directly
    // but if you do not override them in your derived class you must override
    // this one as the default implementation of it simply asserts
    virtual void DoLogText(const wxString& msg);


    // the rest of the functions are for backwards compatibility only, don't
    // use them in new code; if you're updating your existing code you need to
    // switch to overriding DoLogRecord/Text() above (although as long as these
    // functions exist, log classes using them will continue to work)
#if WXWIN_COMPATIBILITY_2_8
    wxDEPRECATED_BUT_USED_INTERNALLY(
        virtual void DoLog(wxLogLevel level, const char *szString, time_t t)
    );

    wxDEPRECATED_BUT_USED_INTERNALLY(
        virtual void DoLog(wxLogLevel level, const wchar_t *wzString, time_t t)
    );

    // these shouldn't be used by new code
    wxDEPRECATED_BUT_USED_INTERNALLY_INLINE(
        virtual void DoLogString(const char *WXUNUSED(szString),
                                 time_t WXUNUSED(t)),
        wxEMPTY_PARAMETER_VALUE
    )

    wxDEPRECATED_BUT_USED_INTERNALLY_INLINE(
        virtual void DoLogString(const wchar_t *WXUNUSED(wzString),
                                 time_t WXUNUSED(t)),
        wxEMPTY_PARAMETER_VALUE
    )
#endif // WXWIN_COMPATIBILITY_2_8


    // log a message indicating the number of times the previous message was
    // repeated if previous repetition counter is strictly positive, does
    // nothing otherwise; return the old value of repetition counter
    unsigned LogLastRepeatIfNeeded();

private:
#if wxUSE_THREADS
    // called from FlushActive() to really log any buffered messages logged
    // from the other threads
    void FlushThreadMessages();

    // these functions are called for non-main thread only by IsEnabled() and
    // EnableLogging() respectively
    static bool IsThreadLoggingEnabled();
    static bool EnableThreadLogging(bool enable = true);
#endif // wxUSE_THREADS

    // get the active log target for the main thread, auto-creating it if
    // necessary
    //
    // this is called from GetActiveTarget() and OnLog() when they're called
    // from the main thread
    static wxLog *GetMainThreadActiveTarget();

    // called from OnLog() if it's called from the main thread or if we have a
    // (presumably MT-safe) thread-specific logger and by FlushThreadMessages()
    // when it plays back the buffered messages logged from the other threads
    void CallDoLogNow(wxLogLevel level,
                      const wxString& msg,
                      const wxLogRecordInfo& info);


    // variables
    // ----------------

    wxLogFormatter    *m_formatter; // We own this pointer.


    // static variables
    // ----------------

    // if true, don't log the same message multiple times, only log it once
    // with the number of times it was repeated
    static bool        ms_bRepetCounting;

    static wxLog      *ms_pLogger;      // currently active log sink
    static bool        ms_doLog;        // false => all logging disabled
    static bool        ms_bAutoCreate;  // create new log targets on demand?
    static bool        ms_bVerbose;     // false => ignore LogInfo messages

    static wxLogLevel  ms_logLevel;     // limit logging to levels <= ms_logLevel

    static size_t      ms_suspendCount; // if positive, logs are not flushed

    // format string for strftime(), if empty, time stamping log messages is
    // disabled
    static wxString    ms_timestamp;

#if WXWIN_COMPATIBILITY_2_8
    static wxTraceMask ms_ulTraceMask;   // controls wxLogTrace behaviour
#endif // WXWIN_COMPATIBILITY_2_8
};

// ----------------------------------------------------------------------------
// "trivial" derivations of wxLog
// ----------------------------------------------------------------------------

// log everything except for the debug/trace messages (which are passed to
// wxMessageOutputDebug) to a buffer
class WXDLLIMPEXP_BASE wxLogBuffer : public wxLog
{
public:
    wxLogBuffer() { }

    // get the string contents with all messages logged
    const wxString& GetBuffer() const { return m_str; }

    // show the buffer contents to the user in the best possible way (this uses
    // wxMessageOutputMessageBox) and clear it
    virtual void Flush();

protected:
    virtual void DoLogTextAtLevel(wxLogLevel level, const wxString& msg);

private:
    wxString m_str;

    wxDECLARE_NO_COPY_CLASS(wxLogBuffer);
};


// log everything to a "FILE *", stderr by default
class WXDLLIMPEXP_BASE wxLogStderr : public wxLog
{
public:
    // redirect log output to a FILE
    wxLogStderr(FILE *fp = NULL);

protected:
    // implement sink function
    virtual void DoLogText(const wxString& msg);

    FILE *m_fp;

    wxDECLARE_NO_COPY_CLASS(wxLogStderr);
};

#if wxUSE_STD_IOSTREAM

// log everything to an "ostream", cerr by default
class WXDLLIMPEXP_BASE wxLogStream : public wxLog
{
public:
    // redirect log output to an ostream
    wxLogStream(wxSTD ostream *ostr = (wxSTD ostream *) NULL);

protected:
    // implement sink function
    virtual void DoLogText(const wxString& msg);

    // using ptr here to avoid including <iostream.h> from this file
    wxSTD ostream *m_ostr;
};

#endif // wxUSE_STD_IOSTREAM

// ----------------------------------------------------------------------------
// /dev/null log target: suppress logging until this object goes out of scope
// ----------------------------------------------------------------------------

// example of usage:
/*
    void Foo()
    {
        wxFile file;

        // wxFile.Open() normally complains if file can't be opened, we don't
        // want it
        wxLogNull logNo;

        if ( !file.Open("bar") )
            ... process error ourselves ...

        // ~wxLogNull called, old log sink restored
    }
 */
class WXDLLIMPEXP_BASE wxLogNull
{
public:
    wxLogNull() : m_flagOld(wxLog::EnableLogging(false)) { }
    ~wxLogNull() { (void)wxLog::EnableLogging(m_flagOld); }

private:
    bool m_flagOld; // the previous value of the wxLog::ms_doLog
};

// ----------------------------------------------------------------------------
// chaining log target: installs itself as a log target and passes all
// messages to the real log target given to it in the ctor but also forwards
// them to the previously active one
//
// note that you don't have to call SetActiveTarget() with this class, it
// does it itself in its ctor
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxLogChain : public wxLog
{
public:
    wxLogChain(wxLog *logger);
    virtual ~wxLogChain();

    // change the new log target
    void SetLog(wxLog *logger);

    // this can be used to temporarily disable (and then reenable) passing
    // messages to the old logger (by default we do pass them)
    void PassMessages(bool bDoPass) { m_bPassMessages = bDoPass; }

    // are we passing the messages to the previous log target?
    bool IsPassingMessages() const { return m_bPassMessages; }

    // return the previous log target (may be NULL)
    wxLog *GetOldLog() const { return m_logOld; }

    // override base class version to flush the old logger as well
    virtual void Flush();

    // call to avoid destroying the old log target
    void DetachOldLog() { m_logOld = NULL; }

protected:
    // pass the record to the old logger if needed
    virtual void DoLogRecord(wxLogLevel level,
                             const wxString& msg,
                             const wxLogRecordInfo& info);

private:
    // the current log target
    wxLog *m_logNew;

    // the previous log target
    wxLog *m_logOld;

    // do we pass the messages to the old logger?
    bool m_bPassMessages;

    wxDECLARE_NO_COPY_CLASS(wxLogChain);
};

// a chain log target which uses itself as the new logger

#define wxLogPassThrough wxLogInterposer

class WXDLLIMPEXP_BASE wxLogInterposer : public wxLogChain
{
public:
    wxLogInterposer();

private:
    wxDECLARE_NO_COPY_CLASS(wxLogInterposer);
};

// a temporary interposer which doesn't destroy the old log target
// (calls DetachOldLog)

class WXDLLIMPEXP_BASE wxLogInterposerTemp : public wxLogChain
{
public:
    wxLogInterposerTemp();

private:
    wxDECLARE_NO_COPY_CLASS(wxLogInterposerTemp);
};

#if wxUSE_GUI
    // include GUI log targets:
    #include "wx/generic/logg.h"
#endif // wxUSE_GUI

// ----------------------------------------------------------------------------
// wxLogger
// ----------------------------------------------------------------------------

// wxLogger is a helper class used by wxLogXXX() functions implementation,
// don't use it directly as it's experimental and subject to change (OTOH it
// might become public in the future if it's deemed to be useful enough)

// contains information about the context from which a log message originates
// and provides Log() vararg method which forwards to wxLog::OnLog() and passes
// this context to it
class wxLogger
{
public:
    // ctor takes the basic information about the log record
    wxLogger(wxLogLevel level,
             const char *filename,
             int line,
             const char *func,
             const char *component)
        : m_level(level),
          m_info(filename, line, func, component)
    {
    }

    // store extra data in our log record and return this object itself (so
    // that further calls to its functions could be chained)
    template <typename T>
    wxLogger& Store(const wxString& key, T val)
    {
        m_info.StoreValue(key, val);
        return *this;
    }

    // hack for "overloaded" wxLogXXX() functions: calling this method
    // indicates that we may have an extra first argument preceding the format
    // string and that if we do have it, we should store it in m_info using the
    // given key (while by default 0 value will be used)
    wxLogger& MaybeStore(const wxString& key, wxUIntPtr value = 0)
    {
        wxASSERT_MSG( m_optKey.empty(), "can only have one optional value" );
        m_optKey = key;

        m_info.StoreValue(key, value);
        return *this;
    }


    // non-vararg function used by wxVLogXXX():

    // log the message at the level specified in the ctor if this log message
    // is enabled
    void LogV(const wxString& format, va_list argptr)
    {
        // remember that fatal errors can't be disabled
        if ( m_level == wxLOG_FatalError ||
                wxLog::IsLevelEnabled(m_level, m_info.component) )
            DoCallOnLog(format, argptr);
    }

    // overloads used by functions with optional leading arguments (whose
    // values are stored in the key passed to MaybeStore())
    void LogV(long num, const wxString& format, va_list argptr)
    {
        Store(m_optKey, num);

        LogV(format, argptr);
    }

    void LogV(void *ptr, const wxString& format, va_list argptr)
    {
        Store(m_optKey, wxPtrToUInt(ptr));

        LogV(format, argptr);
    }

    void LogVTrace(const wxString& mask, const wxString& format, va_list argptr)
    {
        if ( !wxLog::IsAllowedTraceMask(mask) )
            return;

        Store(wxLOG_KEY_TRACE_MASK, mask);

        LogV(format, argptr);
    }


    // vararg functions used by wxLogXXX():

    // will log the message at the level specified in the ctor
    //
    // notice that this function supposes that the caller already checked that
    // the level was enabled and does no checks itself
    WX_DEFINE_VARARG_FUNC_VOID
    (
        Log,
        1, (const wxFormatString&),
        DoLog, DoLogUtf8
    )

    // same as Log() but with an extra numeric or pointer parameters: this is
    // used to pass an optional value by storing it in m_info under the name
    // passed to MaybeStore() and is required to support "overloaded" versions
    // of wxLogStatus() and wxLogSysError()
    WX_DEFINE_VARARG_FUNC_VOID
    (
        Log,
        2, (long, const wxFormatString&),
        DoLogWithNum, DoLogWithNumUtf8
    )

    // unfortunately we can't use "void *" here as we get overload ambiguities
    // with Log(wxFormatString, ...) when the first argument is a "char *" or
    // "wchar_t *" then -- so we only allow passing wxObject here, which is
    // ugly but fine in practice as this overload is only used by wxLogStatus()
    // whose first argument is a wxFrame
    WX_DEFINE_VARARG_FUNC_VOID
    (
        Log,
        2, (wxObject *, const wxFormatString&),
        DoLogWithPtr, DoLogWithPtrUtf8
    )

    // log the message at the level specified as its first argument
    //
    // as the macros don't have access to the level argument in this case, this
    // function does check that the level is enabled itself
    WX_DEFINE_VARARG_FUNC_VOID
    (
        LogAtLevel,
        2, (wxLogLevel, const wxFormatString&),
        DoLogAtLevel, DoLogAtLevelUtf8
    )

    // special versions for wxLogTrace() which is passed either string or
    // integer mask as first argument determining whether the message should be
    // logged or not
    WX_DEFINE_VARARG_FUNC_VOID
    (
        LogTrace,
        2, (const wxString&, const wxFormatString&),
        DoLogTrace, DoLogTraceUtf8
    )

#if WXWIN_COMPATIBILITY_2_8
    WX_DEFINE_VARARG_FUNC_VOID
    (
        LogTrace,
        2, (wxTraceMask, const wxFormatString&),
        DoLogTraceMask, DoLogTraceMaskUtf8
    )
#endif // WXWIN_COMPATIBILITY_2_8

#ifdef __WATCOMC__
    // workaround for http://bugzilla.openwatcom.org/show_bug.cgi?id=351
    WX_VARARG_WATCOM_WORKAROUND(void, Log,
                               1, (const wxString&),
                               (wxFormatString(f1)))
    WX_VARARG_WATCOM_WORKAROUND(void, Log,
                               1, (const wxCStrData&),
                               (wxFormatString(f1)))
    WX_VARARG_WATCOM_WORKAROUND(void, Log,
                               1, (const char*),
                               (wxFormatString(f1)))
    WX_VARARG_WATCOM_WORKAROUND(void, Log,
                               1, (const wchar_t*),
                               (wxFormatString(f1)))

    WX_VARARG_WATCOM_WORKAROUND(void, Log,
                               2, (long, const wxString&),
                               (f1, wxFormatString(f2)))
    WX_VARARG_WATCOM_WORKAROUND(void, Log,
                               2, (long, const wxCStrData&),
                               (f1, wxFormatString(f2)))
    WX_VARARG_WATCOM_WORKAROUND(void, Log,
                               2, (long, const char *),
                               (f1, wxFormatString(f2)))
    WX_VARARG_WATCOM_WORKAROUND(void, Log,
                               2, (long, const wchar_t *),
                               (f1, wxFormatString(f2)))

    WX_VARARG_WATCOM_WORKAROUND(void, Log,
                               2, (wxObject *, const wxString&),
                               (f1, wxFormatString(f2)))
    WX_VARARG_WATCOM_WORKAROUND(void, Log,
                               2, (wxObject *, const wxCStrData&),
                               (f1, wxFormatString(f2)))
    WX_VARARG_WATCOM_WORKAROUND(void, Log,
                               2, (wxObject *, const char *),
                               (f1, wxFormatString(f2)))
    WX_VARARG_WATCOM_WORKAROUND(void, Log,
                               2, (wxObject *, const wchar_t *),
                               (f1, wxFormatString(f2)))

    WX_VARARG_WATCOM_WORKAROUND(void, LogAtLevel,
                               2, (wxLogLevel, const wxString&),
                               (f1, wxFormatString(f2)))
    WX_VARARG_WATCOM_WORKAROUND(void, LogAtLevel,
                               2, (wxLogLevel, const wxCStrData&),
                               (f1, wxFormatString(f2)))
    WX_VARARG_WATCOM_WORKAROUND(void, LogAtLevel,
                               2, (wxLogLevel, const char *),
                               (f1, wxFormatString(f2)))
    WX_VARARG_WATCOM_WORKAROUND(void, LogAtLevel,
                               2, (wxLogLevel, const wchar_t *),
                               (f1, wxFormatString(f2)))

    WX_VARARG_WATCOM_WORKAROUND(void, LogTrace,
                               2, (const wxString&, const wxString&),
                               (f1, wxFormatString(f2)))
    WX_VARARG_WATCOM_WORKAROUND(void, LogTrace,
                               2, (const wxString&, const wxCStrData&),
                               (f1, wxFormatString(f2)))
    WX_VARARG_WATCOM_WORKAROUND(void, LogTrace,
                               2, (const wxString&, const char *),
                               (f1, wxFormatString(f2)))
    WX_VARARG_WATCOM_WORKAROUND(void, LogTrace,
                               2, (const wxString&, const wchar_t *),
                               (f1, wxFormatString(f2)))

#if WXWIN_COMPATIBILITY_2_8
    WX_VARARG_WATCOM_WORKAROUND(void, LogTrace,
                               2, (wxTraceMask, wxTraceMask),
                               (f1, wxFormatString(f2)))
    WX_VARARG_WATCOM_WORKAROUND(void, LogTrace,
                               2, (wxTraceMask, const wxCStrData&),
                               (f1, wxFormatString(f2)))
    WX_VARARG_WATCOM_WORKAROUND(void, LogTrace,
                               2, (wxTraceMask, const char *),
                               (f1, wxFormatString(f2)))
    WX_VARARG_WATCOM_WORKAROUND(void, LogTrace,
                               2, (wxTraceMask, const wchar_t *),
                               (f1, wxFormatString(f2)))
#endif // WXWIN_COMPATIBILITY_2_8
#endif // __WATCOMC__

private:
#if !wxUSE_UTF8_LOCALE_ONLY
    void DoLog(const wxChar *format, ...)
    {
        va_list argptr;
        va_start(argptr, format);
        DoCallOnLog(format, argptr);
        va_end(argptr);
    }

    void DoLogWithNum(long num, const wxChar *format, ...)
    {
        Store(m_optKey, num);

        va_list argptr;
        va_start(argptr, format);
        DoCallOnLog(format, argptr);
        va_end(argptr);
    }

    void DoLogWithPtr(void *ptr, const wxChar *format, ...)
    {
        Store(m_optKey, wxPtrToUInt(ptr));

        va_list argptr;
        va_start(argptr, format);
        DoCallOnLog(format, argptr);
        va_end(argptr);
    }

    void DoLogAtLevel(wxLogLevel level, const wxChar *format, ...)
    {
        if ( !wxLog::IsLevelEnabled(level, m_info.component) )
            return;

        va_list argptr;
        va_start(argptr, format);
        DoCallOnLog(level, format, argptr);
        va_end(argptr);
    }

    void DoLogTrace(const wxString& mask, const wxChar *format, ...)
    {
        if ( !wxLog::IsAllowedTraceMask(mask) )
            return;

        Store(wxLOG_KEY_TRACE_MASK, mask);

        va_list argptr;
        va_start(argptr, format);
        DoCallOnLog(format, argptr);
        va_end(argptr);
    }

#if WXWIN_COMPATIBILITY_2_8
    void DoLogTraceMask(wxTraceMask mask, const wxChar *format, ...)
    {
        if ( (wxLog::GetTraceMask() & mask) != mask )
            return;

        Store(wxLOG_KEY_TRACE_MASK, mask);

        va_list argptr;
        va_start(argptr, format);
        DoCallOnLog(format, argptr);
        va_end(argptr);
    }
#endif // WXWIN_COMPATIBILITY_2_8
#endif // !wxUSE_UTF8_LOCALE_ONLY

#if wxUSE_UNICODE_UTF8
    void DoLogUtf8(const char *format, ...)
    {
        va_list argptr;
        va_start(argptr, format);
        DoCallOnLog(format, argptr);
        va_end(argptr);
    }

    void DoLogWithNumUtf8(long num, const char *format, ...)
    {
        Store(m_optKey, num);

        va_list argptr;
        va_start(argptr, format);
        DoCallOnLog(format, argptr);
        va_end(argptr);
    }

    void DoLogWithPtrUtf8(void *ptr, const char *format, ...)
    {
        Store(m_optKey, wxPtrToUInt(ptr));

        va_list argptr;
        va_start(argptr, format);
        DoCallOnLog(format, argptr);
        va_end(argptr);
    }

    void DoLogAtLevelUtf8(wxLogLevel level, const char *format, ...)
    {
        if ( !wxLog::IsLevelEnabled(level, m_info.component) )
            return;

        va_list argptr;
        va_start(argptr, format);
        DoCallOnLog(level, format, argptr);
        va_end(argptr);
    }

    void DoLogTraceUtf8(const wxString& mask, const char *format, ...)
    {
        if ( !wxLog::IsAllowedTraceMask(mask) )
            return;

        Store(wxLOG_KEY_TRACE_MASK, mask);

        va_list argptr;
        va_start(argptr, format);
        DoCallOnLog(format, argptr);
        va_end(argptr);
    }

#if WXWIN_COMPATIBILITY_2_8
    void DoLogTraceMaskUtf8(wxTraceMask mask, const char *format, ...)
    {
        if ( (wxLog::GetTraceMask() & mask) != mask )
            return;

        Store(wxLOG_KEY_TRACE_MASK, mask);

        va_list argptr;
        va_start(argptr, format);
        DoCallOnLog(format, argptr);
        va_end(argptr);
    }
#endif // WXWIN_COMPATIBILITY_2_8
#endif // wxUSE_UNICODE_UTF8

    void DoCallOnLog(wxLogLevel level, const wxString& format, va_list argptr)
    {
        wxLog::OnLog(level, wxString::FormatV(format, argptr), m_info);
    }

    void DoCallOnLog(const wxString& format, va_list argptr)
    {
        DoCallOnLog(m_level, format, argptr);
    }


    const wxLogLevel m_level;
    wxLogRecordInfo m_info;

    wxString m_optKey;

    wxDECLARE_NO_COPY_CLASS(wxLogger);
};

// ============================================================================
// global functions
// ============================================================================

// ----------------------------------------------------------------------------
// get error code/error message from system in a portable way
// ----------------------------------------------------------------------------

// return the last system error code
WXDLLIMPEXP_BASE unsigned long wxSysErrorCode();

// return the error message for given (or last if 0) error code
WXDLLIMPEXP_BASE const wxChar* wxSysErrorMsg(unsigned long nErrCode = 0);

// ----------------------------------------------------------------------------
// define wxLog<level>() functions which can be used by application instead of
// stdio, iostream &c for log messages for easy redirection
// ----------------------------------------------------------------------------

/*
    The code below is unreadable because it (unfortunately unavoidably)
    contains a lot of macro magic but all it does is to define wxLogXXX() such
    that you can call them as vararg functions to log a message at the
    corresponding level.

    More precisely, it defines:

        - wxLog{FatalError,Error,Warning,Message,Verbose,Debug}() functions
        taking the format string and additional vararg arguments if needed.
        - wxLogGeneric(wxLogLevel level, const wxString& format, ...) which
        takes the log level explicitly.
        - wxLogSysError(const wxString& format, ...) and wxLogSysError(long
        err, const wxString& format, ...) which log a wxLOG_Error severity
        message with the error message corresponding to the system error code
        err or the last error.
        - wxLogStatus(const wxString& format, ...) which logs the message into
        the status bar of the main application window and its overload
        wxLogStatus(wxFrame *frame, const wxString& format, ...) which logs it
        into the status bar of the specified frame.
        - wxLogTrace(Mask mask, const wxString& format, ...) which only logs
        the message is the specified mask is enabled. This comes in two kinds:
        Mask can be a wxString or a long. Both are deprecated.

    In addition, wxVLogXXX() versions of all the functions above are also
    defined. They take a va_list argument instead of "...".
 */

// creates wxLogger object for the current location
#define wxMAKE_LOGGER(level) \
    wxLogger(wxLOG_##level, __FILE__, __LINE__, __WXFUNCTION__, wxLOG_COMPONENT)

// this macro generates the expression which logs whatever follows it in
// parentheses at the level specified as argument
#define wxDO_LOG(level) wxMAKE_LOGGER(level).Log

// this is the non-vararg equivalent
#define wxDO_LOGV(level, format, argptr) \
    wxMAKE_LOGGER(level).LogV(format, argptr)

// this macro declares wxLog<level>() macro which logs whatever follows it if
// logging at specified level is enabled (notice that if it is false, the
// following arguments are not even evaluated which is good as it avoids
// unnecessary overhead)
//
// Note: the strange (because executing at most once) for() loop because we
//       must arrange for wxDO_LOG() to be at the end of the macro and using a
//       more natural "if (IsLevelEnabled()) wxDO_LOG()" would result in wrong
//       behaviour for the following code ("else" would bind to the wrong "if"):
//
//          if ( cond )
//              wxLogError("!!!");
//          else
//              ...
//
//       See also #11829 for the problems with other simpler approaches,
//       notably the need for two macros due to buggy __LINE__ in MSVC.
//
// Note 2: Unfortunately we can't use the same solution for all compilers
//         because the loop-based one results in problems with MSVC6 due to its
//         wrong (pre-C++98) rules for the scope of the variables declared
//         inside the loop, as this prevents us from using wxLogXXX() in switch
//         statement clauses ("initialization of loopvar skipped by case"). So
//         for now, i.e. while we still support VC6, use the previous solution
//         for it (FIXME-VC6).
#ifdef __VISUALC6__
#define wxDO_LOG_IF_ENABLED(level)                                            \
    if ( !wxLog::IsLevelEnabled(wxLOG_##level, wxLOG_COMPONENT) )             \
    {}                                                                        \
    else                                                                      \
        wxDO_LOG(level)
#else
#define wxDO_LOG_IF_ENABLED_HELPER(level, loopvar)                            \
    for ( bool loopvar = false;                                               \
          !loopvar && wxLog::IsLevelEnabled(wxLOG_##level, wxLOG_COMPONENT);  \
          loopvar = true )                                                    \
        wxDO_LOG(level)

#define wxDO_LOG_IF_ENABLED(level)                                            \
    wxDO_LOG_IF_ENABLED_HELPER(level, wxMAKE_UNIQUE_NAME(wxlogcheck))
#endif

// wxLogFatalError() is special as it can't be disabled
#define wxLogFatalError wxDO_LOG(FatalError)
#define wxVLogFatalError(format, argptr) wxDO_LOGV(FatalError, format, argptr)

#define wxLogError wxDO_LOG_IF_ENABLED(Error)
#define wxVLogError(format, argptr) wxDO_LOGV(Error, format, argptr)

#define wxLogWarning wxDO_LOG_IF_ENABLED(Warning)
#define wxVLogWarning(format, argptr) wxDO_LOGV(Warning, format, argptr)

#define wxLogMessage wxDO_LOG_IF_ENABLED(Message)
#define wxVLogMessage(format, argptr) wxDO_LOGV(Message, format, argptr)

// this one is special as it only logs if we're in verbose mode
#define wxLogVerbose                                                          \
    if ( !(wxLog::IsLevelEnabled(wxLOG_Info, wxLOG_COMPONENT) &&              \
            wxLog::GetVerbose()) )                                            \
    {}                                                                        \
    else                                                                      \
        wxDO_LOG(Info)
#define wxVLogVerbose(format, argptr)                                         \
    if ( !(wxLog::IsLevelEnabled(wxLOG_Info, wxLOG_COMPONENT) &&              \
            wxLog::GetVerbose()) )                                            \
    {}                                                                        \
    else                                                                      \
        wxDO_LOGV(Info, format, argptr)

// deprecated synonyms for wxLogVerbose() and wxVLogVerbose()
#define wxLogInfo wxLogVerbose
#define wxVLogInfo wxVLogVerbose


// another special case: the level is passed as first argument of the function
// and so is not available to the macro
//
// notice that because of this, arguments of wxLogGeneric() are currently
// always evaluated, unlike for the other log functions
#define wxLogGeneric wxMAKE_LOGGER(Max).LogAtLevel
#define wxVLogGeneric(level, format, argptr) \
    if ( !wxLog::IsLevelEnabled(wxLOG_##level, wxLOG_COMPONENT) )             \
    {}                                                                        \
    else                                                                      \
        wxDO_LOGV(level, format, argptr)


// wxLogSysError() needs to stash the error code value in the log record info
// so it needs special handling too; additional complications arise because the
// error code may or not be present as the first argument
//
// notice that we unfortunately can't avoid the call to wxSysErrorCode() even
// though it may be unneeded if an explicit error code is passed to us because
// the message might not be logged immediately (e.g. it could be queued for
// logging from the main thread later) and so we can't to wait until it is
// logged to determine whether we have last error or not as it will be too late
// and it will have changed already by then (in fact it even changes when
// wxString::Format() is called because of vsnprintf() inside it so it can
// change even much sooner)
#define wxLOG_KEY_SYS_ERROR_CODE "wx.sys_error"

#define wxLogSysError                                                         \
    if ( !wxLog::IsLevelEnabled(wxLOG_Error, wxLOG_COMPONENT) )               \
    {}                                                                        \
    else                                                                      \
        wxMAKE_LOGGER(Error).MaybeStore(wxLOG_KEY_SYS_ERROR_CODE,             \
                                        wxSysErrorCode()).Log

// unfortunately we can't have overloaded macros so we can't define versions
// both with and without error code argument and have to rely on LogV()
// overloads in wxLogger to select between them
#define wxVLogSysError \
    wxMAKE_LOGGER(Error).MaybeStore(wxLOG_KEY_SYS_ERROR_CODE, \
                                    wxSysErrorCode()).LogV

#if wxUSE_GUI
    // wxLogStatus() is similar to wxLogSysError() as it allows to optionally
    // specify the frame to which the message should go
    #define wxLOG_KEY_FRAME "wx.frame"

    #define wxLogStatus                                                       \
        if ( !wxLog::IsLevelEnabled(wxLOG_Status, wxLOG_COMPONENT) )          \
        {}                                                                    \
        else                                                                  \
            wxMAKE_LOGGER(Status).MaybeStore(wxLOG_KEY_FRAME).Log

    #define wxVLogStatus \
        wxMAKE_LOGGER(Status).MaybeStore(wxLOG_KEY_FRAME).LogV
#endif // wxUSE_GUI


#else // !wxUSE_LOG

#undef wxUSE_LOG_DEBUG
#define wxUSE_LOG_DEBUG 0

#undef wxUSE_LOG_TRACE
#define wxUSE_LOG_TRACE 0

#if defined(__WATCOMC__) || defined(__MINGW32__)
    // Mingw has similar problem with wxLogSysError:
    #define WX_WATCOM_OR_MINGW_ONLY_CODE( x )  x
#else
    #define WX_WATCOM_OR_MINGW_ONLY_CODE( x )
#endif

// define macros for defining log functions which do nothing at all
//
// WX_WATCOM_ONLY_CODE is needed to work around
// http://bugzilla.openwatcom.org/show_bug.cgi?id=351
#define wxDEFINE_EMPTY_LOG_FUNCTION(level)                                  \
    WX_DEFINE_VARARG_FUNC_NOP(wxLog##level, 1, (const wxFormatString&))     \
    WX_WATCOM_ONLY_CODE(                                                    \
        WX_DEFINE_VARARG_FUNC_NOP(wxLog##level, 1, (const char*))           \
        WX_DEFINE_VARARG_FUNC_NOP(wxLog##level, 1, (const wchar_t*))        \
        WX_DEFINE_VARARG_FUNC_NOP(wxLog##level, 1, (const wxCStrData&))     \
    )                                                                       \
    inline void wxVLog##level(const wxFormatString& WXUNUSED(format),       \
                              va_list WXUNUSED(argptr)) { }                 \

#define wxDEFINE_EMPTY_LOG_FUNCTION2(level, argclass)                       \
    WX_DEFINE_VARARG_FUNC_NOP(wxLog##level, 2, (argclass, const wxFormatString&)) \
    WX_WATCOM_ONLY_CODE(                                                    \
        WX_DEFINE_VARARG_FUNC_NOP(wxLog##level, 2, (argclass, const char*)) \
        WX_DEFINE_VARARG_FUNC_NOP(wxLog##level, 2, (argclass, const wchar_t*)) \
        WX_DEFINE_VARARG_FUNC_NOP(wxLog##level, 2, (argclass, const wxCStrData&)) \
    )                                                                       \
    inline void wxVLog##level(argclass WXUNUSED(arg),                       \
                              const wxFormatString& WXUNUSED(format),       \
                              va_list WXUNUSED(argptr)) {}

wxDEFINE_EMPTY_LOG_FUNCTION(FatalError);
wxDEFINE_EMPTY_LOG_FUNCTION(Error);
wxDEFINE_EMPTY_LOG_FUNCTION(SysError);
wxDEFINE_EMPTY_LOG_FUNCTION2(SysError, long);
wxDEFINE_EMPTY_LOG_FUNCTION(Warning);
wxDEFINE_EMPTY_LOG_FUNCTION(Message);
wxDEFINE_EMPTY_LOG_FUNCTION(Info);
wxDEFINE_EMPTY_LOG_FUNCTION(Verbose);

wxDEFINE_EMPTY_LOG_FUNCTION2(Generic, wxLogLevel);

#if wxUSE_GUI
    wxDEFINE_EMPTY_LOG_FUNCTION(Status);
    wxDEFINE_EMPTY_LOG_FUNCTION2(Status, wxFrame *);
#endif // wxUSE_GUI

// Empty Class to fake wxLogNull
class WXDLLIMPEXP_BASE wxLogNull
{
public:
    wxLogNull() { }
};

// Dummy macros to replace some functions.
#define wxSysErrorCode() (unsigned long)0
#define wxSysErrorMsg( X ) (const wxChar*)NULL

// Fake symbolic trace masks... for those that are used frequently
#define wxTRACE_OleCalls wxEmptyString // OLE interface calls

#endif // wxUSE_LOG/!wxUSE_LOG


// debug functions can be completely disabled in optimized builds

// if these log functions are disabled, we prefer to define them as (empty)
// variadic macros as this completely removes them and their argument
// evaluation from the object code but if this is not supported by compiler we
// use empty inline functions instead (defining them as nothing would result in
// compiler warnings)
//
// note that making wxVLogDebug/Trace() themselves (empty inline) functions is
// a bad idea as some compilers are stupid enough to not inline even empty
// functions if their parameters are complicated enough, but by defining them
// as an empty inline function we ensure that even dumbest compilers optimise
// them away
#ifdef __BORLANDC__
    // but Borland gives "W8019: Code has no effect" for wxLogNop() so we need
    // to define it differently for it to avoid these warnings (same problem as
    // with wxUnusedVar())
    #define wxLogNop() { }
#else
    inline void wxLogNop() { }
#endif

#if wxUSE_LOG_DEBUG
    #define wxLogDebug wxDO_LOG_IF_ENABLED(Debug)
    #define wxVLogDebug(format, argptr) wxDO_LOGV(Debug, format, argptr)
#else // !wxUSE_LOG_DEBUG
    #define wxVLogDebug(fmt, valist) wxLogNop()

    #ifdef HAVE_VARIADIC_MACROS
        #define wxLogDebug(fmt, ...) wxLogNop()
    #else // !HAVE_VARIADIC_MACROS
        WX_DEFINE_VARARG_FUNC_NOP(wxLogDebug, 1, (const wxFormatString&))
    #endif
#endif // wxUSE_LOG_DEBUG/!wxUSE_LOG_DEBUG

#if wxUSE_LOG_TRACE
    #define wxLogTrace                                                        \
        if ( !wxLog::IsLevelEnabled(wxLOG_Trace, wxLOG_COMPONENT) )           \
        {}                                                                    \
        else                                                                  \
            wxMAKE_LOGGER(Trace).LogTrace
    #define wxVLogTrace                                                       \
        if ( !wxLog::IsLevelEnabled(wxLOG_Trace, wxLOG_COMPONENT) )           \
        {}                                                                    \
        else                                                                  \
            wxMAKE_LOGGER(Trace).LogVTrace
#else  // !wxUSE_LOG_TRACE
    #define wxVLogTrace(mask, fmt, valist) wxLogNop()

    #ifdef HAVE_VARIADIC_MACROS
        #define wxLogTrace(mask, fmt, ...) wxLogNop()
    #else // !HAVE_VARIADIC_MACROS
        #if WXWIN_COMPATIBILITY_2_8
        WX_DEFINE_VARARG_FUNC_NOP(wxLogTrace, 2, (wxTraceMask, const wxFormatString&))
        #endif
        WX_DEFINE_VARARG_FUNC_NOP(wxLogTrace, 2, (const wxString&, const wxFormatString&))
        #ifdef __WATCOMC__
        // workaround for http://bugzilla.openwatcom.org/show_bug.cgi?id=351
        WX_DEFINE_VARARG_FUNC_NOP(wxLogTrace, 2, (const char*, const char*))
        WX_DEFINE_VARARG_FUNC_NOP(wxLogTrace, 2, (const wchar_t*, const wchar_t*))
        #endif
    #endif // HAVE_VARIADIC_MACROS/!HAVE_VARIADIC_MACROS
#endif // wxUSE_LOG_TRACE/!wxUSE_LOG_TRACE

// wxLogFatalError helper: show the (fatal) error to the user in a safe way,
// i.e. without using wxMessageBox() for example because it could crash
void WXDLLIMPEXP_BASE
wxSafeShowMessage(const wxString& title, const wxString& text);

// ----------------------------------------------------------------------------
// debug only logging functions: use them with API name and error code
// ----------------------------------------------------------------------------

#if wxUSE_LOG_DEBUG
    // make life easier for people using VC++ IDE: clicking on the message
    // will take us immediately to the place of the failed API
#ifdef __VISUALC__
    #define wxLogApiError(api, rc)                                            \
        wxLogDebug(wxT("%s(%d): '%s' failed with error 0x%08lx (%s)."),       \
                   __FILE__, __LINE__, api,                                   \
                   (long)rc, wxSysErrorMsg(rc))
#else // !VC++
    #define wxLogApiError(api, rc)                                            \
        wxLogDebug(wxT("In file %s at line %d: '%s' failed with ")            \
                   wxT("error 0x%08lx (%s)."),                                \
                   __FILE__, __LINE__, api,                                   \
                   (long)rc, wxSysErrorMsg(rc))
#endif // VC++/!VC++

    #define wxLogLastError(api) wxLogApiError(api, wxSysErrorCode())

#else // !wxUSE_LOG_DEBUG
    #define wxLogApiError(api, err) wxLogNop()
    #define wxLogLastError(api) wxLogNop()
#endif // wxUSE_LOG_DEBUG/!wxUSE_LOG_DEBUG

// wxCocoa has additiional trace masks
#if defined(__WXCOCOA__)
#include "wx/cocoa/log.h"
#endif

#ifdef WX_WATCOM_ONLY_CODE
    #undef WX_WATCOM_ONLY_CODE
#endif

// macro which disables debug logging in release builds: this is done by
// default by wxIMPLEMENT_APP() so usually it doesn't need to be used explicitly
#if defined(NDEBUG) && wxUSE_LOG_DEBUG
    #define wxDISABLE_DEBUG_LOGGING_IN_RELEASE_BUILD() \
        wxLog::SetLogLevel(wxLOG_Info)
#else // !NDEBUG
    #define wxDISABLE_DEBUG_LOGGING_IN_RELEASE_BUILD()
#endif // NDEBUG/!NDEBUG

#endif  // _WX_LOG_H_

