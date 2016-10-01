/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/utilscmn.cpp
// Purpose:     Miscellaneous utility functions and classes
// Author:      Julian Smart
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1998 Julian Smart
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

// See comment about this hack in time.cpp: here we do it for environ external
// variable which can't be easily declared when using MinGW in strict ANSI mode.
#ifdef wxNEEDS_STRICT_ANSI_WORKAROUNDS
    #undef __STRICT_ANSI__
    #include <stdlib.h>
    #define __STRICT_ANSI__
#endif

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/string.h"
    #include "wx/utils.h"
    #include "wx/intl.h"
    #include "wx/log.h"

    #if wxUSE_GUI
        #include "wx/window.h"
        #include "wx/frame.h"
        #include "wx/menu.h"
        #include "wx/msgdlg.h"
        #include "wx/textdlg.h"
        #include "wx/textctrl.h"    // for wxTE_PASSWORD
        #if wxUSE_ACCEL
            #include "wx/menuitem.h"
            #include "wx/accel.h"
        #endif // wxUSE_ACCEL
    #endif // wxUSE_GUI
#endif // WX_PRECOMP

#include "wx/apptrait.h"

#include "wx/process.h"
#include "wx/txtstrm.h"
#include "wx/uri.h"
#include "wx/mimetype.h"
#include "wx/config.h"
#include "wx/versioninfo.h"

#if defined(__WXWINCE__) && wxUSE_DATETIME
    #include "wx/datetime.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !wxONLY_WATCOM_EARLIER_THAN(1,4)
    #if !(defined(_MSC_VER) && (_MSC_VER > 800))
        #include <errno.h>
    #endif
#endif

#if wxUSE_GUI
    #include "wx/notebook.h"
    #include "wx/statusbr.h"
#endif // wxUSE_GUI

#ifndef __WXWINCE__
    #include <time.h>
#else
    #include "wx/msw/wince/time.h"
#endif

#ifdef __WXMAC__
    #include "wx/osx/private.h"
#endif

#if !defined(__WXWINCE__)
    #include <sys/types.h>
    #include <sys/stat.h>
#endif

#if defined(__WINDOWS__)
    #include "wx/msw/private.h"
    #include "wx/filesys.h"
#endif

#if wxUSE_GUI && defined(__WXGTK__)
    #include <gtk/gtk.h>    // for GTK_XXX_VERSION constants
#endif

#if wxUSE_BASE

// ============================================================================
// implementation
// ============================================================================

// Array used in DecToHex conversion routine.
static const wxChar hexArray[] = wxT("0123456789ABCDEF");

// Convert 2-digit hex number to decimal
int wxHexToDec(const wxString& str)
{
    char buf[2];
    buf[0] = str.GetChar(0);
    buf[1] = str.GetChar(1);
    return wxHexToDec((const char*) buf);
}

// Convert decimal integer to 2-character hex string
void wxDecToHex(int dec, wxChar *buf)
{
    int firstDigit = (int)(dec/16.0);
    int secondDigit = (int)(dec - (firstDigit*16.0));
    buf[0] = hexArray[firstDigit];
    buf[1] = hexArray[secondDigit];
    buf[2] = 0;
}

// Convert decimal integer to 2 characters
void wxDecToHex(int dec, char* ch1, char* ch2)
{
    int firstDigit = (int)(dec/16.0);
    int secondDigit = (int)(dec - (firstDigit*16.0));
    (*ch1) = (char) hexArray[firstDigit];
    (*ch2) = (char) hexArray[secondDigit];
}

// Convert decimal integer to 2-character hex string
wxString wxDecToHex(int dec)
{
    wxChar buf[3];
    wxDecToHex(dec, buf);
    return wxString(buf);
}

// ----------------------------------------------------------------------------
// misc functions
// ----------------------------------------------------------------------------

// Return the current date/time
wxString wxNow()
{
#ifdef __WXWINCE__
#if wxUSE_DATETIME
    wxDateTime now = wxDateTime::Now();
    return now.Format();
#else
    return wxEmptyString;
#endif
#else
    time_t now = time(NULL);
    char *date = ctime(&now);
    date[24] = '\0';
    return wxString::FromAscii(date);
#endif
}

#if WXWIN_COMPATIBILITY_2_8
void wxUsleep(unsigned long milliseconds)
{
    wxMilliSleep(milliseconds);
}
#endif

const wxChar *wxGetInstallPrefix()
{
    wxString prefix;

    if ( wxGetEnv(wxT("WXPREFIX"), &prefix) )
        return prefix.c_str();

#ifdef wxINSTALL_PREFIX
    return wxT(wxINSTALL_PREFIX);
#else
    return wxEmptyString;
#endif
}

wxString wxGetDataDir()
{
    wxString dir = wxGetInstallPrefix();
    dir <<  wxFILE_SEP_PATH << wxT("share") << wxFILE_SEP_PATH << wxT("wx");
    return dir;
}

bool wxIsPlatformLittleEndian()
{
    // Are we little or big endian? This method is from Harbison & Steele.
    union
    {
        long l;
        char c[sizeof(long)];
    } u;
    u.l = 1;

    return u.c[0] == 1;
}


// ----------------------------------------------------------------------------
// wxPlatform
// ----------------------------------------------------------------------------

/*
 * Class to make it easier to specify platform-dependent values
 */

wxArrayInt*  wxPlatform::sm_customPlatforms = NULL;

void wxPlatform::Copy(const wxPlatform& platform)
{
    m_longValue = platform.m_longValue;
    m_doubleValue = platform.m_doubleValue;
    m_stringValue = platform.m_stringValue;
}

