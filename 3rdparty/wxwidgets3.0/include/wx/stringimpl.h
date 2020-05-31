///////////////////////////////////////////////////////////////////////////////
// Name:        wx/stringimpl.h
// Purpose:     wxStringImpl class, implementation of wxString
// Author:      Vadim Zeitlin
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

/*
    This header implements std::string-like string class, wxStringImpl, that is
    used by wxString to store the data. Alternatively, if wxUSE_STD_STRING=1,
    wxStringImpl is just a typedef to std:: string class.
*/

#ifndef _WX_WXSTRINGIMPL_H__
#define _WX_WXSTRINGIMPL_H__

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/defs.h"        // everybody should include this
#include "wx/chartype.h"    // for wxChar
#include "wx/wxcrtbase.h"   // for wxStrlen() etc.

#include <stdlib.h>

// ---------------------------------------------------------------------------
// macros
// ---------------------------------------------------------------------------

// implementation only
#define   wxASSERT_VALID_INDEX(i) \
    wxASSERT_MSG( (size_t)(i) <= length(), wxT("invalid index in wxString") )


// ----------------------------------------------------------------------------
// global data
// ----------------------------------------------------------------------------

// global pointer to empty string
extern WXDLLIMPEXP_DATA_BASE(const wxChar*) wxEmptyString;
#if wxUSE_UNICODE_UTF8
// FIXME-UTF8: we should have only one wxEmptyString
extern WXDLLIMPEXP_DATA_BASE(const wxStringCharType*) wxEmptyStringImpl;
#endif


// ----------------------------------------------------------------------------
// deal with various build options
// ----------------------------------------------------------------------------

// we use STL-based string internally if we use std::string at all now, there
// should be no reason to prefer our internal implement but if you really need
// it you can predefine wxUSE_STL_BASED_WXSTRING as 0 when building the library
#ifndef wxUSE_STL_BASED_WXSTRING
    #define wxUSE_STL_BASED_WXSTRING  wxUSE_STD_STRING
#endif

// in both cases we need to define wxStdString
#if wxUSE_STL_BASED_WXSTRING || wxUSE_STD_STRING

#include "wx/beforestd.h"
#include <string>
#include "wx/afterstd.h"

#ifdef HAVE_STD_WSTRING
    typedef std::wstring wxStdWideString;
#else
    typedef std::basic_string<wchar_t> wxStdWideString;
#endif

#if wxUSE_UNICODE_WCHAR
    typedef wxStdWideString wxStdString;
#else
    typedef std::string wxStdString;
#endif

#endif // wxUSE_STL_BASED_WXSTRING || wxUSE_STD_STRING


#if wxUSE_STL_BASED_WXSTRING

    // we always want ctor from std::string when using std::string internally
    #undef wxUSE_STD_STRING
    #define wxUSE_STD_STRING 1

    // the versions of std::string included with gcc 2.95 and VC6 (for which
    // _MSC_VER == 1200) and eVC4 (_MSC_VER == 1201) lack clear() method
    #if (defined(__GNUG__) && (__GNUG__ < 3)) || \
        !wxCHECK_VISUALC_VERSION(7) || defined(__EVC4__)
        #define wxSTRING_BASE_HASNT_CLEAR
    #endif

    typedef wxStdString wxStringImpl;
#else // if !wxUSE_STL_BASED_WXSTRING

// in non-STL mode, compare() is implemented in wxString and not wxStringImpl
#undef HAVE_STD_STRING_COMPARE

// ---------------------------------------------------------------------------
// string data prepended with some housekeeping info (used by wxString class),
// is never used directly (but had to be put here to allow inlining)
// ---------------------------------------------------------------------------

struct WXDLLIMPEXP_BASE wxStringData
{
  int     nRefs;        // reference count
  size_t  nDataLength,  // actual string length
          nAllocLength; // allocated memory size

  // mimics declaration 'wxStringCharType data[nAllocLength]'
  wxStringCharType* data() const { return (wxStringCharType*)(this + 1); }

  // empty string has a special ref count so it's never deleted
  bool  IsEmpty()   const { return (nRefs == -1); }
  bool  IsShared()  const { return (nRefs > 1);   }

  // lock/unlock
  void  Lock()   { if ( !IsEmpty() ) nRefs++;                    }

