/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/hash.cpp
// Purpose:     wxHashTable implementation
// Author:      Julian Smart
// Modified by: VZ at 25.02.00: type safe hashes with WX_DECLARE_HASH()
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/hash.h"
    #include "wx/object.h"
#endif

wxHashTableBase_Node::wxHashTableBase_Node( long key, void* value,
                                            wxHashTableBase* table )
    : m_value( value ), m_hashPtr( table )
{
    m_key.integer = key;
}

wxHashTableBase_Node::wxHashTableBase_Node( const wxString& key, void* value,
                                            wxHashTableBase* table )
    : m_value( value ), m_hashPtr( table )
{
    m_key.string = new wxString(key);
}

wxHashTableBase_Node::~wxHashTableBase_Node()
{
    if( m_hashPtr ) m_hashPtr->DoRemoveNode( this );
}

//

wxHashTableBase::wxHashTableBase()
    : m_size( 0 ), m_count( 0 ), m_table( NULL ), m_keyType( wxKEY_NONE ),
      m_deleteContents( false )
{
}

void wxHashTableBase::Create( wxKeyType keyType, size_t size )
{
    m_keyType = keyType;
    m_size = size;
    m_table = new wxHashTableBase_Node*[ m_size ];

    for( size_t i = 0; i < m_size; ++i )
        m_table[i] = NULL;
}

void wxHashTableBase::Clear()
{
    for( size_t i = 0; i < m_size; ++i )
    {
        Node* end = m_table[i];

        if( end == NULL )
            continue;

        Node *curr, *next = end->GetNext();

        do
        {
            curr = next;
            next = curr->GetNext();

            DoDestroyNode( curr );

            delete curr;
        }
        while( curr != end );

        m_table[i] = NULL;
    }

    m_count = 0;
}

void wxHashTableBase::DoRemoveNode( wxHashTableBase_Node* node )
{
    size_t bucket = ( m_keyType == wxKEY_INTEGER ?
                      node->m_key.integer        :
                      MakeKey( *node->m_key.string ) ) % m_size;

    if( node->GetNext() == node )
    {
        // single-node chain (common case)
        m_table[bucket] = NULL;
    }
    else
    {
        Node *start = m_table[bucket], *curr;
        Node* prev = start;

        for( curr = prev->GetNext(); curr != node;
             prev = curr, curr = curr->GetNext() ) ;

        DoUnlinkNode( bucket, node, prev );
    }

    DoDestroyNode( node );
}

void wxHashTableBase::DoDestroyNode( wxHashTableBase_Node* node )
{
    // if it is called from DoRemoveNode, node has already been
    // removed, from other places it does not matter
    node->m_hashPtr = NULL;

    if( m_keyType == wxKEY_STRING )
        delete node->m_key.string;
    if( m_deleteContents )
        DoDeleteContents( node );
}

void wxHashTableBase::Destroy()
{
    Clear();

    wxDELETEA(m_table);
    m_size = 0;
}

void wxHashTableBase::DoInsertNode( size_t bucket, wxHashTableBase_Node* node )
{
    if( m_table[bucket] == NULL )
    {
        m_table[bucket] = node->m_next = node;
    }
    else
    {
        Node *prev = m_table[bucket];
        Node *next = prev->m_next;

        prev->m_next = node;
        node->m_next = next;
        m_table[bucket] = node;
    }

    ++m_count;
}

void wxHashTableBase::DoPut( long key, long hash, void* data )
{
    wxASSERT( m_keyType == wxKEY_INTEGER );

    size_t bucket = size_t(hash) % m_size;
    Node* node = new wxHashTableBase_Node( key, data, this );

    DoInsertNode( bucket, node );
}

void wxHashTableBase::DoPut( const wxString& key, long hash, void* data )
{
    wxASSERT( m_keyType == wxKEY_STRING );

    size_t bucket = size_t(hash) % m_size;
    Node* node = new wxHashTableBase_Node( key, data, this );

    DoInsertNode( bucket, node );
}

