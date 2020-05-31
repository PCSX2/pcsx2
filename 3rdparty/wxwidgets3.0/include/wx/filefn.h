/////////////////////////////////////////////////////////////////////////////
// Name:        wx/filefn.h
// Purpose:     File- and directory-related functions
// Author:      Julian Smart
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1998 Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef   _FILEFN_H_
#define   _FILEFN_H_

#include "wx/list.h"
#include "wx/arrstr.h"

#ifdef __WXWINCE__
    #include "wx/msw/wince/time.h"
    #include "wx/msw/private.h"
#else
    #include <time.h>
#endif

#ifndef __WXWINCE__
    #include <sys/types.h>
    #include <sys/stat.h>
#endif

#ifdef __OS2__
// need to check for __OS2__ first since currently both
// __OS2__ and __UNIX__ are defined.
    #include <process.h>
    #include "wx/os2/private.h"
    #ifdef __WATCOMC__
        #include <direct.h>
    #endif
    #include <io.h>
    #ifdef __EMX__
        #include <unistd.h>
    #endif
#elif defined(__UNIX__)
    #include <unistd.h>
    #include <dirent.h>
#endif

#if defined(__WINDOWS__) && !defined(__WXMICROWIN__)
#if !defined( __GNUWIN32__ ) && !defined(__WXWINCE__) && !defined(__CYGWIN__)
    #include <direct.h>
    #include <dos.h>
    #include <io.h>
#endif // __WINDOWS__
#endif // native Win compiler

#if defined(__DOS__)
    #ifdef __WATCOMC__
        #include <direct.h>
        #include <dos.h>
        #include <io.h>
    #endif
    #ifdef __DJGPP__
        #include <io.h>
        #include <unistd.h>
    #endif
#endif

#ifdef __BORLANDC__ // Please someone tell me which version of Borland needs
                    // this (3.1 I believe) and how to test for it.
                    // If this works for Borland 4.0 as well, then no worries.
    #include <dir.h>
#endif

#ifndef __WXWINCE__
    #include  <fcntl.h>       // O_RDONLY &c
#endif

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

#if defined(__VISUALC__) || defined(__DIGITALMARS__)
    typedef int mode_t;
#endif

#ifdef __WXWINCE__
    typedef long off_t;
#else
    // define off_t
    #if !defined(__WXMAC__) || defined(__UNIX__) || defined(__MACH__)
        #include  <sys/types.h>
    #else
        typedef long off_t;
    #endif
#endif

#if defined(__VISUALC__) && !defined(__WXWINCE__)
    typedef _off_t off_t;
#elif defined(__SYMANTEC__)
    typedef long off_t;
#endif

enum wxSeekMode
{
  wxFromStart,
  wxFromCurrent,
  wxFromEnd
};

enum wxFileKind
{
  wxFILE_KIND_UNKNOWN,
  wxFILE_KIND_DISK,     // a file supporting seeking to arbitrary offsets
  wxFILE_KIND_TERMINAL, // a tty
  wxFILE_KIND_PIPE      // a pipe
};

// we redefine these constants here because S_IREAD &c are _not_ standard
// however, we do assume that the values correspond to the Unix umask bits
enum wxPosixPermissions
{
    // standard Posix names for these permission flags:
    wxS_IRUSR = 00400,
    wxS_IWUSR = 00200,
    wxS_IXUSR = 00100,

    wxS_IRGRP = 00040,
    wxS_IWGRP = 00020,
    wxS_IXGRP = 00010,

    wxS_IROTH = 00004,
    wxS_IWOTH = 00002,
    wxS_IXOTH = 00001,

    // longer but more readable synonyms for the constants above:
    wxPOSIX_USER_READ = wxS_IRUSR,
    wxPOSIX_USER_WRITE = wxS_IWUSR,
    wxPOSIX_USER_EXECUTE = wxS_IXUSR,

    wxPOSIX_GROUP_READ = wxS_IRGRP,
    wxPOSIX_GROUP_WRITE = wxS_IWGRP,
    wxPOSIX_GROUP_EXECUTE = wxS_IXGRP,

    wxPOSIX_OTHERS_READ = wxS_IROTH,
    wxPOSIX_OTHERS_WRITE = wxS_IWOTH,
    wxPOSIX_OTHERS_EXECUTE = wxS_IXOTH,

    // default mode for the new files: allow reading/writing them to everybody but
    // the effective file mode will be set after anding this value with umask and
    // so won't include wxS_IW{GRP,OTH} for the default 022 umask value
    wxS_DEFAULT = (wxPOSIX_USER_READ | wxPOSIX_USER_WRITE | \
                   wxPOSIX_GROUP_READ | wxPOSIX_GROUP_WRITE | \
                   wxPOSIX_OTHERS_READ | wxPOSIX_OTHERS_WRITE),

