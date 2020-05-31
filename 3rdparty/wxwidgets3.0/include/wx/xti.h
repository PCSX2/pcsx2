/////////////////////////////////////////////////////////////////////////////
// Name:        wx/xti.h
// Purpose:     runtime metadata information (extended class info)
// Author:      Stefan Csomor
// Modified by: Francesco Montorsi
// Created:     27/07/03
// Copyright:   (c) 1997 Julian Smart
//              (c) 2003 Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XTIH__
#define _WX_XTIH__

// We want to support properties, event sources and events sinks through
// explicit declarations, using templates and specialization to make the
// effort as painless as possible.
//
// This means we have the following domains :
//
// - Type Information for categorizing built in types as well as custom types
//   this includes information about enums, their values and names
// - Type safe value storage : a kind of wxVariant, called right now wxAny
//   which will be merged with wxVariant
// - Property Information and Property Accessors providing access to a class'
//   values and exposed event delegates
// - Information about event handlers
// - extended Class Information for accessing all these

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/defs.h"

#if wxUSE_EXTENDED_RTTI

class WXDLLIMPEXP_FWD_BASE wxAny;
class WXDLLIMPEXP_FWD_BASE wxAnyList;
class WXDLLIMPEXP_FWD_BASE wxObject;
class WXDLLIMPEXP_FWD_BASE wxString;
class WXDLLIMPEXP_FWD_BASE wxClassInfo;
class WXDLLIMPEXP_FWD_BASE wxHashTable;
class WXDLLIMPEXP_FWD_BASE wxObject;
class WXDLLIMPEXP_FWD_BASE wxPluginLibrary;
class WXDLLIMPEXP_FWD_BASE wxHashTable;
class WXDLLIMPEXP_FWD_BASE wxHashTable_Node;

class WXDLLIMPEXP_FWD_BASE wxStringToAnyHashMap;
class WXDLLIMPEXP_FWD_BASE wxPropertyInfoMap;
class WXDLLIMPEXP_FWD_BASE wxPropertyAccessor;
class WXDLLIMPEXP_FWD_BASE wxObjectAllocatorAndCreator;
class WXDLLIMPEXP_FWD_BASE wxObjectAllocator;


#define wx_dynamic_cast(t, x) dynamic_cast<t>(x)

#include "wx/xtitypes.h"
#include "wx/xtihandler.h"

// ----------------------------------------------------------------------------
// wxClassInfo
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxObjectFunctor
{
public:
    virtual ~wxObjectFunctor();

    // Invoke the actual event handler:
    virtual void operator()(const wxObject *) = 0;
};

class WXDLLIMPEXP_FWD_BASE wxPropertyInfo;
class WXDLLIMPEXP_FWD_BASE wxHandlerInfo;

typedef wxObject *(*wxObjectConstructorFn)(void);
typedef wxPropertyInfo *(*wxPropertyInfoFn)(void);
typedef wxHandlerInfo *(*wxHandlerInfoFn)(void);
typedef void (*wxVariantToObjectConverter)( const wxAny &data, wxObjectFunctor* fn );
typedef wxObject* (*wxVariantToObjectPtrConverter) ( const wxAny& data);
typedef wxAny (*wxObjectToVariantConverter)( wxObject* );

WXDLLIMPEXP_BASE wxString wxAnyGetAsString( const wxAny& data);
WXDLLIMPEXP_BASE const wxObject* wxAnyGetAsObjectPtr( const wxAny& data);

class WXDLLIMPEXP_BASE wxObjectWriter;
class WXDLLIMPEXP_BASE wxObjectWriterCallback;

typedef bool (*wxObjectStreamingCallback) ( const wxObject *, wxObjectWriter *, \
                                            wxObjectWriterCallback *, const wxStringToAnyHashMap & );



