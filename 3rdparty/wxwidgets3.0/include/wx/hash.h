/////////////////////////////////////////////////////////////////////////////
// Name:        wx/hash.h
// Purpose:     wxHashTable class
// Author:      Julian Smart
// Modified by: VZ at 25.02.00: type safe hashes with WX_DECLARE_HASH()
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_HASH_H__
#define _WX_HASH_H__

#include "wx/defs.h"
#include "wx/string.h"

#if !wxUSE_STD_CONTAINERS
    #include "wx/object.h"
#else
    class WXDLLIMPEXP_FWD_BASE wxObject;
#endif

// the default size of the hash
#define wxHASH_SIZE_DEFAULT     (1000)

/*
 * A hash table is an array of user-definable size with lists
 * of data items hanging off the array positions.  Usually there'll
 * be a hit, so no search is required; otherwise we'll have to run down
 * the list to find the desired item.
*/

union wxHashKeyValue
{
    long integer;
    wxString *string;
};

// for some compilers (AIX xlC), defining it as friend inside the class is not
// enough, so provide a real forward declaration
class WXDLLIMPEXP_FWD_BASE wxHashTableBase;

class WXDLLIMPEXP_BASE wxHashTableBase_Node
{
    friend class WXDLLIMPEXP_FWD_BASE wxHashTableBase;
    typedef class WXDLLIMPEXP_FWD_BASE wxHashTableBase_Node _Node;
public:
    wxHashTableBase_Node( long key, void* value,
                          wxHashTableBase* table );
    wxHashTableBase_Node( const wxString&  key, void* value,
                          wxHashTableBase* table );
    ~wxHashTableBase_Node();

    long GetKeyInteger() const { return m_key.integer; }
    const wxString& GetKeyString() const { return *m_key.string; }

    void* GetData() const { return m_value; }
    void SetData( void* data ) { m_value = data; }

protected:
    _Node* GetNext() const { return m_next; }

protected:
    // next node in the chain
    wxHashTableBase_Node* m_next;

    // key
    wxHashKeyValue m_key;

    // value
    void* m_value;

    // pointer to the hash containing the node, used to remove the
    // node from the hash when the user deletes the node iterating
    // through it
    // TODO: move it to wxHashTable_Node (only wxHashTable supports
    //       iteration)
    wxHashTableBase* m_hashPtr;
};

class WXDLLIMPEXP_BASE wxHashTableBase
#if !wxUSE_STD_CONTAINERS
    : public wxObject
#endif
{
    friend class WXDLLIMPEXP_FWD_BASE wxHashTableBase_Node;
public:
    typedef wxHashTableBase_Node Node;

    wxHashTableBase();
    virtual ~wxHashTableBase() { }

    void Create( wxKeyType keyType = wxKEY_INTEGER,
                 size_t size = wxHASH_SIZE_DEFAULT );
    void Clear();
    void Destroy();

    size_t GetSize() const { return m_size; }
    size_t GetCount() const { return m_count; }

    void DeleteContents( bool flag ) { m_deleteContents = flag; }

    static long MakeKey(const wxString& string);

protected:
    void DoPut( long key, long hash, void* data );
    void DoPut( const wxString&  key, long hash, void* data );
    void* DoGet( long key, long hash ) const;
    void* DoGet( const wxString&  key, long hash ) const;
    void* DoDelete( long key, long hash );
    void* DoDelete( const wxString&  key, long hash );

private:
    // Remove the node from the hash, *only called from
    // ~wxHashTable*_Node destructor
    void DoRemoveNode( wxHashTableBase_Node* node );

    // destroys data contained in the node if appropriate:
    // deletes the key if it is a string and destrys
    // the value if m_deleteContents is true
    void DoDestroyNode( wxHashTableBase_Node* node );

    // inserts a node in the table (at the end of the chain)
    void DoInsertNode( size_t bucket, wxHashTableBase_Node* node );

    // removes a node from the table (fiven a pointer to the previous
    // but does not delete it (only deletes its contents)
    void DoUnlinkNode( size_t bucket, wxHashTableBase_Node* node,
                       wxHashTableBase_Node* prev );

    // unconditionally deletes node value (invoking the
    // correct destructor)
    virtual void DoDeleteContents( wxHashTableBase_Node* node ) = 0;

protected:
    // number of buckets
    size_t m_size;

    // number of nodes (key/value pairs)
    size_t m_count;

    // table
    Node** m_table;

    // key typ (INTEGER/STRING)
    wxKeyType m_keyType;

    // delete contents when hash is cleared
    bool m_deleteContents;

private:
    wxDECLARE_NO_COPY_CLASS(wxHashTableBase);
};

// ----------------------------------------------------------------------------
// for compatibility only
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxHashTable_Node : public wxHashTableBase_Node
{
    friend class WXDLLIMPEXP_FWD_BASE wxHashTable;
public:
    wxHashTable_Node( long key, void* value,
                      wxHashTableBase* table )
        : wxHashTableBase_Node( key, value, table ) { }
    wxHashTable_Node( const wxString&  key, void* value,
                      wxHashTableBase* table )
        : wxHashTableBase_Node( key, value, table ) { }

    wxObject* GetData() const
        { return (wxObject*)wxHashTableBase_Node::GetData(); }
    void SetData( wxObject* data )
        { wxHashTableBase_Node::SetData( data ); }

    wxHashTable_Node* GetNext() const
        { return (wxHashTable_Node*)wxHashTableBase_Node::GetNext(); }
};

