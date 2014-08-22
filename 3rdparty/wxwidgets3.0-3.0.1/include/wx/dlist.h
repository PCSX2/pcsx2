///////////////////////////////////////////////////////////////////////////////
// Name:        wx/dlist.h
// Purpose:     wxDList<T> which is a template version of wxList
// Author:      Robert Roebling
// Created:     18.09.2008
// Copyright:   (c) 2008 wxWidgets team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DLIST_H_
#define _WX_DLIST_H_

#include "wx/defs.h"
#include "wx/utils.h"

#if wxUSE_STD_CONTAINERS

#include "wx/beforestd.h"
#include <algorithm>
#include <iterator>
#include <list>
#include "wx/afterstd.h"

template<typename T>
class wxDList: public std::list<T*>
{
private:
    bool m_destroy;
    typedef std::list<T*> BaseListType;
    typedef wxDList<T> ListType;

public:
    typedef typename BaseListType::iterator iterator;

    class compatibility_iterator
    {
    private:
        /* Workaround for broken VC6 nested class name resolution */
        typedef typename BaseListType::iterator iterator;
        friend class wxDList<T>;

        iterator m_iter;
        ListType *m_list;

    public:
        compatibility_iterator()
            : m_iter(), m_list( NULL ) {}
        compatibility_iterator( ListType* li, iterator i )
            : m_iter( i ), m_list( li ) {}
        compatibility_iterator( const ListType* li, iterator i )
            : m_iter( i ), m_list( const_cast<ListType*>(li) ) {}

        compatibility_iterator* operator->() { return this; }
        const compatibility_iterator* operator->() const { return this; }

        bool operator==(const compatibility_iterator& i) const
        {
            wxASSERT_MSG( m_list && i.m_list,
                          "comparing invalid iterators is illegal" );
            return (m_list == i.m_list) && (m_iter == i.m_iter);
        }
        bool operator!=(const compatibility_iterator& i) const
            { return !( operator==( i ) ); }
        operator bool() const
            { return m_list ? m_iter != m_list->end() : false; }
        bool operator !() const
            { return !( operator bool() ); }

        T* GetData() const { return *m_iter; }
        void SetData( T* e ) { *m_iter = e; }

        compatibility_iterator GetNext() const
        {
            iterator i = m_iter;
            return compatibility_iterator( m_list, ++i );
        }

        compatibility_iterator GetPrevious() const
        {
            if ( m_iter == m_list->begin() )
                return compatibility_iterator();

            iterator i = m_iter;
            return compatibility_iterator( m_list, --i );
        }

        int IndexOf() const
        {
            return *this ? std::distance( m_list->begin(), m_iter )
                : wxNOT_FOUND;
        }
    };

public:
    wxDList() : m_destroy( false ) {}

    ~wxDList() { Clear(); }

    compatibility_iterator Find( const T* e ) const
    {
        return compatibility_iterator( this,
                std::find( const_cast<ListType*>(this)->begin(),
                           const_cast<ListType*>(this)->end(), e ) );
    }

    bool IsEmpty() const
        { return this->empty(); }
    size_t GetCount() const
        { return this->size(); }

    compatibility_iterator Item( size_t idx ) const
    {
        iterator i = const_cast<ListType*>(this)->begin();
        std::advance( i, idx );
        return compatibility_iterator( this, i );
    }

    T* operator[](size_t idx) const
    {
        return Item(idx).GetData();
    }

    compatibility_iterator GetFirst() const
    {
        return compatibility_iterator( this, const_cast<ListType*>(this)->begin() );
    }
    compatibility_iterator GetLast() const
    {
        iterator i = const_cast<ListType*>(this)->end();
        return compatibility_iterator( this, !(this->empty()) ? --i : i );
    }
    compatibility_iterator Member( T* e ) const
        { return Find( e ); }
    compatibility_iterator Nth( int n ) const
        { return Item( n ); }
    int IndexOf( T* e ) const
        { return Find( e ).IndexOf(); }