class WXDLLIMPEXP_BASE wxClassInfo
{
    friend class WXDLLIMPEXP_BASE wxPropertyInfo;
    friend class /* WXDLLIMPEXP_BASE */ wxHandlerInfo;
    friend wxObject *wxCreateDynamicObject(const wxString& name);

public:
    wxClassInfo(const wxClassInfo **_Parents,
                const wxChar *_UnitName,
                const wxChar *_ClassName,
                int size,
                wxObjectConstructorFn ctor,
                wxPropertyInfoFn _Props,
                wxHandlerInfoFn _Handlers,
                wxObjectAllocatorAndCreator* _Constructor,
                const wxChar ** _ConstructorProperties,
                const int _ConstructorPropertiesCount,
                wxVariantToObjectPtrConverter _PtrConverter1,
                wxVariantToObjectConverter _Converter2,
                wxObjectToVariantConverter _Converter3,
                wxObjectStreamingCallback _streamingCallback = NULL) :
            m_className(_ClassName),
            m_objectSize(size),
            m_objectConstructor(ctor),
            m_next(sm_first),
            m_firstPropertyFn(_Props),
            m_firstHandlerFn(_Handlers),
            m_firstProperty(NULL),
            m_firstHandler(NULL),
            m_firstInited(false),
            m_parents(_Parents),
            m_unitName(_UnitName),
            m_constructor(_Constructor),
            m_constructorProperties(_ConstructorProperties),
            m_constructorPropertiesCount(_ConstructorPropertiesCount),
            m_variantOfPtrToObjectConverter(_PtrConverter1),
            m_variantToObjectConverter(_Converter2),
            m_objectToVariantConverter(_Converter3),
            m_streamingCallback(_streamingCallback)
    {
        sm_first = this;
        Register();
    }

    wxClassInfo(const wxChar *_UnitName, const wxChar *_ClassName,
                const wxClassInfo **_Parents) :
            m_className(_ClassName),
            m_objectSize(0),
            m_objectConstructor(NULL),
            m_next(sm_first),
            m_firstPropertyFn(NULL),
            m_firstHandlerFn(NULL),
            m_firstProperty(NULL),
            m_firstHandler(NULL),
            m_firstInited(true),
            m_parents(_Parents),
            m_unitName(_UnitName),
            m_constructor(NULL),
            m_constructorProperties(NULL),
            m_constructorPropertiesCount(0),
            m_variantOfPtrToObjectConverter(NULL),
            m_variantToObjectConverter(NULL),
            m_objectToVariantConverter(NULL),
            m_streamingCallback(NULL)
    {
        sm_first = this;
        Register();
    }

    // ctor compatible with old RTTI system
    wxClassInfo(const wxChar *_ClassName,
                const wxClassInfo *_Parent1,
                const wxClassInfo *_Parent2,
                int size,
                wxObjectConstructorFn ctor) :
            m_className(_ClassName),
            m_objectSize(size),
            m_objectConstructor(ctor),
            m_next(sm_first),
            m_firstPropertyFn(NULL),
            m_firstHandlerFn(NULL),
            m_firstProperty(NULL),
            m_firstHandler(NULL),
            m_firstInited(true),
            m_parents(NULL),
            m_unitName(NULL),
            m_constructor(NULL),
            m_constructorProperties(NULL),
            m_constructorPropertiesCount(0),
            m_variantOfPtrToObjectConverter(NULL),
            m_variantToObjectConverter(NULL),
            m_objectToVariantConverter(NULL),
            m_streamingCallback(NULL)
    {
        sm_first = this;
        m_parents[0] = _Parent1;
        m_parents[1] = _Parent2;
        m_parents[2] = NULL;
        Register();
    }

    virtual ~wxClassInfo();

    // allocates an instance of this class, this object does not have to be 
    // initialized or fully constructed as this call will be followed by a call to Create
    virtual wxObject *AllocateObject() const 
        { return m_objectConstructor ? (*m_objectConstructor)() : 0; }

    // 'old naming' for AllocateObject staying here for backward compatibility
    wxObject *CreateObject() const { return AllocateObject(); }

    // direct construction call for classes that cannot construct instances via alloc/create
    wxObject *ConstructObject(int ParamCount, wxAny *Params) const;

    bool NeedsDirectConstruction() const;
    
    const wxChar       *GetClassName() const 
        { return m_className; }
    const wxChar       *GetBaseClassName1() const
        { return m_parents[0] ? m_parents[0]->GetClassName() : NULL; }
    const wxChar       *GetBaseClassName2() const
        { return (m_parents[0] && m_parents[1]) ? m_parents[1]->GetClassName() : NULL; }

    const wxClassInfo  *GetBaseClass1() const
        { return m_parents[0]; }
    const wxClassInfo  *GetBaseClass2() const
        { return m_parents[0] ? m_parents[1] : NULL; }

    const wxChar       *GetIncludeName() const 
        { return m_unitName; }
    const wxClassInfo **GetParents() const 
        { return m_parents; }
    int                 GetSize() const 
        { return m_objectSize; }
    bool                IsDynamic() const 
        { return (NULL != m_objectConstructor); }