    // default mode for the new directories (see wxFileName::Mkdir): allow
    // reading/writing/executing them to everybody, but just like wxS_DEFAULT
    // the effective directory mode will be set after anding this value with umask
    wxS_DIR_DEFAULT = (wxPOSIX_USER_READ | wxPOSIX_USER_WRITE | wxPOSIX_USER_EXECUTE | \
                       wxPOSIX_GROUP_READ | wxPOSIX_GROUP_WRITE | wxPOSIX_GROUP_EXECUTE | \
                       wxPOSIX_OTHERS_READ | wxPOSIX_OTHERS_WRITE | wxPOSIX_OTHERS_EXECUTE)
};

// ----------------------------------------------------------------------------
// declare our versions of low level file functions: some compilers prepend
// underscores to the usual names, some also have Unicode versions of them
// ----------------------------------------------------------------------------

// Wrappers around Win32 api functions like CreateFile, ReadFile and such
// Implemented in filefnwce.cpp
#if defined( __WXWINCE__)
    typedef __int64 wxFileOffset;
    #define wxFileOffsetFmtSpec wxT("I64")
    WXDLLIMPEXP_BASE int wxCRT_Open(const wxChar *filename, int oflag, int WXUNUSED(pmode));
    WXDLLIMPEXP_BASE int wxCRT_Access(const wxChar *name, int WXUNUSED(how));
    WXDLLIMPEXP_BASE int wxCRT_Chmod(const wxChar *name, int WXUNUSED(how));
    WXDLLIMPEXP_BASE int wxClose(int fd);
    WXDLLIMPEXP_BASE int wxFsync(int WXUNUSED(fd));
    WXDLLIMPEXP_BASE int wxRead(int fd, void *buf, unsigned int count);
    WXDLLIMPEXP_BASE int wxWrite(int fd, const void *buf, unsigned int count);
    WXDLLIMPEXP_BASE int wxEof(int fd);
    WXDLLIMPEXP_BASE wxFileOffset wxSeek(int fd, wxFileOffset offset, int origin);
    #define wxLSeek wxSeek
    WXDLLIMPEXP_BASE wxFileOffset wxTell(int fd);

    // always Unicode under WinCE
    #define   wxCRT_MkDir      _wmkdir
    #define   wxCRT_RmDir      _wrmdir
    #define   wxCRT_Stat       _wstat
    #define   wxStructStat struct _stat
