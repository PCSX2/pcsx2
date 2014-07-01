/////////////////////////////////////////////////////////////////////////////
// Name:        wx/rtti.h
// Purpose:     old RTTI macros (use XTI when possible instead)
// Author:      Julian Smart
// Modified by: Ron Lee
// Created:     01/02/97
// Copyright:   (c) 1997 Julian Smart
//              (c) 2001 Ron Lee <ron@debian.org>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_RTTIH__
#define _WX_RTTIH__

#if !wxUSE_EXTENDED_RTTI     // XTI system is meant to replace these macros

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/memory.h"

// ----------------------------------------------------------------------------
// forward declarations
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_BASE wxObject;
class WXDLLIMPEXP_FWD_BASE wxString;
class WXDLLIMPEXP_FWD_BASE wxClassInfo;
class WXDLLIMPEXP_FWD_BASE wxHashTable;
class WXDLLIMPEXP_FWD_BASE wxObject;
class WXDLLIMPEXP_FWD_BASE wxPluginLibrary;
class WXDLLIMPEXP_FWD_BASE wxHashTable_Node;

// ----------------------------------------------------------------------------
// wxClassInfo
// ----------------------------------------------------------------------------

typedef wxObject *(*wxObjectConstructorFn)(void);

class WXDLLIMPEXP_BASE wxClassInfo
{
    friend class WXDLLIMPEXP_FWD_BASE wxObject;
    friend WXDLLIMPEXP_BASE wxObject *wxCreateDynamicObject(const wxString& name);
public:
    wxClassInfo( const wxChar *className,
                 const wxClassInfo *baseInfo1,
                 const wxClassInfo *baseInfo2,
                 int size,
                 wxObjectConstructorFn ctor )
        : m_className(className)
        , m_objectSize(size)
        , m_objectConstructor(ctor)
        , m_baseInfo1(baseInfo1)
        , m_baseInfo2(baseInfo2)
        , m_next(sm_first)
        {
            sm_first = this;
            Register();
        }

    ~wxClassInfo();

    wxObject *CreateObject() const
        { return m_objectConstructor ? (*m_objectConstructor)() : 0; }
    bool IsDynamic() const { return (NULL != m_objectConstructor); }

    const wxChar       *GetClassName() const { return m_className; }
    const wxChar       *GetBaseClassName1() const
        { return m_baseInfo1 ? m_baseInfo1->GetClassName() : NULL; }
    const wxChar       *GetBaseClassName2() const
        { return m_baseInfo2 ? m_baseInfo2->GetClassName() : NULL; }
    const wxClassInfo  *GetBaseClass1() const { return m_baseInfo1; }
    const wxClassInfo  *GetBaseClass2() const { return m_baseInfo2; }
    int                 GetSize() const { return m_objectSize; }

    wxObjectConstructorFn      GetConstructor() const
        { return m_objectConstructor; }
    static const wxClassInfo  *GetFirst() { return sm_first; }
    const wxClassInfo         *GetNext() const { return m_next; }
    static wxClassInfo        *FindClass(const wxString& className);

        // Climb upwards through inheritance hierarchy.
        // Dual inheritance is catered for.

    bool IsKindOf(const wxClassInfo *info) const
    {
        return info != 0 &&
               ( info == this ||
                 ( m_baseInfo1 && m_baseInfo1->IsKindOf(info) ) ||
                 ( m_baseInfo2 && m_baseInfo2->IsKindOf(info) ) );
    }

    wxDECLARE_CLASS_INFO_ITERATORS();

private:
    const wxChar            *m_className;
    int                      m_objectSize;
    wxObjectConstructorFn    m_objectConstructor;

        // Pointers to base wxClassInfos

    const wxClassInfo       *m_baseInfo1;
    const wxClassInfo       *m_baseInfo2;

        // class info object live in a linked list:
        // pointers to its head and the next element in it

    static wxClassInfo      *sm_first;
    wxClassInfo             *m_next;

    static wxHashTable      *sm_classTable;

protected:
    // registers the class
    void Register();
    void Unregister();

