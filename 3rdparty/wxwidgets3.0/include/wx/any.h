/////////////////////////////////////////////////////////////////////////////
// Name:        wx/any.h
// Purpose:     wxAny class
// Author:      Jaakko Salli
// Modified by:
// Created:     07/05/2009
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ANY_H_
#define _WX_ANY_H_

#include "wx/defs.h"

#if wxUSE_ANY

#include <new> // for placement new
#include "wx/string.h"
#include "wx/meta/if.h"
#include "wx/typeinfo.h"
#include "wx/list.h"

// Size of the wxAny value buffer.
enum
{
    WX_ANY_VALUE_BUFFER_SIZE = 16
};

union wxAnyValueBuffer
{
    union Alignment
    {
    #if wxHAS_INT64
        wxInt64 m_int64;
    #endif
        long double m_longDouble;
        void ( *m_funcPtr )(void);
        void ( wxAnyValueBuffer::*m_mFuncPtr )(void);
    } m_alignment;

    void*   m_ptr;
    wxByte  m_buffer[WX_ANY_VALUE_BUFFER_SIZE];
};

//
// wxAnyValueType is base class for value type functionality for C++ data
// types used with wxAny. Usually the default template (wxAnyValueTypeImpl<>)
// will create a satisfactory wxAnyValueType implementation for a data type.
//
class WXDLLIMPEXP_BASE wxAnyValueType
{
    WX_DECLARE_ABSTRACT_TYPEINFO(wxAnyValueType)
public:
    /**
        Default constructor.
    */
    wxAnyValueType()
    {
    }

    /**
        Destructor.
    */
    virtual ~wxAnyValueType()
    {
    }

    /**
        This function is used for internal type matching.
    */
    virtual bool IsSameType(const wxAnyValueType* otherType) const = 0;

    /**
        This function is called every time the data in wxAny
        buffer needs to be freed.
    */
    virtual void DeleteValue(wxAnyValueBuffer& buf) const = 0;

    /**
        Implement this for buffer-to-buffer copy.

        @param src
            This is the source data buffer.

        @param dst
            This is the destination data buffer that is in either
            uninitialized or freed state.
    */
    virtual void CopyBuffer(const wxAnyValueBuffer& src,
                            wxAnyValueBuffer& dst) const = 0;

    /**
        Convert value into buffer of different type. Return false if
        not possible.
    */
    virtual bool ConvertValue(const wxAnyValueBuffer& src,
                              wxAnyValueType* dstType,
                              wxAnyValueBuffer& dst) const = 0;

    /**
        Use this template function for checking if wxAnyValueType represents
        a specific C++ data type.

        @remarks This template function does not work on some older compilers
                (such as Visual C++ 6.0). For full compiler compatibility
                please use wxANY_VALUE_TYPE_CHECK_TYPE(valueTypePtr, T) macro
                instead.

        @see wxAny::CheckType()
    */
    // FIXME-VC6: remove this hack when VC6 is no longer supported
    template <typename T>
    bool CheckType(T* reserved = NULL) const;

#if wxUSE_EXTENDED_RTTI
    virtual const wxTypeInfo* GetTypeInfo() const = 0;
#endif
private:
};


//
// We need to allocate wxAnyValueType instances in heap, and need to use
// scoped ptr to properly deallocate them in dynamic library use cases.
// Here we have a minimal specialized scoped ptr implementation to deal
// with various compiler-specific problems with template class' static
// member variable of template type with explicit constructor which
// is initialized in global scope.
//
class wxAnyValueTypeScopedPtr
{
public:
    wxAnyValueTypeScopedPtr(wxAnyValueType* ptr) : m_ptr(ptr) { }
    ~wxAnyValueTypeScopedPtr() { delete m_ptr; }
    wxAnyValueType* get() const { return m_ptr; }
private:
    wxAnyValueType* m_ptr;
};


//
// This method of checking the type is compatible with VC6
#define wxANY_VALUE_TYPE_CHECK_TYPE(valueTypePtr, T) \
    wxAnyValueTypeImpl<T>::IsSameClass(valueTypePtr)


/**
    Helper macro for defining user value types.

    Even though C++ RTTI would be fully available to use, we'd have to to
    facilitate sub-type system which allows, for instance, wxAny with
    signed short '15' to be treated equal to wxAny with signed long long '15'.
    Having sm_instance is important here.

    NB: We really need to have wxAnyValueType instances allocated
        in heap. They are stored as static template member variables,
        and with them we just can't be too careful (eg. not allocating
        them in heap broke the type identification in GCC).
*/
#define WX_DECLARE_ANY_VALUE_TYPE(CLS) \
    friend class wxAny; \
    WX_DECLARE_TYPEINFO_INLINE(CLS) \
