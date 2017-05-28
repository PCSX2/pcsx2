/////////////////////////////////////////////////////////////////////////////
// Name:        wx/xtitypes.h
// Purpose:     enum, set, basic types support
// Author:      Stefan Csomor
// Modified by: Francesco Montorsi
// Created:     27/07/03
// Copyright:   (c) 1997 Julian Smart
//              (c) 2003 Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _XTITYPES_H_
#define _XTITYPES_H_

#include "wx/defs.h"

#if wxUSE_EXTENDED_RTTI

#include "wx/string.h"
#include "wx/hashmap.h"
#include "wx/arrstr.h"
#include "wx/flags.h"
#include "wx/intl.h"
#include "wx/log.h"
#include <typeinfo>

class WXDLLIMPEXP_BASE wxClassInfo;

// ----------------------------------------------------------------------------
// Enum Support
//
// In the header files XTI requires no change from pure c++ code, however in the
// implementation, an enum needs to be enumerated eg:
//
// wxBEGIN_ENUM( wxFlavor )
//   wxENUM_MEMBER( Vanilla )
//   wxENUM_MEMBER( Chocolate )
//   wxENUM_MEMBER( Strawberry )
// wxEND_ENUM( wxFlavor )
// ----------------------------------------------------------------------------

struct WXDLLIMPEXP_BASE wxEnumMemberData
{
    const wxChar*   m_name;
    int             m_value;
};

class WXDLLIMPEXP_BASE wxEnumData
{
public:
    wxEnumData( wxEnumMemberData* data );

    // returns true if the member has been found and sets the int value
    // pointed to accordingly (if ptr != null )
    // if not found returns false, value left unchanged
    bool HasEnumMemberValue( const wxChar *name, int *value = NULL ) const;

    // returns the value of the member, if not found in debug mode an
    // assert is issued, in release 0 is returned
    int GetEnumMemberValue(const wxChar *name ) const;

    // returns the name of the enum member having the passed in value
    // returns an emtpy string if not found
    const wxChar *GetEnumMemberName(int value) const;

    // returns the number of members in this enum
    int GetEnumCount() const { return m_count; }

    // returns the value of the nth member
    int GetEnumMemberValueByIndex( int n ) const;

    // returns the value of the nth member
    const wxChar *GetEnumMemberNameByIndex( int n ) const;

private:
    wxEnumMemberData *m_members;
    int m_count;
};

#define wxBEGIN_ENUM( e ) \
    wxEnumMemberData s_enumDataMembers##e[] = {

#define wxENUM_MEMBER( v ) { wxT(#v), v },