    wxDECLARE_NO_COPY_CLASS(wxClassInfo);
};

WXDLLIMPEXP_BASE wxObject *wxCreateDynamicObject(const wxString& name);

// ----------------------------------------------------------------------------
// Dynamic class macros
// ----------------------------------------------------------------------------

#define wxDECLARE_ABSTRACT_CLASS(name)                                        \
    public:                                                                   \
        static wxClassInfo ms_classInfo;                                      \
        virtual wxClassInfo *GetClassInfo() const

#define wxDECLARE_DYNAMIC_CLASS_NO_ASSIGN(name)                               \
    wxDECLARE_NO_ASSIGN_CLASS(name);                                          \
    wxDECLARE_DYNAMIC_CLASS(name)

#define wxDECLARE_DYNAMIC_CLASS_NO_COPY(name)                                 \
    wxDECLARE_NO_COPY_CLASS(name);                                            \
    wxDECLARE_DYNAMIC_CLASS(name)

#define wxDECLARE_DYNAMIC_CLASS(name)                                         \
    wxDECLARE_ABSTRACT_CLASS(name);                                           \
    static wxObject* wxCreateObject()

#define wxDECLARE_CLASS(name)                                                 \
    wxDECLARE_ABSTRACT_CLASS(name)


// common part of the macros below
#define wxIMPLEMENT_CLASS_COMMON(name, basename, baseclsinfo2, func)          \
    wxClassInfo name::ms_classInfo(wxT(#name),                                \
            &basename::ms_classInfo,                                          \
            baseclsinfo2,                                                     \
            (int) sizeof(name),                                               \
            func);                                                            \
                                                                              \
    wxClassInfo *name::GetClassInfo() const                                   \
        { return &name::ms_classInfo; }

#define wxIMPLEMENT_CLASS_COMMON1(name, basename, func)                       \
    wxIMPLEMENT_CLASS_COMMON(name, basename, NULL, func)

#define wxIMPLEMENT_CLASS_COMMON2(name, basename1, basename2, func)           \
    wxIMPLEMENT_CLASS_COMMON(name, basename1, &basename2::ms_classInfo, func)

// -----------------------------------
// for concrete classes
// -----------------------------------

    // Single inheritance with one base class
#define wxIMPLEMENT_DYNAMIC_CLASS(name, basename)                             \
    wxIMPLEMENT_CLASS_COMMON1(name, basename, name::wxCreateObject)           \
    wxObject* name::wxCreateObject()                                          \
        { return new name; }

    // Multiple inheritance with two base classes
#define wxIMPLEMENT_DYNAMIC_CLASS2(name, basename1, basename2)                \
    wxIMPLEMENT_CLASS_COMMON2(name, basename1, basename2,                     \
                              name::wxCreateObject)                           \
    wxObject* name::wxCreateObject()                                          \
        { return new name; }

// -----------------------------------
// for abstract classes
// -----------------------------------

    // Single inheritance with one base class
#define wxIMPLEMENT_ABSTRACT_CLASS(name, basename)                            \
    wxIMPLEMENT_CLASS_COMMON1(name, basename, NULL)

    // Multiple inheritance with two base classes
#define wxIMPLEMENT_ABSTRACT_CLASS2(name, basename1, basename2)               \
    wxIMPLEMENT_CLASS_COMMON2(name, basename1, basename2, NULL)

// -----------------------------------
// XTI-compatible macros
// -----------------------------------

#include "wx/flags.h"

// these macros only do something when wxUSE_EXTENDED_RTTI=1
// (and in that case they are defined by xti.h); however to avoid
// to be forced to wrap these macros (in user's source files) with
//
//  #if wxUSE_EXTENDED_RTTI
//  ...
//  #endif
//
// blocks, we define them here as empty.

#define wxEMPTY_PARAMETER_VALUE /**/

#define wxBEGIN_ENUM( e ) wxEMPTY_PARAMETER_VALUE
#define wxENUM_MEMBER( v ) wxEMPTY_PARAMETER_VALUE
#define wxEND_ENUM( e ) wxEMPTY_PARAMETER_VALUE

#define wxIMPLEMENT_SET_STREAMING(SetName,e) wxEMPTY_PARAMETER_VALUE

#define wxBEGIN_FLAGS( e ) wxEMPTY_PARAMETER_VALUE
#define wxFLAGS_MEMBER( v ) wxEMPTY_PARAMETER_VALUE
#define wxEND_FLAGS( e ) wxEMPTY_PARAMETER_VALUE

#define wxCOLLECTION_TYPE_INFO( element, collection ) wxEMPTY_PARAMETER_VALUE

#define wxHANDLER(name,eventClassType) wxEMPTY_PARAMETER_VALUE
#define wxBEGIN_HANDLERS_TABLE(theClass) wxEMPTY_PARAMETER_VALUE
#define wxEND_HANDLERS_TABLE() wxEMPTY_PARAMETER_VALUE

#define wxIMPLEMENT_DYNAMIC_CLASS_XTI( name, basename, unit ) \
    wxIMPLEMENT_DYNAMIC_CLASS( name, basename )
#define wxIMPLEMENT_DYNAMIC_CLASS_XTI_CALLBACK( name, basename, unit, callback ) \
    wxIMPLEMENT_DYNAMIC_CLASS( name, basename )

#define wxIMPLEMENT_DYNAMIC_CLASS_WITH_COPY_XTI( name, basename, unit ) \
    wxIMPLEMENT_DYNAMIC_CLASS( name, basename)

#define wxIMPLEMENT_DYNAMIC_CLASS_WITH_COPY_AND_STREAMERS_XTI( name, basename,  \
                                                             unit, toString,    \
                                                             fromString ) wxEMPTY_PARAMETER_VALUE
