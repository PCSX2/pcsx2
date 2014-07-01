/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/xti.cpp
// Purpose:     runtime metadata information (extended class info)
// Author:      Stefan Csomor
// Modified by:
// Created:     27/07/03
// Copyright:   (c) 1997 Julian Smart
//              (c) 2003 Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_EXTENDED_RTTI

#ifndef WX_PRECOMP
    #include "wx/object.h"
    #include "wx/list.h"
    #include "wx/hash.h"
#endif

#include "wx/xti.h"
#include "wx/xml/xml.h"
#include "wx/tokenzr.h"
#include "wx/range.h"

#include <string.h>

#include "wx/beforestd.h"
#include <map>
#include <string>
#include <list>
#include "wx/afterstd.h"

using namespace std;

// ----------------------------------------------------------------------------
// wxEnumData
// ----------------------------------------------------------------------------

wxEnumData::wxEnumData( wxEnumMemberData* data )
{
    m_members = data;
    for ( m_count = 0; m_members[m_count].m_name; m_count++)
    {};
}

bool wxEnumData::HasEnumMemberValue(const wxChar *name, int *value) const
{
    int i;
    for (i = 0; m_members[i].m_name; i++ )
    {
        if (!wxStrcmp(name, m_members[i].m_name))
        {
            if ( value )
                *value = m_members[i].m_value;
            return true;
        }
    }
    return false;
}

int wxEnumData::GetEnumMemberValue(const wxChar *name) const
{
    int i;
    for (i = 0; m_members[i].m_name; i++ )
    {
        if (!wxStrcmp(name, m_members[i].m_name))
        {
            return m_members[i].m_value;
        }
    }
    return 0;
}

const wxChar *wxEnumData::GetEnumMemberName(int value) const
{
    int i;
    for (i = 0; m_members[i].m_name; i++)
        if (value == m_members[i].m_value)
            return m_members[i].m_name;

    return wxEmptyString;
}

int wxEnumData::GetEnumMemberValueByIndex( int idx ) const
{
    // we should cache the count in order to avoid out-of-bounds errors
    return m_members[idx].m_value;
}

const wxChar * wxEnumData::GetEnumMemberNameByIndex( int idx ) const
{
    // we should cache the count in order to avoid out-of-bounds errors
    return m_members[idx].m_name;
}

// ----------------------------------------------------------------------------
// Type Information
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// value streaming
// ----------------------------------------------------------------------------

// streamer specializations
// for all built-in types

// bool

template<> void wxStringReadValue(const wxString &s, bool &data )
{
    int intdata;
    wxSscanf(s, wxT("%d"), &intdata );
    data = (bool)(intdata != 0);
}

template<> void wxStringWriteValue(wxString &s, const bool &data )
{
    s = wxString::Format(wxT("%d"), data );
}

// char

template<> void wxStringReadValue(const wxString &s, char &data )
{
    int intdata;
    wxSscanf(s, wxT("%d"), &intdata );
    data = char(intdata);
}

template<> void wxStringWriteValue(wxString &s, const char &data )
{
    s = wxString::Format(wxT("%d"), data );
}

// unsigned char

template<> void wxStringReadValue(const wxString &s, unsigned char &data )
{
    int intdata;
    wxSscanf(s, wxT("%d"), &intdata );
    data = (unsigned char)(intdata);
}

template<> void wxStringWriteValue(wxString &s, const unsigned char &data )
{
    s = wxString::Format(wxT("%d"), data );
}

// int

template<> void wxStringReadValue(const wxString &s, int &data )
{
    wxSscanf(s, wxT("%d"), &data );
}

template<> void wxStringWriteValue(wxString &s, const int &data )
{
    s = wxString::Format(wxT("%d"), data );
}

// unsigned int

template<> void wxStringReadValue(const wxString &s, unsigned int &data )
{
    wxSscanf(s, wxT("%d"), &data );
}

template<> void wxStringWriteValue(wxString &s, const unsigned int &data )
{
    s = wxString::Format(wxT("%d"), data );
}

// long

template<> void wxStringReadValue(const wxString &s, long &data )
{
    wxSscanf(s, wxT("%ld"), &data );
}

template<> void wxStringWriteValue(wxString &s, const long &data )
{
    s = wxString::Format(wxT("%ld"), data );
}

// unsigned long

template<> void wxStringReadValue(const wxString &s, unsigned long &data )
{
    wxSscanf(s, wxT("%ld"), &data );
}

