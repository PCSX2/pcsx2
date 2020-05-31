/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/arrstr.cpp
// Purpose:     wxArrayString class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ===========================================================================
// headers, declarations, constants
// ===========================================================================

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/arrstr.h"
#include "wx/scopedarray.h"

#include "wx/beforestd.h"
#include <algorithm>
#include <functional>
#include "wx/afterstd.h"

// ============================================================================
// ArrayString
// ============================================================================

wxArrayString::wxArrayString(size_t sz, const char** a)
{
#if !wxUSE_STD_CONTAINERS
    Init(false);
#endif
    for (size_t i=0; i < sz; i++)
        Add(a[i]);
}

wxArrayString::wxArrayString(size_t sz, const wchar_t** a)
{
#if !wxUSE_STD_CONTAINERS
    Init(false);
#endif
    for (size_t i=0; i < sz; i++)
        Add(a[i]);
}

wxArrayString::wxArrayString(size_t sz, const wxString* a)
{
#if !wxUSE_STD_CONTAINERS
    Init(false);
#endif
    for (size_t i=0; i < sz; i++)
        Add(a[i]);
}

#if !wxUSE_STD_CONTAINERS

// size increment = min(50% of current size, ARRAY_MAXSIZE_INCREMENT)
#define   ARRAY_MAXSIZE_INCREMENT       4096

#ifndef   ARRAY_DEFAULT_INITIAL_SIZE    // also defined in dynarray.h
#define   ARRAY_DEFAULT_INITIAL_SIZE    (16)
#endif

// ctor
void wxArrayString::Init(bool autoSort)
{
  m_nSize  =
  m_nCount = 0;
  m_pItems = NULL;
  m_autoSort = autoSort;
}

// copy ctor
wxArrayString::wxArrayString(const wxArrayString& src)
{
  Init(src.m_autoSort);

  *this = src;
}

// assignment operator
wxArrayString& wxArrayString::operator=(const wxArrayString& src)
{
  if ( m_nSize > 0 )
    Clear();

  Copy(src);

  m_autoSort = src.m_autoSort;

  return *this;
}

void wxArrayString::Copy(const wxArrayString& src)
{
  if ( src.m_nCount > ARRAY_DEFAULT_INITIAL_SIZE )
    Alloc(src.m_nCount);

  for ( size_t n = 0; n < src.m_nCount; n++ )
    Add(src[n]);
}

// grow the array
wxString *wxArrayString::Grow(size_t nIncrement)
{
    if ( (m_nSize - m_nCount) >= nIncrement )
    {
        // We already have enough space.
        return NULL;
    }

    // if ARRAY_DEFAULT_INITIAL_SIZE were set to 0, the initially empty would
    // be never resized!
    #if ARRAY_DEFAULT_INITIAL_SIZE == 0
      #error "ARRAY_DEFAULT_INITIAL_SIZE must be > 0!"
    #endif

    if ( m_nSize == 0 ) {
      // was empty, alloc some memory
      m_nSize = ARRAY_DEFAULT_INITIAL_SIZE;
      if (m_nSize < nIncrement)
          m_nSize = nIncrement;
      m_pItems = new wxString[m_nSize];

      // Nothing to free, we hadn't had any memory before.
      return NULL;
    }
    else {
      // otherwise when it's called for the first time, nIncrement would be 0
      // and the array would never be expanded
      // add 50% but not too much
      size_t ndefIncrement = m_nSize < ARRAY_DEFAULT_INITIAL_SIZE
                          ? ARRAY_DEFAULT_INITIAL_SIZE : m_nSize >> 1;
      if ( ndefIncrement > ARRAY_MAXSIZE_INCREMENT )
        ndefIncrement = ARRAY_MAXSIZE_INCREMENT;
      if ( nIncrement < ndefIncrement )
        nIncrement = ndefIncrement;
      m_nSize += nIncrement;
      wxString *pNew = new wxString[m_nSize];

      // copy data to new location
      for ( size_t j = 0; j < m_nCount; j++ )
          pNew[j] = m_pItems[j];

      wxString* const pItemsOld = m_pItems;

      m_pItems = pNew;

      return pItemsOld;
    }
}

// deletes all the strings from the list
void wxArrayString::Empty()
{
  m_nCount = 0;
}

// as Empty, but also frees memory
void wxArrayString::Clear()
{
  m_nSize  =
  m_nCount = 0;

  wxDELETEA(m_pItems);
}

// dtor
wxArrayString::~wxArrayString()
{
  delete [] m_pItems;
}

void wxArrayString::reserve(size_t nSize)
{
    Alloc(nSize);
}

// pre-allocates memory (frees the previous data!)
void wxArrayString::Alloc(size_t nSize)
{
  // only if old buffer was not big enough
  if ( nSize > m_nSize ) {
    wxString *pNew = new wxString[nSize];
    if ( !pNew )
        return;

    for ( size_t j = 0; j < m_nCount; j++ )
        pNew[j] = m_pItems[j];
    delete [] m_pItems;

    m_pItems = pNew;
    m_nSize  = nSize;
  }
}