#define wxIMPLEMENT_DYNAMIC_CLASS_NO_WXOBJECT_NO_BASE_XTI( name, unit ) wxEMPTY_PARAMETER_VALUE
#define wxIMPLEMENT_DYNAMIC_CLASS_NO_WXOBJECT_XTI( name, basename, unit ) wxEMPTY_PARAMETER_VALUE

#define wxIMPLEMENT_DYNAMIC_CLASS2_XTI( name, basename, basename2, unit) wxIMPLEMENT_DYNAMIC_CLASS2( name, basename, basename2 )

#define wxCONSTRUCTOR_0(klass) \
    wxEMPTY_PARAMETER_VALUE
#define wxCONSTRUCTOR_DUMMY(klass) \
    wxEMPTY_PARAMETER_VALUE
#define wxDIRECT_CONSTRUCTOR_0(klass) \
    wxEMPTY_PARAMETER_VALUE
#define wxCONSTRUCTOR_1(klass,t0,v0) \
    wxEMPTY_PARAMETER_VALUE
#define wxDIRECT_CONSTRUCTOR_1(klass,t0,v0) \
    wxEMPTY_PARAMETER_VALUE
#define wxCONSTRUCTOR_2(klass,t0,v0,t1,v1) \
    wxEMPTY_PARAMETER_VALUE
#define wxDIRECT_CONSTRUCTOR_2(klass,t0,v0,t1,v1) \
    wxEMPTY_PARAMETER_VALUE
#define wxCONSTRUCTOR_3(klass,t0,v0,t1,v1,t2,v2) \
    wxEMPTY_PARAMETER_VALUE
#define wxDIRECT_CONSTRUCTOR_3(klass,t0,v0,t1,v1,t2,v2) \
    wxEMPTY_PARAMETER_VALUE
#define wxCONSTRUCTOR_4(klass,t0,v0,t1,v1,t2,v2,t3,v3) \
    wxEMPTY_PARAMETER_VALUE
#define wxDIRECT_CONSTRUCTOR_4(klass,t0,v0,t1,v1,t2,v2,t3,v3) \
    wxEMPTY_PARAMETER_VALUE
#define wxCONSTRUCTOR_5(klass,t0,v0,t1,v1,t2,v2,t3,v3,t4,v4) \
    wxEMPTY_PARAMETER_VALUE
