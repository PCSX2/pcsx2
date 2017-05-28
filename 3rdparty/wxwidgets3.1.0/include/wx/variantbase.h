/////////////////////////////////////////////////////////////////////////////
// Name:        wx/variantbase.h
// Purpose:     wxVariantBase class, a minimal version of wxVariant used by XTI
// Author:      Julian Smart
// Modified by: Francesco Montorsi
// Created:     10/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_VARIANTBASE_H_
#define _WX_VARIANTBASE_H_

#include "wx/defs.h"

#if wxUSE_VARIANT

#include "wx/string.h"
#include "wx/arrstr.h"
#include "wx/cpp.h"
#include <typeinfo>

#if wxUSE_DATETIME
    #include "wx/datetime.h"
#endif // wxUSE_DATETIME

#include "wx/iosfwrap.h"

class wxTypeInfo;
class wxObject;
class wxClassInfo;

/*
 * wxVariantData stores the actual data in a wxVariant object,
 * to allow it to store any type of data.
 * Derive from this to provide custom data handling.
 *
 * NB: To prevent addition of extra vtbl pointer to wxVariantData,
 *     we don't multiple-inherit from wxObjectRefData. Instead,
 *     we simply replicate the wxObject ref-counting scheme.
 *
 * NB: When you construct a wxVariantData, it will have refcount
 *     of one. Refcount will not be further increased when
 *     it is passed to wxVariant. This simulates old common
 *     scenario where wxVariant took ownership of wxVariantData
 *     passed to it.
 *     If you create wxVariantData for other reasons than passing
 *     it to wxVariant, technically you are not required to call
 *     DecRef() before deleting it.
 *
 * TODO: in order to replace wxPropertyValue, we would need
 * to consider adding constructors that take pointers to C++ variables,
 * or removing that functionality from the wxProperty library.
 * Essentially wxPropertyValue takes on some of the wxValidator functionality
 * by storing pointers and not just actual values, allowing update of C++ data
 * to be handled automatically. Perhaps there's another way of doing this without
 * overloading wxVariant with unnecessary functionality.
 */

class WXDLLIMPEXP_BASE wxVariantData
{
    friend class wxVariantBase;

public:
    wxVariantData()
        : m_count(1)
    { }

#if wxUSE_STD_IOSTREAM
    virtual bool Write(wxSTD ostream& WXUNUSED(str)) const { return false; }
    virtual bool Read(wxSTD istream& WXUNUSED(str)) { return false; }
#endif
    virtual bool Write(wxString& WXUNUSED(str)) const { return false; }
    virtual bool Read(wxString& WXUNUSED(str)) { return false; }

    // Override these to provide common functionality
    virtual bool Eq(wxVariantData& data) const = 0;

    // What type is it? Return a string name.
    virtual wxString GetType() const = 0;

    // returns the type info of the content
    virtual const wxTypeInfo* GetTypeInfo() const = 0;

    // If it based on wxObject return the ClassInfo.
    virtual wxClassInfo* GetValueClassInfo() { return NULL; }

    int GetRefCount() const 
        { return m_count; }
    void IncRef() 
        { m_count++; }
    void DecRef()
    {
        if ( --m_count == 0 )
            delete this;
    }

protected:
    // Protected dtor should make some incompatible code
    // break more louder. That is, they should do data->DecRef()
    // instead of delete data.
    virtual ~wxVariantData() {}

private:
    int m_count;
};

template<typename T> class wxVariantDataT : public wxVariantData
{
public:
    wxVariantDataT(const T& d) : m_data(d) {}
    virtual ~wxVariantDataT() {}

    // get a ref to the stored data
    T & Get() { return m_data; }

    // get a const ref to the stored data
    const T & Get() const { return m_data; }

    // set the data
    void Set(const T& d) { m_data =  d; }

    // Override these to provide common functionality
    virtual bool Eq(wxVariantData& WXUNUSED(data)) const
        { return false; /* FIXME!! */    }

    // What type is it? Return a string name.
    virtual wxString GetType() const
        { return GetTypeInfo()->GetTypeName(); }

    // return a heap allocated duplicate
    //virtual wxVariantData* Clone() const { return new wxVariantDataT<T>( Get() ); }

    // returns the type info of the contentc
    virtual const wxTypeInfo* GetTypeInfo() const { return wxGetTypeInfo( (T*) NULL ); }

private:
    T m_data;
};


