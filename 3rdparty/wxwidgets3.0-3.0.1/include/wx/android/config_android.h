///////////////////////////////////////////////////////////////////////////////
// Name:        wx/android/config_android.h
// Purpose:     configurations for Android builds
// Author:      Zsolt Bakcsi
// Modified by:
// Created:     2011-12-02
// RCS-ID:
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// Please note that most of these settings are based on config_xcode.h and
// 'fine-tuned' on a trial-and-error basis. This means, no in-depth analysis
// of Android docs / source was done.

#define wxUSE_UNIX 1
#define __UNIX__ 1

#define HAVE_NANOSLEEP
#define HAVE_FCNTL 1
#define HAVE_GCC_ATOMIC_BUILTINS
#define HAVE_GETHOSTBYNAME 1
#define HAVE_GETSERVBYNAME 1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_GMTIME_R 1
#define HAVE_INET_ADDR 1
#define HAVE_INET_ATON 1
#define HAVE_LOCALTIME_R 1
#define HAVE_PTHREAD_MUTEXATTR_T 1
#define HAVE_PTHREAD_MUTEXATTR_SETTYPE_DECL 1
#define HAVE_PTHREAD_ATTR_SETSTACKSIZE 1
#define HAVE_THREAD_PRIORITY_FUNCTIONS 1
#define HAVE_SSIZE_T 1
#define HAVE_WPRINTF 1

#define SIZEOF_INT 4
#define SIZEOF_LONG 4
#define SIZEOF_LONG_LONG 8
#define SIZEOF_SIZE_T 4
#define SIZEOF_VOID_P 4
#define SIZEOF_WCHAR_T 4

#define wxHAVE_PTHREAD_CLEANUP 1
#define wxNO_WOSTREAM
#define wxSIZE_T_IS_UINT 1
#define wxWCHAR_T_IS_REAL_TYPE 1

#define wxTYPE_SA_HANDLER int

#define wxUSE_SELECT_DISPATCHER 1

#ifdef HAVE_PTHREAD_CANCEL
// Android doesn't support pthread_cancel().
#undef HAVE_PTHREAD_CANCEL
#endif