wxPlatform wxPlatform::If(int platform, long value)
{
    if (Is(platform))
        return wxPlatform(value);
    else
        return wxPlatform();
}

wxPlatform wxPlatform::IfNot(int platform, long value)
{
    if (!Is(platform))
        return wxPlatform(value);
    else
        return wxPlatform();
}

wxPlatform& wxPlatform::ElseIf(int platform, long value)
{
    if (Is(platform))
        m_longValue = value;
    return *this;
}

wxPlatform& wxPlatform::ElseIfNot(int platform, long value)
{
    if (!Is(platform))
        m_longValue = value;
    return *this;
}

wxPlatform wxPlatform::If(int platform, double value)
{
    if (Is(platform))
        return wxPlatform(value);
    else
        return wxPlatform();
}

wxPlatform wxPlatform::IfNot(int platform, double value)
{
    if (!Is(platform))
        return wxPlatform(value);
    else
        return wxPlatform();
}

wxPlatform& wxPlatform::ElseIf(int platform, double value)
{
    if (Is(platform))
        m_doubleValue = value;
    return *this;
}

wxPlatform& wxPlatform::ElseIfNot(int platform, double value)
{
    if (!Is(platform))
        m_doubleValue = value;
    return *this;
}

wxPlatform wxPlatform::If(int platform, const wxString& value)
{
    if (Is(platform))
        return wxPlatform(value);
    else
        return wxPlatform();
}

wxPlatform wxPlatform::IfNot(int platform, const wxString& value)
{
    if (!Is(platform))
        return wxPlatform(value);
    else
        return wxPlatform();
}

wxPlatform& wxPlatform::ElseIf(int platform, const wxString& value)
{
    if (Is(platform))
        m_stringValue = value;
    return *this;
}

wxPlatform& wxPlatform::ElseIfNot(int platform, const wxString& value)
{
    if (!Is(platform))
        m_stringValue = value;
    return *this;
}

wxPlatform& wxPlatform::Else(long value)
{
    m_longValue = value;
    return *this;
}

wxPlatform& wxPlatform::Else(double value)
{
    m_doubleValue = value;
    return *this;
}

wxPlatform& wxPlatform::Else(const wxString& value)
{
    m_stringValue = value;
    return *this;
}

void wxPlatform::AddPlatform(int platform)
{
    if (!sm_customPlatforms)
        sm_customPlatforms = new wxArrayInt;
    sm_customPlatforms->Add(platform);
}

void wxPlatform::ClearPlatforms()
{
    wxDELETE(sm_customPlatforms);
}

/// Function for testing current platform

bool wxPlatform::Is(int platform)
{
#ifdef __WINDOWS__
    if (platform == wxOS_WINDOWS)
        return true;
#endif
#ifdef __WXWINCE__
    if (platform == wxOS_WINDOWS_CE)
        return true;
#endif

#if 0

// FIXME: wxWinPocketPC and wxWinSmartPhone are unknown symbols

#if defined(__WXWINCE__) && defined(__POCKETPC__)
    if (platform == wxWinPocketPC)
        return true;
#endif
#if defined(__WXWINCE__) && defined(__SMARTPHONE__)
    if (platform == wxWinSmartPhone)
        return true;
#endif

#endif

#ifdef __WXGTK__
    if (platform == wxPORT_GTK)
        return true;
#endif
#ifdef __WXMAC__
    if (platform == wxPORT_MAC)
        return true;
#endif
#ifdef __WXX11__
    if (platform == wxPORT_X11)
        return true;
#endif
#ifdef __UNIX__
    if (platform == wxOS_UNIX)
        return true;
#endif
#ifdef __OS2__
    if (platform == wxOS_OS2)
        return true;
#endif
#ifdef __WXPM__
    if (platform == wxPORT_PM)
        return true;
#endif
#ifdef __WXCOCOA__
    if (platform == wxPORT_MAC)
        return true;
#endif

    if (sm_customPlatforms && sm_customPlatforms->Index(platform) != wxNOT_FOUND)
        return true;

    return false;
}

// ----------------------------------------------------------------------------
// network and user id functions
// ----------------------------------------------------------------------------

// Get Full RFC822 style email address
bool wxGetEmailAddress(wxChar *address, int maxSize)
{
    wxString email = wxGetEmailAddress();
    if ( !email )
        return false;

    wxStrlcpy(address, email.t_str(), maxSize);

    return true;
}

wxString wxGetEmailAddress()
{
    wxString email;

    wxString host = wxGetFullHostName();
    if ( !host.empty() )
    {
        wxString user = wxGetUserId();
        if ( !user.empty() )
        {
            email << user << wxT('@') << host;
        }
    }

    return email;
}

wxString wxGetUserId()
{
    static const int maxLoginLen = 256; // FIXME arbitrary number

    wxString buf;
    bool ok = wxGetUserId(wxStringBuffer(buf, maxLoginLen), maxLoginLen);

    if ( !ok )
        buf.Empty();

    return buf;
}

wxString wxGetUserName()
{
    static const int maxUserNameLen = 1024; // FIXME arbitrary number

    wxString buf;
    bool ok = wxGetUserName(wxStringBuffer(buf, maxUserNameLen), maxUserNameLen);

    if ( !ok )
        buf.Empty();

    return buf;
}

wxString wxGetHostName()
{
    static const size_t hostnameSize = 257;

    wxString buf;
    bool ok = wxGetHostName(wxStringBuffer(buf, hostnameSize), hostnameSize);

    if ( !ok )
        buf.Empty();

    return buf;
}