    compatibility_iterator Append( T* e )
    {
        this->push_back( e );
        return GetLast();
    }

    compatibility_iterator Insert( T* e )
    {
        this->push_front( e );
        return compatibility_iterator( this, this->begin() );
    }

    compatibility_iterator Insert( compatibility_iterator & i, T* e )
    {
        return compatibility_iterator( this, this->insert( i.m_iter, e ) );
    }

    compatibility_iterator Insert( size_t idx, T* e )
    {
        return compatibility_iterator( this,
                this->insert( Item( idx ).m_iter, e ) );
    }

    void DeleteContents( bool destroy )
        { m_destroy = destroy; }

    bool GetDeleteContents() const
        { return m_destroy; }

    void Erase( const compatibility_iterator& i )
    {
        if ( m_destroy )
            delete i->GetData();
        this->erase( i.m_iter );
    }

    bool DeleteNode( const compatibility_iterator& i )
    {
        if( i )
        {
            Erase( i );
            return true;
        }
        return false;
    }

    bool DeleteObject( T* e )
    {
        return DeleteNode( Find( e ) );
    }

    void Clear()
    {
        if ( m_destroy )
        {
            iterator it, en;
            for ( it = this->begin(), en = this->end(); it != en; ++it )
                delete *it;
        }
        this->clear();
    }
};

#else  // !wxUSE_STD_CONTAINERS

template <typename T>
class wxDList
{
public:
    class Node
    {
    public:
        Node(wxDList<T> *list = NULL,
             Node *previous = NULL,
             Node *next = NULL,
             T *data = NULL)
        {
            m_list = list;
            m_previous = previous;
            m_next = next;
            m_data = data;
            if (previous)
                previous->m_next = this;
            if (next)
                next->m_previous = this;
        }

        ~Node()
        {
            // handle the case when we're being deleted from the list by
            // the user (i.e. not by the list itself from DeleteNode) -
            // we must do it for compatibility with old code
            if (m_list != NULL)
                m_list->DetachNode(this);
        }

        void DeleteData()
        {
            delete m_data;
        }

        Node *GetNext() const      { return m_next; }
        Node *GetPrevious() const  { return m_previous; }
        T *GetData() const         { return m_data; }
        T **GetDataPtr() const     { return &(wx_const_cast(nodetype*,this)->m_data); }
        void SetData( T *data )    { m_data = data; }

        int IndexOf() const
        {
            wxCHECK_MSG( m_list, wxNOT_FOUND,
                         "node doesn't belong to a list in IndexOf" );

            int i;
            Node *prev = m_previous;
            for( i = 0; prev; i++ )
                prev = prev->m_previous;
            return i;
        }

    private:
        T           *m_data;        // user data
        Node        *m_next,        // next and previous nodes in the list
                    *m_previous;
        wxDList<T>   *m_list;         // list we belong to

        friend class wxDList<T>;
    };

    typedef Node nodetype;

    class compatibility_iterator
    {
    public:
        compatibility_iterator(nodetype *ptr = NULL) : m_ptr(ptr) { }
        nodetype *operator->() const { return m_ptr; }
        operator nodetype *() const  { return m_ptr; }

    private:
        nodetype *m_ptr;
    };

private:
    void Init()
    {
        m_nodeFirst =
        m_nodeLast = NULL;
        m_count = 0;
        m_destroy = false;
    }

    void DoDeleteNode( nodetype *node )
    {
        if ( m_destroy )
            node->DeleteData();
        // so that the node knows that it's being deleted by the list
        node->m_list = NULL;
        delete node;
    }

    size_t m_count;             // number of elements in the list
    bool m_destroy;             // destroy user data when deleting list items?
    nodetype *m_nodeFirst,      // pointers to the head and tail of the list
             *m_nodeLast;

public:
    wxDList()
    {
        Init();
    }

    wxDList( const wxDList<T>& list )
    {
        Init();
        Assign(list);
    }