template<> void wxStringWriteValue(wxString &s, const unsigned long &data )
{
    s = wxString::Format(wxT("%ld"), data );
}

#ifdef wxLongLong_t
template<> void wxStringReadValue(const wxString &s, wxLongLong_t &data )
{
    wxSscanf(s, wxT("%lld"), &data );
}

template<> void wxStringWriteValue(wxString &s, const wxLongLong_t &data )
{
    s = wxString::Format(wxT("%lld"), data );
}

template<> void wxStringReadValue(const wxString &s, wxULongLong_t &data )
{
    wxSscanf(s, wxT("%lld"), &data );
}

template<> void wxStringWriteValue(wxString &s, const wxULongLong_t &data )
{
    s = wxString::Format(wxT("%lld"), data );
}
#endif
// float

template<> void wxStringReadValue(const wxString &s, float &data )
{
    wxSscanf(s, wxT("%f"), &data );
}

template<> void wxStringWriteValue(wxString &s, const float &data )
{
    s = wxString::Format(wxT("%f"), data );
}

// double

template<> void wxStringReadValue(const wxString &s, double &data )
{
    wxSscanf(s, wxT("%lf"), &data );
}

template<> void wxStringWriteValue(wxString &s, const double &data )
{
    s = wxString::Format(wxT("%lf"), data );
}

// wxString

template<> void wxStringReadValue(const wxString &s, wxString &data )
{
    data = s;
}

template<> void wxStringWriteValue(wxString &s, const wxString &data )
{
    s = data;
}


// built-ins
//

#if wxUSE_FUNC_TEMPLATE_POINTER
    #define wxBUILTIN_TYPE_INFO( element, type )                                    \
        wxBuiltInTypeInfo                                                           \
            s_typeInfo##type(element, &wxToStringConverter<type>,                   \
                             &wxFromStringConverter<type>, typeid(type).name());
#else
    #define wxBUILTIN_TYPE_INFO( element, type )                                    \
        void _toString##element( const wxAny& data, wxString &result )         \
            { wxToStringConverter<type, data, result); }                            \
        void _fromString##element( const wxString& data, wxAny &result )       \
            { wxFromStringConverter<type, data, result); }                          \
        wxBuiltInTypeInfo s_typeInfo##type(element, &_toString##element,            \
                                           &_fromString##element, typeid(type).name());
#endif

typedef unsigned char unsigned_char;
typedef unsigned int unsigned_int;
typedef unsigned long unsigned_long;

wxBuiltInTypeInfo s_typeInfovoid( wxT_VOID, NULL, NULL, typeid(void).name());
wxBUILTIN_TYPE_INFO( wxT_BOOL,  bool);
wxBUILTIN_TYPE_INFO( wxT_CHAR,  char);
wxBUILTIN_TYPE_INFO( wxT_UCHAR, unsigned_char);
wxBUILTIN_TYPE_INFO( wxT_INT, int);
wxBUILTIN_TYPE_INFO( wxT_UINT, unsigned_int);
wxBUILTIN_TYPE_INFO( wxT_LONG, long);
wxBUILTIN_TYPE_INFO( wxT_ULONG, unsigned_long);
wxBUILTIN_TYPE_INFO( wxT_FLOAT, float);
wxBUILTIN_TYPE_INFO( wxT_DOUBLE, double);
wxBUILTIN_TYPE_INFO( wxT_STRING, wxString);

#ifdef wxLongLong_t
wxBUILTIN_TYPE_INFO(wxT_LONGLONG, wxLongLong_t)
wxBUILTIN_TYPE_INFO(wxT_ULONGLONG, wxULongLong_t)
#endif

// this are compiler induced specialization which are never used anywhere

wxILLEGAL_TYPE_SPECIALIZATION( char const * )
wxILLEGAL_TYPE_SPECIALIZATION( char * )
wxILLEGAL_TYPE_SPECIALIZATION( unsigned char * )
wxILLEGAL_TYPE_SPECIALIZATION( int * )
wxILLEGAL_TYPE_SPECIALIZATION( bool * )
wxILLEGAL_TYPE_SPECIALIZATION( long * )
wxILLEGAL_TYPE_SPECIALIZATION( wxString * )

// wxRange