public: \
    static bool IsSameClass(const wxAnyValueType* otherType) \
    { \
        return AreSameClasses(*sm_instance.get(), *otherType); \
    } \
    virtual bool IsSameType(const wxAnyValueType* otherType) const \
    { \
        return IsSameClass(otherType); \
    } \
private: \
    static bool AreSameClasses(const wxAnyValueType& a, const wxAnyValueType& b) \
    { \
        return wxTypeId(a) == wxTypeId(b); \
    } \
    static wxAnyValueTypeScopedPtr sm_instance; \
public: \
    static wxAnyValueType* GetInstance() \
    { \
        return sm_instance.get(); \
    }


#define WX_IMPLEMENT_ANY_VALUE_TYPE(CLS) \
wxAnyValueTypeScopedPtr CLS::sm_instance(new CLS());


#ifdef __VISUALC6__
    // "non dll-interface class 'xxx' used as base interface
    #pragma warning (push)
    #pragma warning (disable:4275)
#endif

/**
    Following are helper classes for the wxAnyValueTypeImplBase.
*/
namespace wxPrivate
{

template<typename T>
class wxAnyValueTypeOpsInplace
{
public:
    static void DeleteValue(wxAnyValueBuffer& buf)
    {
        T* value = reinterpret_cast<T*>(&buf.m_buffer[0]);
        value->~T();

        // Some compiler may given 'unused variable' warnings without this
        wxUnusedVar(value);
    }

    static void SetValue(const T& value,
                         wxAnyValueBuffer& buf)
    {
        // Use placement new
        void* const place = buf.m_buffer;
        ::new(place) T(value);
    }

    static const T& GetValue(const wxAnyValueBuffer& buf)
    {
        // Breaking this code into two lines should suppress
        // GCC's 'type-punned pointer will break strict-aliasing rules'
        // warning.
        const T* value = reinterpret_cast<const T*>(&buf.m_buffer[0]);
        return *value;
    }
};


template<typename T>
class wxAnyValueTypeOpsGeneric
{
public:
    template<typename T2>
    class DataHolder
    {
    public:
        DataHolder(const T2& value)
        {
            m_value = value;
        }
        virtual ~DataHolder() { }

        T2   m_value;
    private:
        wxDECLARE_NO_COPY_CLASS(DataHolder);
    };

    static void DeleteValue(wxAnyValueBuffer& buf)
    {
        DataHolder<T>* holder = static_cast<DataHolder<T>*>(buf.m_ptr);
        delete holder;
    }

    static void SetValue(const T& value,
                         wxAnyValueBuffer& buf)
    {
        DataHolder<T>* holder = new DataHolder<T>(value);
        buf.m_ptr = holder;
    }

    static const T& GetValue(const wxAnyValueBuffer& buf)
    {
        DataHolder<T>* holder = static_cast<DataHolder<T>*>(buf.m_ptr);
        return holder->m_value;
    }
};

} // namespace wxPrivate


/**
    Intermediate template for the generic value type implementation.
    We can derive from this same value type for multiple actual types
    (for instance, we can have wxAnyValueTypeImplInt for all signed
    integer types), and also easily implement specialized templates
    with specific dynamic type conversion.
*/
template<typename T>
class wxAnyValueTypeImplBase : public wxAnyValueType
{
    typedef typename wxIf< sizeof(T) <= WX_ANY_VALUE_BUFFER_SIZE,
                           wxPrivate::wxAnyValueTypeOpsInplace<T>,
                           wxPrivate::wxAnyValueTypeOpsGeneric<T> >::value
            Ops;

public:
    wxAnyValueTypeImplBase() : wxAnyValueType() { }
    virtual ~wxAnyValueTypeImplBase() { }

    virtual void DeleteValue(wxAnyValueBuffer& buf) const
    {
        Ops::DeleteValue(buf);
    }

    virtual void CopyBuffer(const wxAnyValueBuffer& src,
                            wxAnyValueBuffer& dst) const
    {
        Ops::SetValue(Ops::GetValue(src), dst);
    }