    wxDList( size_t count, T *elements[] )
    {
        Init();
        size_t n;
        for (n = 0; n < count; n++)
            Append( elements[n] );
    }

    wxDList& operator=( const wxDList<T>& list )
    {
        if (&list != this)
            Assign(list);
        return *this;
    }

    ~wxDList()
    {
        nodetype *each = m_nodeFirst;
        while ( each != NULL )
        {
            nodetype *next = each->GetNext();
                DoDeleteNode(each);
            each = next;
        }
    }

    void Assign(const wxDList<T> &list)
    {
        wxASSERT_MSG( !list.m_destroy,
                      "copying list which owns it's elements is a bad idea" );
        Clear();
        m_destroy = list.m_destroy;
        m_nodeFirst = NULL;
        m_nodeLast = NULL;
        nodetype* node;
        for (node = list.GetFirst(); node; node = node->GetNext() )
            Append(node->GetData());
        wxASSERT_MSG( m_count == list.m_count, "logic error in Assign()" );
    }

    nodetype *Append( T *object )
    {
        nodetype *node = new nodetype( this, m_nodeLast, NULL, object );

        if ( !m_nodeFirst )
        {
            m_nodeFirst = node;
            m_nodeLast = m_nodeFirst;
        }
        else
        {
            m_nodeLast->m_next = node;
            m_nodeLast = node;
        }
        m_count++;
        return node;
    }

    nodetype *Insert( T* object )
    {
        return Insert( NULL, object );
    }

    nodetype *Insert( size_t pos, T* object )
    {
        if (pos == m_count)
            return Append( object );
        else
            return Insert( Item(pos), object );
    }

    nodetype *Insert( nodetype *position, T* object )
    {
        wxCHECK_MSG( !position || position->m_list == this, NULL,
                     "can't insert before a node from another list" );

        // previous and next node for the node being inserted
        nodetype *prev, *next;
        if ( position )
        {
           prev = position->GetPrevious();
           next = position;
        }
        else
        {
            // inserting in the beginning of the list
            prev = NULL;
            next = m_nodeFirst;
        }
        nodetype *node = new nodetype( this, prev, next, object );
        if ( !m_nodeFirst )
            m_nodeLast = node;
        if ( prev == NULL )
            m_nodeFirst = node;
        m_count++;
        return node;
    }

    nodetype *GetFirst() const { return m_nodeFirst; }
    nodetype *GetLast() const { return m_nodeLast; }
    size_t GetCount() const { return m_count; }
    bool IsEmpty() const { return m_count == 0; }

    void DeleteContents(bool destroy) { m_destroy = destroy; }
    bool GetDeleteContents() const { return m_destroy; }

    nodetype *Item(size_t index) const
    {
        for ( nodetype *current = GetFirst(); current; current = current->GetNext() )
        {
            if ( index-- == 0 )
               return current;
        }
        wxFAIL_MSG( "invalid index in Item()" );
        return NULL;
    }

    T *operator[](size_t index) const
    {
        nodetype *node = Item(index);
        return node ? node->GetData() : NULL;
    }

    nodetype *DetachNode( nodetype *node )
    {
        wxCHECK_MSG( node, NULL, "detaching NULL wxNodeBase" );
        wxCHECK_MSG( node->m_list == this, NULL,
                     "detaching node which is not from this list" );
        // update the list
        nodetype **prevNext = node->GetPrevious() ? &node->GetPrevious()->m_next
                                                  : &m_nodeFirst;
        nodetype **nextPrev = node->GetNext() ? &node->GetNext()->m_previous
                                              : &m_nodeLast;
        *prevNext = node->GetNext();
        *nextPrev = node->GetPrevious();
        m_count--;
        // mark the node as not belonging to this list any more
        node->m_list = NULL;
        return node;
    }

    void Erase( nodetype *node )
    {
         DeleteNode(node);
    }

    bool DeleteNode( nodetype *node )
    {
        if ( !DetachNode(node) )
           return false;
        DoDeleteNode(node);
        return true;
    }