    wxObjectConstructorFn      GetConstructor() const 
        { return m_objectConstructor; }
    const wxClassInfo         *GetNext() const 
        { return m_next; }

    // statics:

    static void                CleanUp();
    static wxClassInfo        *FindClass(const wxString& className);
    static const wxClassInfo  *GetFirst() 
        { return sm_first; }


    // Climb upwards through inheritance hierarchy.
    // Dual inheritance is catered for.

    bool IsKindOf(const wxClassInfo *info) const;

    wxDECLARE_CLASS_INFO_ITERATORS();

    // if there is a callback registered with that class it will be called
    // before this object will be written to disk, it can veto streaming out
    // this object by returning false, if this class has not registered a
    // callback, the search will go up the inheritance tree if no callback has
    // been registered true will be returned by default
    bool BeforeWriteObject( const wxObject *obj, wxObjectWriter *streamer, 
                            wxObjectWriterCallback *writercallback, const wxStringToAnyHashMap &metadata) const;

    // gets the streaming callback from this class or any superclass
    wxObjectStreamingCallback GetStreamingCallback() const;

    // returns the first property
    wxPropertyInfo* GetFirstProperty() const 
        { EnsureInfosInited(); return m_firstProperty; }

    // returns the first handler
    wxHandlerInfo* GetFirstHandler() const 
        { EnsureInfosInited(); return m_firstHandler; }

    // Call the Create upon an instance of the class, in the end the object is fully
    // initialized
    virtual bool Create (wxObject *object, int ParamCount, wxAny *Params) const;

    // get number of parameters for constructor
    virtual int GetCreateParamCount() const 
        { return m_constructorPropertiesCount; }

    // get n-th constructor parameter
    virtual const wxChar* GetCreateParamName(int n) const 
        { return m_constructorProperties[n]; }

    // Runtime access to objects for simple properties (get/set) by property 
    // name and variant data
    virtual void SetProperty (wxObject *object, const wxChar *propertyName, 
                              const wxAny &value) const;
    virtual wxAny GetProperty (wxObject *object, const wxChar *propertyName) const;

    // Runtime access to objects for collection properties by property name
    virtual wxAnyList GetPropertyCollection(wxObject *object, 
                                                  const wxChar *propertyName) const;
    virtual void AddToPropertyCollection(wxObject *object, const wxChar *propertyName, 
                                         const wxAny& value) const;

    // we must be able to cast variants to wxObject pointers, templates seem 
    // not to be suitable
    void CallOnAny( const wxAny &data, wxObjectFunctor* functor ) const;

    wxObject* AnyToObjectPtr( const wxAny &data) const;

    wxAny ObjectPtrToAny( wxObject *object ) const;

    // find property by name
    virtual const wxPropertyInfo *FindPropertyInfo (const wxChar *PropertyName) const;

    // find handler by name
    virtual const wxHandlerInfo *FindHandlerInfo (const wxChar *handlerName) const;

    // find property by name
    virtual wxPropertyInfo *FindPropertyInfoInThisClass (const wxChar *PropertyName) const;

    // find handler by name
    virtual wxHandlerInfo *FindHandlerInfoInThisClass (const wxChar *handlerName) const;

    // puts all the properties of this class and its superclasses in the map, 
    // as long as there is not yet an entry with the same name (overriding mechanism)
    void GetProperties( wxPropertyInfoMap &map ) const;

private:
    const wxChar            *m_className;
    int                      m_objectSize;
    wxObjectConstructorFn     m_objectConstructor;

    // class info object live in a linked list:
    // pointers to its head and the next element in it

    static wxClassInfo      *sm_first;
    wxClassInfo              *m_next;

    static wxHashTable      *sm_classTable;

    wxPropertyInfoFn          m_firstPropertyFn;
    wxHandlerInfoFn           m_firstHandlerFn;


protected:
    void                      EnsureInfosInited() const
    {
        if ( !m_firstInited)
        {
            if ( m_firstPropertyFn != NULL)
                m_firstProperty = (*m_firstPropertyFn)();
            if ( m_firstHandlerFn != NULL)
                m_firstHandler = (*m_firstHandlerFn)();
            m_firstInited = true;
        }
    }
    mutable wxPropertyInfo*   m_firstProperty;
    mutable wxHandlerInfo*    m_firstHandler;

private:
    mutable bool              m_firstInited;

    const wxClassInfo**       m_parents;
    const wxChar*             m_unitName;

