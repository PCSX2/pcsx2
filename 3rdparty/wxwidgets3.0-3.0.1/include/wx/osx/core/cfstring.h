/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/core/cfstring.h
// Purpose:     wxCFStringRef and other string functions
// Author:      Stefan Csomor
// Modified by:
// Created:     2004-10-29 (from code in wx/mac/carbon/private.h)
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
// Usage:       Darwin (base library)
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_CFSTRINGHOLDER_H__
#define __WX_CFSTRINGHOLDER_H__

#include <CoreFoundation/CFString.h>

#include "wx/dlimpexp.h"
#include "wx/fontenc.h"
#include "wx/osx/core/cfref.h"

#ifdef WORDS_BIGENDIAN
    #define kCFStringEncodingUTF32Native kCFStringEncodingUTF32BE
#else
    #define kCFStringEncodingUTF32Native kCFStringEncodingUTF32LE
#endif

class WXDLLIMPEXP_FWD_BASE wxString;

WXDLLIMPEXP_BASE void wxMacConvertNewlines13To10( wxString *data ) ;
WXDLLIMPEXP_BASE void wxMacConvertNewlines10To13( wxString *data ) ;

WXDLLIMPEXP_BASE void wxMacConvertNewlines13To10( char * data ) ;
WXDLLIMPEXP_BASE void wxMacConvertNewlines10To13( char * data ) ;

WXDLLIMPEXP_BASE wxUint32 wxMacGetSystemEncFromFontEnc(wxFontEncoding encoding) ;
WXDLLIMPEXP_BASE wxFontEncoding wxMacGetFontEncFromSystemEnc(wxUint32 encoding) ;
WXDLLIMPEXP_BASE void wxMacWakeUp() ;

class WXDLLIMPEXP_BASE wxCFStringRef : public wxCFRef< CFStringRef >
{
public:
    wxCFStringRef()
    {
    }

    wxCFStringRef(const wxString &str,
                        wxFontEncoding encoding = wxFONTENCODING_DEFAULT) ;

#if wxOSX_USE_COCOA_OR_IPHONE
    wxCFStringRef(NSString* ref)
        : wxCFRef< CFStringRef >((CFStringRef) ref)
    {
    }
#endif

    wxCFStringRef(CFStringRef ref)
        : wxCFRef< CFStringRef >(ref)
    {
    }

    wxCFStringRef(const wxCFStringRef& otherRef )
        : wxCFRef< CFStringRef >(otherRef)
    {
    }

    ~wxCFStringRef()
    {
    }

    wxString AsString( wxFontEncoding encoding = wxFONTENCODING_DEFAULT ) const;

    static wxString AsString( CFStringRef ref, wxFontEncoding encoding = wxFONTENCODING_DEFAULT ) ;
    static wxString AsStringWithNormalizationFormC( CFStringRef ref, wxFontEncoding encoding = wxFONTENCODING_DEFAULT ) ;
#if wxOSX_USE_COCOA_OR_IPHONE
    static wxString AsString( NSString* ref, wxFontEncoding encoding = wxFONTENCODING_DEFAULT ) ;
    static wxString AsStringWithNormalizationFormC( NSString* ref, wxFontEncoding encoding = wxFONTENCODING_DEFAULT ) ;
#endif

#if wxOSX_USE_COCOA_OR_IPHONE
    NSString* AsNSString() const { return (NSString*)(CFStringRef) *this; }
#endif
private:
} ;

// corresponding class for holding UniChars (native unicode characters)

class WXDLLIMPEXP_BASE wxMacUniCharBuffer
{
public :
    wxMacUniCharBuffer( const wxString &str ) ;

    ~wxMacUniCharBuffer() ;

    UniCharPtr GetBuffer() ;

    UniCharCount GetChars() ;

private :
    UniCharPtr m_ubuf ;
    UniCharCount m_chars ;
};
#endif //__WXCFSTRINGHOLDER_H__