  // VC++ will refuse to inline Unlock but profiling shows that it is wrong
#if defined(__VISUALC__) && (__VISUALC__ >= 1200)
  __forceinline
#endif
  // VC++ free must take place in same DLL as allocation when using non dll
  // run-time library (e.g. Multithreaded instead of Multithreaded DLL)
#if defined(__VISUALC__) && defined(_MT) && !defined(_DLL)
  void  Unlock() { if ( !IsEmpty() && --nRefs == 0) Free();  }
  // we must not inline deallocation since allocation is not inlined
  void  Free();
#else
  void  Unlock() { if ( !IsEmpty() && --nRefs == 0) free(this);  }
#endif

  // if we had taken control over string memory (GetWriteBuf), it's
  // intentionally put in invalid state
  void  Validate(bool b)  { nRefs = (b ? 1 : 0); }
  bool  IsValid() const   { return (nRefs != 0); }
};

class WXDLLIMPEXP_BASE wxStringImpl
{
public:
  // an 'invalid' value for string index, moved to this place due to a CW bug
  static const size_t npos;

protected:
  // points to data preceded by wxStringData structure with ref count info
  wxStringCharType *m_pchData;

  // accessor to string data
  wxStringData* GetStringData() const { return (wxStringData*)m_pchData - 1; }

  // string (re)initialization functions
    // initializes the string to the empty value (must be called only from
    // ctors, use Reinit() otherwise)
#if wxUSE_UNICODE_UTF8
  void Init() { m_pchData = (wxStringCharType *)wxEmptyStringImpl; } // FIXME-UTF8
#else
  void Init() { m_pchData = (wxStringCharType *)wxEmptyString; }
#endif
    // initializes the string with (a part of) C-string
  void InitWith(const wxStringCharType *psz, size_t nPos = 0, size_t nLen = npos);
    // as Init, but also frees old data
  void Reinit() { GetStringData()->Unlock(); Init(); }

  // memory allocation
    // allocates memory for string of length nLen
  bool AllocBuffer(size_t nLen);
    // effectively copies data to string
  bool AssignCopy(size_t, const wxStringCharType *);

  // append a (sub)string
  bool ConcatSelf(size_t nLen, const wxStringCharType *src, size_t nMaxLen);
  bool ConcatSelf(size_t nLen, const wxStringCharType *src)
    { return ConcatSelf(nLen, src, nLen); }

  // functions called before writing to the string: they copy it if there
  // are other references to our data (should be the only owner when writing)
  bool CopyBeforeWrite();
  bool AllocBeforeWrite(size_t);

    // compatibility with wxString
  bool Alloc(size_t nLen);

public:
  // standard types
  typedef wxStringCharType value_type;
  typedef wxStringCharType char_type;
  typedef size_t size_type;
  typedef value_type& reference;
  typedef const value_type& const_reference;
  typedef value_type* pointer;
  typedef const value_type* const_pointer;

  // macro to define the bulk of iterator and const_iterator classes
  #define WX_DEFINE_STRINGIMPL_ITERATOR(iterator_name, ref_type, ptr_type)    \
    public:                                                                   \
        typedef wxStringCharType value_type;                                  \
        typedef ref_type reference;                                           \
        typedef ptr_type pointer;                                             \
        typedef int difference_type;                                          \
                                                                              \
        iterator_name() : m_ptr(NULL) { }                                     \
        iterator_name(pointer ptr) : m_ptr(ptr) { }                           \
                                                                              \
        reference operator*() const { return *m_ptr; }                        \
                                                                              \
        iterator_name& operator++() { m_ptr++; return *this; }                \
        iterator_name operator++(int)                                         \
        {                                                                     \
            const iterator_name tmp(*this);                                   \
            m_ptr++;                                                          \
            return tmp;                                                       \
        }                                                                     \
                                                                              \
        iterator_name& operator--() { m_ptr--; return *this; }                \
        iterator_name operator--(int)                                         \
        {                                                                     \
            const iterator_name tmp(*this);                                   \
            m_ptr--;                                                          \
            return tmp;                                                       \
        }                                                                     \
                                                                              \
        iterator_name operator+(ptrdiff_t n) const                            \
            { return iterator_name(m_ptr + n); }                              \
        iterator_name operator-(ptrdiff_t n) const                            \
            { return iterator_name(m_ptr - n); }                              \
        iterator_name& operator+=(ptrdiff_t n)                                \
            { m_ptr += n; return *this; }                                     \
        iterator_name& operator-=(ptrdiff_t n)                                \
            { m_ptr -= n; return *this; }                                     \
                                                                              \
        difference_type operator-(const iterator_name& i) const               \
            { return m_ptr - i.m_ptr; }                                       \
                                                                              \
        bool operator==(const iterator_name& i) const                         \
          { return m_ptr == i.m_ptr; }                                        \
        bool operator!=(const iterator_name& i) const                         \
          { return m_ptr != i.m_ptr; }                                        \
                                                                              \
        bool operator<(const iterator_name& i) const                          \
          { return m_ptr < i.m_ptr; }                                         \
        bool operator>(const iterator_name& i) const                          \
          { return m_ptr > i.m_ptr; }                                         \
        bool operator<=(const iterator_name& i) const                         \
          { return m_ptr <= i.m_ptr; }                                        \
        bool operator>=(const iterator_name& i) const                         \
          { return m_ptr >= i.m_ptr; }                                        \
                                                                              \
    private:                                                                  \
        /* for wxStringImpl use only */                                       \
        pointer GetPtr() const { return m_ptr; }                              \
                                                                              \
        friend class wxStringImpl;                                            \
                                                                              \
        pointer m_ptr