// minimizes the memory usage by freeing unused memory
void wxArrayString::Shrink()
{
  // only do it if we have some memory to free
  if( m_nCount < m_nSize ) {
    // allocates exactly as much memory as we need
    wxString *pNew = new wxString[m_nCount];

    // copy data to new location
    for ( size_t j = 0; j < m_nCount; j++ )
        pNew[j] = m_pItems[j];
    delete [] m_pItems;
    m_pItems = pNew;
    m_nSize = m_nCount;
  }
}

// searches the array for an item (forward or backwards)
int wxArrayString::Index(const wxString& str, bool bCase, bool bFromEnd) const
{
  if ( m_autoSort ) {
    // use binary search in the sorted array
    wxASSERT_MSG( bCase && !bFromEnd,
                  wxT("search parameters ignored for auto sorted array") );

    size_t i,
           lo = 0,
           hi = m_nCount;
    int res;
    while ( lo < hi ) {
      i = (lo + hi)/2;

      res = str.compare(m_pItems[i]);
      if ( res < 0 )
        hi = i;
      else if ( res > 0 )
        lo = i + 1;
      else
        return i;
    }

    return wxNOT_FOUND;
  }
  else {
    // use linear search in unsorted array
    if ( bFromEnd ) {
      if ( m_nCount > 0 ) {
        size_t ui = m_nCount;
        do {
          if ( m_pItems[--ui].IsSameAs(str, bCase) )
            return ui;
        }
        while ( ui != 0 );
      }
    }
    else {
      for( size_t ui = 0; ui < m_nCount; ui++ ) {
        if( m_pItems[ui].IsSameAs(str, bCase) )
          return ui;
      }
    }
  }

  return wxNOT_FOUND;
}

// add item at the end
size_t wxArrayString::Add(const wxString& str, size_t nInsert)
{
  if ( m_autoSort ) {
    // insert the string at the correct position to keep the array sorted
    size_t i,
           lo = 0,
           hi = m_nCount;
    int res;
    while ( lo < hi ) {
      i = (lo + hi)/2;

      res = str.Cmp(m_pItems[i]);
      if ( res < 0 )
        hi = i;
      else if ( res > 0 )
        lo = i + 1;
      else {
        lo = hi = i;
        break;
      }
    }

    wxASSERT_MSG( lo == hi, wxT("binary search broken") );

    Insert(str, lo, nInsert);

    return (size_t)lo;
  }
  else {
    // Now that we must postpone freeing the old memory until we don't need it
    // any more, i.e. don't reference "str" which could be a reference to one
    // of our own strings.
    wxScopedArray<wxString> oldStrings(Grow(nInsert));

    for (size_t i = 0; i < nInsert; i++)
    {
        // just append
        m_pItems[m_nCount + i] = str;
    }
    size_t ret = m_nCount;
    m_nCount += nInsert;
    return ret;
  }
}

// add item at the given position
void wxArrayString::Insert(const wxString& str, size_t nIndex, size_t nInsert)
{
  wxCHECK_RET( nIndex <= m_nCount, wxT("bad index in wxArrayString::Insert") );
  wxCHECK_RET( m_nCount <= m_nCount + nInsert,
               wxT("array size overflow in wxArrayString::Insert") );

  wxScopedArray<wxString> oldStrings(Grow(nInsert));

  for (int j = m_nCount - nIndex - 1; j >= 0; j--)
      m_pItems[nIndex + nInsert + j] = m_pItems[nIndex + j];

  for (size_t i = 0; i < nInsert; i++)
  {
      m_pItems[nIndex + i] = str;
  }
  m_nCount += nInsert;
}

// range insert (STL 23.2.4.3)
void
wxArrayString::insert(iterator it, const_iterator first, const_iterator last)
{
    const int idx = it - begin();

    // grow it once
    wxScopedArray<wxString> oldStrings(Grow(last - first));

    // reset "it" since it can change inside Grow()
    it = begin() + idx;

    while ( first != last )
    {
        it = insert(it, *first);

        // insert returns an iterator to the last element inserted but we need
        // insert the next after this one, that is before the next one
        ++it;

        ++first;
    }
}

void wxArrayString::resize(size_type n, value_type v)
{
  if ( n < m_nCount )
      m_nCount = n;
  else if ( n > m_nCount )
      Add(v, n - m_nCount);
}

// expand the array
void wxArrayString::SetCount(size_t count)
{
    Alloc(count);

    wxString s;
    while ( m_nCount < count )
        m_pItems[m_nCount++] = s;
}

// removes item from array (by index)
void wxArrayString::RemoveAt(size_t nIndex, size_t nRemove)
{
  wxCHECK_RET( nIndex < m_nCount, wxT("bad index in wxArrayString::Remove") );
  wxCHECK_RET( nIndex + nRemove <= m_nCount,
               wxT("removing too many elements in wxArrayString::Remove") );

  for ( size_t j =  0; j < m_nCount - nIndex -nRemove; j++)
      m_pItems[nIndex + j] = m_pItems[nIndex + nRemove + j];

  m_nCount -= nRemove;
}