template<> void wxStringReadValue(const wxString &s , wxRange &data)
{
    int minValue, maxValue;
    wxSscanf(s, wxT("%d,%d"), &minValue , &maxValue);
    data = wxRange(minValue, maxValue);
}

template<> void wxStringWriteValue(wxString &s , const wxRange &data)
{
    s = wxString::Format(wxT("%d,%d"), data.GetMin() , data.GetMax());
}

wxCUSTOM_TYPE_INFO(wxRange, wxToStringConverter<wxRange> , wxFromStringConverter<wxRange>)

// other types

wxCOLLECTION_TYPE_INFO( wxString, wxArrayString );

template<> void wxCollectionToVariantArray( wxArrayString const &theArray, 
                                            wxAnyList &value)
{
    wxArrayCollectionToVariantArray( theArray, value );
}

wxTypeInfoMap *wxTypeInfo::ms_typeTable = NULL;

wxTypeInfo *wxTypeInfo::FindType(const wxString& typeName)
{
    wxTypeInfoMap::iterator iter = ms_typeTable->find(typeName);

    if (iter == ms_typeTable->end())
        return NULL;

    return (wxTypeInfo *)iter->second;
}

wxClassTypeInfo::wxClassTypeInfo( wxTypeKind kind, wxClassInfo* classInfo, 
                                  wxVariant2StringFnc to, 
                                  wxString2VariantFnc from, 
                                  const wxString &name) :
    wxTypeInfo( kind, to, from, name)
{ 
    wxASSERT_MSG( kind == wxT_OBJECT_PTR || kind == wxT_OBJECT, 
                  wxT("Illegal Kind for Enum Type")); m_classInfo = classInfo;
}

wxEventSourceTypeInfo::wxEventSourceTypeInfo( int eventType, wxClassInfo* eventClass, 
                                        wxVariant2StringFnc to, 
                                        wxString2VariantFnc from ) :
    wxTypeInfo ( wxT_DELEGATE, to, from, wxEmptyString )
{ 
    m_eventClass = eventClass; 
    m_eventType = eventType; 
    m_lastEventType = -1;
}

wxEventSourceTypeInfo::wxEventSourceTypeInfo( int eventType, int lastEventType, 
                                        wxClassInfo* eventClass, 
                                        wxVariant2StringFnc to,
                                        wxString2VariantFnc from ) :
    wxTypeInfo ( wxT_DELEGATE, to, from, wxEmptyString )
{ 
    m_eventClass = eventClass; 
    m_eventType = eventType; 
    m_lastEventType = lastEventType; 
}

void wxTypeInfo::Register()
{
    if ( ms_typeTable == NULL )
        ms_typeTable = new wxTypeInfoMap();

    if( !m_name.empty() )
        (*ms_typeTable)[m_name] = this;
}

void wxTypeInfo::Unregister()
{
    if( !m_name.empty() )
        ms_typeTable->erase(m_name);
}

// removing header dependency on string tokenizer

void wxSetStringToArray( const wxString &s, wxArrayString &array )
{
    wxStringTokenizer tokenizer(s, wxT("| \t\n"), wxTOKEN_STRTOK);
    wxString flag;
    array.Clear();
    while (tokenizer.HasMoreTokens())
    {
        array.Add(tokenizer.GetNextToken());
    }
}

// ----------------------------------------------------------------------------
// wxPropertyInfo
// ----------------------------------------------------------------------------

void wxPropertyInfo::Insert(wxPropertyInfo* &iter)
{
    m_next = NULL;
    if ( iter == NULL )
        iter = this;
    else
    {
        wxPropertyInfo* i = iter;
        while( i->m_next )
            i = i->m_next;

        i->m_next = this;
    }
}

void wxPropertyInfo::Remove()
{
    if ( this == m_itsClass->m_firstProperty )
    {
        m_itsClass->m_firstProperty = m_next;
    }
    else
    {
        wxPropertyInfo *info = m_itsClass->m_firstProperty;
        while (info)
        {
            if ( info->m_next == this )
            {
                info->m_next = m_next;
                break;
            }

            info = info->m_next;
        }
    }

}

// ----------------------------------------------------------------------------
// wxHandlerInfo
// ----------------------------------------------------------------------------

void wxHandlerInfo::Insert(wxHandlerInfo* &iter)
{
    m_next = NULL;
    if ( iter == NULL )
        iter = this;
    else
    {
        wxHandlerInfo* i = iter;
        while( i->m_next )
            i = i->m_next;

        i->m_next = this;
    }
}

