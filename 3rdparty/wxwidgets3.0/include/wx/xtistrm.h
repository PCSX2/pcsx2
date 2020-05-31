/////////////////////////////////////////////////////////////////////////////
// Name:        wx/xtistrm.h
// Purpose:     streaming runtime metadata information (extended class info)
// Author:      Stefan Csomor
// Modified by:
// Created:     27/07/03
// Copyright:   (c) 2003 Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XTISTRMH__
#define _WX_XTISTRMH__

#include "wx/defs.h"

#if wxUSE_EXTENDED_RTTI

#include "wx/object.h"

const int wxInvalidObjectID = -2;
const int wxNullObjectID = -3;

// Filer contains the interfaces for streaming objects in and out of XML,
// rendering them either to objects in memory, or to code.  Note:  We
// consider the process of generating code to be one of *depersisting* the
// object from xml, *not* of persisting the object to code from an object
// in memory.  This distinction can be confusing, and should be kept
// in mind when looking at the property streamers and callback interfaces
// listed below.


// ----------------------------------------------------------------------------
// wxObjectWriterCallback
//
// This class will be asked during the streaming-out process about every single
// property or object instance. It can veto streaming out by returning false
// or modify the value before it is streamed-out.
// ----------------------------------------------------------------------------

/*
 class WXDLLIMPEXP_BASE wxClassInfo;
 class WXDLLIMPEXP_BASE wxAnyList;
 class WXDLLIMPEXP_BASE wxPropertyInfo;
 class WXDLLIMPEXP_BASE wxAny;
 class WXDLLIMPEXP_BASE wxHandlerInfo;
 */

class WXDLLIMPEXP_BASE wxObjectWriter;
class WXDLLIMPEXP_BASE wxObjectReader;

class WXDLLIMPEXP_BASE wxObjectWriterCallback
{
public:
    virtual ~wxObjectWriterCallback() {}

    // will be called before an object is written, may veto by returning false
    virtual bool BeforeWriteObject( wxObjectWriter *WXUNUSED(writer), 
                                    const wxObject *WXUNUSED(object), 
                                    const wxClassInfo *WXUNUSED(classInfo), 
                                    const wxStringToAnyHashMap &WXUNUSED(metadata)) 
        { return true; }

    // will be called after this object has been written, may be 
    // needed for adjusting stacks
    virtual void AfterWriteObject( wxObjectWriter *WXUNUSED(writer), 
                                   const wxObject *WXUNUSED(object), 
                                   const wxClassInfo *WXUNUSED(classInfo) ) 
        {}

    // will be called before a property gets written, may change the value, 
    // eg replace a concrete wxSize by wxSize( wxDefaultCoord, wxDefaultCoord ) 
    // or veto writing that property at all by returning false
    virtual bool BeforeWriteProperty( wxObjectWriter *WXUNUSED(writer), 
                                      const wxObject *WXUNUSED(object), 
                                      const wxPropertyInfo *WXUNUSED(propInfo), 
                                      const wxAny &WXUNUSED(value) )  
        { return true; }

    // will be called before a property gets written, may change the value, 
    // eg replace a concrete wxSize by wxSize( wxDefaultCoord, wxDefaultCoord ) 
    // or veto writing that property at all by returning false
    virtual bool BeforeWriteProperty( wxObjectWriter *WXUNUSED(writer), 
                                      const wxObject *WXUNUSED(object), 
                                      const wxPropertyInfo *WXUNUSED(propInfo), 
                                      const wxAnyList &WXUNUSED(value) )  
        { return true; }

    // will be called after a property has been written out, may be needed 
    // for adjusting stacks
    virtual void AfterWriteProperty( wxObjectWriter *WXUNUSED(writer), 
                                     const wxPropertyInfo *WXUNUSED(propInfo) ) 
        {}