wxString wxGetFullHostName()
{
    static const size_t hostnameSize = 257;

    wxString buf;
    bool ok = wxGetFullHostName(wxStringBuffer(buf, hostnameSize), hostnameSize);

    if ( !ok )
        buf.Empty();

    return buf;
}

wxString wxGetHomeDir()
{
    wxString home;
    wxGetHomeDir(&home);

    return home;
}

#if 0

wxString wxGetCurrentDir()
{
    wxString dir;
    size_t len = 1024;
    bool ok;
    do
    {
        ok = getcwd(dir.GetWriteBuf(len + 1), len) != NULL;
        dir.UngetWriteBuf();

        if ( !ok )
        {
            if ( errno != ERANGE )
            {
                wxLogSysError(wxT("Failed to get current directory"));

                return wxEmptyString;
            }
            else
            {
                // buffer was too small, retry with a larger one
                len *= 2;
            }
        }
        //else: ok
    } while ( !ok );

    return dir;
}

#endif // 0

// ----------------------------------------------------------------------------
// Environment
// ----------------------------------------------------------------------------

#ifdef __WXOSX__
#if wxOSX_USE_COCOA_OR_CARBON
    #include <crt_externs.h>
#endif
#endif

bool wxGetEnvMap(wxEnvVariableHashMap *map)
{
    wxCHECK_MSG( map, false, wxS("output pointer can't be NULL") );

#if defined(__VISUALC__)
    // This variable only exists to force the CRT to fill the wide char array,
    // it might only have it in narrow char version until now as we use main()
    // (and not _wmain()) as our entry point.
    static wxChar* s_dummyEnvVar = _tgetenv(wxT("TEMP"));

    wxChar **env = _tenviron;
#elif defined(__VMS)
   // Now this routine wil give false for OpenVMS
   // TODO : should we do something with logicals?
    char **env=NULL;
#elif defined(__DARWIN__)
#if wxOSX_USE_COCOA_OR_CARBON
    // Under Mac shared libraries don't have access to the global environ
    // variable so use this Mac-specific function instead as advised by
    // environ(7) under Darwin
    char ***penv = _NSGetEnviron();
    if ( !penv )
        return false;
    char **env = *penv;
#else
    char **env=NULL;
    // todo translate NSProcessInfo environment into map
#endif
#else // non-MSVC non-Mac
    // Not sure if other compilers have _tenviron so use the (more standard)
    // ANSI version only for them.

    // Both POSIX and Single UNIX Specification say that this variable must
    // exist but not that it must be declared anywhere and, indeed, it's not
    // declared in several common systems (some BSDs, Solaris with native CC)
    // so we (re)declare it ourselves to deal with these cases. However we do
    // not do this under MSW where there can be DLL-related complications, i.e.
    // the variable might be DLL-imported or not. Luckily we don't have to
    // worry about this as all MSW compilers do seem to define it in their
    // standard headers anyhow so we can just rely on already having the
    // correct declaration. And if this turns out to be wrong, we can always
    // add a configure test checking whether it is declared later.
#ifndef __WINDOWS__
    extern char **environ;
#endif // !__WINDOWS__

    char **env = environ;
#endif

    if ( env )
    {
        wxString name,
                 value;
        while ( *env )
        {
            const wxString var(*env);

            name = var.BeforeFirst(wxS('='), &value);

            (*map)[name] = value;

            env++;
        }

        return true;
    }

    return false;
}

// ----------------------------------------------------------------------------
// wxExecute
// ----------------------------------------------------------------------------

// wxDoExecuteWithCapture() helper: reads an entire stream into one array if
// the stream is non-NULL (it doesn't do anything if it's NULL).
//
// returns true if ok, false if error
#if wxUSE_STREAMS
static bool ReadAll(wxInputStream *is, wxArrayString& output)
{
    if ( !is )
        return true;

    // the stream could be already at EOF or in wxSTREAM_BROKEN_PIPE state
    is->Reset();

    // Notice that wxTextInputStream doesn't work correctly with wxConvAuto
    // currently, see #14720, so use the current locale conversion explicitly
    // under assumption that any external program should be using it too.
    wxTextInputStream tis(*is, " \t"
#if wxUSE_UNICODE
                                    , wxConvLibc
#endif
                                                );

    for ( ;; )
    {
        wxString line = tis.ReadLine();

        // check for EOF before other errors as it's not really an error
        if ( is->Eof() )
        {
            // add the last, possibly incomplete, line
            if ( !line.empty() )
                output.Add(line);
            break;
        }

        // any other error is fatal
        if ( !*is )
            return false;

        output.Add(line);
    }

    return true;
}
#endif // wxUSE_STREAMS

// this is a private function because it hasn't a clean interface: the first
// array is passed by reference, the second by pointer - instead we have 2
// public versions of wxExecute() below
static long wxDoExecuteWithCapture(const wxString& command,
                                   wxArrayString& output,
                                   wxArrayString* error,
                                   int flags,
                                   const wxExecuteEnv *env)
{
    // create a wxProcess which will capture the output
    wxProcess *process = new wxProcess;
    process->Redirect();

    long rc = wxExecute(command, wxEXEC_SYNC | flags, process, env);

#if wxUSE_STREAMS
    // Notice that while -1 indicates an error exit code for us, a program
    // exiting with this code could still have written something to its stdout
    // and, especially, stderr, so we still need to read from them.
    if ( !ReadAll(process->GetInputStream(), output) )
        rc = -1;

    if ( error )
    {
        if ( !ReadAll(process->GetErrorStream(), *error) )
            rc = -1;
    }
#else
    wxUnusedVar(output);
    wxUnusedVar(error);
#endif // wxUSE_STREAMS/!wxUSE_STREAMS

    delete process;

    return rc;
}

