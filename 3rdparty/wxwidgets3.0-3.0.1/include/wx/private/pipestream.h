///////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/pipestream.h
// Purpose:     Declares wxPipeInputStream and wxPipeOutputStream.
// Author:      Vadim Zeitlin
// Modified by: Rob Bresalier
// Created:     2013-04-27
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwidgets.org>
//              (c) 2013 Rob Bresalier
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_PIPESTREAM_H_
#define _WX_PRIVATE_PIPESTREAM_H_

#include "wx/platform.h"

// wxPipeInputStream is a platform-dependent input stream class (i.e. deriving,
// possible indirectly, from wxInputStream) for reading from a pipe, i.e. a
// pipe FD under Unix or a pipe HANDLE under MSW. It provides a single extra
// IsOpened() method.
//
// wxPipeOutputStream is similar but has no additional methods at all.
#ifdef __UNIX__
    #include "wx/unix/private/pipestream.h"
#elif defined(__WINDOWS__) && !defined(__WXWINCE__)
    #include "wx/msw/private/pipestream.h"
#endif

#endif // _WX_PRIVATE_PIPESTREAM_H_
