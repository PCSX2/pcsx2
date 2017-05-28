/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/xtixml.cpp
// Purpose:     streaming runtime metadata information
// Author:      Stefan Csomor
// Modified by:
// Created:     27/07/03
// Copyright:   (c) 2003 Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#if wxUSE_EXTENDED_RTTI

#include "wx/xtistrm.h"

#ifndef WX_PRECOMP
    #include "wx/object.h"
    #include "wx/hash.h"
    #include "wx/event.h"
#endif

#include "wx/xml/xml.h"
#include "wx/tokenzr.h"
#include "wx/txtstrm.h"
#include "wx/xtistrm.h"
#include "wx/xtixml.h"

// STL headers

#include "wx/beforestd.h"
#include <map>
#include <vector>
#include <string>
#include "wx/afterstd.h"

using namespace std;




// ----------------------------------------------------------------------------
// wxObjectXmlWriter
// ----------------------------------------------------------------------------

// convenience functions

void wxXmlAddContentToNode( wxXmlNode* node, const wxString& data )
{
    node->AddChild(new wxXmlNode(wxXML_TEXT_NODE, wxT("value"), data ) );
}

wxString wxXmlGetContentFromNode( wxXmlNode *node )
{
    if ( node->GetChildren() )
        return node->GetChildren()->GetContent();
    else
        return wxEmptyString;
}

struct wxObjectXmlWriter::wxObjectXmlWriterInternal
{
    wxXmlNode *m_root;
    wxXmlNode *m_current;
    vector< wxXmlNode * > m_objectStack;

    void Push( wxXmlNode *newCurrent )
    {
        m_objectStack.push_back( m_current );
        m_current = newCurrent;
    }

    void Pop()
    {
        m_current = m_objectStack.back();
        m_objectStack.pop_back();
    }
};

wxObjectXmlWriter::wxObjectXmlWriter( wxXmlNode * rootnode )
{
    m_data = new wxObjectXmlWriterInternal();
    m_data->m_root = rootnode;
    m_data->m_current = rootnode;
}

wxObjectXmlWriter::~wxObjectXmlWriter()
{
    delete m_data;
}

void wxObjectXmlWriter::DoBeginWriteTopLevelEntry( const wxString &name )
{
    wxXmlNode *pnode;
    pnode = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("entry"));
    pnode->AddProperty(wxString(wxT("name")), name);
    m_data->m_current->AddChild(pnode);
    m_data->Push( pnode );
}

void wxObjectXmlWriter::DoEndWriteTopLevelEntry( const wxString &WXUNUSED(name) )
{
    m_data->Pop();
}

void wxObjectXmlWriter::DoBeginWriteObject(const wxObject *WXUNUSED(object), 
                                     const wxClassInfo *classInfo, 
                                     int objectID, const wxStringToAnyHashMap &metadata   )
{
    wxXmlNode *pnode;
    pnode = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("object"));
    pnode->AddProperty(wxT("class"), wxString(classInfo->GetClassName()));
    pnode->AddProperty(wxT("id"), wxString::Format( wxT("%d"), objectID ) );

    wxStringToAnyHashMap::const_iterator it, en;
    for( it = metadata.begin(), en = metadata.end(); it != en; ++it )
    {
        pnode->AddProperty( it->first, wxAnyGetAsString(it->second) );
    }

    m_data->m_current->AddChild(pnode);
    m_data->Push( pnode );
}

void wxObjectXmlWriter::DoEndWriteObject(const wxObject *WXUNUSED(object), 
                                   const wxClassInfo *WXUNUSED(classInfo), 
                                   int WXUNUSED(objectID) )
{
    m_data->Pop();
}

void wxObjectXmlWriter::DoWriteSimpleType( const wxAny &value )
{
    wxXmlAddContentToNode( m_data->m_current,wxAnyGetAsString(value) );
}

void wxObjectXmlWriter::DoBeginWriteElement()
{
    wxXmlNode *pnode;
    pnode = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("element") );
    m_data->m_current->AddChild(pnode);
    m_data->Push( pnode );
}

void wxObjectXmlWriter::DoEndWriteElement()
{
    m_data->Pop();
}

void wxObjectXmlWriter::DoBeginWriteProperty(const wxPropertyInfo *pi )
{
    wxXmlNode *pnode;
    pnode = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("prop") );
    pnode->AddProperty(wxT("name"), pi->GetName() );
    m_data->m_current->AddChild(pnode);
    m_data->Push( pnode );
}

