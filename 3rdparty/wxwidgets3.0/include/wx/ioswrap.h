///////////////////////////////////////////////////////////////////////////////
// Name:        wx/ioswrap.h
// Purpose:     includes the correct iostream headers for current compiler
// Author:      Vadim Zeitlin
// Modified by:
// Created:     03.02.99
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#if wxUSE_STD_IOSTREAM

#include "wx/beforestd.h"

#if wxUSE_IOSTREAMH
#   include <iostream.h>
#else
#   include <iostream>
#endif

#include "wx/afterstd.h"

#ifdef __WINDOWS__
#   include "wx/msw/winundef.h"
#endif

#endif
  // wxUSE_STD_IOSTREAM