long wxExecute(const wxString& command, wxArrayString& output, int flags,
               const wxExecuteEnv *env)
{
    return wxDoExecuteWithCapture(command, output, NULL, flags, env);
}

long wxExecute(const wxString& command,
               wxArrayString& output,
               wxArrayString& error,
               int flags,
               const wxExecuteEnv *env)
{
    return wxDoExecuteWithCapture(command, output, &error, flags, env);
}

// ----------------------------------------------------------------------------
// Id functions
// ----------------------------------------------------------------------------

// Id generation
static int wxCurrentId = 100;

int wxNewId()
{
    // skip the part of IDs space that contains hard-coded values:
    if (wxCurrentId == wxID_LOWEST)
        wxCurrentId = wxID_HIGHEST + 1;

    return wxCurrentId++;
}

int
wxGetCurrentId(void) { return wxCurrentId; }

void
wxRegisterId (int id)
{
  if (id >= wxCurrentId)
    wxCurrentId = id + 1;
}

// ----------------------------------------------------------------------------
// wxQsort, adapted by RR to allow user_data
// ----------------------------------------------------------------------------

/* This file is part of the GNU C Library.
   Written by Douglas C. Schmidt (schmidt@ics.uci.edu).

   Douglas Schmidt kindly gave permission to relicence the
   code under the wxWindows licence:

From: "Douglas C. Schmidt" <schmidt@dre.vanderbilt.edu>
To: Robert Roebling <robert.roebling@uni-ulm.de>
Subject: Re: qsort licence
Date: Mon, 23 Jul 2007 03:44:25 -0500
Sender: schmidt@dre.vanderbilt.edu
Message-Id: <20070723084426.64F511000A8@tango.dre.vanderbilt.edu>

Hi Robert,

> [...] I'm asking if you'd be willing to relicence your code
> under the wxWindows licence. [...]

That's fine with me [...]

Thanks,

     Doug */


/* Byte-wise swap two items of size SIZE. */
#define SWAP(a, b, size)                                                      \
  do                                                                          \
    {                                                                         \
      size_t __size = (size);                                                 \
      char *__a = (a), *__b = (b);                                            \
      do                                                                      \
        {                                                                     \
          char __tmp = *__a;                                                  \
          *__a++ = *__b;                                                      \
          *__b++ = __tmp;                                                     \
        } while (--__size > 0);                                               \
    } while (0)

/* Discontinue quicksort algorithm when partition gets below this size.
   This particular magic number was chosen to work best on a Sun 4/260. */
#define MAX_THRESH 4

/* Stack node declarations used to store unfulfilled partition obligations. */
typedef struct
  {
    char *lo;
    char *hi;
  } stack_node;

/* The next 4 #defines implement a very fast in-line stack abstraction. */
#define STACK_SIZE        (8 * sizeof(unsigned long int))
#define PUSH(low, high)   ((void) ((top->lo = (low)), (top->hi = (high)), ++top))
#define POP(low, high)    ((void) (--top, (low = top->lo), (high = top->hi)))
#define STACK_NOT_EMPTY   (stack < top)


/* Order size using quicksort.  This implementation incorporates
   four optimizations discussed in Sedgewick:

   1. Non-recursive, using an explicit stack of pointer that store the
      next array partition to sort.  To save time, this maximum amount
      of space required to store an array of MAX_INT is allocated on the
      stack.  Assuming a 32-bit integer, this needs only 32 *
      sizeof(stack_node) == 136 bits.  Pretty cheap, actually.

   2. Chose the pivot element using a median-of-three decision tree.
      This reduces the probability of selecting a bad pivot value and
      eliminates certain extraneous comparisons.

   3. Only quicksorts TOTAL_ELEMS / MAX_THRESH partitions, leaving
      insertion sort to order the MAX_THRESH items within each partition.
      This is a big win, since insertion sort is faster for small, mostly
      sorted array segments.

   4. The larger of the two sub-partitions is always pushed onto the
      stack first, with the algorithm then concentrating on the
      smaller partition.  This *guarantees* no more than log (n)
      stack size is needed (actually O(1) in this case)!  */