void wxHandlerInfo::Remove()
{
    if ( this == m_itsClass->m_firstHandler )
    {
        m_itsClass->m_firstHandler = m_next;
    }
    else
    {
        wxHandlerInfo *info = m_itsClass->m_firstHandler;
        while (info)
        {
            if ( info->m_next == this )
            {
                info->m_next = m_next;
                break;
            }

            info = info->m_next;
        }
    }
}


// ----------------------------------------------------------------------------
// wxClassInfo
// ----------------------------------------------------------------------------

bool wxClassInfo::Create(wxObject *object, int ParamCount, wxAny *Params) const
{
    if ( ParamCount != m_constructorPropertiesCount )
    {
        // FIXME: shouldn't we just return false and let the caller handle it?
        wxLogError( _("Illegal Parameter Count for Create Method") );
        return false;
    }

    return m_constructor->Create( object, Params );
}

wxObject *wxClassInfo::ConstructObject(int ParamCount, wxAny *Params) const
{
    if ( ParamCount != m_constructorPropertiesCount )
    {
        // FIXME: shouldn't we just return NULL and let the caller handle this case?
        wxLogError( _("Illegal Parameter Count for ConstructObject Method") );
        return NULL;
    }

    wxObject *object = NULL;
    if (!m_constructor->Create( object, Params ))
        return NULL;
    return object;
}

bool wxClassInfo::IsKindOf(const wxClassInfo *info) const
{
    if ( info != 0 )
    {
        if ( info == this )
            return true;

        for ( int i = 0; m_parents[i]; ++ i )
        {
            if ( m_parents[i]->IsKindOf( info ) )
                return true;
        }
    }
    return false;
}

const wxPropertyAccessor *wxClassInfo::FindAccessor(const wxChar *PropertyName) const
{
    const wxPropertyInfo* info = FindPropertyInfo( PropertyName );

    if ( info )
        return info->GetAccessor();

    return NULL;
}

wxPropertyInfo *wxClassInfo::FindPropertyInfoInThisClass (const wxChar *PropertyName) const
{
    wxPropertyInfo* info = GetFirstProperty();

    while( info )
    {
        if ( wxStrcmp( info->GetName(), PropertyName ) == 0 )
            return info;
        info = info->GetNext();
    }

    return 0;
}

const wxPropertyInfo *wxClassInfo::FindPropertyInfo (const wxChar *PropertyName) const
{
    const wxPropertyInfo* info = FindPropertyInfoInThisClass( PropertyName );
    if ( info )
        return info;

    const wxClassInfo** parents = GetParents();
    for ( int i = 0; parents[i]; ++ i )
    {
        if ( ( info = parents[i]->FindPropertyInfo( PropertyName ) ) != NULL )
            return info;
    }

    return 0;
}

wxHandlerInfo *wxClassInfo::FindHandlerInfoInThisClass (const wxChar *PropertyName) const
{
    wxHandlerInfo* info = GetFirstHandler();

    while( info )
    {
        if ( wxStrcmp( info->GetName(), PropertyName ) == 0 )
            return info;
        info = info->GetNext();
    }

    return 0;
}

const wxHandlerInfo *wxClassInfo::FindHandlerInfo (const wxChar *PropertyName) const
{
    const wxHandlerInfo* info = FindHandlerInfoInThisClass( PropertyName );

    if ( info )
        return info;

    const wxClassInfo** parents = GetParents();
    for ( int i = 0; parents[i]; ++ i )
    {
        if ( ( info = parents[i]->FindHandlerInfo( PropertyName ) ) != NULL )
            return info;
    }

    return 0;
}

wxObjectStreamingCallback wxClassInfo::GetStreamingCallback() const
{
    if ( m_streamingCallback )
        return m_streamingCallback;

    wxObjectStreamingCallback retval = NULL;
    const wxClassInfo** parents = GetParents();
    for ( int i = 0; parents[i] && retval == NULL; ++ i )
    {
        retval = parents[i]->GetStreamingCallback();
    }
    return retval;
}

bool wxClassInfo::BeforeWriteObject( const wxObject *obj, wxObjectWriter *streamer, 
                                     wxObjectWriterCallback *writercallback, const wxStringToAnyHashMap &metadata) const
{
    wxObjectStreamingCallback sb = GetStreamingCallback();
    if ( sb )
        return (*sb)(obj, streamer, writercallback, metadata );

    return true;
}