    /**
        It is important to reimplement this in any specialized template
        classes that inherit from wxAnyValueTypeImplBase.
    */
    static void SetValue(const T& value,
                         wxAnyValueBuffer& buf)
    {
        Ops::SetValue(value, buf);
    }

    /**
        It is important to reimplement this in any specialized template
        classes that inherit from wxAnyValueTypeImplBase.
    */
    static const T& GetValue(const wxAnyValueBuffer& buf)
    {
        return Ops::GetValue(buf);
    }
#if wxUSE_EXTENDED_RTTI
    virtual const wxTypeInfo* GetTypeInfo() const 
    {
        return wxGetTypeInfo((T*)NULL);
    }
#endif
};


/*
    Generic value type template. Note that bulk of the implementation
    resides in wxAnyValueTypeImplBase.
*/
template<typename T>
class wxAnyValueTypeImpl : public wxAnyValueTypeImplBase<T>
{
    WX_DECLARE_ANY_VALUE_TYPE(wxAnyValueTypeImpl<T>)
public:
    wxAnyValueTypeImpl() : wxAnyValueTypeImplBase<T>() { }
    virtual ~wxAnyValueTypeImpl() { }

    virtual bool ConvertValue(const wxAnyValueBuffer& src,
                              wxAnyValueType* dstType,
                              wxAnyValueBuffer& dst) const
    {
        wxUnusedVar(src);
        wxUnusedVar(dstType);
        wxUnusedVar(dst);
        return false;
    }
};

template<typename T>
wxAnyValueTypeScopedPtr wxAnyValueTypeImpl<T>::sm_instance = new wxAnyValueTypeImpl<T>();


//
// Helper macro for using same base value type implementation for multiple
// actual C++ data types.
//
#define _WX_ANY_DEFINE_SUB_TYPE(T, CLSTYPE) \
template<> \
class wxAnyValueTypeImpl<T> : public wxAnyValueTypeImpl##CLSTYPE \
{ \
    typedef wxAnyBase##CLSTYPE##Type UseDataType; \
public: \
    wxAnyValueTypeImpl() : wxAnyValueTypeImpl##CLSTYPE() { } \
    virtual ~wxAnyValueTypeImpl() { } \
    static void SetValue(const T& value, wxAnyValueBuffer& buf) \
    { \
        void* voidPtr = reinterpret_cast<void*>(&buf.m_buffer[0]); \
        UseDataType* dptr = reinterpret_cast<UseDataType*>(voidPtr); \
        *dptr = static_cast<UseDataType>(value); \
    } \
    static T GetValue(const wxAnyValueBuffer& buf) \
    { \
        const void* voidPtr = \
            reinterpret_cast<const void*>(&buf.m_buffer[0]); \
        const UseDataType* sptr = \
            reinterpret_cast<const UseDataType*>(voidPtr); \
        return static_cast<T>(*sptr); \
    } 

#if wxUSE_EXTENDED_RTTI
#define WX_ANY_DEFINE_SUB_TYPE(T, CLSTYPE) \
_WX_ANY_DEFINE_SUB_TYPE(T, CLSTYPE)\
    virtual const wxTypeInfo* GetTypeInfo() const  \
    { \
        return wxGetTypeInfo((T*)NULL); \
    } \
};
#else
#define WX_ANY_DEFINE_SUB_TYPE(T, CLSTYPE) \
_WX_ANY_DEFINE_SUB_TYPE(T, CLSTYPE)\
};
#endif

//
//  Integer value types
//

#ifdef wxLongLong_t
    typedef wxLongLong_t wxAnyBaseIntType;
    typedef wxULongLong_t wxAnyBaseUintType;
#else
    typedef long wxAnyBaseIntType;
    typedef unsigned long wxAnyBaseUintType;
#endif


class WXDLLIMPEXP_BASE wxAnyValueTypeImplInt :
    public wxAnyValueTypeImplBase<wxAnyBaseIntType>
{
    WX_DECLARE_ANY_VALUE_TYPE(wxAnyValueTypeImplInt)
public:
    wxAnyValueTypeImplInt() :
        wxAnyValueTypeImplBase<wxAnyBaseIntType>() { }
    virtual ~wxAnyValueTypeImplInt() { }

    virtual bool ConvertValue(const wxAnyValueBuffer& src,
                              wxAnyValueType* dstType,
                              wxAnyValueBuffer& dst) const;
};