    // will be called before this delegate gets written
    virtual bool BeforeWriteDelegate( wxObjectWriter *WXUNUSED(writer), 
                                      const wxObject *WXUNUSED(object),  
                                      const wxClassInfo* WXUNUSED(classInfo), 
                                      const wxPropertyInfo *WXUNUSED(propInfo),
                                      const wxObject *&WXUNUSED(eventSink), 
                                      const wxHandlerInfo* &WXUNUSED(handlerInfo) ) 
        { return true; }

    virtual void AfterWriteDelegate( wxObjectWriter *WXUNUSED(writer), 
                                     const wxObject *WXUNUSED(object), 
                                     const wxClassInfo* WXUNUSED(classInfo), 
                                     const wxPropertyInfo *WXUNUSED(propInfo),
                                     const wxObject *&WXUNUSED(eventSink), 
                                     const wxHandlerInfo* &WXUNUSED(handlerInfo) ) 
        { }
};

class WXDLLIMPEXP_BASE wxObjectWriterFunctor: public wxObjectFunctor
{
};

class WXDLLIMPEXP_BASE wxObjectWriter: public wxObject
{
    friend class wxObjectWriterFunctor;
public:
    wxObjectWriter();
    virtual ~wxObjectWriter();

    // with this call you start writing out a new top-level object
    void WriteObject(const wxObject *object, const wxClassInfo *classInfo, 
                     wxObjectWriterCallback *writercallback, const wxString &name, 
                     const wxStringToAnyHashMap &metadata);

    // Managing the object identity table a.k.a context
    //
    // these methods make sure that no object gets written twice, 
    // because sometimes multiple calls to the WriteObject will be
    // made without wanting to have duplicate objects written, the 
    // object identity table will be reset manually
    virtual void ClearObjectContext();

    // gets the object Id for a passed in object in the context
    int GetObjectID(const wxObject *obj);

    // returns true if this object has already been written in this context
    bool IsObjectKnown( const wxObject *obj );

    //
    // streaming callbacks
    //
    // these callbacks really write out the values in the stream format

    // begins writing out a new toplevel entry which has the indicated unique name
    virtual void DoBeginWriteTopLevelEntry( const wxString &name ) = 0;

    // ends writing out a new toplevel entry which has the indicated unique name
    virtual void DoEndWriteTopLevelEntry( const wxString &name ) = 0;

    // start of writing an object having the passed in ID
    virtual void DoBeginWriteObject(const wxObject *object, const wxClassInfo *classInfo, 
                                    int objectID, const wxStringToAnyHashMap &metadata ) = 0;

    // end of writing an toplevel object name param is used for unique 
    // identification within the container
    virtual void DoEndWriteObject(const wxObject *object, 
                const wxClassInfo *classInfo, int objectID ) = 0;

    // writes a simple property in the stream format
    virtual void DoWriteSimpleType( const wxAny &value ) = 0;

    // start of writing a complex property into the stream (
    virtual void DoBeginWriteProperty( const wxPropertyInfo *propInfo ) = 0;

    // end of writing a complex property into the stream
    virtual void DoEndWriteProperty( const wxPropertyInfo *propInfo ) = 0;

    virtual void DoBeginWriteElement() = 0;
    virtual void DoEndWriteElement() = 0;
    // insert an object reference to an already written object
    virtual void DoWriteRepeatedObject( int objectID ) = 0;

    // insert a null reference
    virtual void DoWriteNullObject() = 0;

    // writes a delegate in the stream format
    virtual void DoWriteDelegate( const wxObject *object,  const wxClassInfo* classInfo, 
                                  const wxPropertyInfo *propInfo, const wxObject *eventSink, 
                                  int sinkObjectID, const wxClassInfo* eventSinkClassInfo,
                                  const wxHandlerInfo* handlerIndo ) = 0;

    void WriteObject(const wxObject *object, const wxClassInfo *classInfo, 
        wxObjectWriterCallback *writercallback, bool isEmbedded, const wxStringToAnyHashMap &metadata );

protected:
    struct wxObjectWriterInternal;
    wxObjectWriterInternal* m_data;

    struct wxObjectWriterInternalPropertiesData;