void wxClassInfo::SetProperty(wxObject *object, const wxChar *propertyName, 
                              const wxAny &value) const
{
    const wxPropertyAccessor *accessor;

    accessor = FindAccessor(propertyName);
    wxASSERT(accessor->HasSetter());
    accessor->SetProperty( object, value );
}

wxAny wxClassInfo::GetProperty(wxObject *object, const wxChar *propertyName) const
{
    const wxPropertyAccessor *accessor;

    accessor = FindAccessor(propertyName);
    wxASSERT(accessor->HasGetter());
    wxAny result;
    accessor->GetProperty(object,result);
    return result;
}

wxAnyList wxClassInfo::GetPropertyCollection(wxObject *object, 
                                                   const wxChar *propertyName) const
{
    const wxPropertyAccessor *accessor;

    accessor = FindAccessor(propertyName);
    wxASSERT(accessor->HasGetter());
    wxAnyList result;
    accessor->GetPropertyCollection(object,result);
    return result;
}

void wxClassInfo::AddToPropertyCollection(wxObject *object, const wxChar *propertyName, 
                                          const wxAny& value) const
{
    const wxPropertyAccessor *accessor;

    accessor = FindAccessor(propertyName);
    wxASSERT(accessor->HasAdder());
    accessor->AddToPropertyCollection( object, value );
}

// void wxClassInfo::GetProperties( wxPropertyInfoMap &map ) const
// The map parameter (the name map that is) seems something special
// to MSVC and so we use a other name.
void wxClassInfo::GetProperties( wxPropertyInfoMap &infomap ) const
{
    const wxPropertyInfo *pi = GetFirstProperty();
    while( pi )
    {
        if ( infomap.find( pi->GetName() ) == infomap.end() )
            infomap[pi->GetName()] = (wxPropertyInfo*) pi;

        pi = pi->GetNext();
    }

    const wxClassInfo** parents = GetParents();
    for ( int i = 0; parents[i]; ++ i )
    {
        parents[i]->GetProperties( infomap );
    }
}

wxObject* wxClassInfo::AnyToObjectPtr( const wxAny &data) const
{
    return m_variantOfPtrToObjectConverter(data);
}

void wxClassInfo::CallOnAny( const wxAny &data, wxObjectFunctor* functor ) const
{
    if ( data.GetTypeInfo()->GetKind() == wxT_OBJECT )
        return m_variantToObjectConverter(data, functor);
    else
        return (*functor)(m_variantOfPtrToObjectConverter(data));
}

wxAny wxClassInfo::ObjectPtrToAny( wxObject* obj) const
{
    return m_objectToVariantConverter(obj);
}

bool wxClassInfo::NeedsDirectConstruction() const 
{ 
    return wx_dynamic_cast(wxObjectAllocator*, m_constructor) != NULL; 
}

// ----------------------------------------------------------------------------
// wxDynamicObject support
// ----------------------------------------------------------------------------

// Dynamic Objects are objects that have a real superclass instance and carry their
// own attributes in a hash map. Like this it is possible to create the objects and
// stream them, as if their class information was already available from compiled data

struct wxDynamicObject::wxDynamicObjectInternal
{
    wxDynamicObjectInternal() {}

    wxStringToAnyHashMap m_properties;
};

typedef list< wxDynamicObject* > wxDynamicObjectList;

struct wxDynamicClassInfo::wxDynamicClassInfoInternal
{
    wxDynamicObjectList m_dynamicObjects;
};

// instantiates this object with an instance of its superclass
wxDynamicObject::wxDynamicObject(wxObject* superClassInstance, const wxDynamicClassInfo *info)
{
    m_superClassInstance = superClassInstance;
    m_classInfo = info;
    m_data = new wxDynamicObjectInternal;
}

wxDynamicObject::~wxDynamicObject()
{
    wx_dynamic_cast(const wxDynamicClassInfo*, m_classInfo)->
        m_data->m_dynamicObjects.remove( this );
    delete m_data;
    delete m_superClassInstance;
}

void wxDynamicObject::SetProperty (const wxChar *propertyName, const wxAny &value)
{
    wxASSERT_MSG(m_classInfo->FindPropertyInfoInThisClass(propertyName),
                 wxT("Accessing Unknown Property in a Dynamic Object") );
    m_data->m_properties[propertyName] = value;
}

