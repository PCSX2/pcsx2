/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/ObjcRef.h
// Purpose:     wxObjcAutoRef template class
// Author:      David Elliott
// Modified by:
// Created:     2004/03/28
// Copyright:   (c) 2004 David Elliott <dfe@cox.net>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COCOA_OBJCREF_H__
#define _WX_COCOA_OBJCREF_H__

// Reuse wxCFRef-related code (e.g. wxCFRetain/wxCFRelease)
#include "wx/osx/core/cfref.h"

// NOTE WELL: We can only know whether or not GC can be used when compiling Objective-C.
// Therefore we cannot implement these functions except when compiling Objective-C.
#ifdef __OBJC__
/*! @function   wxGCSafeRetain
    @templatefield  Type    (implicit) An Objective-C class type
    @arg            r       Pointer to Objective-C object.  May be null.
    @abstract   Retains the Objective-C object, even when using Apple's garbage collector
    @discussion
    When Apple's garbage collector is enabled, the usual [obj retain] and [obj release] messages
    are ignored.  Instead the collector with help from compiler-generated write-barriers tracks
    reachable objects.  The write-barriers are generated when setting i-vars of C++ classes but
    they are ignored by the garbage collector unless the C++ object is in GC-managed memory.

    The simple solution is to use CFRetain on the Objective-C object which has been enhanced in
    GC mode to forcibly retain the object.  In Retain/Release (RR) mode the CFRetain function has
    the same effect as [obj retain].  Note that GC vs. RR is selected at runtime.

    Take care that wxGCSafeRetain must be balanced with wxGCSafeRelease and that conversely
    wxGCSafeRelease must only be called on objects to balance wxGCSafeRetain.  In particular when
    receiving an Objective-C object from an alloc or copy method take care that you must retain
    it with wxGCSafeRetain and balance the initial alloc with a standard release.

    Example:
    wxGCSafeRelease(m_obj); // release current object (if any)
    NSObject *obj = [[NSObject alloc] init];
    m_obj = wxGCSafeRetain(obj);
    [obj release];

    Alternatively (same effect, perhaps less clear):
    wxGCSafeRelease(m_obj); // release current object (if any)
    m_obj = wxGCSafeRetain([[NSObject alloc] init]);
    [m_obj release]; // balance alloc

    Consider the effect on the retain count from each statement (alloc, CFRetain, release)
    In RR mode:     retainCount = 1, +1, -1
    In GC mode:     strongRetainCount = 0, +1, -0

    This is a template function to ensure it is used on raw pointers and never on pointer-holder
    objects via implicit conversion operators.
*/
template <class Type>
inline Type * wxGCSafeRetain(Type *r)
{
#ifdef __OBJC_GC__
    return static_cast<Type*>(wxCFRetain(r));
#else
    return [r retain];
#endif
}

/*! @function   wxGCSafeRelease
    @templatefield  Type    (implicit) An Objective-C class type
    @arg            r       Pointer to Objective-C object.  May be null.
    @abstract   Balances wxGCSafeRetain.  Particularly useful with the Apple Garbage Collector.
    @discussion
    See the wxGCSafeRetain documentation for more details.

    Example (from wxGCSafeRetain documentation):
    wxGCSafeRelease(m_obj); // release current object (if any)
    m_obj = wxGCSafeRetain([[NSObject alloc] init]);
    [m_obj release]; // balance alloc

    When viewed from the start, m_obj ought to start as nil. However, the second time through
    the wxGCSafeRelease call becomes critical as it releases the retain from the first time
    through.

    In the destructor for this C++ object with the m_obj i-var you ought to do the following:
    wxGCSafeRelease(m_obj);
    m_obj = nil; // Not strictly needed, but safer.

    Under no circumstances should you balance an alloc or copy with a wxGCSafeRelease.
*/
template <class Type>
inline void wxGCSafeRelease(Type *r)
{
#ifdef __OBJC_GC__
    wxCFRelease(r);
#else
    [r release];
#endif
}
#else
// NOTE: When not compiling Objective-C, declare these functions such that they can be
// used by other inline-implemented methods.  Since those methods in turn will not actually
// be used from non-ObjC code the compiler ought not emit them.  If it emits an out of
// line copy of those methods then presumably it will have also emitted at least one
// out of line copy of these functions from at least one Objective-C++ translation unit.
// That means the out of line implementation will be available at link time.

template <class Type>
inline Type * wxGCSafeRetain(Type *r);

template <class Type>
inline void wxGCSafeRelease(Type *r);

#endif //def __OBJC__

/*
wxObjcAutoRefFromAlloc: construct a reference to an object that was
[NSObject -alloc]'ed and thus does not need a retain
wxObjcAutoRef: construct a reference to an object that was
either autoreleased or is retained by something else.
*/

struct objc_object;