    void WriteAllProperties( const wxObject * obj, const wxClassInfo* ci, 
                             wxObjectWriterCallback *writercallback, 
                             wxObjectWriterInternalPropertiesData * data );

    void WriteOneProperty( const wxObject *obj, const wxClassInfo* ci, 
                           const wxPropertyInfo* pi, wxObjectWriterCallback *writercallback,
                           wxObjectWriterInternalPropertiesData *data );


    void FindConnectEntry(const wxEvtHandler * evSource,
                          const wxEventSourceTypeInfo* dti, const wxObject* &sink, 
                          const wxHandlerInfo *&handler);
};


/*
Streaming callbacks for depersisting XML to code, or running objects
*/

class WXDLLIMPEXP_BASE wxObjectReaderCallback;

/*
wxObjectReader handles streaming in a class from a arbitrary format. 
While walking through it issues calls out to interfaces to readercallback 
the guts from the underlying storage format.
*/

class WXDLLIMPEXP_BASE wxObjectReader: public wxObject
{
public:
    wxObjectReader();
    virtual ~wxObjectReader();

    // the only thing wxObjectReader knows about is the class info by object ID
    wxClassInfo *GetObjectClassInfo(int objectID);
    bool HasObjectClassInfo( int objectID );
    void SetObjectClassInfo(int objectID, wxClassInfo* classInfo);

    // Reads the component the reader is pointed at from the underlying format.
    // The return value is the root object ID, which can
    // then be used to ask the depersister about that object
    // if there was a problem you will get back wxInvalidObjectID and the current
    // error log will carry the problems encoutered
    virtual int ReadObject( const wxString &name, wxObjectReaderCallback *readercallback ) = 0;

private:
    struct wxObjectReaderInternal;
    wxObjectReaderInternal *m_data;
};

// This abstract class matches the allocate-init/create model of creation of objects.
// At runtime, these will create actual instances, and manipulate them.
// When generating code, these will just create statements of C++
// code to create the objects.

class WXDLLIMPEXP_BASE wxObjectReaderCallback
{
public:
    virtual ~wxObjectReaderCallback() {}

    // allocate the new object on the heap, that object will have the passed in ID
    virtual void AllocateObject(int objectID, wxClassInfo *classInfo, 
                                wxStringToAnyHashMap &metadata) = 0;

    // initialize the already allocated object having the ID objectID with the Create method
    // creation parameters which are objects are having their Ids passed in objectIDValues
    // having objectId <> wxInvalidObjectID

    virtual void CreateObject(int objectID,
        const wxClassInfo *classInfo,
        int paramCount,
        wxAny *VariantValues,
        int *objectIDValues,
        const wxClassInfo **objectClassInfos,
        wxStringToAnyHashMap &metadata) = 0;

    // construct the new object on the heap, that object will have the passed in ID 
    // (for objects that don't support allocate-create type of creation)
    // creation parameters which are objects are having their Ids passed in 
    // objectIDValues having objectId <> wxInvalidObjectID

    virtual void ConstructObject(int objectID,
        const wxClassInfo *classInfo,
        int paramCount,
        wxAny *VariantValues,
        int *objectIDValues,
        const wxClassInfo **objectClassInfos,
        wxStringToAnyHashMap &metadata) = 0;

    // destroy the heap-allocated object having the ID objectID, this may be used 
    // if an object is embedded in another object and set via value semantics, 
    // so the intermediate object can be destroyed after safely
    virtual void DestroyObject(int objectID, wxClassInfo *classInfo) = 0;

    // set the corresponding property
    virtual void SetProperty(int objectID,
        const wxClassInfo *classInfo,
        const wxPropertyInfo* propertyInfo,
        const wxAny &VariantValue) = 0;

    // sets the corresponding property (value is an object)
    virtual void SetPropertyAsObject(int objectID,
        const wxClassInfo *classInfo,
        const wxPropertyInfo* propertyInfo,
        int valueObjectId) = 0;