  // we need to declare const_iterator in wxStringImpl scope, the friend
  // declaration inside iterator class itself is not enough, or at least not
  // for g++ 3.4 (g++ 4 is ok)
  class WXDLLIMPEXP_FWD_BASE const_iterator;

  class WXDLLIMPEXP_BASE iterator
  {
    WX_DEFINE_STRINGIMPL_ITERATOR(iterator,
                                  wxStringCharType&,
                                  wxStringCharType*);

    friend class const_iterator;
  };

  class WXDLLIMPEXP_BASE const_iterator
  {
  public:
      const_iterator(iterator i) : m_ptr(i.m_ptr) { }

      WX_DEFINE_STRINGIMPL_ITERATOR(const_iterator,
                                    const wxStringCharType&,
                                    const wxStringCharType*);
  };

  #undef WX_DEFINE_STRINGIMPL_ITERATOR


  // constructors and destructor
    // ctor for an empty string
  wxStringImpl() { Init(); }
    // copy ctor
  wxStringImpl(const wxStringImpl& stringSrc)
  {
    wxASSERT_MSG( stringSrc.GetStringData()->IsValid(),
                  wxT("did you forget to call UngetWriteBuf()?") );

    if ( stringSrc.empty() ) {
      // nothing to do for an empty string
      Init();
    }
    else {
      m_pchData = stringSrc.m_pchData;            // share same data
      GetStringData()->Lock();                    // => one more copy
    }
  }
    // string containing nRepeat copies of ch
  wxStringImpl(size_type nRepeat, wxStringCharType ch);
    // ctor takes first nLength characters from C string
    // (default value of npos means take all the string)
  wxStringImpl(const wxStringCharType *psz)
      { InitWith(psz, 0, npos); }
  wxStringImpl(const wxStringCharType *psz, size_t nLength)
      { InitWith(psz, 0, nLength); }
    // take nLen chars starting at nPos
  wxStringImpl(const wxStringImpl& str, size_t nPos, size_t nLen)
  {
    wxASSERT_MSG( str.GetStringData()->IsValid(),
                  wxT("did you forget to call UngetWriteBuf()?") );
    Init();
    size_t strLen = str.length() - nPos; nLen = strLen < nLen ? strLen : nLen;
    InitWith(str.c_str(), nPos, nLen);
  }
    // take everything between start and end
  wxStringImpl(const_iterator start, const_iterator end);


    // ctor from and conversion to std::string
#if wxUSE_STD_STRING
  wxStringImpl(const wxStdString& impl)
      { InitWith(impl.c_str(), 0, impl.length()); }

  operator wxStdString() const
      { return wxStdString(c_str(), length()); }
#endif

#if defined(__VISUALC__) && (__VISUALC__ >= 1200)
    // disable warning about Unlock() below not being inlined (first, it
    // seems to be inlined nevertheless and second, even if it isn't, there
    // is nothing we can do about this
    #pragma warning(push)
    #pragma warning (disable:4714)
#endif

    // dtor is not virtual, this class must not be inherited from!
  ~wxStringImpl()
  {
      GetStringData()->Unlock();
  }

#if defined(__VISUALC__) && (__VISUALC__ >= 1200)
    #pragma warning(pop)
#endif

  // overloaded assignment
    // from another wxString
  wxStringImpl& operator=(const wxStringImpl& stringSrc);
    // from a character
  wxStringImpl& operator=(wxStringCharType ch);
    // from a C string
  wxStringImpl& operator=(const wxStringCharType *psz);

