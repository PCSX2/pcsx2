///////////////////////////////////////////////////////////////////////////////
// Name:        msw/ole/safearray.h
// Purpose:     Helpers for working with OLE SAFEARRAYs.
// Author:      PB
// Created:     2012-09-23
// Copyright:   (c) 2012 wxWidgets development team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _MSW_OLE_SAFEARRAY_H_
#define _MSW_OLE_SAFEARRAY_H_

#include "wx/msw/ole/oleutils.h"

#if wxUSE_OLE && wxUSE_VARIANT

/*
    wxSafeArray is wxWidgets wrapper for working with MS Windows SAFEARRAYs.
    It also has convenience functions for converting between SAFEARRAY
    and wxVariant with list type or wxArrayString.
*/

// The base class with type-independent methods. It exists solely in order to
// reduce the template bloat.
class WXDLLIMPEXP_CORE wxSafeArrayBase
{
public:
    // If owns a SAFEARRAY, it's unlocked and destroyed.
    virtual ~wxSafeArrayBase() { Destroy(); }

    // Unlocks and destroys the owned SAFEARRAY.
    void Destroy();

    // Unlocks the owned SAFEARRAY, returns it and gives up its ownership.
    SAFEARRAY* Detach();

    // Returns true if has a valid SAFEARRAY.
    bool HasArray() const { return m_array != NULL; }

    // Returns the number of dimensions.
    size_t GetDim() const;

    // Returns lower bound for dimension dim in bound. Dimensions start at 1.
    bool GetLBound(size_t dim, long& bound) const;

    // Returns upper bound for dimension dim in bound. Dimensions start at 1.
    bool GetUBound(size_t dim, long& bound) const;

    // Returns element count for dimension dim. Dimensions start at 1.
    size_t GetCount(size_t dim) const;

protected:
    // Default constructor, protected so the class can't be used on its own,
    // it's only used as a base class of wxSafeArray<>.
    wxSafeArrayBase()
    {
        m_array = NULL;
    }

    bool Lock();
    bool Unlock();

    SAFEARRAY* m_array;
};

// wxSafeArrayConvertor<> must be specialized for the type in order to allow
// using it with wxSafeArray<>.
//
// We specialize it below for the standard types.
template <VARTYPE varType>
struct wxSafeArrayConvertor {};

/**
    Macro for specializing wxSafeArrayConvertor for simple types.

    The template parameters are:
        - externType: basic C data type, e.g. wxFloat64 or wxInt32
        - varType: corresponding VARIANT type constant, e.g. VT_R8 or VT_I4.
*/
#define wxSPECIALIZE_WXSAFEARRAY_CONVERTOR_SIMPLE(externType, varType) \
template <>                                                 \
struct wxSafeArrayConvertor<varType>                        \
{                                                           \
    typedef externType externT;                             \
    typedef externT    internT;                             \
    static bool ToArray(const externT& from, internT& to)   \
    {                                                       \
        to = from;                                          \
        return true;                                        \
    }                                                       \
    static bool FromArray(const internT& from, externT& to) \
    {                                                       \
        to = from;                                          \
        return true;                                        \
    }                                                       \
}

wxSPECIALIZE_WXSAFEARRAY_CONVERTOR_SIMPLE(wxInt16, VT_I2);
wxSPECIALIZE_WXSAFEARRAY_CONVERTOR_SIMPLE(wxInt32, VT_I4);
wxSPECIALIZE_WXSAFEARRAY_CONVERTOR_SIMPLE(wxFloat32, VT_R4);
wxSPECIALIZE_WXSAFEARRAY_CONVERTOR_SIMPLE(wxFloat64, VT_R8);

// Specialization for VT_BSTR using wxString.
template <>
struct wxSafeArrayConvertor<VT_BSTR>
{
    typedef wxString externT;
    typedef BSTR internT;