    // adds an element to a property collection
    virtual void AddToPropertyCollection( int objectID,
        const wxClassInfo *classInfo,
        const wxPropertyInfo* propertyInfo,
        const wxAny &VariantValue) = 0;

    // sets the corresponding property (value is an object)
    virtual void AddToPropertyCollectionAsObject(int objectID,
        const wxClassInfo *classInfo,
        const wxPropertyInfo* propertyInfo,
        int valueObjectId) = 0;

    // sets the corresponding event handler
    virtual void SetConnect(int EventSourceObjectID,
        const wxClassInfo *EventSourceClassInfo,
        const wxPropertyInfo *delegateInfo,
        const wxClassInfo *EventSinkClassInfo,
        const wxHandlerInfo* handlerInfo,
        int EventSinkObjectID ) = 0;
};

/*
wxObjectRuntimeReaderCallback implements the callbacks that will bring back
an object into a life memory instance
*/

class WXDLLIMPEXP_BASE wxObjectRuntimeReaderCallback: public wxObjectReaderCallback
{
    struct wxObjectRuntimeReaderCallbackInternal;
    wxObjectRuntimeReaderCallbackInternal * m_data;

public:
    wxObjectRuntimeReaderCallback();
    virtual ~wxObjectRuntimeReaderCallback();

    // returns the object having the corresponding ID fully constructed
    wxObject *GetObject(int objectID);

    // allocate the new object on the heap, that object will have the passed in ID
    virtual void AllocateObject(int objectID, wxClassInfo *classInfo,
        wxStringToAnyHashMap &metadata);

    // initialize the already allocated object having the ID objectID with 
    // the Create method creation parameters which are objects are having 
    // their Ids passed in objectIDValues having objectId <> wxInvalidObjectID

    virtual void CreateObject(int objectID,
        const wxClassInfo *classInfo,
        int paramCount,
        wxAny *VariantValues,
        int *objectIDValues,
        const wxClassInfo **objectClassInfos,
        wxStringToAnyHashMap &metadata
        );

    // construct the new object on the heap, that object will have the 
    // passed in ID (for objects that don't support allocate-create type of 
    // creation) creation parameters which are objects are having their Ids 
    // passed in objectIDValues having objectId <> wxInvalidObjectID

    virtual void ConstructObject(int objectID,
        const wxClassInfo *classInfo,
        int paramCount,
        wxAny *VariantValues,
        int *objectIDValues,
        const wxClassInfo **objectClassInfos,
        wxStringToAnyHashMap &metadata);

    // destroy the heap-allocated object having the ID objectID, this may be 
    // used if an object is embedded in another object and set via value semantics, 
    // so the intermediate object can be destroyed after safely
    virtual void DestroyObject(int objectID, wxClassInfo *classInfo);

    // set the corresponding property
    virtual void SetProperty(int objectID,
        const wxClassInfo *classInfo,
        const wxPropertyInfo* propertyInfo,
        const wxAny &variantValue);

    // sets the corresponding property (value is an object)
    virtual void SetPropertyAsObject(int objectId,
        const wxClassInfo *classInfo,
        const wxPropertyInfo* propertyInfo,
        int valueObjectId);

    // adds an element to a property collection
    virtual void AddToPropertyCollection( int objectID,
        const wxClassInfo *classInfo,
        const wxPropertyInfo* propertyInfo,
        const wxAny &VariantValue);

    // sets the corresponding property (value is an object)
    virtual void AddToPropertyCollectionAsObject(int objectID,
        const wxClassInfo *classInfo,
        const wxPropertyInfo* propertyInfo,
        int valueObjectId);

    // sets the corresponding event handler
    virtual void SetConnect(int eventSourceObjectID,
        const wxClassInfo *eventSourceClassInfo,
        const wxPropertyInfo *delegateInfo,
        const wxClassInfo *eventSinkClassInfo,
        const wxHandlerInfo* handlerInfo,
        int eventSinkObjectID );
};

#endif // wxUSE_EXTENDED_RTTI

#endif