// removes item from array (by value)
void wxArrayString::Remove(const wxString& sz)
{
  int iIndex = Index(sz);

  wxCHECK_RET( iIndex != wxNOT_FOUND,
               wxT("removing inexistent element in wxArrayString::Remove") );

  RemoveAt(iIndex);
}

// ----------------------------------------------------------------------------
// sorting
// ----------------------------------------------------------------------------

// we need an adaptor as our predicates use qsort() convention and so return
// negative, null or positive value depending on whether the first item is less
// than, equal to or greater than the other one while we need a real boolean
// predicate now that we use std::sort()
struct wxSortPredicateAdaptor
{
    wxSortPredicateAdaptor(wxArrayString::CompareFunction compareFunction)
        : m_compareFunction(compareFunction)
    {
    }

    bool operator()(const wxString& first, const wxString& second) const
    {
        return (*m_compareFunction)(first, second) < 0;
    }

    wxArrayString::CompareFunction m_compareFunction;
};

void wxArrayString::Sort(CompareFunction compareFunction)
{
    wxCHECK_RET( !m_autoSort, wxT("can't use this method with sorted arrays") );

    std::sort(m_pItems, m_pItems + m_nCount,
                wxSortPredicateAdaptor(compareFunction));
}

struct wxSortPredicateAdaptor2
{
    wxSortPredicateAdaptor2(wxArrayString::CompareFunction2 compareFunction)
        : m_compareFunction(compareFunction)
    {
    }

    bool operator()(const wxString& first, const wxString& second) const
    {
        return (*m_compareFunction)(const_cast<wxString *>(&first),
                                    const_cast<wxString *>(&second)) < 0;
    }

    wxArrayString::CompareFunction2 m_compareFunction;
};

void wxArrayString::Sort(CompareFunction2 compareFunction)
{
    std::sort(m_pItems, m_pItems + m_nCount,
                wxSortPredicateAdaptor2(compareFunction));
}

void wxArrayString::Sort(bool reverseOrder)
{
    if ( reverseOrder )
        std::sort(m_pItems, m_pItems + m_nCount, std::greater<wxString>());
    else // normal sort
        std::sort(m_pItems, m_pItems + m_nCount);
}

bool wxArrayString::operator==(const wxArrayString& a) const
{
    if ( m_nCount != a.m_nCount )
        return false;

    for ( size_t n = 0; n < m_nCount; n++ )
    {
        if ( Item(n) != a[n] )
            return false;
    }

    return true;
}

#endif // !wxUSE_STD_CONTAINERS

// ===========================================================================
// wxJoin and wxSplit
// ===========================================================================

#include "wx/tokenzr.h"

wxString wxJoin(const wxArrayString& arr, const wxChar sep, const wxChar escape)
{
    size_t count = arr.size();
    if ( count == 0 )
        return wxEmptyString;

    wxString str;

    // pre-allocate memory using the estimation of the average length of the
    // strings in the given array: this is very imprecise, of course, but
    // better than nothing
    str.reserve(count*(arr[0].length() + arr[count-1].length()) / 2);

    if ( escape == wxT('\0') )
    {
        // escaping is disabled:
        for ( size_t i = 0; i < count; i++ )
        {
            if ( i )
                str += sep;
            str += arr[i];
        }
    }
    else // use escape character
    {
        for ( size_t n = 0; n < count; n++ )
        {
            if ( n )
                str += sep;

            for ( wxString::const_iterator i = arr[n].begin(),
                                         end = arr[n].end();
                  i != end;
                  ++i )
            {
                const wxChar ch = *i;
                if ( ch == sep )
                    str += escape;      // escape this separator
                str += ch;
            }
        }
    }

    str.Shrink(); // release extra memory if we allocated too much
    return str;
}

wxArrayString wxSplit(const wxString& str, const wxChar sep, const wxChar escape)
{
    if ( escape == wxT('\0') )
    {
        // simple case: we don't need to honour the escape character
        return wxStringTokenize(str, sep, wxTOKEN_RET_EMPTY_ALL);
    }

    wxArrayString ret;
    wxString curr;
    wxChar prev = wxT('\0');

    for ( wxString::const_iterator i = str.begin(),
                                 end = str.end();
          i != end;
          ++i )
    {
        const wxChar ch = *i;

        if ( ch == sep )
        {
            if ( prev == escape )
            {
                // remove the escape character and don't consider this
                // occurrence of 'sep' as a real separator
                *curr.rbegin() = sep;
            }
            else // real separator
            {
                ret.push_back(curr);
                curr.clear();
            }
        }
        else // normal character
        {
            curr += ch;
        }

        prev = ch;
    }

    // add the last token
    if ( !curr.empty() || prev == sep )
        ret.Add(curr);

    return ret;
}