#define wxEND_ENUM( e )                                                 \
        { NULL, 0 } };                                                 \
    wxEnumData s_enumData##e( s_enumDataMembers##e );                   \
    wxEnumData *wxGetEnumData(e) { return &s_enumData##e; }             \
    template<> void wxStringReadValue(const wxString& s, e &data )     \
        { data = (e) s_enumData##e.GetEnumMemberValue(s.c_str()); }     \
    template<> void wxStringWriteValue(wxString &s, const e &data )    \
        { s = s_enumData##e.GetEnumMemberName((int)data); }             \
    void FromLong##e( long data, wxAny& result )                  \
        { result = wxAny((e)data); }                               \
    void ToLong##e( const wxAny& data, long &result )             \
        { result = (long) (data).As(static_cast<e*>(NULL)); }      \
                                                                        \
    wxTO_STRING_IMP( e )                                                \
    wxFROM_STRING_IMP( e )                                              \
    wxEnumTypeInfo s_typeInfo##e(wxT_ENUM, &s_enumData##e,            \
            &wxTO_STRING( e ), &wxFROM_STRING( e ), &ToLong##e,      \
            &FromLong##e, typeid(e).name() );


// ----------------------------------------------------------------------------
// Set Support
//
// in the header :
//
// enum wxFlavor
// {
//  Vanilla,
//  Chocolate,
//  Strawberry,
// };
//
// typedef wxBitset<wxFlavor> wxCoupe;
//
// in the implementation file :
//
// wxBEGIN_ENUM( wxFlavor )
//  wxENUM_MEMBER( Vanilla )
//  wxENUM_MEMBER( Chocolate )
//  wxENUM_MEMBER( Strawberry )
// wxEND_ENUM( wxFlavor )
//
// wxIMPLEMENT_SET_STREAMING( wxCoupe, wxFlavor )
//
// implementation note: no partial specialization for streaming, but a delegation 
//                      to a different class
//
// ----------------------------------------------------------------------------

void WXDLLIMPEXP_BASE wxSetStringToArray( const wxString &s, wxArrayString &array );

template<typename e>
void wxSetFromString(const wxString &s, wxBitset<e> &data )
{
    wxEnumData* edata = wxGetEnumData((e) 0);
    data.reset();

    wxArrayString array;
    wxSetStringToArray( s, array );
    wxString flag;
    for ( int i = 0; i < array.Count(); ++i )
    {
        flag = array[i];
        int ivalue;
        if ( edata->HasEnumMemberValue( flag.c_str(), &ivalue ) )
        {
            data.set( (e) ivalue );
        }
    }
}

template<typename e>
void wxSetToString( wxString &s, const wxBitset<e> &data )
{
    wxEnumData* edata = wxGetEnumData((e) 0);
    int count = edata->GetEnumCount();
    int i;
    s.Clear();
    for ( i = 0; i < count; i++ )
    {
        e value = (e) edata->GetEnumMemberValueByIndex(i);
        if ( data.test( value ) )
        {
            // this could also be done by the templated calls
            if ( !s.empty() )
                s += wxT("|");
            s += edata->GetEnumMemberNameByIndex(i);
        }
    }
}

#define wxIMPLEMENT_SET_STREAMING(SetName,e)                                    \
    template<> void wxStringReadValue(const wxString &s, wxBitset<e> &data )   \
        { wxSetFromString( s, data ); }                                        \
    template<> void wxStringWriteValue( wxString &s, const wxBitset<e> &data ) \
        { wxSetToString( s, data ); }                                          \
    void FromLong##SetName( long data, wxAny& result )                    \
        { result = wxAny(SetName((unsigned long)data)); }                  \
    void ToLong##SetName( const wxAny& data, long &result )               \
        { result = (long) (data).As(static_cast<SetName*>(NULL)).to_ulong(); } \
    wxTO_STRING_IMP( SetName )                                                  \
    wxFROM_STRING_IMP( SetName )                                                \
    wxEnumTypeInfo s_typeInfo##SetName(wxT_SET, &s_enumData##e,               \
            &wxTO_STRING( SetName ), &wxFROM_STRING( SetName ),               \
            &ToLong##SetName, &FromLong##SetName, typeid(SetName).name() );

template<typename e>
void wxFlagsFromString(const wxString &s, e &data )
{
    wxEnumData* edata = wxGetEnumData((e*) 0);
    data.m_data = 0;

    wxArrayString array;
    wxSetStringToArray( s, array );
    wxString flag;
    for ( size_t i = 0; i < array.Count(); ++i )
    {
        flag = array[i];
        int ivalue;
        if ( edata->HasEnumMemberValue( flag.c_str(), &ivalue ) )
        {
            data.m_data |= ivalue;
        }
    }
}

template<typename e>
void wxFlagsToString( wxString &s, const e& data )
{
    wxEnumData* edata = wxGetEnumData((e*) 0);
    int count = edata->GetEnumCount();
    int i;
    s.Clear();
    long dataValue = data.m_data;
    for ( i = 0; i < count; i++ )
    {
        int value = edata->GetEnumMemberValueByIndex(i);
        // make this to allow for multi-bit constants to work
        if ( value && ( dataValue & value ) == value )
        {
            // clear the flags we just set
            dataValue &= ~value;
            // this could also be done by the templated calls
            if ( !s.empty() )
                s +=wxT("|");
            s += edata->GetEnumMemberNameByIndex(i);
        }
    }
}

#define wxBEGIN_FLAGS( e ) \
    wxEnumMemberData s_enumDataMembers##e[] = {

#define wxFLAGS_MEMBER( v ) { wxT(#v), static_cast<int>(v) },

#define wxEND_FLAGS( e )                                                \
        { NULL, 0 } };                                                 \
    wxEnumData s_enumData##e( s_enumDataMembers##e );                   \
    wxEnumData *wxGetEnumData(e*) { return &s_enumData##e; }            \
    template<>  void wxStringReadValue(const wxString &s, e &data )    \
        { wxFlagsFromString<e>( s, data ); }                           \
    template<>  void wxStringWriteValue( wxString &s, const e& data )  \
        { wxFlagsToString<e>( s, data ); }                             \
    void FromLong##e( long data, wxAny& result )                  \
        { result = wxAny(e(data)); }                               \
    void ToLong##e( const wxAny& data, long &result )             \
        { result = (long) (data).As(static_cast<e*>(NULL)).m_data; } \
    wxTO_STRING_IMP( e )                                                \
    wxFROM_STRING_IMP( e )                                              \
    wxEnumTypeInfo s_typeInfo##e(wxT_SET, &s_enumData##e,             \
            &wxTO_STRING( e ), &wxFROM_STRING( e ), &ToLong##e,      \
            &FromLong##e, typeid(e).name() );

// ----------------------------------------------------------------------------
// Type Information
// ----------------------------------------------------------------------------

//  All data exposed by the RTTI is characterized using the following classes.
//  The first characterization is done by wxTypeKind. All enums up to and including
//  wxT_CUSTOM represent so called simple types. These cannot be divided any further.
//  They can be converted to and from wxStrings, that's all.
//  Other wxTypeKinds can instead be splitted recursively into smaller parts until
//  the simple types are reached.

enum wxTypeKind
{
    wxT_VOID = 0,   // unknown type
    wxT_BOOL,
    wxT_CHAR,
    wxT_UCHAR,
    wxT_INT,
    wxT_UINT,
    wxT_LONG,
    wxT_ULONG,
    wxT_LONGLONG,
    wxT_ULONGLONG,
    wxT_FLOAT,
    wxT_DOUBLE,
    wxT_STRING,     // must be wxString
    wxT_SET,        // must be wxBitset<> template
    wxT_ENUM,
    wxT_CUSTOM,     // user defined type (e.g. wxPoint)

    wxT_LAST_SIMPLE_TYPE_KIND = wxT_CUSTOM,

    wxT_OBJECT_PTR, // object reference
    wxT_OBJECT,     // embedded object
    wxT_COLLECTION, // collection

    wxT_DELEGATE,   // for connecting against an event source

    wxT_LAST_TYPE_KIND = wxT_DELEGATE // sentinel for bad data, asserts, debugging
};

class WXDLLIMPEXP_BASE wxAny;
class WXDLLIMPEXP_BASE wxTypeInfo;

WX_DECLARE_STRING_HASH_MAP_WITH_DECL( wxTypeInfo*, wxTypeInfoMap, class WXDLLIMPEXP_BASE );

class WXDLLIMPEXP_BASE wxTypeInfo
{
public:
    typedef void (*wxVariant2StringFnc)( const wxAny& data, wxString &result );
    typedef void (*wxString2VariantFnc)( const wxString& data, wxAny &result );

    wxTypeInfo(wxTypeKind kind,
               wxVariant2StringFnc to = NULL, wxString2VariantFnc from = NULL,
               const wxString &name = wxEmptyString):
            m_toString(to), m_fromString(from), m_kind(kind), m_name(name)
    {
        Register();
    }
#if 0 // wxUSE_UNICODE
    wxTypeInfo(wxTypeKind kind,
               wxVariant2StringFnc to, wxString2VariantFnc from,
               const char *name):
            m_toString(to), m_fromString(from), m_kind(kind), 
            m_name(wxString::FromAscii(name))
    {
        Register();
    }
#endif

    virtual ~wxTypeInfo()
    {
        Unregister();
    }

    // return the kind of this type (wxT_... constants)
    wxTypeKind GetKind() const { return m_kind; }

    // returns the unique name of this type
    const wxString& GetTypeName() const { return m_name; }

    // is this type a delegate type
    bool IsDelegateType() const { return m_kind == wxT_DELEGATE; }

    // is this type a custom type
    bool IsCustomType() const { return m_kind == wxT_CUSTOM; }

    // is this type an object type
    bool IsObjectType() const { return m_kind == wxT_OBJECT || m_kind == wxT_OBJECT_PTR; }

    // can the content of this type be converted to and from strings ?
    bool HasStringConverters() const { return m_toString != NULL && m_fromString != NULL; }

    // convert a wxAny holding data of this type into a string
    void ConvertToString( const wxAny& data, wxString &result ) const
    { 
        if ( m_toString ) 
            (*m_toString)( data, result ); 
        else 
            wxLogError( wxGetTranslation(wxT("String conversions not supported")) ); 
    }

    // convert a string into a wxAny holding the corresponding data in this type
    void ConvertFromString( const wxString& data, wxAny &result ) const
    { 
        if( m_fromString ) 
            (*m_fromString)( data, result ); 
        else 
            wxLogError( wxGetTranslation(wxT("String conversions not supported")) ); 
    }

    // statics:

    // looks for the corresponding type, will return NULL if not found
    static wxTypeInfo *FindType( const wxString& typeName );
private:
    void Register();
    void Unregister();

    wxVariant2StringFnc m_toString;
    wxString2VariantFnc m_fromString;

    wxTypeKind m_kind;
    wxString m_name;

    // the static list of all types we know about
    static wxTypeInfoMap* ms_typeTable;
};

class WXDLLIMPEXP_BASE wxBuiltInTypeInfo : public wxTypeInfo
{
public:
    wxBuiltInTypeInfo( wxTypeKind kind, wxVariant2StringFnc to = NULL, 
                       wxString2VariantFnc from = NULL, 
                       const wxString &name = wxEmptyString ) :
            wxTypeInfo( kind, to, from, name )
       { wxASSERT_MSG( GetKind() < wxT_SET, wxT("Illegal Kind for Base Type") ); }
};

class WXDLLIMPEXP_BASE wxCustomTypeInfo : public wxTypeInfo
{
public:
    wxCustomTypeInfo( const wxString &name, wxVariant2StringFnc to, 
                      wxString2VariantFnc from ) :
            wxTypeInfo( wxT_CUSTOM, to, from, name )
       {}
};

class WXDLLIMPEXP_BASE wxEnumTypeInfo : public wxTypeInfo
{
public:
    typedef void (*converterToLong_t)( const wxAny& data, long &result );
    typedef void (*converterFromLong_t)( long data, wxAny &result );

    wxEnumTypeInfo( wxTypeKind kind, wxEnumData* enumInfo, wxVariant2StringFnc to,
                    wxString2VariantFnc from, converterToLong_t toLong,
                    converterFromLong_t fromLong, const wxString &name  ) :
        wxTypeInfo( kind, to, from, name ), m_toLong( toLong ), m_fromLong( fromLong )
    { 
        wxASSERT_MSG( kind == wxT_ENUM || kind == wxT_SET,
                      wxT("Illegal Kind for Enum Type")); 
        m_enumInfo = enumInfo; 
    }

    const wxEnumData* GetEnumData() const { return m_enumInfo; }

    // convert a wxAny holding data of this type into a long
    void ConvertToLong( const wxAny& data, long &result ) const
    { 
        if( m_toLong ) 
            (*m_toLong)( data, result ); 
        else 
            wxLogError( wxGetTranslation(wxT("Long Conversions not supported")) ); 
    }

    // convert a long into a wxAny holding the corresponding data in this type
    void ConvertFromLong( long data, wxAny &result ) const
    { 
        if( m_fromLong ) 
            (*m_fromLong)( data, result ); 
        else 
            wxLogError( wxGetTranslation(wxT("Long Conversions not supported")) ); 
    }

private:
    converterToLong_t m_toLong;
    converterFromLong_t m_fromLong;

    wxEnumData *m_enumInfo; // Kind == wxT_ENUM or Kind == wxT_SET
};

class WXDLLIMPEXP_BASE wxClassTypeInfo : public wxTypeInfo
{
public:
    wxClassTypeInfo( wxTypeKind kind, wxClassInfo* classInfo, 
                     wxVariant2StringFnc to = NULL, wxString2VariantFnc from = NULL, 
                     const wxString &name = wxEmptyString);

    const wxClassInfo *GetClassInfo() const { return m_classInfo; }

private:
    wxClassInfo *m_classInfo; // Kind == wxT_OBJECT - could be NULL
};

class WXDLLIMPEXP_BASE wxCollectionTypeInfo : public wxTypeInfo
{
public:
    wxCollectionTypeInfo( const wxString &elementName, wxVariant2StringFnc to,
                          wxString2VariantFnc from , const wxString &name) :
            wxTypeInfo( wxT_COLLECTION, to, from, name )
       { m_elementTypeName = elementName; m_elementType = NULL; }

    const wxTypeInfo* GetElementType() const
    {
        if ( m_elementType == NULL )
            m_elementType = wxTypeInfo::FindType( m_elementTypeName );
        return m_elementType; 
    }

private:
    mutable wxTypeInfo * m_elementType;
    wxString    m_elementTypeName;
};

class WXDLLIMPEXP_BASE wxEventSourceTypeInfo : public wxTypeInfo
{
public:
    wxEventSourceTypeInfo( int eventType, wxClassInfo* eventClass, 
                        wxVariant2StringFnc to = NULL, 
                        wxString2VariantFnc from = NULL );
    wxEventSourceTypeInfo( int eventType, int lastEventType, wxClassInfo* eventClass, 
                        wxVariant2StringFnc to = NULL, wxString2VariantFnc from = NULL );

    int GetEventType() const { return m_eventType; }
    int GetLastEventType() const { return m_lastEventType; }
    const wxClassInfo* GetEventClass() const { return m_eventClass; }

private:
    const wxClassInfo *m_eventClass; // (extended will merge into classinfo)
    int m_eventType;
    int m_lastEventType;
};

template<typename T> const wxTypeInfo* wxGetTypeInfo( T * ) 
    { return wxTypeInfo::FindType(typeid(T).name()); }

// this macro is for usage with custom, non-object derived classes and structs, 
// wxPoint is such a custom type

#if wxUSE_FUNC_TEMPLATE_POINTER
    #define wxCUSTOM_TYPE_INFO( e, toString, fromString ) \
        wxCustomTypeInfo s_typeInfo##e(typeid(e).name(), &toString, &fromString);
#else
    #define wxCUSTOM_TYPE_INFO( e, toString, fromString )             \
        void ToString##e( const wxAny& data, wxString &result )   \
            { toString(data, result); }                                 \
        void FromString##e( const wxString& data, wxAny &result ) \
            { fromString(data, result); }                               \
        wxCustomTypeInfo s_typeInfo##e(typeid(e).name(),               \
                                       &ToString##e, &FromString##e);
#endif

#define wxCOLLECTION_TYPE_INFO( element, collection )                      \
    wxCollectionTypeInfo s_typeInfo##collection( typeid(element).name(),   \
                                NULL, NULL, typeid(collection).name() );

// sometimes a compiler invents specializations that are nowhere called, 
// use this macro to satisfy the refs, currently we don't have to play 
// tricks, but if we will have to according to the compiler, we will use 
// that macro for that

#define wxILLEGAL_TYPE_SPECIALIZATION( a )

#endif      // wxUSE_EXTENDED_RTTI
#endif      // _XTITYPES_H_
