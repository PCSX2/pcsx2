///////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/string.h
// Purpose:     String conversion methods
// Author:      David Elliott
// Modified by:
// Created:     2003/04/13
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_STRING_H__
#define __WX_COCOA_STRING_H__

#import <Foundation/NSString.h>
#include "wx/string.h"

// FIXME: In unicode mode we are doing the conversion twice.  wxString
// converts to UTF-8 and NSString converts from UTF-8.
// One possible optimization is to convert to the wxString internal
// representation which is an unsigned short (unichar) but unfortunately
// there is little documentation on which encoding it uses by default.

// Return an autoreleased NSString
inline NSString* wxNSStringWithWxString(const wxString &wxstring)
{
#if wxUSE_UNICODE
    return [NSString stringWithUTF8String: wxstring.utf8_str()];
#else
    return [NSString stringWithCString: wxstring.c_str() length:wxstring.Len()];
#endif // wxUSE_UNICODE
}

// Intialize an NSString which has already been allocated
inline NSString* wxInitNSStringWithWxString(NSString *nsstring, const wxString &wxstring)
{
#if wxUSE_UNICODE
    return [nsstring initWithUTF8String: wxstring.utf8_str()];
#else
    return [nsstring initWithCString: wxstring.c_str() length:wxstring.Len()];
#endif // wxUSE_UNICODE
}

inline wxString wxStringWithNSString(NSString *nsstring)
{
#if wxUSE_UNICODE
    return wxString::FromUTF8Unchecked([nsstring UTF8String]);
#else
    return wxString([nsstring lossyCString]);
#endif // wxUSE_UNICODE
}

#endif // __WX_COCOA_STRING_H__