    static bool ToArray(const wxString& from, BSTR& to)
    {
        BSTR bstr = wxConvertStringToOle(from);

        if ( !bstr && !from.empty() )
        {
            // BSTR can be NULL for empty strings but if the string was
            // not empty, it means we failed to allocate memory for it.
            return false;
        }
        to = bstr;
        return true;
    }

    static bool FromArray(const BSTR from, wxString& to)
    {
        to = wxConvertStringFromOle(from);
        return true;
    }
};

// Specialization for VT_VARIANT using wxVariant.
template <>
struct wxSafeArrayConvertor<VT_VARIANT>
{
    typedef wxVariant externT;
    typedef VARIANT internT;

    static bool ToArray(const wxVariant& from, VARIANT& to)
    {
        return wxConvertVariantToOle(from, to);
    }

    static bool FromArray(const VARIANT& from, wxVariant& to)
    {
        return wxConvertOleToVariant(from, to);
    }
};


template <VARTYPE varType>
class wxSafeArray : public wxSafeArrayBase
{
public:
    typedef wxSafeArrayConvertor<varType> Convertor;
    typedef typename Convertor::internT internT;
    typedef typename Convertor::externT externT;

    // Default constructor.
    wxSafeArray()
    {
        m_array = NULL;
    }

    // Creates and locks a zero-based one-dimensional SAFEARRAY with the given
    // number of elements.
    bool Create(size_t count)
    {
        SAFEARRAYBOUND bound;

        bound.lLbound = 0;
        bound.cElements = count;
        return Create(&bound, 1);
    }

    // Creates and locks a SAFEARRAY. See SafeArrayCreate() in MSDN
    // documentation for more information.
    bool Create(SAFEARRAYBOUND* bound, size_t dimensions)
    {
        wxCHECK_MSG( !m_array, false, wxS("Can't be created twice") );

        m_array = SafeArrayCreate(varType, dimensions, bound);
        if ( !m_array )
            return false;

        return Lock();
    }

    /**
        Creates a 0-based one-dimensional SAFEARRAY from wxVariant with the
        list type.

        Can be called only for wxSafeArray<VT_VARIANT>.
    */
    bool CreateFromListVariant(const wxVariant& variant)
    {
        wxCHECK(varType == VT_VARIANT, false);
        wxCHECK(variant.GetType() == wxS("list"), false);

        if ( !Create(variant.GetCount()) )
            return false;

        VARIANT* data = static_cast<VARIANT*>(m_array->pvData);

        for ( size_t i = 0; i < variant.GetCount(); i++)
        {
            if ( !Convertor::ToArray(variant[i], data[i]) )
                return false;
        }
        return true;
    }

    /**
        Creates a 0-based one-dimensional SAFEARRAY from wxArrayString.

        Can be called only for wxSafeArray<VT_BSTR>.
    */
    bool CreateFromArrayString(const wxArrayString& strings)
    {
        wxCHECK(varType == VT_BSTR, false);

        if ( !Create(strings.size()) )
            return false;

        BSTR* data = static_cast<BSTR*>(m_array->pvData);

        for ( size_t i = 0; i < strings.size(); i++ )
        {
            if ( !Convertor::ToArray(strings[i], data[i]) )
                return false;
        }
        return true;
    }

    /**
        Attaches and locks an existing SAFEARRAY.
        The array must have the same VARTYPE as this wxSafeArray was
        instantiated with.
    */
    bool Attach(SAFEARRAY* array)
    {
        wxCHECK_MSG(!m_array && array, false,
                    wxS("Can only attach a valid array to an uninitialized one") );

        VARTYPE vt;
        HRESULT hr = SafeArrayGetVartype(array, &vt);
        if ( FAILED(hr) )
        {
            wxLogApiError(wxS("SafeArrayGetVarType()"), hr);
            return false;
        }

        wxCHECK_MSG(vt == varType, false,
                    wxS("Attaching array of invalid type"));

        m_array = array;
        return Lock();
    }

