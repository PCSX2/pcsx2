///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/core/objcid.h
// Purpose:     Define wxObjCID working in both C++ and Objective-C.
// Author:      Vadim Zeitlin
// Created:     2012-05-20
// Copyright:   (c) 2012 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_CORE_OBJCID_H_
#define _WX_OSX_CORE_OBJCID_H_

// ----------------------------------------------------------------------------
// wxObjCID: Equivalent of Objective-C "id" that works in C++ code.
// ----------------------------------------------------------------------------

#ifdef __OBJC__
    #define wxObjCID id
#else
    typedef struct objc_object* wxObjCID;
#endif

#endif // _WX_OSX_CORE_OBJCID_H_