void* wxHashTableBase::DoGet( long key, long hash ) const
{
    wxASSERT( m_keyType == wxKEY_INTEGER );

    size_t bucket = size_t(hash) % m_size;

    if( m_table[bucket] == NULL )
        return NULL;

    Node *first = m_table[bucket]->GetNext(),
         *curr = first;

    do
    {
        if( curr->m_key.integer == key )
            return curr->m_value;

        curr = curr->GetNext();
    }
    while( curr != first );

    return NULL;
}

void* wxHashTableBase::DoGet( const wxString& key, long hash ) const
{
    wxASSERT( m_keyType == wxKEY_STRING );

    size_t bucket = size_t(hash) % m_size;

    if( m_table[bucket] == NULL )
        return NULL;

    Node *first = m_table[bucket]->GetNext(),
         *curr = first;

    do
    {
        if( *curr->m_key.string == key )
            return curr->m_value;

        curr = curr->GetNext();
    }
    while( curr != first );

    return NULL;
}

void wxHashTableBase::DoUnlinkNode( size_t bucket, wxHashTableBase_Node* node,
                                    wxHashTableBase_Node* prev )
{
    if( node == m_table[bucket] )
        m_table[bucket] = prev;

    if( prev == node && prev == node->GetNext() )
        m_table[bucket] = NULL;
    else
        prev->m_next = node->m_next;

    DoDestroyNode( node );
    --m_count;
}

void* wxHashTableBase::DoDelete( long key, long hash )
{
    wxASSERT( m_keyType == wxKEY_INTEGER );

    size_t bucket = size_t(hash) % m_size;

    if( m_table[bucket] == NULL )
        return NULL;

    Node *first = m_table[bucket]->GetNext(),
         *curr = first,
         *prev = m_table[bucket];

    do
    {
        if( curr->m_key.integer == key )
        {
            void* retval = curr->m_value;
            curr->m_value = NULL;

            DoUnlinkNode( bucket, curr, prev );
            delete curr;

            return retval;
        }

        prev = curr;
        curr = curr->GetNext();
    }
    while( curr != first );

    return NULL;
}

void* wxHashTableBase::DoDelete( const wxString& key, long hash )
{
    wxASSERT( m_keyType == wxKEY_STRING );

    size_t bucket = size_t(hash) % m_size;

    if( m_table[bucket] == NULL )
        return NULL;

    Node *first = m_table[bucket]->GetNext(),
         *curr = first,
         *prev = m_table[bucket];

    do
    {
        if( *curr->m_key.string == key )
        {
            void* retval = curr->m_value;
            curr->m_value = NULL;

            DoUnlinkNode( bucket, curr, prev );
            delete curr;

            return retval;
        }

        prev = curr;
        curr = curr->GetNext();
    }
    while( curr != first );

    return NULL;
}

long wxHashTableBase::MakeKey( const wxString& str )
{
    long int_key = 0;

    const wxStringCharType *p = str.wx_str();
    while( *p )
        int_key += *p++;

    return int_key;
}

// ----------------------------------------------------------------------------
// wxHashTable
// ----------------------------------------------------------------------------

wxHashTable::wxHashTable( const wxHashTable& table )
           : wxHashTableBase()
{
    DoCopy( table );
}

const wxHashTable& wxHashTable::operator=( const wxHashTable& table )
{
    Destroy();
    DoCopy( table );

    return *this;
}

void wxHashTable::DoCopy( const wxHashTable& WXUNUSED(table) )
{
    Create( m_keyType, m_size );

    wxFAIL;
}

void wxHashTable::DoDeleteContents( wxHashTableBase_Node* node )
{
    delete ((wxHashTable_Node*)node)->GetData();
}

void wxHashTable::GetNextNode( size_t bucketStart )
{
    for( size_t i = bucketStart; i < m_size; ++i )
    {
        if( m_table[i] != NULL )
        {
            m_curr = ((Node*)m_table[i])->GetNext();
            m_currBucket = i;
            return;
        }
    }

    m_curr = NULL;
    m_currBucket = 0;
}

wxHashTable::Node* wxHashTable::Next()
{
    if( m_curr == NULL )
        GetNextNode( 0 );
    else
    {
        m_curr = m_curr->GetNext();

        if( m_curr == ( (Node*)m_table[m_currBucket] )->GetNext() )
            GetNextNode( m_currBucket + 1 );
    }

    return m_curr;
}