    // return the length of the string
  size_type length() const { return GetStringData()->nDataLength; }
    // return the length of the string
  size_type size() const { return length(); }
    // return the maximum size of the string
  size_type max_size() const { return npos; }
    // resize the string, filling the space with c if c != 0
  void resize(size_t nSize, wxStringCharType ch = '\0');
    // delete the contents of the string
  void clear() { erase(0, npos); }
    // returns true if the string is empty
  bool empty() const { return length() == 0; }
    // inform string about planned change in size
  void reserve(size_t sz) { Alloc(sz); }
  size_type capacity() const { return GetStringData()->nAllocLength; }

  // lib.string.access
    // return the character at position n
  value_type operator[](size_type n) const { return m_pchData[n]; }
  value_type at(size_type n) const
    { wxASSERT_VALID_INDEX( n ); return m_pchData[n]; }
    // returns the writable character at position n
  reference operator[](size_type n) { CopyBeforeWrite(); return m_pchData[n]; }
  reference at(size_type n)
  {
    wxASSERT_VALID_INDEX( n );
    CopyBeforeWrite();
    return m_pchData[n];
  } // FIXME-UTF8: not useful for us...?

  // lib.string.modifiers
    // append elements str[pos], ..., str[pos+n]
  wxStringImpl& append(const wxStringImpl& str, size_t pos, size_t n)
  {
    wxASSERT(pos <= str.length());
    ConcatSelf(n, str.c_str() + pos, str.length() - pos);
    return *this;
  }
    // append a string
  wxStringImpl& append(const wxStringImpl& str)
    { ConcatSelf(str.length(), str.c_str()); return *this; }
    // append first n (or all if n == npos) characters of sz
  wxStringImpl& append(const wxStringCharType *sz)
    { ConcatSelf(wxStrlen(sz), sz); return *this; }
  wxStringImpl& append(const wxStringCharType *sz, size_t n)
    { ConcatSelf(n, sz); return *this; }
    // append n copies of ch
  wxStringImpl& append(size_t n, wxStringCharType ch);
    // append from first to last
  wxStringImpl& append(const_iterator first, const_iterator last)
    { ConcatSelf(last - first, first.GetPtr()); return *this; }

    // same as `this_string = str'
  wxStringImpl& assign(const wxStringImpl& str)
    { return *this = str; }
    // same as ` = str[pos..pos + n]
  wxStringImpl& assign(const wxStringImpl& str, size_t pos, size_t n)
    { return replace(0, npos, str, pos, n); }
    // same as `= first n (or all if n == npos) characters of sz'
  wxStringImpl& assign(const wxStringCharType *sz)
    { return replace(0, npos, sz, wxStrlen(sz)); }
  wxStringImpl& assign(const wxStringCharType *sz, size_t n)
    { return replace(0, npos, sz, n); }
    // same as `= n copies of ch'
  wxStringImpl& assign(size_t n, wxStringCharType ch)
    { return replace(0, npos, n, ch); }
    // assign from first to last
  wxStringImpl& assign(const_iterator first, const_iterator last)
    { return replace(begin(), end(), first, last); }

    // first valid index position
  const_iterator begin() const { return m_pchData; }
  iterator begin();
    // position one after the last valid one
  const_iterator end() const { return m_pchData + length(); }
  iterator end();

    // insert another string
  wxStringImpl& insert(size_t nPos, const wxStringImpl& str)
  {
    wxASSERT( str.GetStringData()->IsValid() );
    return insert(nPos, str.c_str(), str.length());
  }
    // insert n chars of str starting at nStart (in str)
  wxStringImpl& insert(size_t nPos, const wxStringImpl& str, size_t nStart, size_t n)
  {
    wxASSERT( str.GetStringData()->IsValid() );
    wxASSERT( nStart < str.length() );
    size_t strLen = str.length() - nStart;
    n = strLen < n ? strLen : n;
    return insert(nPos, str.c_str() + nStart, n);
  }
    // insert first n (or all if n == npos) characters of sz
  wxStringImpl& insert(size_t nPos, const wxStringCharType *sz, size_t n = npos);
    // insert n copies of ch
  wxStringImpl& insert(size_t nPos, size_t n, wxStringCharType ch)
    { return insert(nPos, wxStringImpl(n, ch)); }
  iterator insert(iterator it, wxStringCharType ch)
    { size_t idx = it - begin(); insert(idx, 1, ch); return begin() + idx; }
  void insert(iterator it, const_iterator first, const_iterator last)
    { insert(it - begin(), first.GetPtr(), last - first); }
  void insert(iterator it, size_type n, wxStringCharType ch)
    { insert(it - begin(), n, ch); }

