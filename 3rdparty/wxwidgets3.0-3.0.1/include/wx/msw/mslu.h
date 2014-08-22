/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/mslu.h
// Purpose:     MSLU-related declarations
// Author:      Vaclav Slavik
// Modified by: Vadim Zeitlin to move out various functions to other files
//              to fix header inter-dependencies
// Created:     2002/02/17
// Copyright:   (c) 2002 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSLU_H_
#define _WX_MSLU_H_

#include "wx/defs.h"

// Returns true if we are running under Unicode emulation in Win9x environment.
// Workaround hacks take effect only if this condition is met
// (NB: this function is needed even if !wxUSE_UNICODE_MSLU)
WXDLLIMPEXP_BASE bool wxUsingUnicowsDll();

#endif // _WX_MSLU_H_