wxAny wxDynamicObject::GetProperty (const wxChar *propertyName) const
{
    wxASSERT_MSG(m_classInfo->FindPropertyInfoInThisClass(propertyName),
                 wxT("Accessing Unknown Property in a Dynamic Object") );
    return m_data->m_properties[propertyName];
}

void wxDynamicObject::RemoveProperty( const wxChar *propertyName )
{
    wxASSERT_MSG(m_classInfo->FindPropertyInfoInThisClass(propertyName),
                 wxT("Removing Unknown Property in a Dynamic Object") );
    m_data->m_properties.erase( propertyName );
}

void wxDynamicObject::RenameProperty( const wxChar *oldPropertyName, 
                                      const wxChar *newPropertyName )
{
    wxASSERT_MSG(m_classInfo->FindPropertyInfoInThisClass(oldPropertyName),
                 wxT("Renaming Unknown Property in a Dynamic Object") );

    wxAny value = m_data->m_properties[oldPropertyName];
    m_data->m_properties.erase( oldPropertyName );
    m_data->m_properties[newPropertyName] = value;
}


// ----------------------------------------------------------------------------
// wxDynamicClassInfo
// ----------------------------------------------------------------------------

wxDynamicClassInfo::wxDynamicClassInfo( const wxChar *unitName, 
                                        const wxChar *className, 
                                        const wxClassInfo* superClass ) :
    wxClassInfo( unitName, className, new const wxClassInfo*[2])
{
    GetParents()[0] = superClass;
    GetParents()[1] = NULL;
    m_data = new wxDynamicClassInfoInternal;
}

wxDynamicClassInfo::~wxDynamicClassInfo()
{
    delete[] GetParents();
    delete m_data;
}

wxObject *wxDynamicClassInfo::AllocateObject() const
{
    wxObject* parent = GetParents()[0]->AllocateObject();
    wxDynamicObject *obj = new wxDynamicObject( parent, this );
    m_data->m_dynamicObjects.push_back( obj );
    return obj;
}

bool wxDynamicClassInfo::Create (wxObject *object, int paramCount, wxAny *params) const
{
    wxDynamicObject *dynobj = wx_dynamic_cast( wxDynamicObject *,  object );
    wxASSERT_MSG( dynobj, 
        wxT("cannot call wxDynamicClassInfo::Create on ")
        wxT("an object other than wxDynamicObject") );

    return GetParents()[0]->Create( dynobj->GetSuperClassInstance(), paramCount, params );
}

// get number of parameters for constructor
int wxDynamicClassInfo::GetCreateParamCount() const
{
    return GetParents()[0]->GetCreateParamCount();
}

// get i-th constructor parameter
const wxChar* wxDynamicClassInfo::GetCreateParamName(int i) const
{
    return GetParents()[0]->GetCreateParamName( i );
}

void wxDynamicClassInfo::SetProperty(wxObject *object, const wxChar *propertyName, const wxAny &value) const
{
    wxDynamicObject* dynobj = wx_dynamic_cast(wxDynamicObject*, object);
    wxASSERT_MSG( dynobj, wxT("cannot call wxDynamicClassInfo::SetProperty on an object other than wxDynamicObject") );
    if ( FindPropertyInfoInThisClass(propertyName) )
        dynobj->SetProperty( propertyName, value );
    else
        GetParents()[0]->SetProperty( dynobj->GetSuperClassInstance(), propertyName, value );
}

wxAny wxDynamicClassInfo::GetProperty(wxObject *object, const wxChar *propertyName) const
{
    wxDynamicObject* dynobj = wx_dynamic_cast(wxDynamicObject*, object);
    wxASSERT_MSG( dynobj, wxT("cannot call wxDynamicClassInfo::SetProperty on an object other than wxDynamicObject") );
    if ( FindPropertyInfoInThisClass(propertyName) )
        return dynobj->GetProperty( propertyName );
    else
        return GetParents()[0]->GetProperty( dynobj->GetSuperClassInstance(), propertyName );
}

void wxDynamicClassInfo::AddProperty( const wxChar *propertyName, const wxTypeInfo* typeInfo )
{
    EnsureInfosInited();
    new wxPropertyInfo( m_firstProperty, this, propertyName, typeInfo->GetTypeName(), new wxGenericPropertyAccessor( propertyName ), wxAny() );
}