// should inherit protectedly, but it is public for compatibility in
// order to publicly inherit from wxObject
class WXDLLIMPEXP_BASE wxHashTable : public wxHashTableBase
{
    typedef wxHashTableBase hash;
public:
    typedef wxHashTable_Node Node;
    typedef wxHashTable_Node* compatibility_iterator;
public:
    wxHashTable( wxKeyType keyType = wxKEY_INTEGER,
                 size_t size = wxHASH_SIZE_DEFAULT )
        : wxHashTableBase() { Create( keyType, size ); BeginFind(); }
    wxHashTable( const wxHashTable& table );

    virtual ~wxHashTable() { Destroy(); }

    const wxHashTable& operator=( const wxHashTable& );

    // key and value are the same
    void Put(long value, wxObject *object)
        { DoPut( value, value, object ); }
    void Put(long lhash, long value, wxObject *object)
        { DoPut( value, lhash, object ); }
    void Put(const wxString& value, wxObject *object)
        { DoPut( value, MakeKey( value ), object ); }
    void Put(long lhash, const wxString& value, wxObject *object)
        { DoPut( value, lhash, object ); }

    // key and value are the same
    wxObject *Get(long value) const
        { return (wxObject*)DoGet( value, value ); }
    wxObject *Get(long lhash, long value) const
        { return (wxObject*)DoGet( value, lhash ); }
    wxObject *Get(const wxString& value) const
        { return (wxObject*)DoGet( value, MakeKey( value ) ); }
    wxObject *Get(long lhash, const wxString& value) const
        { return (wxObject*)DoGet( value, lhash ); }

    // Deletes entry and returns data if found
    wxObject *Delete(long key)
        { return (wxObject*)DoDelete( key, key ); }
    wxObject *Delete(long lhash, long key)
        { return (wxObject*)DoDelete( key, lhash ); }
    wxObject *Delete(const wxString& key)
        { return (wxObject*)DoDelete( key, MakeKey( key ) ); }
    wxObject *Delete(long lhash, const wxString& key)
        { return (wxObject*)DoDelete( key, lhash ); }

    // Way of iterating through whole hash table (e.g. to delete everything)
    // Not necessary, of course, if you're only storing pointers to
    // objects maintained separately
    void BeginFind() { m_curr = NULL; m_currBucket = 0; }
    Node* Next();

    void Clear() { wxHashTableBase::Clear(); }

    size_t GetCount() const { return wxHashTableBase::GetCount(); }
protected:
    // copy helper
    void DoCopy( const wxHashTable& copy );

    // searches the next node starting from bucket bucketStart and sets
    // m_curr to it and m_currBucket to its bucket
    void GetNextNode( size_t bucketStart );
private:
    virtual void DoDeleteContents( wxHashTableBase_Node* node );

    // current node
    Node* m_curr;

    // bucket the current node belongs to
    size_t m_currBucket;
};

// defines a new type safe hash table which stores the elements of type eltype
// in lists of class listclass
#define _WX_DECLARE_HASH(eltype, dummy, hashclass, classexp)                  \
    classexp hashclass : public wxHashTableBase                               \
    {                                                                         \
    public:                                                                   \
        hashclass(wxKeyType keyType = wxKEY_INTEGER,                          \
                  size_t size = wxHASH_SIZE_DEFAULT)                          \
            : wxHashTableBase() { Create(keyType, size); }                    \
                                                                              \
        virtual ~hashclass() { Destroy(); }                                   \
                                                                              \
        void Put(long key, eltype *data) { DoPut(key, key, (void*)data); }    \
        void Put(long lhash, long key, eltype *data)                          \
            { DoPut(key, lhash, (void*)data); }                               \
        eltype *Get(long key) const { return (eltype*)DoGet(key, key); }      \
        eltype *Get(long lhash, long key) const                               \
            { return (eltype*)DoGet(key, lhash); }                            \
        eltype *Delete(long key) { return (eltype*)DoDelete(key, key); }      \
        eltype *Delete(long lhash, long key)                                  \
            { return (eltype*)DoDelete(key, lhash); }                         \
    private:                                                                  \
        virtual void DoDeleteContents( wxHashTableBase_Node* node )           \
            { delete (eltype*)node->GetData(); }                              \
                                                                              \
        DECLARE_NO_COPY_CLASS(hashclass)                                      \
    }


// this macro is to be used in the user code
#define WX_DECLARE_HASH(el, list, hash) \
    _WX_DECLARE_HASH(el, list, hash, class)

// and this one does exactly the same thing but should be used inside the
// library
#define WX_DECLARE_EXPORTED_HASH(el, list, hash)  \
    _WX_DECLARE_HASH(el, list, hash, class WXDLLIMPEXP_CORE)

#define WX_DECLARE_USER_EXPORTED_HASH(el, list, hash, usergoo)  \
    _WX_DECLARE_HASH(el, list, hash, class usergoo)

// delete all hash elements
//
// NB: the class declaration of the hash elements must be visible from the
//     place where you use this macro, otherwise the proper destructor may not
//     be called (a decent compiler should give a warning about it, but don't
//     count on it)!
#define WX_CLEAR_HASH_TABLE(hash)                                            \
    {                                                                        \
        (hash).BeginFind();                                                  \
        wxHashTable::compatibility_iterator it = (hash).Next();              \
        while( it )                                                          \
        {                                                                    \
            delete it->GetData();                                            \
            it = (hash).Next();                                              \
        }                                                                    \
        (hash).Clear();                                                      \
    }

#endif // _WX_HASH_H__