    wxObjectAllocatorAndCreator*     m_constructor;
    const wxChar **           m_constructorProperties;
    const int                 m_constructorPropertiesCount;
    wxVariantToObjectPtrConverter m_variantOfPtrToObjectConverter;
    wxVariantToObjectConverter m_variantToObjectConverter;
    wxObjectToVariantConverter m_objectToVariantConverter;
    wxObjectStreamingCallback  m_streamingCallback;

    const wxPropertyAccessor *FindAccessor (const wxChar *propertyName) const;

protected:
    // registers the class
    void Register();
    void Unregister();

    DECLARE_NO_COPY_CLASS(wxClassInfo)
};

WXDLLIMPEXP_BASE wxObject *wxCreateDynamicObject(const wxString& name);

// ----------------------------------------------------------------------------
// wxDynamicClassInfo
// ----------------------------------------------------------------------------

// this object leads to having a pure runtime-instantiation

class WXDLLIMPEXP_BASE wxDynamicClassInfo : public wxClassInfo
{
    friend class WXDLLIMPEXP_BASE wxDynamicObject;

public:
    wxDynamicClassInfo( const wxChar *_UnitName, const wxChar *_ClassName, 
                        const wxClassInfo* superClass );
    virtual ~wxDynamicClassInfo();

    // constructs a wxDynamicObject with an instance
    virtual wxObject *AllocateObject() const;

    // Call the Create method for a class
    virtual bool Create (wxObject *object, int ParamCount, wxAny *Params) const;

    // get number of parameters for constructor
    virtual int GetCreateParamCount() const;

    // get i-th constructor parameter
    virtual const wxChar* GetCreateParamName(int i) const;

    // Runtime access to objects by property name, and variant data
    virtual void SetProperty (wxObject *object, const wxChar *PropertyName, 
                              const wxAny &Value) const;
    virtual wxAny GetProperty (wxObject *object, const wxChar *PropertyName) const;

    // adds a property to this class at runtime
    void AddProperty( const wxChar *propertyName, const wxTypeInfo* typeInfo );

    // removes an existing runtime-property
    void RemoveProperty( const wxChar *propertyName );

    // renames an existing runtime-property
    void RenameProperty( const wxChar *oldPropertyName, const wxChar *newPropertyName );

    // as a handler to this class at runtime
    void AddHandler( const wxChar *handlerName, wxObjectEventFunction address, 
                     const wxClassInfo* eventClassInfo );

    // removes an existing runtime-handler
    void RemoveHandler( const wxChar *handlerName );

    // renames an existing runtime-handler
    void RenameHandler( const wxChar *oldHandlerName, const wxChar *newHandlerName );

private:
    struct wxDynamicClassInfoInternal;
    wxDynamicClassInfoInternal* m_data;
};

// ----------------------------------------------------------------------------
// wxDECLARE class macros
// ----------------------------------------------------------------------------

#define _DECLARE_DYNAMIC_CLASS(name)                        \
    public:                                                 \
        static wxClassInfo ms_classInfo;                    \
        static const wxClassInfo* ms_classParents[];        \
        static wxPropertyInfo* GetPropertiesStatic();       \
        static wxHandlerInfo* GetHandlersStatic();          \
        static wxClassInfo *GetClassInfoStatic()            \
            { return &name::ms_classInfo; }                 \
        virtual wxClassInfo *GetClassInfo() const           \
            { return &name::ms_classInfo; }

#define wxDECLARE_DYNAMIC_CLASS(name)                       \
    static wxObjectAllocatorAndCreator* ms_constructor;           \
    static const wxChar * ms_constructorProperties[];       \
    static const int ms_constructorPropertiesCount;         \
    _DECLARE_DYNAMIC_CLASS(name)

#define wxDECLARE_DYNAMIC_CLASS_NO_ASSIGN(name)             \
    wxDECLARE_NO_ASSIGN_CLASS(name);                        \
    wxDECLARE_DYNAMIC_CLASS(name)

#define wxDECLARE_DYNAMIC_CLASS_NO_COPY(name)               \
    wxDECLARE_NO_COPY_CLASS(name);                          \
    wxDECLARE_DYNAMIC_CLASS(name)

#define wxDECLARE_CLASS(name)                               \
    wxDECLARE_DYNAMIC_CLASS(name)

#define wxDECLARE_ABSTRACT_CLASS(name)    _DECLARE_DYNAMIC_CLASS(name)
#define wxCLASSINFO(name)                 (&name::ms_classInfo)

#endif  // wxUSE_EXTENDED_RTTI
#endif // _WX_XTIH__