void wxDynamicClassInfo::AddHandler( const wxChar *handlerName, wxObjectEventFunction address, const wxClassInfo* eventClassInfo )
{
    EnsureInfosInited();
    new wxHandlerInfo( m_firstHandler, this, handlerName, address, eventClassInfo );
}

// removes an existing runtime-property
void wxDynamicClassInfo::RemoveProperty( const wxChar *propertyName )
{
    for ( wxDynamicObjectList::iterator iter = m_data->m_dynamicObjects.begin(); iter != m_data->m_dynamicObjects.end(); ++iter )
        (*iter)->RemoveProperty( propertyName );
    delete FindPropertyInfoInThisClass(propertyName);
}

// removes an existing runtime-handler
void wxDynamicClassInfo::RemoveHandler( const wxChar *handlerName )
{
    delete FindHandlerInfoInThisClass(handlerName);
}

// renames an existing runtime-property
void wxDynamicClassInfo::RenameProperty( const wxChar *oldPropertyName, const wxChar *newPropertyName )
{
    wxPropertyInfo* pi = FindPropertyInfoInThisClass(oldPropertyName);
    wxASSERT_MSG( pi,wxT("not existing property") );
    pi->m_name = newPropertyName;
    wx_dynamic_cast(wxGenericPropertyAccessor*, pi->GetAccessor())->RenameProperty( oldPropertyName, newPropertyName );
    for ( wxDynamicObjectList::iterator iter = m_data->m_dynamicObjects.begin(); iter != m_data->m_dynamicObjects.end(); ++iter )
        (*iter)->RenameProperty( oldPropertyName, newPropertyName );
}

// renames an existing runtime-handler
void wxDynamicClassInfo::RenameHandler( const wxChar *oldHandlerName, const wxChar *newHandlerName )
{
    wxASSERT_MSG(FindHandlerInfoInThisClass(oldHandlerName),wxT("not existing handler") );
    FindHandlerInfoInThisClass(oldHandlerName)->m_name = newHandlerName;
}

// ----------------------------------------------------------------------------
// wxGenericPropertyAccessor
// ----------------------------------------------------------------------------

struct wxGenericPropertyAccessor::wxGenericPropertyAccessorInternal
{
    char filler;
};

wxGenericPropertyAccessor::wxGenericPropertyAccessor( const wxString& propertyName )
: wxPropertyAccessor( NULL, NULL, NULL, NULL )
{
    m_data = new wxGenericPropertyAccessorInternal;
    m_propertyName = propertyName;
    m_getterName = wxT("Get")+propertyName;
    m_setterName = wxT("Set")+propertyName;
}

wxGenericPropertyAccessor::~wxGenericPropertyAccessor()
{
    delete m_data;
}

void wxGenericPropertyAccessor::SetProperty(wxObject *object, const wxAny &value) const
{
    wxDynamicObject* dynobj = wx_dynamic_cast(wxDynamicObject*, object);
    wxASSERT_MSG( dynobj, wxT("cannot call wxDynamicClassInfo::SetProperty on an object other than wxDynamicObject") );
    dynobj->SetProperty(m_propertyName.c_str(), value );
}

void wxGenericPropertyAccessor::GetProperty(const wxObject *object, wxAny& value) const
{
    const wxDynamicObject* dynobj = wx_dynamic_cast( const wxDynamicObject * ,  object );
    wxASSERT_MSG( dynobj, wxT("cannot call wxDynamicClassInfo::SetProperty on an object other than wxDynamicObject") );
    value = dynobj->GetProperty( m_propertyName.c_str() );
}

// ----------------------------------------------------------------------------
// wxGenericPropertyAccessor
// ----------------------------------------------------------------------------

wxString wxAnyGetAsString( const wxAny& data)
{
    if ( data.IsNull() || data.GetTypeInfo()==NULL )
        return wxEmptyString;

    wxString s;
    data.GetTypeInfo()->ConvertToString(data,s);
    return s;
}

const wxObject* wxAnyGetAsObjectPtr( const wxAny& data)
{
    if ( !data.IsNull() )
    {
        const wxClassTypeInfo* ti = wx_dynamic_cast(const wxClassTypeInfo*, data.GetTypeInfo());
        if( ti )
            return ti->GetClassInfo()->AnyToObjectPtr(data);
    }
    return NULL;
}

wxObjectFunctor::~wxObjectFunctor()
{};

#endif // wxUSE_EXTENDED_RTTI