#elif (defined(__WINDOWS__) || defined(__OS2__)) && \
      ( \
        defined(__VISUALC__) || \
        defined(__MINGW64_TOOLCHAIN__) || \
        (defined(__MINGW32__) && !defined(__WINE__) && \
                                wxCHECK_W32API_VERSION(0, 5)) || \
        defined(__DMC__) || \
        defined(__WATCOMC__) || \
        defined(__BORLANDC__) \
      )

    // temporary defines just used immediately below
    #undef wxHAS_HUGE_FILES
    #undef wxHAS_HUGE_STDIO_FILES

    // detect compilers which have support for huge files
    #if defined(__VISUALC__)
        #define wxHAS_HUGE_FILES 1
    #elif defined(__MINGW32__)
        #define wxHAS_HUGE_FILES 1
    #elif defined(_LARGE_FILES)
        #define wxHAS_HUGE_FILES 1
    #endif

    // detect compilers which have support for huge stdio files
    #if wxCHECK_VISUALC_VERSION(8)
        #define wxHAS_HUGE_STDIO_FILES
        #define wxFseek _fseeki64
        #define wxFtell _ftelli64
    #elif wxCHECK_MINGW32_VERSION(3, 5) // mingw-runtime version (not gcc)
        #define wxHAS_HUGE_STDIO_FILES

        wxDECL_FOR_STRICT_MINGW32(int, fseeko64, (FILE*, long long, int));
        #define wxFseek fseeko64

        #ifdef wxNEEDS_STRICT_ANSI_WORKAROUNDS
            // Unfortunately ftello64() is not defined in the library for
            // whatever reason but as an inline function, so define wxFtell()
            // here similarly.
            inline long long wxFtell(FILE* fp)
            {
                fpos_t pos;
                return fgetpos(fp, &pos) == 0 ? pos : -1LL;
            }
        #else
            #define wxFtell ftello64
        #endif
    #endif

    // other Windows compilers (DMC, Watcom, and Borland) don't have huge file
    // support (or at least not all functions needed for it by wx) currently

    // types

    #ifdef wxHAS_HUGE_FILES
        typedef wxLongLong_t wxFileOffset;
        #define wxFileOffsetFmtSpec wxLongLongFmtSpec
    #else
        typedef off_t wxFileOffset;
    #endif

    // at least Borland 5.5 doesn't like "struct ::stat" so don't use the scope
    // resolution operator present in wxPOSIX_IDENT for it
    #ifdef __BORLANDC__
        #define wxPOSIX_STRUCT(s)    struct s
    #else
        #define wxPOSIX_STRUCT(s)    struct wxPOSIX_IDENT(s)
    #endif

    // Notice that Watcom is the only compiler to have a wide char
    // version of struct stat as well as a wide char stat function variant.
    // This was dropped since OW 1.4 "for consistency across platforms".
    //
    // Borland is also special in that it uses _stat with Unicode functions
    // (for MSVC compatibility?) but stat with ANSI ones
    #ifdef __BORLANDC__
        #if wxHAS_HUGE_FILES
            #define wxStructStat struct stati64
        #else
            #if wxUSE_UNICODE
                #define wxStructStat struct _stat
            #else
                #define wxStructStat struct stat
            #endif
        #endif
    #else // !__BORLANDC__
        #ifdef wxHAS_HUGE_FILES
            #if wxUSE_UNICODE && wxONLY_WATCOM_EARLIER_THAN(1,4)
                #define wxStructStat struct _wstati64
            #else
                #define wxStructStat struct _stati64
            #endif
        #else
            #if wxUSE_UNICODE && wxONLY_WATCOM_EARLIER_THAN(1,4)
                #define wxStructStat struct _wstat
            #else
                #define wxStructStat struct _stat
            #endif
        #endif
    #endif // __BORLANDC__/!__BORLANDC__


    // functions

    // MSVC and compatible compilers prepend underscores to the POSIX function
    // names, other compilers don't and even if their later versions usually do
    // define the versions with underscores for MSVC compatibility, it's better
    // to avoid using them as they're not present in earlier versions and
    // always using the native functions spelling is easier than testing for
    // the versions
    #if defined(__BORLANDC__) || defined(__DMC__) || defined(__WATCOMC__) || defined(__MINGW64_TOOLCHAIN__)
        #define wxPOSIX_IDENT(func)    ::func
    #else // by default assume MSVC-compatible names
        #define wxPOSIX_IDENT(func)    _ ## func
        #define wxHAS_UNDERSCORES_IN_POSIX_IDENTS
    #endif

    // first functions not working with strings, i.e. without ANSI/Unicode
    // complications
    #define   wxClose      wxPOSIX_IDENT(close)

    #define wxRead         wxPOSIX_IDENT(read)
    #define wxWrite        wxPOSIX_IDENT(write)

    #ifdef wxHAS_HUGE_FILES
        #ifndef __MINGW64_TOOLCHAIN__
            #define   wxSeek       wxPOSIX_IDENT(lseeki64)
            #define   wxLseek      wxPOSIX_IDENT(lseeki64)
            #define   wxTell       wxPOSIX_IDENT(telli64)
        #else
            // unfortunately, mingw-W64 is somewhat inconsistent...
            #define   wxSeek       _lseeki64
            #define   wxLseek      _lseeki64
            #define   wxTell       _telli64
        #endif
    #else // !wxHAS_HUGE_FILES
        #define   wxSeek       wxPOSIX_IDENT(lseek)
        #define   wxLseek      wxPOSIX_IDENT(lseek)
        #define   wxTell       wxPOSIX_IDENT(tell)
    #endif // wxHAS_HUGE_FILES/!wxHAS_HUGE_FILES

    #ifndef __WATCOMC__
         #if !defined(__BORLANDC__) || (__BORLANDC__ > 0x540)
             // NB: this one is not POSIX and always has the underscore
             #define   wxFsync      _commit

             // could be already defined by configure (Cygwin)
             #ifndef HAVE_FSYNC
                 #define HAVE_FSYNC
             #endif
        #endif // BORLANDC
    #endif

    #define   wxEof        wxPOSIX_IDENT(eof)

    // then the functions taking strings

    // first the ANSI versions
    #define   wxCRT_OpenA       wxPOSIX_IDENT(open)
    #define   wxCRT_AccessA     wxPOSIX_IDENT(access)
    #define   wxCRT_ChmodA      wxPOSIX_IDENT(chmod)
    #define   wxCRT_MkDirA      wxPOSIX_IDENT(mkdir)
    #define   wxCRT_RmDirA      wxPOSIX_IDENT(rmdir)
    #ifdef wxHAS_HUGE_FILES
        // MinGW-64 provides underscore-less versions of all file functions
        // except for this one.
        #ifdef __MINGW64_TOOLCHAIN__
            #define   wxCRT_StatA       _stati64
        #else
            #define   wxCRT_StatA       wxPOSIX_IDENT(stati64)
        #endif
    #else
        // Unfortunately Watcom is not consistent
        #if defined(__OS2__) && defined(__WATCOMC__)
            #define   wxCRT_StatA       _stat
        #else
            #define   wxCRT_StatA       wxPOSIX_IDENT(stat)
        #endif
    #endif

    // then wide char ones
    #if wxUSE_UNICODE
        // special workaround for buggy wopen() in bcc 5.5
        #if defined(__BORLANDC__) && \
            (__BORLANDC__ >= 0x550 && __BORLANDC__ <= 0x551)
                WXDLLIMPEXP_BASE int wxCRT_OpenW(const wxChar *pathname,
                                                 int flags, mode_t mode);
        #else
            #define wxCRT_OpenW       _wopen
        #endif

        #define   wxCRT_AccessW     _waccess
        #define   wxCRT_ChmodW      _wchmod
        #define   wxCRT_MkDirW      _wmkdir
        #define   wxCRT_RmDirW      _wrmdir
        #ifdef wxHAS_HUGE_FILES
            #define   wxCRT_StatW       _wstati64
        #else
            #define   wxCRT_StatW       _wstat
        #endif
    #endif // wxUSE_UNICODE


    // finally the default char-type versions
    #if wxUSE_UNICODE
        #if wxUSE_UNICODE_MSLU
            // implement the missing file functions in Win9x ourselves
            WXDLLIMPEXP_BASE int wxMSLU__wopen(const wxChar *name,
                                               int flags, int mode);
            WXDLLIMPEXP_BASE int wxMSLU__waccess(const wxChar *name,
                                                 int mode);
            WXDLLIMPEXP_BASE int wxMSLU__wchmod(const wxChar *name,
                                                 int mode);
            WXDLLIMPEXP_BASE int wxMSLU__wmkdir(const wxChar *name);
            WXDLLIMPEXP_BASE int wxMSLU__wrmdir(const wxChar *name);

            WXDLLIMPEXP_BASE int
            wxMSLU__wstat(const wxChar *name, wxStructStat *buffer);

            #define   wxCRT_Open       wxMSLU__wopen

            #define   wxCRT_Access     wxMSLU__waccess
            #define   wxCRT_Chmod      wxMSLU__wchmod
            #define   wxCRT_MkDir      wxMSLU__wmkdir
            #define   wxCRT_RmDir      wxMSLU__wrmdir
            #define   wxCRT_Stat       wxMSLU__wstat
        #else // !wxUSE_UNICODE_MSLU
            #define wxCRT_Open      wxCRT_OpenW
            #define wxCRT_Access    wxCRT_AccessW
            #define wxCRT_Chmod     wxCRT_ChmodW
            #define wxCRT_MkDir     wxCRT_MkDirW
            #define wxCRT_RmDir     wxCRT_RmDirW
            #define wxCRT_Stat      wxCRT_StatW

            wxDECL_FOR_STRICT_MINGW32(int, _wmkdir, (const wchar_t*))
            wxDECL_FOR_STRICT_MINGW32(int, _wrmdir, (const wchar_t*))
        #endif // wxUSE_UNICODE_MSLU/!wxUSE_UNICODE_MSLU
    #else // !wxUSE_UNICODE
        #define wxCRT_Open      wxCRT_OpenA
        #define wxCRT_Access    wxCRT_AccessA
        #define wxCRT_Chmod     wxCRT_ChmodA
        #define wxCRT_MkDir     wxCRT_MkDirA
        #define wxCRT_RmDir     wxCRT_RmDirA
        #define wxCRT_Stat      wxCRT_StatA
    #endif // wxUSE_UNICODE/!wxUSE_UNICODE


    // constants (unless already defined by the user code)
    #ifdef wxHAS_UNDERSCORES_IN_POSIX_IDENTS
        #ifndef O_RDONLY
            #define   O_RDONLY    _O_RDONLY
            #define   O_WRONLY    _O_WRONLY
            #define   O_RDWR      _O_RDWR
            #define   O_EXCL      _O_EXCL
            #define   O_CREAT     _O_CREAT
            #define   O_BINARY    _O_BINARY
        #endif

        #ifndef S_IFMT
            #define   S_IFMT      _S_IFMT
            #define   S_IFDIR     _S_IFDIR
            #define   S_IFREG     _S_IFREG
        #endif
    #endif // wxHAS_UNDERSCORES_IN_POSIX_IDENTS

    #ifdef wxHAS_HUGE_FILES
        // wxFile is present and supports large files.
        #if wxUSE_FILE
            #define wxHAS_LARGE_FILES
        #endif
        // wxFFile is present and supports large files
        #if wxUSE_FFILE && defined wxHAS_HUGE_STDIO_FILES
            #define wxHAS_LARGE_FFILES
        #endif
    #endif

    // private defines, undefine so that nobody gets tempted to use
    #undef wxHAS_HUGE_FILES
    #undef wxHAS_HUGE_STDIO_FILES