    bool DeleteObject( T *object )
    {
        for ( nodetype *current = GetFirst(); current; current = current->GetNext() )
        {
            if ( current->GetData() == object )
            {
                DeleteNode(current);
                return true;
            }
        }
        // not found
        return false;
    }

    nodetype *Find(const T *object) const
    {
        for ( nodetype *current = GetFirst(); current; current = current->GetNext() )
        {
            if ( current->GetData() == object )
                return current;
        }
        // not found
        return NULL;
    }

    int IndexOf(const T *object) const
    {
        int n = 0;
        for ( nodetype *current = GetFirst(); current; current = current->GetNext() )
        {
            if ( current->GetData() == object )
                return n;
            n++;
        }
        return wxNOT_FOUND;
    }

    void Clear()
    {
        nodetype *current = m_nodeFirst;
        while ( current )
        {
            nodetype *next = current->GetNext();
            DoDeleteNode(current);
            current = next;
        }
        m_nodeFirst =
        m_nodeLast = NULL;
        m_count = 0;
    }

    void Reverse()
    {
        nodetype * node = m_nodeFirst;
        nodetype* tmp;
        while (node)
        {
            // swap prev and next pointers
            tmp = node->m_next;
            node->m_next = node->m_previous;
            node->m_previous = tmp;
            // this is the node that was next before swapping
            node = tmp;
        }
        // swap first and last node
        tmp = m_nodeFirst; m_nodeFirst = m_nodeLast; m_nodeLast = tmp;
    }

    void DeleteNodes(nodetype* first, nodetype* last)
    {
        nodetype * node = first;
        while (node != last)
        {
            nodetype* next = node->GetNext();
            DeleteNode(node);
            node = next;
        }
    }

    void ForEach(wxListIterateFunction F)
    {
        for ( nodetype *current = GetFirst(); current; current = current->GetNext() )
            (*F)(current->GetData());
    }

    T *FirstThat(wxListIterateFunction F)
    {
        for ( nodetype *current = GetFirst(); current; current = current->GetNext() )
        {
            if ( (*F)(current->GetData()) )
                return current->GetData();
        }
        return NULL;
    }

    T *LastThat(wxListIterateFunction F)
    {
        for ( nodetype *current = GetLast(); current; current = current->GetPrevious() )
        {
            if ( (*F)(current->GetData()) )
                return current->GetData();
        }
        return NULL;
    }

    /* STL interface */
public:
    typedef size_t size_type;
    typedef int difference_type;
    typedef T* value_type;
    typedef value_type& reference;
    typedef const value_type& const_reference;

    class iterator
    {
    public:
        typedef nodetype Node;
        typedef iterator itor;
        typedef T* value_type;
        typedef value_type* ptr_type;
        typedef value_type& reference;

        Node* m_node;
        Node* m_init;
    public:
        typedef reference reference_type;
        typedef ptr_type pointer_type;

        iterator(Node* node, Node* init) : m_node(node), m_init(init) {}
        iterator() : m_node(NULL), m_init(NULL) { }
        reference_type operator*() const
            { return *m_node->GetDataPtr(); }
        // ptrop
        itor& operator++() { m_node = m_node->GetNext(); return *this; }
        const itor operator++(int)
            { itor tmp = *this; m_node = m_node->GetNext(); return tmp; }
        itor& operator--()
        {
            m_node = m_node ? m_node->GetPrevious() : m_init;
            return *this;
        }
        const itor operator--(int)
        {
            itor tmp = *this;
            m_node = m_node ? m_node->GetPrevious() : m_init;
            return tmp;
        }
        bool operator!=(const itor& it) const
            { return it.m_node != m_node; }
        bool operator==(const itor& it) const
            { return it.m_node == m_node; }
    };
    class const_iterator
    {
    public:
        typedef nodetype Node;
        typedef T* value_type;
        typedef const value_type& const_reference;
        typedef const_iterator itor;
        typedef value_type* ptr_type;