void wxObjectXmlWriter::DoEndWriteProperty(const wxPropertyInfo *WXUNUSED(propInfo) )
{
    m_data->Pop();
}

void wxObjectXmlWriter::DoWriteRepeatedObject( int objectID )
{
    wxXmlNode *pnode;
    pnode = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("object"));
    pnode->AddProperty(wxString(wxT("href")), wxString::Format( wxT("%d"), objectID ) );
    m_data->m_current->AddChild(pnode);
}

void wxObjectXmlWriter::DoWriteNullObject()
{
    wxXmlNode *pnode;
    pnode = new wxXmlNode(wxXML_ELEMENT_NODE, wxT("object"));
    m_data->m_current->AddChild(pnode);
}

void wxObjectXmlWriter::DoWriteDelegate( const wxObject *WXUNUSED(object), 
                                   const wxClassInfo* WXUNUSED(classInfo), 
                                   const wxPropertyInfo *WXUNUSED(pi),
                                   const wxObject *eventSink, int sinkObjectID, 
                                   const wxClassInfo* WXUNUSED(eventSinkClassInfo), 
                                   const wxHandlerInfo* handlerInfo )
{
    if ( eventSink != NULL && handlerInfo != NULL )
    {
        wxXmlAddContentToNode( m_data->m_current,
            wxString::Format(wxT("%d.%s"), sinkObjectID, handlerInfo->GetName().c_str()) );
    }
}


// ----------------------------------------------------------------------------
// wxObjectXmlReader
// ----------------------------------------------------------------------------

/*
Reading components has not to be extended for components
as properties are always sought by typeinfo over all levels
and create params are always toplevel class only
*/

