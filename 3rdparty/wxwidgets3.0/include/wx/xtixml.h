/////////////////////////////////////////////////////////////////////////////
// Name:        wx/xtixml.h
// Purpose:     xml streaming runtime metadata information (extended class info)
// Author:      Stefan Csomor
// Modified by:
// Created:     27/07/03
// Copyright:   (c) 2003 Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_XTIXMLH__
#define _WX_XTIXMLH__

#include "wx/defs.h"

#if wxUSE_EXTENDED_RTTI

#include "wx/string.h"
#include "wx/xtistrm.h"

/*
class WXDLLIMPEXP_XML wxXmlNode;
class WXDLLIMPEXP_BASE wxPropertyInfo;
class WXDLLIMPEXP_BASE wxObject;
class WXDLLIMPEXP_BASE wxClassInfo;
class WXDLLIMPEXP_BASE wxAnyList;
class WXDLLIMPEXP_BASE wxHandlerInfo;
class WXDLLIMPEXP_BASE wxObjectWriterCallback;
*/

class WXDLLIMPEXP_XML wxObjectXmlWriter: public wxObjectWriter
{
public:

    wxObjectXmlWriter( wxXmlNode * parent );
    virtual ~wxObjectXmlWriter();

    //
    // streaming callbacks
    //
    // these callbacks really write out the values in the stream format
    //

    //
    // streaming callbacks
    //
    // these callbacks really write out the values in the stream format

    // begins writing out a new toplevel entry which has the indicated unique name
    virtual void DoBeginWriteTopLevelEntry( const wxString &name );

    // ends writing out a new toplevel entry which has the indicated unique name
    virtual void DoEndWriteTopLevelEntry( const wxString &name );

    // start of writing an object having the passed in ID
    virtual void DoBeginWriteObject(const wxObject *object, 
        const wxClassInfo *classInfo, int objectID, const wxStringToAnyHashMap &metadata );

    // end of writing an toplevel object name param is used for unique 
    // identification within the container
    virtual void DoEndWriteObject(const wxObject *object, 
        const wxClassInfo *classInfo, int objectID );

    // writes a simple property in the stream format
    virtual void DoWriteSimpleType( const wxAny &value );

    // start of writing a complex property into the stream (
    virtual void DoBeginWriteProperty( const wxPropertyInfo *propInfo );

    // end of writing a complex property into the stream
    virtual void DoEndWriteProperty( const wxPropertyInfo *propInfo );

    virtual void DoBeginWriteElement();
    virtual void DoEndWriteElement();

    // insert an object reference to an already written object
    virtual void DoWriteRepeatedObject( int objectID );

    // insert a null reference
    virtual void DoWriteNullObject();

    // writes a delegate in the stream format
    virtual void DoWriteDelegate( const wxObject *object,  
        const wxClassInfo* classInfo, const wxPropertyInfo *propInfo,
        const wxObject *eventSink, int sinkObjectID, 
        const wxClassInfo* eventSinkClassInfo, const wxHandlerInfo* handlerIndo );

private:
    struct wxObjectXmlWriterInternal;
    wxObjectXmlWriterInternal* m_data;
};

/*
wxObjectXmlReader handles streaming in a class from XML
*/

class WXDLLIMPEXP_XML wxObjectXmlReader: public wxObjectReader
{
public:
    wxObjectXmlReader(wxXmlNode *parent) { m_parent = parent; }
    virtual ~wxObjectXmlReader() {}

    // Reads a component from XML.  The return value is the root object ID, which can
    // then be used to ask the readercallback about that object

    virtual int ReadObject( const wxString &name, wxObjectReaderCallback *readercallback );

private:
    int ReadComponent(wxXmlNode *parent, wxObjectReaderCallback *callbacks);

    // read the content of this node (simple type) and return the corresponding value
    wxAny ReadValue(wxXmlNode *Node, const wxTypeInfo *type );

    wxXmlNode * m_parent;
};

#endif // wxUSE_EXTENDED_RTTI

#endif