    /**
        Indices have the same row-column order as rgIndices in
        SafeArrayPutElement(), i.e. they follow BASIC rules, NOT C ones.
    */
    bool SetElement(long* indices, const externT& element)
    {
        wxCHECK_MSG( m_array, false, wxS("Uninitialized array") );
        wxCHECK_MSG( indices, false, wxS("Invalid index") );

        internT* data;

        if ( FAILED( SafeArrayPtrOfIndex(m_array, (LONG *)indices, (void**)&data) ) )
            return false;

        return Convertor::ToArray(element, *data);
    }

    /**
        Indices have the same row-column order as rgIndices in
        SafeArrayPutElement(), i.e. they follow BASIC rules, NOT C ones.
    */
    bool GetElement(long* indices, externT& element) const
    {
        wxCHECK_MSG( m_array, false, wxS("Uninitialized array") );
        wxCHECK_MSG( indices, false, wxS("Invalid index") );

        internT* data;

        if ( FAILED( SafeArrayPtrOfIndex(m_array, (LONG *)indices, (void**)&data) ) )
            return false;

        return Convertor::FromArray(*data, element);
    }

    /**
        Converts the array to a wxVariant with the list type, regardless of the
        underlying SAFEARRAY type.

        If the array is multidimensional, it is flattened using the alghoritm
        originally employed in wxConvertOleToVariant().
    */
    bool ConvertToVariant(wxVariant& variant) const
    {
        wxCHECK_MSG( m_array, false, wxS("Uninitialized array") );

        size_t dims = m_array->cDims;
        size_t count = 1;

        for ( size_t i = 0; i < dims; i++ )
            count *= m_array->rgsabound[i].cElements;

        const internT* data = static_cast<const internT*>(m_array->pvData);
        externT element;

        variant.ClearList();
        for ( size_t i1 = 0; i1 < count; i1++ )
        {
            if ( !Convertor::FromArray(data[i1], element) )
            {
                variant.ClearList();
                return false;
            }
            variant.Append(element);
        }
        return true;
    }

    /**
        Converts an array to an ArrayString.

        Can be called only for wxSafeArray<VT_BSTR>. If the array is
        multidimensional, it is flattened using the alghoritm originally
        employed in wxConvertOleToVariant().
    */
    bool ConvertToArrayString(wxArrayString& strings) const
    {
        wxCHECK_MSG( m_array, false, wxS("Uninitialized array") );
        wxCHECK(varType == VT_BSTR, false);

        size_t dims = m_array->cDims;
        size_t count = 1;

        for ( size_t i = 0; i < dims; i++ )
            count *= m_array->rgsabound[i].cElements;

        const BSTR* data = static_cast<const BSTR*>(m_array->pvData);
        wxString element;

        strings.clear();
        strings.reserve(count);
        for ( size_t i1 = 0; i1 < count; i1++ )
        {
            if ( !Convertor::FromArray(data[i1], element) )
            {
                strings.clear();
                return false;
            }
            strings.push_back(element);
        }
        return true;
    }

    static bool ConvertToVariant(SAFEARRAY* psa, wxVariant& variant)
    {
        wxSafeArray<varType> sa;
        bool result = false;

        if ( sa.Attach(psa) )
            result = sa.ConvertToVariant(variant);

        if ( sa.HasArray() )
            sa.Detach();

        return result;
    }

    static bool ConvertToArrayString(SAFEARRAY* psa, wxArrayString& strings)
    {
        wxSafeArray<varType> sa;
        bool result = false;

        if ( sa.Attach(psa) )
            result = sa.ConvertToArrayString(strings);

        if ( sa.HasArray() )
            sa.Detach();

        return result;
    }

    wxDECLARE_NO_COPY_TEMPLATE_CLASS(wxSafeArray, varType);
};

#endif // wxUSE_OLE && wxUSE_VARIANT

#endif // _MSW_OLE_SAFEARRAY_H_