class WXDLLIMPEXP_BASE wxAnyValueTypeImplUint :
    public wxAnyValueTypeImplBase<wxAnyBaseUintType>
{
    WX_DECLARE_ANY_VALUE_TYPE(wxAnyValueTypeImplUint)
public:
    wxAnyValueTypeImplUint() :
        wxAnyValueTypeImplBase<wxAnyBaseUintType>() { }
    virtual ~wxAnyValueTypeImplUint() { }

    virtual bool ConvertValue(const wxAnyValueBuffer& src,
                              wxAnyValueType* dstType,
                              wxAnyValueBuffer& dst) const;
};


WX_ANY_DEFINE_SUB_TYPE(signed long, Int)
WX_ANY_DEFINE_SUB_TYPE(signed int, Int)
WX_ANY_DEFINE_SUB_TYPE(signed short, Int)
WX_ANY_DEFINE_SUB_TYPE(signed char, Int)
#ifdef wxLongLong_t
WX_ANY_DEFINE_SUB_TYPE(wxLongLong_t, Int)
#endif

WX_ANY_DEFINE_SUB_TYPE(unsigned long, Uint)
WX_ANY_DEFINE_SUB_TYPE(unsigned int, Uint)
WX_ANY_DEFINE_SUB_TYPE(unsigned short, Uint)
WX_ANY_DEFINE_SUB_TYPE(unsigned char, Uint)
#ifdef wxLongLong_t
WX_ANY_DEFINE_SUB_TYPE(wxULongLong_t, Uint)
#endif