    // delete characters from nStart to nStart + nLen
  wxStringImpl& erase(size_type pos = 0, size_type n = npos);
  iterator erase(iterator first, iterator last)
  {
    size_t idx = first - begin();
    erase(idx, last - first);
    return begin() + idx;
  }
  iterator erase(iterator first);

  // explicit conversion to C string (use this with printf()!)
  const wxStringCharType* c_str() const { return m_pchData; }
  const wxStringCharType* data() const { return m_pchData; }

    // replaces the substring of length nLen starting at nStart
  wxStringImpl& replace(size_t nStart, size_t nLen, const wxStringCharType* sz)
    { return replace(nStart, nLen, sz, npos); }
    // replaces the substring of length nLen starting at nStart
  wxStringImpl& replace(size_t nStart, size_t nLen, const wxStringImpl& str)
    { return replace(nStart, nLen, str.c_str(), str.length()); }
    // replaces the substring with nCount copies of ch
  wxStringImpl& replace(size_t nStart, size_t nLen,
                        size_t nCount, wxStringCharType ch)
    { return replace(nStart, nLen, wxStringImpl(nCount, ch)); }
    // replaces a substring with another substring
  wxStringImpl& replace(size_t nStart, size_t nLen,
                        const wxStringImpl& str, size_t nStart2, size_t nLen2)
    { return replace(nStart, nLen, str.substr(nStart2, nLen2)); }
    // replaces the substring with first nCount chars of sz
  wxStringImpl& replace(size_t nStart, size_t nLen,
                        const wxStringCharType* sz, size_t nCount);

  wxStringImpl& replace(iterator first, iterator last, const_pointer s)
    { return replace(first - begin(), last - first, s); }
  wxStringImpl& replace(iterator first, iterator last, const_pointer s,
                        size_type n)
    { return replace(first - begin(), last - first, s, n); }
  wxStringImpl& replace(iterator first, iterator last, const wxStringImpl& s)
    { return replace(first - begin(), last - first, s); }
  wxStringImpl& replace(iterator first, iterator last, size_type n, wxStringCharType c)
    { return replace(first - begin(), last - first, n, c); }
  wxStringImpl& replace(iterator first, iterator last,
                        const_iterator first1, const_iterator last1)
    { return replace(first - begin(), last - first, first1.GetPtr(), last1 - first1); }

    // swap two strings
  void swap(wxStringImpl& str);

    // All find() functions take the nStart argument which specifies the
    // position to start the search on, the default value is 0. All functions
    // return npos if there were no match.

    // find a substring
  size_t find(const wxStringImpl& str, size_t nStart = 0) const;

    // find first n characters of sz
  size_t find(const wxStringCharType* sz, size_t nStart = 0, size_t n = npos) const;

    // find the first occurrence of character ch after nStart
  size_t find(wxStringCharType ch, size_t nStart = 0) const;

    // rfind() family is exactly like find() but works right to left

    // as find, but from the end
  size_t rfind(const wxStringImpl& str, size_t nStart = npos) const;

    // as find, but from the end
  size_t rfind(const wxStringCharType* sz, size_t nStart = npos,
               size_t n = npos) const;
    // as find, but from the end
  size_t rfind(wxStringCharType ch, size_t nStart = npos) const;

  size_type copy(wxStringCharType* s, size_type n, size_type pos = 0);

  // substring extraction
  wxStringImpl substr(size_t nStart = 0, size_t nLen = npos) const;

      // string += string
  wxStringImpl& operator+=(const wxStringImpl& s) { return append(s); }
      // string += C string
  wxStringImpl& operator+=(const wxStringCharType *psz) { return append(psz); }
      // string += char
  wxStringImpl& operator+=(wxStringCharType ch) { return append(1, ch); }

  // helpers for wxStringBuffer and wxStringBufferLength
  wxStringCharType *DoGetWriteBuf(size_t nLen);
  void DoUngetWriteBuf();
  void DoUngetWriteBuf(size_t nLen);

  friend class WXDLLIMPEXP_FWD_BASE wxString;
};

#endif // !wxUSE_STL_BASED_WXSTRING

// don't pollute the library user's name space
#undef wxASSERT_VALID_INDEX

#endif  // _WX_WXSTRINGIMPL_H__
