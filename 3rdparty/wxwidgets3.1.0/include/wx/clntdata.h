/////////////////////////////////////////////////////////////////////////////
// Name:        wx/clntdata.h
// Purpose:     A mixin class for holding a wxClientData or void pointer
// Author:      Robin Dunn
// Modified by:
// Created:     9-Oct-2001
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CLNTDATAH__
#define _WX_CLNTDATAH__

#include "wx/defs.h"
#include "wx/string.h"
#include "wx/hashmap.h"

typedef int (*wxShadowObjectMethod)(void*, void*);
WX_DECLARE_STRING_HASH_MAP_WITH_DECL(
    wxShadowObjectMethod,
    wxShadowObjectMethods,
    class WXDLLIMPEXP_BASE
);
WX_DECLARE_STRING_HASH_MAP_WITH_DECL(
    void *,
    wxShadowObjectFields,
    class WXDLLIMPEXP_BASE
);

class WXDLLIMPEXP_BASE wxShadowObject
{
public:
    wxShadowObject() { }

    void AddMethod( const wxString &name, wxShadowObjectMethod method )
    {
        wxShadowObjectMethods::iterator it = m_methods.find( name );
        if (it == m_methods.end())
            m_methods[ name ] = method;
        else
            it->second = method;
    }

    bool InvokeMethod( const wxString &name, void* window, void* param, int* returnValue )
    {
        wxShadowObjectMethods::iterator it = m_methods.find( name );
        if (it == m_methods.end())
            return false;
        wxShadowObjectMethod method = it->second;
        int ret = (*method)(window, param);
        if (returnValue)
            *returnValue = ret;
        return true;
    }

    void AddField( const wxString &name, void* initialValue = NULL )
    {
        wxShadowObjectFields::iterator it = m_fields.find( name );
        if (it == m_fields.end())
            m_fields[ name ] = initialValue;
        else
            it->second = initialValue;
    }

    void SetField( const wxString &name, void* value )
    {
        wxShadowObjectFields::iterator it = m_fields.find( name );
        if (it == m_fields.end())
            return;
        it->second = value;
    }

    void* GetField( const wxString &name, void *defaultValue = NULL )
    {
        wxShadowObjectFields::iterator it = m_fields.find( name );
        if (it == m_fields.end())
            return defaultValue;
        return it->second;
    }

private:
    wxShadowObjectMethods   m_methods;
    wxShadowObjectFields    m_fields;
};


// ----------------------------------------------------------------------------

// what kind of client data do we have?
enum wxClientDataType
{
    wxClientData_None,    // we don't know yet because we don't have it at all
    wxClientData_Object,  // our client data is typed and we own it
    wxClientData_Void     // client data is untyped and we don't own it
};

class WXDLLIMPEXP_BASE wxClientData
{
public:
    wxClientData() { }
    virtual ~wxClientData() { }
};

class WXDLLIMPEXP_BASE wxStringClientData : public wxClientData
{
public:
    wxStringClientData() : m_data() { }
    wxStringClientData( const wxString &data ) : m_data(data) { }
    void SetData( const wxString &data ) { m_data = data; }
    const wxString& GetData() const { return m_data; }

private:
    wxString  m_data;
};

// This class is a mixin that provides storage and management of "client
// data."  The client data stored can either be a pointer to a wxClientData
// object in which case it is managed by the container (i.e. it will delete
// the data when it's destroyed) or an untyped pointer which won't be deleted
// by the container - but not both of them
//
// NOTE:  This functionality is currently duplicated in wxEvtHandler in order
//        to avoid having more than one vtable in that class hierarchy.

class WXDLLIMPEXP_BASE wxClientDataContainer
{
public:
    wxClientDataContainer();
    virtual ~wxClientDataContainer();

    void SetClientObject( wxClientData *data ) { DoSetClientObject(data); }
    wxClientData *GetClientObject() const { return DoGetClientObject(); }

    void SetClientData( void *data ) { DoSetClientData(data); }
    void *GetClientData() const { return DoGetClientData(); }

protected:
    // The user data: either an object which will be deleted by the container
    // when it's deleted or some raw pointer which we do nothing with. Only
    // one type of data can be used with the given window, i.e. you cannot set
    // the void data and then associate the container with wxClientData or vice
    // versa.
    union
    {
        wxClientData *m_clientObject;
        void         *m_clientData;
    };

    // client data accessors
    virtual void DoSetClientObject( wxClientData *data );
    virtual wxClientData *DoGetClientObject() const;

    virtual void DoSetClientData( void *data );
    virtual void *DoGetClientData() const;

    // what kind of data do we have?
    wxClientDataType m_clientDataType;

};

#endif // _WX_CLNTDATAH__