#else // Unix or Windows using unknown compiler, assume POSIX supported
    typedef off_t wxFileOffset;
    #ifdef HAVE_LARGEFILE_SUPPORT
        #define wxFileOffsetFmtSpec wxLongLongFmtSpec
        wxCOMPILE_TIME_ASSERT( sizeof(off_t) == sizeof(wxLongLong_t),
                                BadFileSizeType );
        // wxFile is present and supports large files
        #if wxUSE_FILE
            #define wxHAS_LARGE_FILES
        #endif
        // wxFFile is present and supports large files
        #if wxUSE_FFILE && (SIZEOF_LONG == 8 || defined HAVE_FSEEKO)
            #define wxHAS_LARGE_FFILES
        #endif
        #ifdef HAVE_FSEEKO
            #define wxFseek fseeko
            #define wxFtell ftello
        #endif
    #else
        #define wxFileOffsetFmtSpec wxT("")
    #endif
    // functions
    #define   wxClose      close
    #define   wxRead       ::read
    #define   wxWrite      ::write
    #define   wxLseek      lseek
    #define   wxSeek       lseek
    #define   wxFsync      fsync
    #define   wxEof        eof
    #define   wxCRT_MkDir      mkdir
    #define   wxCRT_RmDir      rmdir

    #define   wxTell(fd)   lseek(fd, 0, SEEK_CUR)

    #define   wxStructStat struct stat

    #define   wxCRT_Open       open
    #define   wxCRT_Stat       stat
    #define   wxCRT_Lstat      lstat
    #define   wxCRT_Access     access
    #define   wxCRT_Chmod      chmod

    #define wxHAS_NATIVE_LSTAT