void wxQsort(void* pbase, size_t total_elems,
             size_t size, wxSortCallback cmp, const void* user_data)
{
  char *base_ptr = (char *) pbase;
  const size_t max_thresh = MAX_THRESH * size;

  if (total_elems == 0)
    /* Avoid lossage with unsigned arithmetic below.  */
    return;

  if (total_elems > MAX_THRESH)
    {
      char *lo = base_ptr;
      char *hi = &lo[size * (total_elems - 1)];
      stack_node stack[STACK_SIZE];
      stack_node *top = stack;

      PUSH (NULL, NULL);

      while (STACK_NOT_EMPTY)
        {
          char *left_ptr;
          char *right_ptr;

          /* Select median value from among LO, MID, and HI. Rearrange
             LO and HI so the three values are sorted. This lowers the
             probability of picking a pathological pivot value and
             skips a comparison for both the LEFT_PTR and RIGHT_PTR. */

          char *mid = lo + size * ((hi - lo) / size >> 1);

          if ((*cmp) ((void *) mid, (void *) lo, user_data) < 0)
            SWAP (mid, lo, size);
          if ((*cmp) ((void *) hi, (void *) mid, user_data) < 0)
            SWAP (mid, hi, size);
          else
            goto jump_over;
          if ((*cmp) ((void *) mid, (void *) lo, user_data) < 0)
            SWAP (mid, lo, size);
        jump_over:;
          left_ptr  = lo + size;
          right_ptr = hi - size;

          /* Here's the famous ``collapse the walls'' section of quicksort.
             Gotta like those tight inner loops!  They are the main reason
             that this algorithm runs much faster than others. */
          do
            {
              while ((*cmp) ((void *) left_ptr, (void *) mid, user_data) < 0)
                left_ptr += size;

              while ((*cmp) ((void *) mid, (void *) right_ptr, user_data) < 0)
                right_ptr -= size;

              if (left_ptr < right_ptr)
                {
                  SWAP (left_ptr, right_ptr, size);
                  if (mid == left_ptr)
                    mid = right_ptr;
                  else if (mid == right_ptr)
                    mid = left_ptr;
                  left_ptr += size;
                  right_ptr -= size;
                }
              else if (left_ptr == right_ptr)
                {
                  left_ptr += size;
                  right_ptr -= size;
                  break;
                }
            }
          while (left_ptr <= right_ptr);

          /* Set up pointers for next iteration.  First determine whether
             left and right partitions are below the threshold size.  If so,
             ignore one or both.  Otherwise, push the larger partition's
             bounds on the stack and continue sorting the smaller one. */

          if ((size_t) (right_ptr - lo) <= max_thresh)
            {
              if ((size_t) (hi - left_ptr) <= max_thresh)
                /* Ignore both small partitions. */
                POP (lo, hi);
              else
                /* Ignore small left partition. */
                lo = left_ptr;
            }
          else if ((size_t) (hi - left_ptr) <= max_thresh)
            /* Ignore small right partition. */
            hi = right_ptr;
          else if ((right_ptr - lo) > (hi - left_ptr))
            {
              /* Push larger left partition indices. */
              PUSH (lo, right_ptr);
              lo = left_ptr;
            }
          else
            {
              /* Push larger right partition indices. */
              PUSH (left_ptr, hi);
              hi = right_ptr;
            }
        }
    }

  /* Once the BASE_PTR array is partially sorted by quicksort the rest
     is completely sorted using insertion sort, since this is efficient
     for partitions below MAX_THRESH size. BASE_PTR points to the beginning
     of the array to sort, and END_PTR points at the very last element in
     the array (*not* one beyond it!). */

  {
    char *const end_ptr = &base_ptr[size * (total_elems - 1)];
    char *tmp_ptr = base_ptr;
    char *thresh = base_ptr + max_thresh;
    if ( thresh > end_ptr )
        thresh = end_ptr;
    char *run_ptr;

    /* Find smallest element in first threshold and place it at the
       array's beginning.  This is the smallest array element,
       and the operation speeds up insertion sort's inner loop. */

    for (run_ptr = tmp_ptr + size; run_ptr <= thresh; run_ptr += size)
      if ((*cmp) ((void *) run_ptr, (void *) tmp_ptr, user_data) < 0)
        tmp_ptr = run_ptr;

    if (tmp_ptr != base_ptr)
      SWAP (tmp_ptr, base_ptr, size);

    /* Insertion sort, running from left-hand-side up to right-hand-side.  */

    run_ptr = base_ptr + size;
    while ((run_ptr += size) <= end_ptr)
      {
        tmp_ptr = run_ptr - size;
        while ((*cmp) ((void *) run_ptr, (void *) tmp_ptr, user_data) < 0)
          tmp_ptr -= size;

        tmp_ptr += size;
        if (tmp_ptr != run_ptr)
          {
            char *trav;

            trav = run_ptr + size;
            while (--trav >= run_ptr)
              {
                char c = *trav;
                char *hi, *lo;

                for (hi = lo = trav; (lo -= size) >= tmp_ptr; hi = lo)
                  *hi = *lo;
                *hi = c;
              }
          }
      }
  }
}

#endif // wxUSE_BASE



// ============================================================================
// GUI-only functions from now on
// ============================================================================

#if wxUSE_GUI

// this function is only really implemented for X11-based ports, including GTK1
// (GTK2 sets detectable auto-repeat automatically anyhow)
#if !(defined(__WXX11__) || defined(__WXMOTIF__) || \
        (defined(__WXGTK__) && !defined(__WXGTK20__)))
bool wxSetDetectableAutoRepeat( bool WXUNUSED(flag) )
{
    return true;
}
#endif // !X11-based port

// ----------------------------------------------------------------------------
// Launch default browser
// ----------------------------------------------------------------------------

#if defined(__WINDOWS__)

// implemented in a port-specific utils source file:
bool wxDoLaunchDefaultBrowser(const wxString& url, const wxString& scheme, int flags);

#elif defined(__WXX11__) || defined(__WXGTK__) || defined(__WXMOTIF__) || defined(__WXCOCOA__) || \
      (defined(__WXOSX__) )

// implemented in a port-specific utils source file:
bool wxDoLaunchDefaultBrowser(const wxString& url, int flags);

#else

