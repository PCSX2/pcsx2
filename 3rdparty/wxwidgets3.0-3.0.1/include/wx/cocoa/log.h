///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/log.h
// Purpose:     Declare wxCocoa-specific trace masks
// Author:      David Elliott
// Modified by:
// Created:     2004/02/07
// Copyright:   (c) 2004 David Elliott
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COCOA_LOG_H__
#define _WX_COCOA_LOG_H__

// General tracing (in lieu of wxLogDebug)
#define wxTRACE_COCOA wxT("COCOA")
// Specific tracing
#define wxTRACE_COCOA_RetainRelease wxT("COCOA_RetainRelease")
#define wxTRACE_COCOA_TopLevelWindow_Size wxT("COCOA_TopLevelWindow_Size")
#define wxTRACE_COCOA_Window_Size wxT("COCOA_Window_Size")

#endif //ndef _WX_COCOA_LOG_H__
