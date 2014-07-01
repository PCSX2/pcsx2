/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/core/cfdataref.h
// Purpose:     wxCFDataRef class
// Author:      Stefan Csomor
// Modified by:
// Created:     2007/05/10
// Copyright:   (c) 2007 Stefan Csomor
// Licence:     wxWindows licence
// Notes:       See http://developer.apple.com/documentation/CoreFoundation/Conceptual/CFBinaryData/index.html
/////////////////////////////////////////////////////////////////////////////
/*! @header     wx/osx/core/cfdataref.h
    @abstract   wxCFDataRef template class
*/

#ifndef _WX_MAC_COREFOUNDATION_CFDATAREF_H__
#define _WX_MAC_COREFOUNDATION_CFDATAREF_H__

#include "wx/osx/core/cfref.h"

#include <CoreFoundation/CFData.h>

/*! @class wxCFDataRef
    @discussion Properly retains/releases reference to CoreFoundation data objects
*/
class wxCFDataRef : public wxCFRef< CFDataRef >
{
public:
    /*! @method     wxCFDataRef
        @abstract   Creates a NULL data ref
    */
    wxCFDataRef()
    {}

    typedef wxCFRef<CFDataRef> super_type;

    /*! @method     wxCFDataRef
        @abstract   Assumes ownership of p and creates a reference to it.
        @templatefield otherType    Any type.
        @param p        The raw pointer to assume ownership of.  May be NULL.
        @discussion Like shared_ptr, it is assumed that the caller has a strong reference to p and intends
                    to transfer ownership of that reference to this ref holder.  If the object comes from
                    a Create or Copy method then this is the correct behaviour.  If the object comes from
                    a Get method then you must CFRetain it yourself before passing it to this constructor.
                    A handy way to do this is to use the non-member wxCFRefFromGet factory funcion.
                    This method is templated and takes an otherType *p.  This prevents implicit conversion
                    using an operator refType() in a different ref-holding class type.
    */
    explicit wxCFDataRef(CFDataRef r)
    :   super_type(r)
    {}

    /*! @method     wxCFDataRef
        @abstract   Copies a ref holder of the same type
        @param otherRef The other ref holder to copy.
        @discussion Ownership will be shared by the original ref and the newly created ref. That is,
                    the object will be explicitly retained by this new ref.
    */
    wxCFDataRef(const wxCFDataRef& otherRef)
    :  super_type( otherRef )
    {}

    /*! @method     wxCFDataRef
        @abstract   Copies raw data into a data ref
        @param data The raw data.
        @param length The data length.
    */
    wxCFDataRef(const UInt8* data, CFIndex length)
    : super_type(CFDataCreate(kCFAllocatorDefault, data, length))
    {
    }

    /*! @method     GetLength
        @abstract   returns the length in bytes of the data stored
    */
    CFIndex GetLength() const
    {
        if ( m_ptr )
            return CFDataGetLength( *this );
        else
            return 0;
    }

    /*! @method     GetBytes
        @abstract   Copies the data into an external buffer
        @param range The desired range.
        @param buffer The target buffer.
    */
    void GetBytes( CFRange range, UInt8 *buffer ) const
    {
        if ( m_ptr )
            CFDataGetBytes(m_ptr, range, buffer);
    }
};

#endif //ifndef _WX_MAC_COREFOUNDATION_CFDATAREF_H__