int wxObjectXmlReader::ReadComponent(wxXmlNode *node, wxObjectReaderCallback *callbacks)
{
    wxASSERT_MSG( callbacks, wxT("Does not support reading without a Depersistor") );
    wxString className;
    wxClassInfo *classInfo;

    wxAny *createParams;
    int *createParamOids;
    const wxClassInfo** createClassInfos;
    wxXmlNode *children;
    int objectID;
    wxString ObjectIdString;

    children = node->GetChildren();
    if (!children)
    {
        // check for a null object or href
        if (node->GetAttribute(wxT("href"), &ObjectIdString ) )
        {
            objectID = atoi( ObjectIdString.ToAscii() );
            if ( HasObjectClassInfo( objectID ) )
            {
                return objectID;
            }
            else
            {
                wxLogError( _("Forward hrefs are not supported") );
                return wxInvalidObjectID;
            }
        }
        if ( !node->GetAttribute(wxT("id"), &ObjectIdString ) )
        {
            return wxNullObjectID;
        }
    }
    if (!node->GetAttribute(wxT("class"), &className))
    {
        // No class name.  Eek. FIXME: error handling
        return wxInvalidObjectID;
    }

    classInfo = wxClassInfo::FindClass(className);
    if ( classInfo == NULL )
    {
        wxLogError( wxString::Format(_("unknown class %s"),className ) );
        return wxInvalidObjectID;
    }

    if ( children != NULL && children->GetType() == wxXML_TEXT_NODE )
    {
        wxLogError(_("objects cannot have XML Text Nodes") );
        return wxInvalidObjectID;
    }
    if (!node->GetAttribute(wxT("id"), &ObjectIdString))
    {
        wxLogError(_("Objects must have an id attribute") );
        // No object id.  Eek. FIXME: error handling
        return wxInvalidObjectID;
    }
    objectID = atoi( ObjectIdString.ToAscii() );

    // is this object already has been streamed in, return it here
    if ( HasObjectClassInfo( objectID ) )
    {
        wxLogError ( wxString::Format(_("Doubly used id : %d"), objectID ) );
        return wxInvalidObjectID;
    }

    // new object, start with allocation
    // first make the object know to our internal registry
    SetObjectClassInfo( objectID, classInfo );

    wxStringToAnyHashMap metadata;
    wxXmlProperty *xp = node->GetAttributes();
    while ( xp )
    {
        if ( xp->GetName() != wxString(wxT("class")) && 
             xp->GetName() != wxString(wxT("id")) )
        {
            metadata[xp->GetName()] = wxAny( xp->GetValue() );
        }
        xp = xp->GetNext();
    }
    if ( !classInfo->NeedsDirectConstruction() )
        callbacks->AllocateObject(objectID, classInfo, metadata);

    //
    // stream back the Create parameters first
    createParams = new wxAny[ classInfo->GetCreateParamCount() ];
    createParamOids = new int[classInfo->GetCreateParamCount() ];
    createClassInfos = new const wxClassInfo*[classInfo->GetCreateParamCount() ];

#if wxUSE_UNICODE
    typedef map<wstring, wxXmlNode *> PropertyNodes;
    typedef vector<wstring> PropertyNames;
#else
    typedef map<string, wxXmlNode *> PropertyNodes;
    typedef vector<string> PropertyNames;
#endif
    PropertyNodes propertyNodes;
    PropertyNames propertyNames;

    while( children )
    {
        wxString name;
        children->GetAttribute( wxT("name"), &name );
        propertyNames.push_back( (const wxChar*)name.c_str() );
        propertyNodes[(const wxChar*)name.c_str()] = children->GetChildren();
        children = children->GetNext();
    }

    for ( int i = 0; i <classInfo->GetCreateParamCount(); ++i )
    {
        const wxChar* paramName = classInfo->GetCreateParamName(i);
        PropertyNodes::iterator propiter = propertyNodes.find( paramName );
        const wxPropertyInfo* pi = classInfo->FindPropertyInfo( paramName );
        if ( pi == 0 )
        {
            wxLogError( wxString::Format(_("Unknown Property %s"),paramName) );
        }
        // if we don't have the value of a create param set in the xml
        // we use the default value
        if ( propiter != propertyNodes.end() )
        {
            wxXmlNode* prop = propiter->second;
            if ( pi->GetTypeInfo()->IsObjectType() )
            {
                createParamOids[i] = ReadComponent( prop, callbacks );
                createClassInfos[i] = 
                    wx_dynamic_cast(const wxClassTypeInfo*, pi->GetTypeInfo())->GetClassInfo();
            }
            else
            {
                createParamOids[i] = wxInvalidObjectID;
                createParams[i] = ReadValue( prop, pi->GetTypeInfo() );
                if( pi->GetFlags() & wxPROP_ENUM_STORE_LONG )
                {
                    const wxEnumTypeInfo *eti = 
                        wx_dynamic_cast(const wxEnumTypeInfo*, pi->GetTypeInfo() );
                    if ( eti )
                    {
                        long realval;
                        eti->ConvertToLong( createParams[i], realval );
                        createParams[i] = wxAny( realval );
                    }
                    else
                    {
                        wxLogError( _("Type must have enum - long conversion") );
                    }
                }
                createClassInfos[i] = NULL;
            }

            for ( size_t j = 0; j < propertyNames.size(); ++j )
            {
                if ( propertyNames[j] == paramName )
                {
                    propertyNames[j] = wxEmptyString;
                    break;
                }
            }
        }
        else
        {
            if ( pi->GetTypeInfo()->IsObjectType() )
            {
                createParamOids[i] = wxNullObjectID;
                createClassInfos[i] = 
                    wx_dynamic_cast(const wxClassTypeInfo*, pi->GetTypeInfo())->GetClassInfo();
            }
            else
            {
                createParams[i] = pi->GetDefaultValue();
                createParamOids[i] = wxInvalidObjectID;
            }
        }
    }

    // got the parameters.  Call the Create method
    if ( classInfo->NeedsDirectConstruction() )
        callbacks->ConstructObject(objectID, classInfo,
            classInfo->GetCreateParamCount(),
            createParams, createParamOids, createClassInfos, metadata );
    else
        callbacks->CreateObject(objectID, classInfo,
            classInfo->GetCreateParamCount(),
            createParams, createParamOids, createClassInfos, metadata );

    // now stream in the rest of the properties, in the sequence their 
    // properties were written in the xml
    for ( size_t j = 0; j < propertyNames.size(); ++j )
    {
        if ( !propertyNames[j].empty() )
        {
            PropertyNodes::iterator propiter = propertyNodes.find( propertyNames[j] );
            if ( propiter != propertyNodes.end() )
            {
                wxXmlNode* prop = propiter->second;
                const wxPropertyInfo* pi = 
                    classInfo->FindPropertyInfo( propertyNames[j].c_str() );
                if ( pi->GetTypeInfo()->GetKind() == wxT_COLLECTION )
                {
                    const wxCollectionTypeInfo* collType = 
                        wx_dynamic_cast( const wxCollectionTypeInfo*, pi->GetTypeInfo() );
                    const wxTypeInfo * elementType = collType->GetElementType();
                    while( prop )
                    {
                        if ( prop->GetName() != wxT("element") )
                        {
                            wxLogError( _("A non empty collection must consist of 'element' nodes" ) );
                            break;
                        }

                        wxXmlNode* elementContent = prop->GetChildren();
                        if ( elementContent )
                        {
                            // we skip empty elements
                            if ( elementType->IsObjectType() )
                            {
                                int valueId = ReadComponent( elementContent, callbacks );
                                if ( valueId != wxInvalidObjectID )
                                {
                                    if ( pi->GetAccessor()->HasAdder() )
                                        callbacks->AddToPropertyCollectionAsObject( objectID, classInfo, pi, valueId );
                                    // TODO for collections we must have a notation on taking over ownership or not
                                    if ( elementType->GetKind() == wxT_OBJECT && valueId != wxNullObjectID )
                                        callbacks->DestroyObject( valueId, GetObjectClassInfo( valueId ) );
                                }
                            }
                            else
                            {
                                wxAny elementValue = ReadValue( elementContent, elementType );
                                if ( pi->GetAccessor()->HasAdder() )
                                    callbacks->AddToPropertyCollection( objectID, classInfo,pi, elementValue );
                            }
                        }
                        prop = prop->GetNext();
                    }
                }
                else if ( pi->GetTypeInfo()->IsObjectType() )
                {
                    // and object can either be streamed out a string or as an object
                    // in case we have no node, then the object's streaming out has been vetoed
                    if ( prop )
                    {
                        if ( prop->GetName() == wxT("object") )
                        {
                            int valueId = ReadComponent( prop, callbacks );
                            if ( valueId != wxInvalidObjectID )
                            {
                                callbacks->SetPropertyAsObject( objectID, classInfo, pi, valueId );
                                if ( pi->GetTypeInfo()->GetKind() == wxT_OBJECT && valueId != wxNullObjectID )
                                    callbacks->DestroyObject( valueId, GetObjectClassInfo( valueId ) );
                            }
                        }
                        else
                        {
                            wxASSERT( pi->GetTypeInfo()->HasStringConverters() );
                            wxAny nodeval = ReadValue( prop, pi->GetTypeInfo() );
                            callbacks->SetProperty( objectID, classInfo,pi, nodeval );
                        }
                    }
                }
                else if ( pi->GetTypeInfo()->IsDelegateType() )
                {
                    if ( prop )
                    {
                        wxString resstring = prop->GetContent();
                        wxInt32 pos = resstring.Find('.');
                        if ( pos != wxNOT_FOUND )
                        {
                            int sinkOid = atol(resstring.Left(pos).ToAscii());
                            wxString handlerName = resstring.Mid(pos+1);
                            wxClassInfo* sinkClassInfo = GetObjectClassInfo( sinkOid );

                            callbacks->SetConnect( objectID, classInfo, pi, sinkClassInfo,
                                sinkClassInfo->FindHandlerInfo(handlerName.c_str()),  sinkOid );
                        }
                        else
                        {
                            wxLogError( _("incorrect event handler string, missing dot") );
                        }
                    }

                }
                else
                {
                    wxAny nodeval = ReadValue( prop, pi->GetTypeInfo() );
                    if( pi->GetFlags() & wxPROP_ENUM_STORE_LONG )
                    {
                        const wxEnumTypeInfo *eti = 
                            wx_dynamic_cast(const wxEnumTypeInfo*, pi->GetTypeInfo() );
                        if ( eti )
                        {
                            long realval;
                            eti->ConvertToLong( nodeval, realval );
                            nodeval = wxAny( realval );
                        }
                        else
                        {
                            wxLogError( _("Type must have enum - long conversion") );
                        }
                    }
                    callbacks->SetProperty( objectID, classInfo,pi, nodeval );
                }
            }
        }
    }

    delete[] createParams;
    delete[] createParamOids;
    delete[] createClassInfos;

    return objectID;
}

wxAny wxObjectXmlReader::ReadValue(wxXmlNode *node,
                                  const wxTypeInfo *type )
{
    wxString content;
    if ( node )
        content = node->GetContent();
    wxAny result;
    type->ConvertFromString( content, result );
    return result;
}

int wxObjectXmlReader::ReadObject( const wxString &name, wxObjectReaderCallback *callbacks)
{
    wxXmlNode *iter = m_parent->GetChildren();
    while ( iter )
    {
        wxString entryName;
        if ( iter->GetAttribute(wxT("name"), &entryName) )
        {
            if ( entryName == name )
                return ReadComponent( iter->GetChildren(), callbacks );
        }
        iter = iter->GetNext();
    }
    return wxInvalidObjectID;
}

#endif // wxUSE_EXTENDED_RTTI