// a "generic" implementation:
bool wxDoLaunchDefaultBrowser(const wxString& url, int flags)
{
    // on other platforms try to use mime types or wxExecute...

    bool ok = false;
    wxString cmd;

#if wxUSE_MIMETYPE
    wxFileType *ft = wxTheMimeTypesManager->GetFileTypeFromExtension(wxT("html"));
    if ( ft )
    {
        wxString mt;
        ft->GetMimeType(&mt);

        ok = ft->GetOpenCommand(&cmd, wxFileType::MessageParameters(url));
        delete ft;
    }
#endif // wxUSE_MIMETYPE

    if ( !ok || cmd.empty() )
    {
        // fallback to checking for the BROWSER environment variable
        if ( !wxGetEnv(wxT("BROWSER"), &cmd) || cmd.empty() )
            cmd << wxT(' ') << url;
    }

    ok = ( !cmd.empty() && wxExecute(cmd) );
    if (ok)
        return ok;

    // no file type for HTML extension
    wxLogError(_("No default application configured for HTML files."));

    return false;
}
#endif

static bool DoLaunchDefaultBrowserHelper(const wxString& urlOrig, int flags)
{
    // NOTE: we don't have to care about the wxBROWSER_NOBUSYCURSOR flag
    //       as it was already handled by wxLaunchDefaultBrowser

    wxUnusedVar(flags);

    wxString url(urlOrig), scheme;
    wxURI uri(url);

    // this check is useful to avoid that wxURI recognizes as scheme parts of
    // the filename, in case urlOrig is a local filename
    // (e.g. "C:\\test.txt" when parsed by wxURI reports a scheme == "C")
    bool hasValidScheme = uri.HasScheme() && uri.GetScheme().length() > 1;

#if defined(__WINDOWS__)

    // NOTE: when testing wxMSW's wxLaunchDefaultBrowser all possible forms
    //       of the URL/flags should be tested; e.g.:
    //
    // for (int i=0; i<2; i++)
    // {
    //   // test arguments without a valid URL scheme:
    //   wxLaunchDefaultBrowser("C:\\test.txt", i==0 ? 0 : wxBROWSER_NEW_WINDOW);
    //   wxLaunchDefaultBrowser("wxwidgets.org", i==0 ? 0 : wxBROWSER_NEW_WINDOW);
    //
    //   // test arguments with different valid schemes:
    //   wxLaunchDefaultBrowser("file:/C%3A/test.txt", i==0 ? 0 : wxBROWSER_NEW_WINDOW);
    //   wxLaunchDefaultBrowser("http://wxwidgets.org", i==0 ? 0 : wxBROWSER_NEW_WINDOW);
    //   wxLaunchDefaultBrowser("mailto:user@host.org", i==0 ? 0 : wxBROWSER_NEW_WINDOW);
    // }
    // (assuming you have a C:\test.txt file)

    if ( !hasValidScheme )
    {
        if (wxFileExists(urlOrig) || wxDirExists(urlOrig))
        {
            scheme = "file";
            // do not prepend the file scheme to the URL as ShellExecuteEx() doesn't like it
        }
        else
        {
            url.Prepend(wxS("http://"));
            scheme = "http";
        }
    }
    else if ( hasValidScheme )
    {
        scheme = uri.GetScheme();

        if ( uri.GetScheme() == "file" )
        {
            // TODO: extract URLToFileName() to some always compiled in
            //       function
#if wxUSE_FILESYSTEM
            // ShellExecuteEx() doesn't like the "file" scheme when opening local files;
            // remove it
            url = wxFileSystem::URLToFileName(url).GetFullPath();
#endif // wxUSE_FILESYSTEM
        }
    }

    if (wxDoLaunchDefaultBrowser(url, scheme, flags))
        return true;
    //else: call wxLogSysError
#else
    if ( !hasValidScheme )
    {
        // set the scheme of url to "http" or "file" if it does not have one
        if (wxFileExists(urlOrig) || wxDirExists(urlOrig))
            url.Prepend(wxS("file://"));
        else
            url.Prepend(wxS("http://"));
    }

    if (wxDoLaunchDefaultBrowser(url, flags))
        return true;
    //else: call wxLogSysError
#endif

    wxLogSysError(_("Failed to open URL \"%s\" in default browser."),
                  url.c_str());

    return false;
}

bool wxLaunchDefaultBrowser(const wxString& url, int flags)
{
    // NOTE: as documented, "url" may be both a real well-formed URL
    //       and a local file name

    if ( flags & wxBROWSER_NOBUSYCURSOR )
        return DoLaunchDefaultBrowserHelper(url, flags);

    wxBusyCursor bc;
    return DoLaunchDefaultBrowserHelper(url, flags);
}

// ----------------------------------------------------------------------------
// Menu accelerators related functions
// ----------------------------------------------------------------------------

#if WXWIN_COMPATIBILITY_2_6
wxChar *wxStripMenuCodes(const wxChar *in, wxChar *out)
{
#if wxUSE_MENUS
    wxString s = wxMenuItem::GetLabelText(in);
#else
    wxString str(in);
    wxString s = wxStripMenuCodes(str);
#endif // wxUSE_MENUS
    if ( out )
    {
        // go smash their buffer if it's not big enough - I love char * params
        memcpy(out, s.c_str(), s.length() * sizeof(wxChar));
    }
    else
    {
        out = new wxChar[s.length() + 1];
        wxStrcpy(out, s.c_str());
    }

    return out;
}
#endif