#endif // platforms

// if the platform doesn't have symlinks, define wxCRT_Lstat to be the same as
// wxCRT_Stat to avoid #ifdefs in the code using it
#ifndef wxHAS_NATIVE_LSTAT
    #define wxCRT_Lstat wxCRT_Stat
#endif

// define wxFseek/wxFtell to large file versions if available (done above) or
// to fseek/ftell if not, to save ifdefs in using code
#ifndef wxFseek
    #define wxFseek fseek
#endif
#ifndef wxFtell
    #define wxFtell ftell
#endif

inline int wxAccess(const wxString& path, mode_t mode)
    { return wxCRT_Access(path.fn_str(), mode); }
inline int wxChmod(const wxString& path, mode_t mode)
    { return wxCRT_Chmod(path.fn_str(), mode); }
inline int wxOpen(const wxString& path, int flags, mode_t mode)
    { return wxCRT_Open(path.fn_str(), flags, mode); }

// FIXME-CE: provide our own implementations of the missing CRT functions
#ifndef __WXWINCE__
inline int wxStat(const wxString& path, wxStructStat *buf)
    { return wxCRT_Stat(path.fn_str(), buf); }
inline int wxLstat(const wxString& path, wxStructStat *buf)
    { return wxCRT_Lstat(path.fn_str(), buf); }
inline int wxRmDir(const wxString& path)
    { return wxCRT_RmDir(path.fn_str()); }
#if (defined(__WINDOWS__) && !defined(__CYGWIN__)) \
        || (defined(__OS2__) && defined(__WATCOMC__))
inline int wxMkDir(const wxString& path, mode_t WXUNUSED(mode) = 0)
    { return wxCRT_MkDir(path.fn_str()); }
#else
inline int wxMkDir(const wxString& path, mode_t mode)
    { return wxCRT_MkDir(path.fn_str(), mode); }
#endif
#endif // !__WXWINCE__

#ifdef O_BINARY
    #define wxO_BINARY O_BINARY
#else
    #define wxO_BINARY 0
#endif