/*
 * wxVariantBase can store any kind of data, but has some basic types
 * built in.
 */

class WXDLLIMPEXP_BASE wxVariantBase
{
public:
    wxVariantBase();
    wxVariantBase(const wxVariantBase& variant);
    wxVariantBase(wxVariantData* data, const wxString& name = wxEmptyString);

    template<typename T> 
        wxVariantBase(const T& data, const wxString& name = wxEmptyString) :
            m_data(new wxVariantDataT<T>(data)), m_name(name) {}

    virtual ~wxVariantBase();

    // generic assignment
    void operator= (const wxVariantBase& variant);

    // Assignment using data, e.g.
    // myVariant = new wxStringVariantData("hello");
    void operator= (wxVariantData* variantData);

    bool operator== (const wxVariantBase& variant) const;
    bool operator!= (const wxVariantBase& variant) const;

    // Sets/gets name
    inline void SetName(const wxString& name) { m_name = name; }
    inline const wxString& GetName() const { return m_name; }

    // Tests whether there is data
    bool IsNull() const;

    // FIXME: used by wxVariantBase code but is nice wording...
    bool IsEmpty() const { return IsNull(); }

    // For compatibility with wxWidgets <= 2.6, this doesn't increase
    // reference count.
    wxVariantData* GetData() const { return m_data; }
    void SetData(wxVariantData* data) ;

    // make a 'clone' of the object
    void Ref(const wxVariantBase& clone);

    // destroy a reference
    void UnRef();

    // Make NULL (i.e. delete the data)
    void MakeNull();

    // write contents to a string (e.g. for debugging)
    wxString MakeString() const;

    // Delete data and name
    void Clear();

    // Returns a string representing the type of the variant,
    // e.g. "string", "bool", "stringlist", "list", "double", "long"
    wxString GetType() const;

    bool IsType(const wxString& type) const;
    bool IsValueKindOf(const wxClassInfo* type) const;

    // FIXME wxXTI methods:

    // get the typeinfo of the stored object
    const wxTypeInfo* GetTypeInfo() const 
    { 
        if (!m_data)
            return NULL;
        return m_data->GetTypeInfo(); 
    }

    // get a ref to the stored data
    template<typename T> T& Get()
    {
        wxVariantDataT<T> *dataptr = 
            wx_dynamic_cast(wxVariantDataT<T>*, m_data);
        wxASSERT_MSG( dataptr, 
            wxString::Format(wxT("Cast to %s not possible"), typeid(T).name()) );
        return dataptr->Get();
    }

    // get a const ref to the stored data
    template<typename T> const T& Get() const
    {
        const wxVariantDataT<T> *dataptr = 
            wx_dynamic_cast(const wxVariantDataT<T>*, m_data);
        wxASSERT_MSG( dataptr, 
            wxString::Format(wxT("Cast to %s not possible"), typeid(T).name()) );
        return dataptr->Get();
    }

    template<typename T> bool HasData() const
    {
        const wxVariantDataT<T> *dataptr = 
            wx_dynamic_cast(const wxVariantDataT<T>*, m_data);
        return dataptr != NULL;
    }

    // returns this value as string
    wxString GetAsString() const;

    // gets the stored data casted to a wxObject*, 
    // returning NULL if cast is not possible
    wxObject* GetAsObject();

protected:
    wxVariantData*  m_data;
    wxString        m_name;
};

#include "wx/dynarray.h"
WX_DECLARE_OBJARRAY_WITH_DECL(wxVariantBase, wxVariantBaseArray, class WXDLLIMPEXP_BASE);


// templated streaming, every type must have their specialization for these methods

template<typename T>
void wxStringReadValue( const wxString &s, T &data );

template<typename T>
void wxStringWriteValue( wxString &s, const T &data);

template<typename T>
void wxToStringConverter( const wxVariantBase &v, wxString &s ) \
    { wxStringWriteValue( s, v.Get<T>() ); }

template<typename T>
void wxFromStringConverter( const wxString &s, wxVariantBase &v ) \
    { T d; wxStringReadValue( s, d ); v = wxVariantBase(d); }


#endif // wxUSE_VARIANT
#endif // _WX_VARIANTBASE_H_
