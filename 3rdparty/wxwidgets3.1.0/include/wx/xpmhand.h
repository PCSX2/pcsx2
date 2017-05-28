/////////////////////////////////////////////////////////////////////////////
// Name:        wx/xpmhand.h
// Purpose:     XPM handler base header
// Author:      Julian Smart
// Modified by:
// Created:
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XPMHAND_H_BASE_
#define _WX_XPMHAND_H_BASE_

// Only wxMSW currently defines a separate XPM handler, since
// mostly Windows apps won't need XPMs.
#if defined(__WXMSW__)
#error xpmhand.h is no longer needed since wxImage now handles XPMs.
#endif

#endif
    // _WX_XPMHAND_H_BASE_