#if defined(__VISAGECPP__) && __IBMCPP__ >= 400
//
// VisualAge C++ V4.0 cannot have any external linkage const decs
// in headers included by more than one primary source
//
extern const int wxInvalidOffset;
#else
const int wxInvalidOffset = -1;
#endif

// ----------------------------------------------------------------------------
// functions
// ----------------------------------------------------------------------------
WXDLLIMPEXP_BASE bool wxFileExists(const wxString& filename);

// does the path exist? (may have or not '/' or '\\' at the end)
WXDLLIMPEXP_BASE bool wxDirExists(const wxString& pathName);

WXDLLIMPEXP_BASE bool wxIsAbsolutePath(const wxString& filename);

// Get filename
WXDLLIMPEXP_BASE wxChar* wxFileNameFromPath(wxChar *path);
WXDLLIMPEXP_BASE wxString wxFileNameFromPath(const wxString& path);

// Get directory
WXDLLIMPEXP_BASE wxString wxPathOnly(const wxString& path);

// all deprecated functions below are deprecated in favour of wxFileName's methods
#if WXWIN_COMPATIBILITY_2_8

wxDEPRECATED( WXDLLIMPEXP_BASE void wxDos2UnixFilename(char *s) );
wxDEPRECATED( WXDLLIMPEXP_BASE void wxDos2UnixFilename(wchar_t *s) );

wxDEPRECATED_BUT_USED_INTERNALLY(
    WXDLLIMPEXP_BASE void wxUnix2DosFilename(char *s) );
wxDEPRECATED_BUT_USED_INTERNALLY(
    WXDLLIMPEXP_BASE void wxUnix2DosFilename(wchar_t *s) );

// Strip the extension, in situ
// Deprecated in favour of wxFileName::StripExtension() but notice that their
// behaviour is slightly different, see the manual
wxDEPRECATED( WXDLLIMPEXP_BASE void wxStripExtension(char *buffer) );
wxDEPRECATED( WXDLLIMPEXP_BASE void wxStripExtension(wchar_t *buffer) );
wxDEPRECATED( WXDLLIMPEXP_BASE void wxStripExtension(wxString& buffer) );

// Get a temporary filename
wxDEPRECATED_BUT_USED_INTERNALLY( WXDLLIMPEXP_BASE wxChar* wxGetTempFileName(const wxString& prefix, wxChar *buf = NULL) );
wxDEPRECATED_BUT_USED_INTERNALLY( WXDLLIMPEXP_BASE bool wxGetTempFileName(const wxString& prefix, wxString& buf) );

// Expand file name (~/ and ${OPENWINHOME}/ stuff)
wxDEPRECATED_BUT_USED_INTERNALLY( WXDLLIMPEXP_BASE char* wxExpandPath(char *dest, const wxString& path) );
wxDEPRECATED_BUT_USED_INTERNALLY( WXDLLIMPEXP_BASE wchar_t* wxExpandPath(wchar_t *dest, const wxString& path) );
    // DEPRECATED: use wxFileName::Normalize(wxPATH_NORM_ENV_VARS)

// Contract w.r.t environment (</usr/openwin/lib, OPENWHOME> -> ${OPENWINHOME}/lib)
// and make (if under the home tree) relative to home
// [caller must copy-- volatile]
wxDEPRECATED(
WXDLLIMPEXP_BASE wxChar* wxContractPath(const wxString& filename,
                                   const wxString& envname = wxEmptyString,
                                   const wxString& user = wxEmptyString) );
    // DEPRECATED: use wxFileName::ReplaceEnvVariable and wxFileName::ReplaceHomeDir

// Destructive removal of /./ and /../ stuff
wxDEPRECATED_BUT_USED_INTERNALLY( WXDLLIMPEXP_BASE char* wxRealPath(char *path) );
wxDEPRECATED_BUT_USED_INTERNALLY( WXDLLIMPEXP_BASE wchar_t* wxRealPath(wchar_t *path) );
wxDEPRECATED_BUT_USED_INTERNALLY( WXDLLIMPEXP_BASE wxString wxRealPath(const wxString& path) );
    // DEPRECATED: use wxFileName::Normalize instead

// Allocate a copy of the full absolute path
wxDEPRECATED( WXDLLIMPEXP_BASE wxChar* wxCopyAbsolutePath(const wxString& path) );
    // DEPRECATED: use wxFileName::MakeAbsolute instead
#endif

// Get first file name matching given wild card.
// Flags are reserved for future use.
#define wxFILE  1
#define wxDIR   2
WXDLLIMPEXP_BASE wxString wxFindFirstFile(const wxString& spec, int flags = wxFILE);
WXDLLIMPEXP_BASE wxString wxFindNextFile();