// We must do any calls to Objective-C from an Objective-C++ source file
class wxObjcAutoRefBase
{
protected:
    /*! @function   ObjcRetain
        @abstract   Simply does [p retain].
     */
    static struct objc_object* ObjcRetain(struct objc_object*);

    /*! @function   ObjcRelease
        @abstract   Simply does [p release].
     */
    static void ObjcRelease(struct objc_object*);
};

/*! @class      wxObjcAutoRefFromAlloc
    @templatefield  T       The type of _pointer_ (e.g. NSString*, NSRunLoop*)
    @abstract   Pointer-holder for Objective-C objects
    @discussion
    When constructing this object from a raw pointer, the pointer is assumed to have
    come from an alloc-style method.  That is, once you construct this object from
    the pointer you must not balance your alloc with a call to release.

    This class has been carefully designed to work with both the traditional Retain/Release
    and the new Garbage Collected modes.  In RR-mode it will prevent the object from being
    released by managing the reference count using the retain/release semantics.  In GC-mode
    it will use a method (currently CFRetain/CFRelease) to ensure the object will never be
    finalized until this object is destroyed.
 */

template <class T>
class wxObjcAutoRefFromAlloc: wxObjcAutoRefBase
{
public:
    wxObjcAutoRefFromAlloc(T p = 0)
    :   m_ptr(p)
    // NOTE: this is from alloc.  Do NOT retain
    {
        //  CFRetain
        //      GC:     Object is strongly retained and prevented from being collected
        //      non-GC: Simply realizes it's an Objective-C object and calls [p retain]
        wxGCSafeRetain(p);
        //  ObjcRelease (e.g. [p release])
        //      GC:     Objective-C retain/release mean nothing in GC mode
        //      non-GC: This is a normal release call, balancing the retain
        ObjcRelease(static_cast<T>(p));
        //  The overall result:
        //      GC:     Object is strongly retained
        //      non-GC: Retain count is the same as it was (retain then release)
    }
    wxObjcAutoRefFromAlloc(const wxObjcAutoRefFromAlloc& otherRef)
    :   m_ptr(otherRef.m_ptr)
    {   wxGCSafeRetain(m_ptr); }
    ~wxObjcAutoRefFromAlloc()
    {   wxGCSafeRelease(m_ptr); }
    wxObjcAutoRefFromAlloc& operator=(const wxObjcAutoRefFromAlloc& otherRef)
    {   wxGCSafeRetain(otherRef.m_ptr);
        wxGCSafeRelease(m_ptr);
        m_ptr = otherRef.m_ptr;
        return *this;
    }
    operator T() const
    {   return static_cast<T>(m_ptr); }
    T operator->() const
    {   return static_cast<T>(m_ptr); }
protected:
    /*! @field      m_ptr       The pointer to the Objective-C object
        @discussion
        The pointer to the Objective-C object is typed as void* to avoid compiler-generated write
        barriers as would be used for implicitly __strong object pointers and to avoid the similar
        read barriers as would be used for an explicitly __weak object pointer.  The write barriers
        are useless unless this object is located in GC-managed heap which is highly unlikely.

        Since we guarantee strong reference via CFRetain/CFRelease the write-barriers are not needed
        at all, even if this object does happen to be allocated in GC-managed heap.
     */
    void *m_ptr;
};

/*!
    @class      wxObjcAutoRef
    @description
    A pointer holder that does retain its argument.
    NOTE: It is suggest that you instead use wxObjcAutoRefFromAlloc<T> foo([aRawPointer retain])
 */
template <class T>
class wxObjcAutoRef: public wxObjcAutoRefFromAlloc<T>
{
public:
    /*! @method     wxObjcAutoRef
        @description
        Uses the underlying wxObjcAutoRefFromAlloc and simply does a typical [p retain] such that
        in RR-mode the object is in effectively the same retain-count state as it would have been
        coming straight from an alloc method.
     */
    wxObjcAutoRef(T p = 0)
    :   wxObjcAutoRefFromAlloc<T>(p)
    {   // NOTE: ObjcRetain is correct because in GC-mode it balances ObjcRelease in our superclass constructor
        // In RR mode it does retain and the superclass does retain/release thus resulting in an overall retain.
        ObjcRetain(static_cast<T>(wxObjcAutoRefFromAlloc<T>::m_ptr));
    }
    ~wxObjcAutoRef() {}
    wxObjcAutoRef(const wxObjcAutoRef& otherRef)
    :   wxObjcAutoRefFromAlloc<T>(otherRef)
    {}
    wxObjcAutoRef(const wxObjcAutoRefFromAlloc<T>& otherRef)
    :   wxObjcAutoRefFromAlloc<T>(otherRef)
    {}
    wxObjcAutoRef& operator=(const wxObjcAutoRef& otherRef)
    {   return wxObjcAutoRefFromAlloc<T>::operator=(otherRef); }
};

#endif //ndef _WX_COCOA_OBJCREF_H__