#define wxDIRECT_CONSTRUCTOR_5(klass,t0,v0,t1,v1,t2,v2,t3,v3,t4,v4) \
    wxEMPTY_PARAMETER_VALUE
#define wxCONSTRUCTOR_6(klass,t0,v0,t1,v1,t2,v2,t3,v3,t4,v4,t5,v5) \
    wxEMPTY_PARAMETER_VALUE
#define wxDIRECT_CONSTRUCTOR_6(klass,t0,v0,t1,v1,t2,v2,t3,v3,t4,v4,t5,v5) \
    wxEMPTY_PARAMETER_VALUE
#define wxCONSTRUCTOR_7(klass,t0,v0,t1,v1,t2,v2,t3,v3,t4,v4,t5,v5,t6,v6) \
    wxEMPTY_PARAMETER_VALUE
#define wxDIRECT_CONSTRUCTOR_7(klass,t0,v0,t1,v1,t2,v2,t3,v3,t4,v4,t5,v5,t6,v6) \
    wxEMPTY_PARAMETER_VALUE
#define wxCONSTRUCTOR_8(klass,t0,v0,t1,v1,t2,v2,t3,v3,t4,v4,t5,v5,t6,v6,t7,v7) \
    wxEMPTY_PARAMETER_VALUE
#define wxDIRECT_CONSTRUCTOR_8(klass,t0,v0,t1,v1,t2,v2,t3,v3,t4,v4,t5,v5,t6,v6,t7,v7) \
    wxEMPTY_PARAMETER_VALUE

#define wxSETTER( property, Klass, valueType, setterMethod ) wxEMPTY_PARAMETER_VALUE
#define wxGETTER( property, Klass, valueType, gettermethod ) wxEMPTY_PARAMETER_VALUE
#define wxADDER( property, Klass, valueType, addermethod ) wxEMPTY_PARAMETER_VALUE
#define wxCOLLECTION_GETTER( property, Klass, valueType, gettermethod ) wxEMPTY_PARAMETER_VALUE

#define wxBEGIN_PROPERTIES_TABLE(theClass) wxEMPTY_PARAMETER_VALUE
#define wxEND_PROPERTIES_TABLE() wxEMPTY_PARAMETER_VALUE
#define wxHIDE_PROPERTY( pname ) wxEMPTY_PARAMETER_VALUE

#define wxPROPERTY( pname, type, setter, getter, defaultValue, flags, help, group) \
    wxEMPTY_PARAMETER_VALUE

#define wxPROPERTY_FLAGS( pname, flags, type, setter, getter,defaultValue,    \
                          pflags, help, group) wxEMPTY_PARAMETER_VALUE

#define wxREADONLY_PROPERTY( pname, type, getter,defaultValue, flags, help, group) \
    wxGETTER( pname, class_t, type, getter ) wxEMPTY_PARAMETER_VALUE

#define wxREADONLY_PROPERTY_FLAGS( pname, flags, type, getter,defaultValue,    \
                                   pflags, help, group)  wxEMPTY_PARAMETER_VALUE

#define wxPROPERTY_COLLECTION( pname, colltype, addelemtype, adder, getter,    \
                               flags, help, group )  wxEMPTY_PARAMETER_VALUE

#define wxREADONLY_PROPERTY_COLLECTION( pname, colltype, addelemtype, getter,   \
                                        flags, help, group) wxEMPTY_PARAMETER_VALUE
#define wxEVENT_PROPERTY( name, eventType, eventClass ) wxEMPTY_PARAMETER_VALUE

#define wxEVENT_RANGE_PROPERTY( name, eventType, lastEventType, eventClass ) wxEMPTY_PARAMETER_VALUE

#define wxIMPLEMENT_PROPERTY(name, type) wxEMPTY_PARAMETER_VALUE

#define wxEMPTY_HANDLERS_TABLE(name) wxEMPTY_PARAMETER_VALUE

#endif // !wxUSE_EXTENDED_RTTI
#endif // _WX_RTTIH__