// Does the pattern contain wildcards?
WXDLLIMPEXP_BASE bool wxIsWild(const wxString& pattern);

// Does the pattern match the text (usually a filename)?
// If dot_special is true, doesn't match * against . (eliminating
// `hidden' dot files)
WXDLLIMPEXP_BASE bool wxMatchWild(const wxString& pattern,  const wxString& text, bool dot_special = true);

// Concatenate two files to form third
WXDLLIMPEXP_BASE bool wxConcatFiles(const wxString& file1, const wxString& file2, const wxString& file3);

// Copy file1 to file2
WXDLLIMPEXP_BASE bool wxCopyFile(const wxString& file1, const wxString& file2,
                                 bool overwrite = true);

// Remove file
WXDLLIMPEXP_BASE bool wxRemoveFile(const wxString& file);

// Rename file
WXDLLIMPEXP_BASE bool wxRenameFile(const wxString& file1, const wxString& file2, bool overwrite = true);

// Get current working directory.
#if WXWIN_COMPATIBILITY_2_6
// If buf is NULL, allocates space using new, else
// copies into buf.
// IMPORTANT NOTE getcwd is know not to work under some releases
// of Win32s 1.3, according to MS release notes!
wxDEPRECATED( WXDLLIMPEXP_BASE wxChar* wxGetWorkingDirectory(wxChar *buf = NULL, int sz = 1000) );
// new and preferred version of wxGetWorkingDirectory
// NB: can't have the same name because of overloading ambiguity
#endif // WXWIN_COMPATIBILITY_2_6
WXDLLIMPEXP_BASE wxString wxGetCwd();

// Set working directory
WXDLLIMPEXP_BASE bool wxSetWorkingDirectory(const wxString& d);

// Make directory
WXDLLIMPEXP_BASE bool wxMkdir(const wxString& dir, int perm = wxS_DIR_DEFAULT);

// Remove directory. Flags reserved for future use.
WXDLLIMPEXP_BASE bool wxRmdir(const wxString& dir, int flags = 0);

// Return the type of an open file
WXDLLIMPEXP_BASE wxFileKind wxGetFileKind(int fd);
WXDLLIMPEXP_BASE wxFileKind wxGetFileKind(FILE *fp);

#if WXWIN_COMPATIBILITY_2_6
// compatibility defines, don't use in new code
wxDEPRECATED( inline bool wxPathExists(const wxChar *pszPathName) );
inline bool wxPathExists(const wxChar *pszPathName)
{
    return wxDirExists(pszPathName);
}
#endif //WXWIN_COMPATIBILITY_2_6

// permissions; these functions work both on files and directories:
WXDLLIMPEXP_BASE bool wxIsWritable(const wxString &path);
WXDLLIMPEXP_BASE bool wxIsReadable(const wxString &path);
WXDLLIMPEXP_BASE bool wxIsExecutable(const wxString &path);

// ----------------------------------------------------------------------------
// separators in file names
// ----------------------------------------------------------------------------

// between file name and extension
#define wxFILE_SEP_EXT        wxT('.')

// between drive/volume name and the path
#define wxFILE_SEP_DSK        wxT(':')

// between the path components
#define wxFILE_SEP_PATH_DOS   wxT('\\')
#define wxFILE_SEP_PATH_UNIX  wxT('/')
#define wxFILE_SEP_PATH_MAC   wxT(':')
#define wxFILE_SEP_PATH_VMS   wxT('.') // VMS also uses '[' and ']'

// separator in the path list (as in PATH environment variable)
// there is no PATH variable in Classic Mac OS so just use the
// semicolon (it must be different from the file name separator)
// NB: these are strings and not characters on purpose!
#define wxPATH_SEP_DOS        wxT(";")
#define wxPATH_SEP_UNIX       wxT(":")
#define wxPATH_SEP_MAC        wxT(";")

// platform independent versions
#if defined(__UNIX__) && !defined(__OS2__)
  // CYGWIN also uses UNIX settings
  #define wxFILE_SEP_PATH     wxFILE_SEP_PATH_UNIX
  #define wxPATH_SEP          wxPATH_SEP_UNIX
#elif defined(__MAC__)
  #define wxFILE_SEP_PATH     wxFILE_SEP_PATH_MAC
  #define wxPATH_SEP          wxPATH_SEP_MAC
#else   // Windows and OS/2
  #define wxFILE_SEP_PATH     wxFILE_SEP_PATH_DOS
  #define wxPATH_SEP          wxPATH_SEP_DOS