        Node* m_node;
        Node* m_init;
    public:
        typedef const_reference reference_type;
        typedef const ptr_type pointer_type;

        const_iterator(Node* node, Node* init)
            : m_node(node), m_init(init) { }
        const_iterator() : m_node(NULL), m_init(NULL) { }
        const_iterator(const iterator& it)
            : m_node(it.m_node), m_init(it.m_init) { }
        reference_type operator*() const
            { return *m_node->GetDataPtr(); }
        // ptrop
        itor& operator++() { m_node = m_node->GetNext(); return *this; }
        const itor operator++(int)
            { itor tmp = *this; m_node = m_node->GetNext(); return tmp; }
        itor& operator--()
        {
            m_node = m_node ? m_node->GetPrevious() : m_init;
            return *this;
        }
        const itor operator--(int)
        {
            itor tmp = *this;
            m_node = m_node ? m_node->GetPrevious() : m_init;
            return tmp;
        }
        bool operator!=(const itor& it) const
            { return it.m_node != m_node; }
        bool operator==(const itor& it) const
            { return it.m_node == m_node; }
    };

    class reverse_iterator
    {
    public:
        typedef nodetype Node;
        typedef T* value_type;
        typedef reverse_iterator itor;
        typedef value_type* ptr_type;
        typedef value_type& reference;

        Node* m_node;
        Node* m_init;
    public:
        typedef reference reference_type;
        typedef ptr_type pointer_type;

        reverse_iterator(Node* node, Node* init)
            : m_node(node), m_init(init) { }
        reverse_iterator() : m_node(NULL), m_init(NULL) { }
        reference_type operator*() const
            { return *m_node->GetDataPtr(); }
        // ptrop
        itor& operator++()
            { m_node = m_node->GetPrevious(); return *this; }
        const itor operator++(int)
        { itor tmp = *this; m_node = m_node->GetPrevious(); return tmp; }
        itor& operator--()
        { m_node = m_node ? m_node->GetNext() : m_init; return *this; }
        const itor operator--(int)
        {
            itor tmp = *this;
            m_node = m_node ? m_node->GetNext() : m_init;
            return tmp;
        }
        bool operator!=(const itor& it) const
            { return it.m_node != m_node; }
        bool operator==(const itor& it) const
            { return it.m_node == m_node; }
    };

    class const_reverse_iterator
    {
    public:
        typedef nodetype Node;
        typedef T* value_type;
        typedef const_reverse_iterator itor;
        typedef value_type* ptr_type;
        typedef const value_type& const_reference;

        Node* m_node;
        Node* m_init;
    public:
        typedef const_reference reference_type;
        typedef const ptr_type pointer_type;

        const_reverse_iterator(Node* node, Node* init)
            : m_node(node), m_init(init) { }
        const_reverse_iterator() : m_node(NULL), m_init(NULL) { }
        const_reverse_iterator(const reverse_iterator& it)
            : m_node(it.m_node), m_init(it.m_init) { }
        reference_type operator*() const
            { return *m_node->GetDataPtr(); }
        // ptrop
        itor& operator++()
            { m_node = m_node->GetPrevious(); return *this; }
        const itor operator++(int)
        { itor tmp = *this; m_node = m_node->GetPrevious(); return tmp; }
        itor& operator--()
            { m_node = m_node ? m_node->GetNext() : m_init; return *this;}
        const itor operator--(int)
        {
            itor tmp = *this;
            m_node = m_node ? m_node->GetNext() : m_init;
            return tmp;
        }
        bool operator!=(const itor& it) const
            { return it.m_node != m_node; }
        bool operator==(const itor& it) const
            { return it.m_node == m_node; }
    };