wxString wxStripMenuCodes(const wxString& in, int flags)
{
    wxASSERT_MSG( flags, wxT("this is useless to call without any flags") );

    wxString out;

    size_t len = in.length();
    out.reserve(len);

    for ( wxString::const_iterator it = in.begin(); it != in.end(); ++it )
    {
        wxChar ch = *it;
        if ( (flags & wxStrip_Mnemonics) && ch == wxT('&') )
        {
            // skip it, it is used to introduce the accel char (or to quote
            // itself in which case it should still be skipped): note that it
            // can't be the last character of the string
            if ( ++it == in.end() )
            {
                wxLogDebug(wxT("Invalid menu string '%s'"), in.c_str());
                break;
            }
            else
            {
                // use the next char instead
                ch = *it;
            }
        }
        else if ( (flags & wxStrip_Accel) && ch == wxT('\t') )
        {
            // everything after TAB is accel string, exit the loop
            break;
        }

        out += ch;
    }

    return out;
}

// ----------------------------------------------------------------------------
// Window search functions
// ----------------------------------------------------------------------------

/*
 * If parent is non-NULL, look through children for a label or title
 * matching the specified string. If NULL, look through all top-level windows.
 *
 */

wxWindow *
wxFindWindowByLabel (const wxString& title, wxWindow * parent)
{
    return wxWindow::FindWindowByLabel( title, parent );
}


/*
 * If parent is non-NULL, look through children for a name
 * matching the specified string. If NULL, look through all top-level windows.
 *
 */

wxWindow *
wxFindWindowByName (const wxString& name, wxWindow * parent)
{
    return wxWindow::FindWindowByName( name, parent );
}

// Returns menu item id or wxNOT_FOUND if none.
int
wxFindMenuItemId(wxFrame *frame,
                 const wxString& menuString,
                 const wxString& itemString)
{
#if wxUSE_MENUS
    wxMenuBar *menuBar = frame->GetMenuBar ();
    if ( menuBar )
        return menuBar->FindMenuItem (menuString, itemString);
#else // !wxUSE_MENUS
    wxUnusedVar(frame);
    wxUnusedVar(menuString);
    wxUnusedVar(itemString);
#endif // wxUSE_MENUS/!wxUSE_MENUS

    return wxNOT_FOUND;
}

// Try to find the deepest child that contains 'pt'.
// We go backwards, to try to allow for controls that are spacially
// within other controls, but are still siblings (e.g. buttons within
// static boxes). Static boxes are likely to be created _before_ controls
// that sit inside them.
wxWindow* wxFindWindowAtPoint(wxWindow* win, const wxPoint& pt)
{
    if (!win->IsShown())
        return NULL;

    // Hack for wxNotebook case: at least in wxGTK, all pages
    // claim to be shown, so we must only deal with the selected one.
#if wxUSE_NOTEBOOK
    if (wxDynamicCast(win, wxNotebook))
    {
      wxNotebook* nb = (wxNotebook*) win;
      int sel = nb->GetSelection();
      if (sel >= 0)
      {
        wxWindow* child = nb->GetPage(sel);
        wxWindow* foundWin = wxFindWindowAtPoint(child, pt);
        if (foundWin)
           return foundWin;
      }
    }
#endif

    wxWindowList::compatibility_iterator node = win->GetChildren().GetLast();
    while (node)
    {
        wxWindow* child = node->GetData();
        wxWindow* foundWin = wxFindWindowAtPoint(child, pt);
        if (foundWin)
          return foundWin;
        node = node->GetPrevious();
    }

    wxPoint pos = win->GetPosition();
    wxSize sz = win->GetSize();
    if ( !win->IsTopLevel() && win->GetParent() )
    {
        pos = win->GetParent()->ClientToScreen(pos);
    }

    wxRect rect(pos, sz);
    if (rect.Contains(pt))
        return win;

    return NULL;
}

wxWindow* wxGenericFindWindowAtPoint(const wxPoint& pt)
{
    // Go backwards through the list since windows
    // on top are likely to have been appended most
    // recently.
    wxWindowList::compatibility_iterator node = wxTopLevelWindows.GetLast();
    while (node)
    {
        wxWindow* win = node->GetData();
        wxWindow* found = wxFindWindowAtPoint(win, pt);
        if (found)
            return found;
        node = node->GetPrevious();
    }
    return NULL;
}

// ----------------------------------------------------------------------------
// GUI helpers
// ----------------------------------------------------------------------------

/*
 * N.B. these convenience functions must be separate from msgdlgg.cpp, textdlgg.cpp
 * since otherwise the generic code may be pulled in unnecessarily.
 */

#if wxUSE_MSGDLG

int wxMessageBox(const wxString& message, const wxString& caption, long style,
                 wxWindow *parent, int WXUNUSED(x), int WXUNUSED(y) )
{
    // add the appropriate icon unless this was explicitly disabled by use of
    // wxICON_NONE
    if ( !(style & wxICON_NONE) && !(style & wxICON_MASK) )
    {
        style |= style & wxYES ? wxICON_QUESTION : wxICON_INFORMATION;
    }

    wxMessageDialog dialog(parent, message, caption, style);

    int ans = dialog.ShowModal();
    switch ( ans )
    {
        case wxID_OK:
            return wxOK;
        case wxID_YES:
            return wxYES;
        case wxID_NO:
            return wxNO;
        case wxID_CANCEL:
            return wxCANCEL;
        case wxID_HELP:
            return wxHELP;
    }

    wxFAIL_MSG( wxT("unexpected return code from wxMessageDialog") );

    return wxCANCEL;
}