#endif  // Unix/Windows

// this is useful for wxString::IsSameAs(): to compare two file names use
// filename1.IsSameAs(filename2, wxARE_FILENAMES_CASE_SENSITIVE)
#if defined(__UNIX__) && !defined(__DARWIN__) && !defined(__OS2__)
  #define wxARE_FILENAMES_CASE_SENSITIVE  true
#else   // Windows, Mac OS and OS/2
  #define wxARE_FILENAMES_CASE_SENSITIVE  false
#endif  // Unix/Windows

// is the char a path separator?
inline bool wxIsPathSeparator(wxChar c)
{
    // under DOS/Windows we should understand both Unix and DOS file separators
#if ( defined(__UNIX__) && !defined(__OS2__) )|| defined(__MAC__)
    return c == wxFILE_SEP_PATH;
#else
    return c == wxFILE_SEP_PATH_DOS || c == wxFILE_SEP_PATH_UNIX;
#endif
}

// does the string ends with path separator?
WXDLLIMPEXP_BASE bool wxEndsWithPathSeparator(const wxString& filename);

#if WXWIN_COMPATIBILITY_2_8
// split the full path into path (including drive for DOS), name and extension
// (understands both '/' and '\\')
// Deprecated in favour of wxFileName::SplitPath
wxDEPRECATED( WXDLLIMPEXP_BASE void wxSplitPath(const wxString& fileName,
                                                wxString *pstrPath,
                                                wxString *pstrName,
                                                wxString *pstrExt) );
#endif

// find a file in a list of directories, returns false if not found
WXDLLIMPEXP_BASE bool wxFindFileInPath(wxString *pStr, const wxString& szPath, const wxString& szFile);

// Get the OS directory if appropriate (such as the Windows directory).
// On non-Windows platform, probably just return the empty string.
WXDLLIMPEXP_BASE wxString wxGetOSDirectory();

#if wxUSE_DATETIME

// Get file modification time
WXDLLIMPEXP_BASE time_t wxFileModificationTime(const wxString& filename);

#endif // wxUSE_DATETIME

// Parses the wildCard, returning the number of filters.
// Returns 0 if none or if there's a problem,
// The arrays will contain an equal number of items found before the error.
// wildCard is in the form:
// "All files (*)|*|Image Files (*.jpeg *.png)|*.jpg;*.png"
WXDLLIMPEXP_BASE int wxParseCommonDialogsFilter(const wxString& wildCard, wxArrayString& descriptions, wxArrayString& filters);

// ----------------------------------------------------------------------------
// classes
// ----------------------------------------------------------------------------

#ifdef __UNIX__

// set umask to the given value in ctor and reset it to the old one in dtor
class WXDLLIMPEXP_BASE wxUmaskChanger
{
public:
    // change the umask to the given one if it is not -1: this allows to write
    // the same code whether you really want to change umask or not, as is in
    // wxFileConfig::Flush() for example
    wxUmaskChanger(int umaskNew)
    {
        m_umaskOld = umaskNew == -1 ? -1 : (int)umask((mode_t)umaskNew);
    }

    ~wxUmaskChanger()
    {
        if ( m_umaskOld != -1 )
            umask((mode_t)m_umaskOld);
    }

private:
    int m_umaskOld;
};

// this macro expands to an "anonymous" wxUmaskChanger object under Unix and
// nothing elsewhere
#define wxCHANGE_UMASK(m) wxUmaskChanger wxMAKE_UNIQUE_NAME(umaskChanger_)(m)

#else // !__UNIX__

#define wxCHANGE_UMASK(m)

#endif // __UNIX__/!__UNIX__


// Path searching
class WXDLLIMPEXP_BASE wxPathList : public wxArrayString
{
public:
    wxPathList() {}
    wxPathList(const wxArrayString &arr)
        { Add(arr); }

    // Adds all paths in environment variable
    void AddEnvList(const wxString& envVariable);

    // Adds given path to this list
    bool Add(const wxString& path);
    void Add(const wxArrayString &paths);

    // Find the first full path for which the file exists
    wxString FindValidPath(const wxString& filename) const;

    // Find the first full path for which the file exists; ensure it's an
    // absolute path that gets returned.
    wxString FindAbsoluteValidPath(const wxString& filename) const;

    // Given full path and filename, add path to list
    bool EnsureFileAccessible(const wxString& path);

#if WXWIN_COMPATIBILITY_2_6
    // Returns true if the path is in the list
    wxDEPRECATED( bool Member(const wxString& path) const );
#endif
};

#endif // _WX_FILEFN_H_