//
// This macro is used in header, but then in source file we must have:
// WX_IMPLEMENT_ANY_VALUE_TYPE(wxAnyValueTypeImpl##TYPENAME)
//
#define _WX_ANY_DEFINE_CONVERTIBLE_TYPE(T, TYPENAME, CONVFUNC, GV) \
class WXDLLIMPEXP_BASE wxAnyValueTypeImpl##TYPENAME : \
    public wxAnyValueTypeImplBase<T> \
{ \
    WX_DECLARE_ANY_VALUE_TYPE(wxAnyValueTypeImpl##TYPENAME) \
public: \
    wxAnyValueTypeImpl##TYPENAME() : \
        wxAnyValueTypeImplBase<T>() { } \
    virtual ~wxAnyValueTypeImpl##TYPENAME() { } \
    virtual bool ConvertValue(const wxAnyValueBuffer& src, \
                              wxAnyValueType* dstType, \
                              wxAnyValueBuffer& dst) const \
    { \
        GV value = GetValue(src); \
        return CONVFUNC(value, dstType, dst); \
    } \
}; \
template<> \
class wxAnyValueTypeImpl<T> : public wxAnyValueTypeImpl##TYPENAME \
{ \
public: \
    wxAnyValueTypeImpl() : wxAnyValueTypeImpl##TYPENAME() { } \
    virtual ~wxAnyValueTypeImpl() { } \
};

#define WX_ANY_DEFINE_CONVERTIBLE_TYPE(T, TYPENAME, CONVFUNC, BT) \
_WX_ANY_DEFINE_CONVERTIBLE_TYPE(T, TYPENAME, CONVFUNC, BT) \

#define WX_ANY_DEFINE_CONVERTIBLE_TYPE_BASE(T, TYPENAME, CONVFUNC) \
_WX_ANY_DEFINE_CONVERTIBLE_TYPE(T, TYPENAME, \
                                CONVFUNC, const T&) \

//
// String value type
//

// Convert wxString to destination wxAny value type
extern WXDLLIMPEXP_BASE bool wxAnyConvertString(const wxString& value,
                                                wxAnyValueType* dstType,
                                                wxAnyValueBuffer& dst);

WX_ANY_DEFINE_CONVERTIBLE_TYPE_BASE(wxString, wxString, wxAnyConvertString)
WX_ANY_DEFINE_CONVERTIBLE_TYPE(const char*, ConstCharPtr,
                               wxAnyConvertString, wxString)
WX_ANY_DEFINE_CONVERTIBLE_TYPE(const wchar_t*, ConstWchar_tPtr,
                               wxAnyConvertString, wxString)

//
// Bool value type
//
template<>
class WXDLLIMPEXP_BASE wxAnyValueTypeImpl<bool> :
    public wxAnyValueTypeImplBase<bool>
{
    WX_DECLARE_ANY_VALUE_TYPE(wxAnyValueTypeImpl<bool>)
public:
    wxAnyValueTypeImpl() :
        wxAnyValueTypeImplBase<bool>() { }
    virtual ~wxAnyValueTypeImpl() { }

    virtual bool ConvertValue(const wxAnyValueBuffer& src,
                              wxAnyValueType* dstType,
                              wxAnyValueBuffer& dst) const;
};

//
// Floating point value type
//
class WXDLLIMPEXP_BASE wxAnyValueTypeImplDouble :
    public wxAnyValueTypeImplBase<double>
{
    WX_DECLARE_ANY_VALUE_TYPE(wxAnyValueTypeImplDouble)
public:
    wxAnyValueTypeImplDouble() :
        wxAnyValueTypeImplBase<double>() { }
    virtual ~wxAnyValueTypeImplDouble() { }

    virtual bool ConvertValue(const wxAnyValueBuffer& src,
                              wxAnyValueType* dstType,
                              wxAnyValueBuffer& dst) const;
};

// WX_ANY_DEFINE_SUB_TYPE requires this
typedef double wxAnyBaseDoubleType;

WX_ANY_DEFINE_SUB_TYPE(float, Double)
WX_ANY_DEFINE_SUB_TYPE(double, Double)


//
// Defines a dummy wxAnyValueTypeImpl<> with given export
// declaration. This is needed if a class is used with
// wxAny in both user shared library and application.
//
#define wxDECLARE_ANY_TYPE(CLS, DECL) \
template<> \
class DECL wxAnyValueTypeImpl<CLS> : \
    public wxAnyValueTypeImplBase<CLS> \
{ \
    WX_DECLARE_ANY_VALUE_TYPE(wxAnyValueTypeImpl<CLS>) \
public: \
    wxAnyValueTypeImpl() : \
        wxAnyValueTypeImplBase<CLS>() { } \
    virtual ~wxAnyValueTypeImpl() { } \
 \
    virtual bool ConvertValue(const wxAnyValueBuffer& src, \
                              wxAnyValueType* dstType, \
                              wxAnyValueBuffer& dst) const \
    { \
        wxUnusedVar(src); \
        wxUnusedVar(dstType); \
        wxUnusedVar(dst); \
        return false; \
    } \
};


// Make sure some of wx's own types get the right wxAnyValueType export
// (this is needed only for types that are referred to from wxBase.
// currently we may not use any of these types from there, but let's
// use the macro on at least one to make sure it compiles since we can't
// really test it properly in unit tests since a separate DLL would
// be needed).
#if wxUSE_DATETIME
    #include "wx/datetime.h"
    wxDECLARE_ANY_TYPE(wxDateTime, WXDLLIMPEXP_BASE)
#endif

//#include "wx/object.h"
//wxDECLARE_ANY_TYPE(wxObject*, WXDLLIMPEXP_BASE)

//#include "wx/arrstr.h"
//wxDECLARE_ANY_TYPE(wxArrayString, WXDLLIMPEXP_BASE)


#if wxUSE_VARIANT

class WXDLLIMPEXP_FWD_BASE wxAnyToVariantRegistration;

// Because of header inter-dependencies, cannot include this earlier
#include "wx/variant.h"

//
// wxVariantData* data type implementation. For cases when appropriate
// wxAny<->wxVariant conversion code is missing.
//

class WXDLLIMPEXP_BASE wxAnyValueTypeImplVariantData :
    public wxAnyValueTypeImplBase<wxVariantData*>
{
    WX_DECLARE_ANY_VALUE_TYPE(wxAnyValueTypeImplVariantData)
public:
    wxAnyValueTypeImplVariantData() :
        wxAnyValueTypeImplBase<wxVariantData*>() { }
    virtual ~wxAnyValueTypeImplVariantData() { }

    virtual void DeleteValue(wxAnyValueBuffer& buf) const
    {
        wxVariantData* data = static_cast<wxVariantData*>(buf.m_ptr);
        if ( data )
            data->DecRef();
    }

    virtual void CopyBuffer(const wxAnyValueBuffer& src,
                            wxAnyValueBuffer& dst) const
    {
        wxVariantData* data = static_cast<wxVariantData*>(src.m_ptr);
        if ( data )
            data->IncRef();
        dst.m_ptr = data;
    }

    static void SetValue(wxVariantData* value,
                         wxAnyValueBuffer& buf)
    {
        value->IncRef();
        buf.m_ptr = value;
    }

    static wxVariantData* GetValue(const wxAnyValueBuffer& buf)
    {
        return static_cast<wxVariantData*>(buf.m_ptr);
    }

    virtual bool ConvertValue(const wxAnyValueBuffer& src,
                              wxAnyValueType* dstType,
                              wxAnyValueBuffer& dst) const
    {
        wxUnusedVar(src);
        wxUnusedVar(dstType);
        wxUnusedVar(dst);
        return false;
    }
};

template<>
class wxAnyValueTypeImpl<wxVariantData*> :
    public wxAnyValueTypeImplVariantData
{
public:
    wxAnyValueTypeImpl() : wxAnyValueTypeImplVariantData() { }
    virtual ~wxAnyValueTypeImpl() { }
};

#endif // wxUSE_VARIANT

#ifdef __VISUALC6__
    // Re-enable useless VC6 warnings
    #pragma warning (pop)
#endif


/*
    Let's define a discrete Null value so we don't have to really
    ever check if wxAny.m_type pointer is NULL or not. This is an
    optimization, mostly. Implementation of this value type is
    "hidden" in the source file.
*/
extern WXDLLIMPEXP_DATA_BASE(wxAnyValueType*) wxAnyNullValueType;


//
// We need to implement custom signed/unsigned int equals operators
// for signed/unsigned (eg. wxAny(128UL) == 128L) comparisons to work.
#define WXANY_IMPLEMENT_INT_EQ_OP(TS, TUS) \
bool operator==(TS value) const \
{ \
    if ( wxAnyValueTypeImpl<TS>::IsSameClass(m_type) ) \
        return (value == static_cast<TS> \
                (wxAnyValueTypeImpl<TS>::GetValue(m_buffer))); \
    if ( wxAnyValueTypeImpl<TUS>::IsSameClass(m_type) ) \
        return (value == static_cast<TS> \
                (wxAnyValueTypeImpl<TUS>::GetValue(m_buffer))); \
    return false; \
} \
bool operator==(TUS value) const \
{ \
    if ( wxAnyValueTypeImpl<TUS>::IsSameClass(m_type) ) \
        return (value == static_cast<TUS> \
                (wxAnyValueTypeImpl<TUS>::GetValue(m_buffer))); \
    if ( wxAnyValueTypeImpl<TS>::IsSameClass(m_type) ) \
        return (value == static_cast<TUS> \
                (wxAnyValueTypeImpl<TS>::GetValue(m_buffer))); \
    return false; \
}


#if wxUSE_VARIANT

// Note that the following functions are implemented outside wxAny class
// so that it can reside entirely in header and lack the export declaration.

// Helper function used to associate wxAnyValueType with a wxVariantData.
extern WXDLLIMPEXP_BASE void
wxPreRegisterAnyToVariant(wxAnyToVariantRegistration* reg);

// This function performs main wxAny to wxVariant conversion duties.
extern WXDLLIMPEXP_BASE bool
wxConvertAnyToVariant(const wxAny& any, wxVariant* variant);

#endif // wxUSE_VARIANT


//
// The wxAny class represents a container for any type. A variant's value
// can be changed at run time, possibly to a different type of value.
//
// As standard, wxAny can store value of almost any type, in a fairly
// optimal manner even.
//
class wxAny
{
public:
    /**
        Default constructor.
    */
    wxAny()
    {
        m_type = wxAnyNullValueType;
    }

    /**
        Destructor.
    */
    ~wxAny()
    {
        m_type->DeleteValue(m_buffer);
    }

    //@{
    /**
        Various constructors.
    */
    template<typename T>
    wxAny(const T& value)
    {
        m_type = wxAnyValueTypeImpl<T>::sm_instance.get();
        wxAnyValueTypeImpl<T>::SetValue(value, m_buffer);
    }

    // These two constructors are needed to deal with string literals
    wxAny(const char* value)
    {
        m_type = wxAnyValueTypeImpl<const char*>::sm_instance.get();
        wxAnyValueTypeImpl<const char*>::SetValue(value, m_buffer);
    }
    wxAny(const wchar_t* value)
    {
        m_type = wxAnyValueTypeImpl<const wchar_t*>::sm_instance.get();
        wxAnyValueTypeImpl<const wchar_t*>::SetValue(value, m_buffer);
    }

    wxAny(const wxAny& any)
    {
        m_type = wxAnyNullValueType;
        AssignAny(any);
    }

#if wxUSE_VARIANT
    wxAny(const wxVariant& variant)
    {
        m_type = wxAnyNullValueType;
        AssignVariant(variant);
    }
#endif

    //@}

    /**
        Use this template function for checking if this wxAny holds
        a specific C++ data type.

        @remarks This template function does not work on some older compilers
                (such as Visual C++ 6.0). For full compiler ccompatibility
                please use wxANY_CHECK_TYPE(any, T) macro instead.

        @see wxAnyValueType::CheckType()
    */
    // FIXME-VC6: remove this hack when VC6 is no longer supported
    template <typename T>
    bool CheckType(T* = NULL) const
    {
        return m_type->CheckType<T>();
    }

    /**
        Returns the value type as wxAnyValueType instance.

        @remarks You cannot reliably test whether two wxAnys are of
                 same value type by simply comparing return values
                 of wxAny::GetType(). Instead, use wxAny::HasSameType().

        @see HasSameType()
    */
    const wxAnyValueType* GetType() const
    {
        return m_type;
    }

    /**
        Returns @true if this and another wxAny have the same
        value type.
    */
    bool HasSameType(const wxAny& other) const
    {
        return GetType()->IsSameType(other.GetType());
    }

    /**
        Tests if wxAny is null (that is, whether there is no data).
    */
    bool IsNull() const
    {
        return (m_type == wxAnyNullValueType);
    }

    /**
        Makes wxAny null (that is, clears it).
    */
    void MakeNull()
    {
        m_type->DeleteValue(m_buffer);
        m_type = wxAnyNullValueType;
    }

    //@{
    /**
        Assignment operators.
    */
    template<typename T>
    wxAny& operator=(const T &value)
    {
        m_type->DeleteValue(m_buffer);
        m_type = wxAnyValueTypeImpl<T>::sm_instance.get();
        wxAnyValueTypeImpl<T>::SetValue(value, m_buffer);
        return *this;
    }

    wxAny& operator=(const wxAny &any)
    {
        if (this != &any)
            AssignAny(any);
        return *this;
    }

#if wxUSE_VARIANT
    wxAny& operator=(const wxVariant &variant)
    {
        AssignVariant(variant);
        return *this;
    }
#endif

    // These two operators are needed to deal with string literals
    wxAny& operator=(const char* value)
    {
        Assign(value);
        return *this;
    }
    wxAny& operator=(const wchar_t* value)
    {
        Assign(value);
        return *this;
    }

    //@{
    /**
        Equality operators.
    */
    bool operator==(const wxString& value) const
    {
        wxString value2;
        if ( !GetAs(&value2) )
            return false;
        return value == value2;
    }

    bool operator==(const char* value) const
        { return (*this) == wxString(value); }
    bool operator==(const wchar_t* value) const
        { return (*this) == wxString(value); }

    //
    // We need to implement custom signed/unsigned int equals operators
    // for signed/unsigned (eg. wxAny(128UL) == 128L) comparisons to work.
    WXANY_IMPLEMENT_INT_EQ_OP(signed char, unsigned char)
    WXANY_IMPLEMENT_INT_EQ_OP(signed short, unsigned short)
    WXANY_IMPLEMENT_INT_EQ_OP(signed int, unsigned int)
    WXANY_IMPLEMENT_INT_EQ_OP(signed long, unsigned long)
#ifdef wxLongLong_t
    WXANY_IMPLEMENT_INT_EQ_OP(wxLongLong_t, wxULongLong_t)
#endif

    wxGCC_WARNING_SUPPRESS(float-equal)

    bool operator==(float value) const
    {
        if ( !wxAnyValueTypeImpl<float>::IsSameClass(m_type) )
            return false;

        return value ==
            static_cast<float>
                (wxAnyValueTypeImpl<float>::GetValue(m_buffer));
    }

    bool operator==(double value) const
    {
        if ( !wxAnyValueTypeImpl<double>::IsSameClass(m_type) )
            return false;

        return value ==
            static_cast<double>
                (wxAnyValueTypeImpl<double>::GetValue(m_buffer));
    }

    wxGCC_WARNING_RESTORE(float-equal)

    bool operator==(bool value) const
    {
        if ( !wxAnyValueTypeImpl<bool>::IsSameClass(m_type) )
            return false;

        return value == (wxAnyValueTypeImpl<bool>::GetValue(m_buffer));
    }

    //@}

    //@{
    /**
        Inequality operators (implement as template).
    */
    template<typename T>
    bool operator!=(const T& value) const
        { return !((*this) == value); }
    //@}

    /**
        This template function converts wxAny into given type. In most cases
        no type conversion is performed, so if the type is incorrect an
        assertion failure will occur.

        @remarks For convenience, conversion is done when T is wxString. This
                 is useful when a string literal (which are treated as
                 const char* and const wchar_t*) has been assigned to wxAny.

                 This template function may not work properly with Visual C++
                 6. For full compiler compatibility, please use
                 wxANY_AS(any, T) macro instead.
    */
    // FIXME-VC6: remove this hack when VC6 is no longer supported
    template<typename T>
    T As(T* = NULL) const
    {
        if ( !wxAnyValueTypeImpl<T>::IsSameClass(m_type) )
        {
            wxFAIL_MSG("Incorrect or non-convertible data type");
        }

        return static_cast<T>(wxAnyValueTypeImpl<T>::GetValue(m_buffer));
    }

    // Allow easy conversion from 'const char *' etc. to wxString
    // FIXME-VC6: remove this hack when VC6 is no longer supported
    //template<>
    wxString As(wxString*) const
    {
        wxString value;
        if ( !GetAs(&value) )
        {
            wxFAIL_MSG("Incorrect or non-convertible data type");
        }
        return value;
    }

#if wxUSE_EXTENDED_RTTI
    const wxTypeInfo* GetTypeInfo() const
    {
        return m_type->GetTypeInfo();
    }
#endif
    /**
        Template function that retrieves and converts the value of this
        variant to the type that T* value is.

        @return Returns @true if conversion was successful.
    */
    template<typename T>
    bool GetAs(T* value) const
    {
        if ( !wxAnyValueTypeImpl<T>::IsSameClass(m_type) )
        {
            wxAnyValueType* otherType =
                wxAnyValueTypeImpl<T>::sm_instance.get();
            wxAnyValueBuffer temp_buf;

            if ( !m_type->ConvertValue(m_buffer, otherType, temp_buf) )
                return false;

            *value =
                static_cast<T>(wxAnyValueTypeImpl<T>::GetValue(temp_buf));
            otherType->DeleteValue(temp_buf);

            return true;
        }
        *value = static_cast<T>(wxAnyValueTypeImpl<T>::GetValue(m_buffer));
        return true;
    }

#if wxUSE_VARIANT
    // GetAs() wxVariant specialization
    bool GetAs(wxVariant* value) const
    {
        return wxConvertAnyToVariant(*this, value);
    }
#endif

private:
    // Assignment functions
    void AssignAny(const wxAny& any)
    {
        // Must delete value - CopyBuffer() never does that
        m_type->DeleteValue(m_buffer);

        wxAnyValueType* newType = any.m_type;

        if ( !newType->IsSameType(m_type) )
            m_type = newType;

        newType->CopyBuffer(any.m_buffer, m_buffer);
    }

#if wxUSE_VARIANT
    void AssignVariant(const wxVariant& variant)
    {
        wxVariantData* data = variant.GetData();

        if ( data && data->GetAsAny(this) )
            return;

        m_type->DeleteValue(m_buffer);

        if ( variant.IsNull() )
        {
            // Init as Null
            m_type = wxAnyNullValueType;
        }
        else
        {
            // If everything else fails, wrap the whole wxVariantData
            m_type = wxAnyValueTypeImpl<wxVariantData*>::sm_instance.get();
            wxAnyValueTypeImpl<wxVariantData*>::SetValue(data, m_buffer);
        }
    }
#endif

    template<typename T>
    void Assign(const T &value)
    {
        m_type->DeleteValue(m_buffer);
        m_type = wxAnyValueTypeImpl<T>::sm_instance.get();
        wxAnyValueTypeImpl<T>::SetValue(value, m_buffer);
    }

    // Data
    wxAnyValueBuffer    m_buffer;
    wxAnyValueType*     m_type;
};


//
// This method of checking the type is compatible with VC6
#define wxANY_CHECK_TYPE(any, T) \
    wxANY_VALUE_TYPE_CHECK_TYPE((any).GetType(), T)


//
// This method of getting the value is compatible with VC6
#define wxANY_AS(any, T) \
    (any).As(static_cast<T*>(NULL))


template<typename T>
inline bool wxAnyValueType::CheckType(T* reserved) const
{
    wxUnusedVar(reserved);
    return wxAnyValueTypeImpl<T>::IsSameClass(this);
}

WX_DECLARE_LIST_WITH_DECL(wxAny, wxAnyList, class WXDLLIMPEXP_BASE);

#endif // wxUSE_ANY

#endif // _WX_ANY_H_