    wxEXPLICIT wxDList(size_type n, const_reference v = value_type())
        { assign(n, v); }
    wxDList(const const_iterator& first, const const_iterator& last)
        { assign(first, last); }
    iterator begin() { return iterator(GetFirst(), GetLast()); }
    const_iterator begin() const
        { return const_iterator(GetFirst(), GetLast()); }
    iterator end() { return iterator(NULL, GetLast()); }
    const_iterator end() const { return const_iterator(NULL, GetLast()); }
    reverse_iterator rbegin()
        { return reverse_iterator(GetLast(), GetFirst()); }
    const_reverse_iterator rbegin() const
        { return const_reverse_iterator(GetLast(), GetFirst()); }
    reverse_iterator rend() { return reverse_iterator(NULL, GetFirst()); }
    const_reverse_iterator rend() const
        { return const_reverse_iterator(NULL, GetFirst()); }
    void resize(size_type n, value_type v = value_type())
    {
        while (n < size())
            pop_back();
        while (n > size())
            push_back(v);
    }
    size_type size() const { return GetCount(); }
    size_type max_size() const { return INT_MAX; }
    bool empty() const { return IsEmpty(); }
    reference front() { return *begin(); }
    const_reference front() const { return *begin(); }
    reference back() { iterator tmp = end(); return *--tmp; }
    const_reference back() const { const_iterator tmp = end(); return *--tmp; }
    void push_front(const_reference v = value_type())
        { Insert(GetFirst(), v); }
    void pop_front() { DeleteNode(GetFirst()); }
    void push_back(const_reference v = value_type())
        { Append( v ); }
    void pop_back() { DeleteNode(GetLast()); }
    void assign(const_iterator first, const const_iterator& last)
    {
        clear();
        for(; first != last; ++first)
            Append(*first);
    }
    void assign(size_type n, const_reference v = value_type())
    {
        clear();
        for(size_type i = 0; i < n; ++i)
            Append(v);
    }
    iterator insert(const iterator& it, const_reference v)
    {
        if (it == end())
            Append( v );
        else
            Insert(it.m_node,v);
        iterator itprev(it);
        return itprev--;
    }
    void insert(const iterator& it, size_type n, const_reference v)
    {
        for(size_type i = 0; i < n; ++i)
            Insert(it.m_node, v);
    }
    void insert(const iterator& it, const_iterator first, const const_iterator& last)
    {
        for(; first != last; ++first)
            Insert(it.m_node, *first);
    }
    iterator erase(const iterator& it)
    {
        iterator next = iterator(it.m_node->GetNext(), GetLast());
        DeleteNode(it.m_node); return next;
    }
    iterator erase(const iterator& first, const iterator& last)
    {
        iterator next = last; ++next;
        DeleteNodes(first.m_node, last.m_node);
        return next;
    }
    void clear() { Clear(); }
    void splice(const iterator& it, wxDList<T>& l, const iterator& first, const iterator& last)
        { insert(it, first, last); l.erase(first, last); }
    void splice(const iterator& it, wxDList<T>& l)
        { splice(it, l, l.begin(), l.end() ); }
    void splice(const iterator& it, wxDList<T>& l, const iterator& first)
    {
        iterator tmp = first; ++tmp;
        if(it == first || it == tmp) return;
        insert(it, *first);
        l.erase(first);
    }
    void remove(const_reference v)
        { DeleteObject(v); }
    void reverse()
        { Reverse(); }
 /* void swap(list<T>& l)
    {
        { size_t t = m_count; m_count = l.m_count; l.m_count = t; }
        { bool t = m_destroy; m_destroy = l.m_destroy; l.m_destroy = t; }
        { wxNodeBase* t = m_nodeFirst; m_nodeFirst = l.m_nodeFirst; l.m_nodeFirst = t; }
        { wxNodeBase* t = m_nodeLast; m_nodeLast = l.m_nodeLast; l.m_nodeLast = t; }
        { wxKeyType t = m_keyType; m_keyType = l.m_keyType; l.m_keyType = t; }
    } */
};

#endif // wxUSE_STD_CONTAINERS/!wxUSE_STD_CONTAINERS

#endif // _WX_DLIST_H_