wxVersionInfo wxGetLibraryVersionInfo()
{
    // don't translate these strings, they're for diagnostics purposes only
    wxString msg;
    msg.Printf(wxS("wxWidgets Library (%s port)\n")
               wxS("Version %d.%d.%d (Unicode: %s, debug level: %d),\n")
               wxS("compiled at %s %s\n\n")
               wxS("Runtime version of toolkit used is %d.%d.\n"),
               wxPlatformInfo::Get().GetPortIdName(),
               wxMAJOR_VERSION,
               wxMINOR_VERSION,
               wxRELEASE_NUMBER,
#if wxUSE_UNICODE_UTF8
               "UTF-8",
#elif wxUSE_UNICODE
               "wchar_t",
#else
               "none",
#endif
               wxDEBUG_LEVEL,
               __TDATE__,
               __TTIME__,
               wxPlatformInfo::Get().GetToolkitMajorVersion(),
               wxPlatformInfo::Get().GetToolkitMinorVersion()
              );

#ifdef __WXGTK__
    msg += wxString::Format("Compile-time GTK+ version is %d.%d.%d.\n",
                            GTK_MAJOR_VERSION,
                            GTK_MINOR_VERSION,
                            GTK_MICRO_VERSION);
#endif // __WXGTK__

    return wxVersionInfo(wxS("wxWidgets"),
                         wxMAJOR_VERSION,
                         wxMINOR_VERSION,
                         wxRELEASE_NUMBER,
                         msg,
                         wxS("Copyright (c) 1995-2013 wxWidgets team"));
}

void wxInfoMessageBox(wxWindow* parent)
{
    wxVersionInfo info = wxGetLibraryVersionInfo();
    wxString msg = info.ToString();

    msg << wxS("\n") << info.GetCopyright();

    wxMessageBox(msg, wxT("wxWidgets information"),
                 wxICON_INFORMATION | wxOK,
                 parent);
}

#endif // wxUSE_MSGDLG

#if wxUSE_TEXTDLG

wxString wxGetTextFromUser(const wxString& message, const wxString& caption,
                        const wxString& defaultValue, wxWindow *parent,
                        wxCoord x, wxCoord y, bool centre )
{
    wxString str;
    long style = wxTextEntryDialogStyle;

    if (centre)
        style |= wxCENTRE;
    else
        style &= ~wxCENTRE;

    wxTextEntryDialog dialog(parent, message, caption, defaultValue, style, wxPoint(x, y));

    if (dialog.ShowModal() == wxID_OK)
    {
        str = dialog.GetValue();
    }

    return str;
}

wxString wxGetPasswordFromUser(const wxString& message,
                               const wxString& caption,
                               const wxString& defaultValue,
                               wxWindow *parent,
                               wxCoord x, wxCoord y, bool centre )
{
    wxString str;
    long style = wxTextEntryDialogStyle;

    if (centre)
        style |= wxCENTRE;
    else
        style &= ~wxCENTRE;

    wxPasswordEntryDialog dialog(parent, message, caption, defaultValue,
                             style, wxPoint(x, y));
    if ( dialog.ShowModal() == wxID_OK )
    {
        str = dialog.GetValue();
    }

    return str;
}

#endif // wxUSE_TEXTDLG

// ----------------------------------------------------------------------------
// wxSafeYield and supporting functions
// ----------------------------------------------------------------------------

void wxEnableTopLevelWindows(bool enable)
{
    wxWindowList::compatibility_iterator node;
    for ( node = wxTopLevelWindows.GetFirst(); node; node = node->GetNext() )
        node->GetData()->Enable(enable);
}

#if defined(__WXOSX__) && wxOSX_USE_COCOA

// defined in evtloop.mm

#else

wxWindowDisabler::wxWindowDisabler(bool disable)
{
    m_disabled = disable;
    if ( disable )
        DoDisable();
}

wxWindowDisabler::wxWindowDisabler(wxWindow *winToSkip)
{
    m_disabled = true;
    DoDisable(winToSkip);
}

void wxWindowDisabler::DoDisable(wxWindow *winToSkip)
{
    // remember the top level windows which were already disabled, so that we
    // don't reenable them later
    m_winDisabled = NULL;

    wxWindowList::compatibility_iterator node;
    for ( node = wxTopLevelWindows.GetFirst(); node; node = node->GetNext() )
    {
        wxWindow *winTop = node->GetData();
        if ( winTop == winToSkip )
            continue;

        // we don't need to disable the hidden or already disabled windows
        if ( winTop->IsEnabled() && winTop->IsShown() )
        {
            winTop->Disable();
        }
        else
        {
            if ( !m_winDisabled )
            {
                m_winDisabled = new wxWindowList;
            }

            m_winDisabled->Append(winTop);
        }
    }
}

wxWindowDisabler::~wxWindowDisabler()
{
    if ( !m_disabled )
        return;

    wxWindowList::compatibility_iterator node;
    for ( node = wxTopLevelWindows.GetFirst(); node; node = node->GetNext() )
    {
        wxWindow *winTop = node->GetData();
        if ( !m_winDisabled || !m_winDisabled->Find(winTop) )
        {
            winTop->Enable();
        }
        //else: had been already disabled, don't reenable
    }

    delete m_winDisabled;
}

#endif

// Yield to other apps/messages and disable user input to all windows except
// the given one
bool wxSafeYield(wxWindow *win, bool onlyIfNeeded)
{
    wxWindowDisabler wd(win);

    bool rc;
    if (onlyIfNeeded)
        rc = wxYieldIfNeeded();
    else
        rc = wxYield();

    return rc;
}

// ----------------------------------------------------------------------------
// wxApp::Yield() wrappers for backwards compatibility
// ----------------------------------------------------------------------------

bool wxYield()
{
    return wxTheApp && wxTheApp->Yield();
}

bool wxYieldIfNeeded()
{
    return wxTheApp && wxTheApp->Yield(true);
}

#endif // wxUSE_GUI
