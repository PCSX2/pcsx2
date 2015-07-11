///////////////////////////////////////////////////////////////////////////////
// Name:        wx/string.h
// Purpose:     wxString class
// Author:      Vadim Zeitlin
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

/*
    Efficient string class [more or less] compatible with MFC CString,
    wxWidgets version 1 wxString and std::string and some handy functions
    missing from string.h.
*/

#ifndef _WX_WXSTRING_H__
#define _WX_WXSTRING_H__

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/defs.h"        // everybody should include this

#if defined(__WXMAC__) || defined(__VISAGECPP__)
    #include <ctype.h>
#endif

#if defined(__VISAGECPP__) && __IBMCPP__ >= 400
   // problem in VACPP V4 with including stdlib.h multiple times
   // strconv includes it anyway
#  include <stdio.h>
#  include <string.h>
#  include <stdarg.h>
#  include <limits.h>
#else
#  include <string.h>
#  include <stdio.h>
#  include <stdarg.h>
#  include <limits.h>
#  include <stdlib.h>
#endif

#include "wx/wxcrtbase.h"   // for wxChar, wxStrlen() etc.
#include "wx/strvararg.h"
#include "wx/buffer.h"      // for wxCharBuffer
#include "wx/strconv.h"     // for wxConvertXXX() macros and wxMBConv classes
#include "wx/stringimpl.h"
#include "wx/stringops.h"
#include "wx/unichar.h"

// by default we cache the mapping of the positions in UTF-8 string to the byte
// offset as this results in noticeable performance improvements for loops over
// strings using indices; comment out this line to disable this
//
// notice that this optimization is well worth using even in debug builds as it
// changes asymptotic complexity of algorithms using indices to iterate over
// wxString back to expected linear from quadratic
//
// also notice that wxTLS_TYPE() (__declspec(thread) in this case) is unsafe to
// use in DLL build under pre-Vista Windows so we disable this code for now, if
// anybody really needs to use UTF-8 build under Windows with this optimization
// it would have to be re-tested and probably corrected
// CS: under OSX release builds the string destructor/cache cleanup sometimes
// crashes, disable until we find the true reason or a better workaround
#if wxUSE_UNICODE_UTF8 && !defined(__WINDOWS__) && !defined(__WXOSX__)
    #define wxUSE_STRING_POS_CACHE 1
#else
    #define wxUSE_STRING_POS_CACHE 0
#endif

#if wxUSE_STRING_POS_CACHE
    #include "wx/tls.h"

    // change this 0 to 1 to enable additional (very expensive) asserts
    // verifying that string caching logic works as expected
    #if 0
        #define wxSTRING_CACHE_ASSERT(cond) wxASSERT(cond)
    #else
        #define wxSTRING_CACHE_ASSERT(cond)
    #endif
#endif // wxUSE_STRING_POS_CACHE

class WXDLLIMPEXP_FWD_BASE wxString;

// unless this symbol is predefined to disable the compatibility functions, do
// use them
#ifndef WXWIN_COMPATIBILITY_STRING_PTR_AS_ITER
    #define WXWIN_COMPATIBILITY_STRING_PTR_AS_ITER 1
#endif

namespace wxPrivate
{
    template <typename T> struct wxStringAsBufHelper;
}

// ---------------------------------------------------------------------------
// macros
// ---------------------------------------------------------------------------

// casts [unfortunately!] needed to call some broken functions which require
// "char *" instead of "const char *"
#define   WXSTRINGCAST (wxChar *)(const wxChar *)
#define   wxCSTRINGCAST (wxChar *)(const wxChar *)
#define   wxMBSTRINGCAST (char *)(const char *)
#define   wxWCSTRINGCAST (wchar_t *)(const wchar_t *)

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

#if WXWIN_COMPATIBILITY_2_6

// deprecated in favour of wxString::npos, don't use in new code
//
// maximum possible length for a string means "take all string" everywhere
#define wxSTRING_MAXLEN wxString::npos

#endif // WXWIN_COMPATIBILITY_2_6

// ---------------------------------------------------------------------------
// global functions complementing standard C string library replacements for
// strlen() and portable strcasecmp()
//---------------------------------------------------------------------------

#if WXWIN_COMPATIBILITY_2_8
// Use wxXXX() functions from wxcrt.h instead! These functions are for
// backwards compatibility only.

// checks whether the passed in pointer is NULL and if the string is empty
wxDEPRECATED_MSG("use wxIsEmpty() instead")
inline bool IsEmpty(const char *p) { return (!p || !*p); }

// safe version of strlen() (returns 0 if passed NULL pointer)
wxDEPRECATED_MSG("use wxStrlen() instead")
inline size_t Strlen(const char *psz)
  { return psz ? strlen(psz) : 0; }

// portable strcasecmp/_stricmp
wxDEPRECATED_MSG("use wxStricmp() instead")
inline int Stricmp(const char *psz1, const char *psz2)
    { return wxCRT_StricmpA(psz1, psz2); }

#endif // WXWIN_COMPATIBILITY_2_8

// ----------------------------------------------------------------------------
// wxCStrData
// ----------------------------------------------------------------------------

// Lightweight object returned by wxString::c_str() and implicitly convertible
// to either const char* or const wchar_t*.
class wxCStrData
{
private:
    // Ctors; for internal use by wxString and wxCStrData only
    wxCStrData(const wxString *str, size_t offset = 0, bool owned = false)
        : m_str(str), m_offset(offset), m_owned(owned) {}

public:
    // Ctor constructs the object from char literal; they are needed to make
    // operator?: compile and they intentionally take char*, not const char*
    inline wxCStrData(char *buf);
    inline wxCStrData(wchar_t *buf);
    inline wxCStrData(const wxCStrData& data);

    inline ~wxCStrData();

    // AsWChar() and AsChar() can't be defined here as they use wxString and so
    // must come after it and because of this won't be inlined when called from
    // wxString methods (without a lot of work to extract these wxString methods
    // from inside the class itself). But we still define them being inline
    // below to let compiler inline them from elsewhere. And because of this we
    // must declare them as inline here because otherwise some compilers give
    // warnings about them, e.g. mingw32 3.4.5 warns about "<symbol> defined
    // locally after being referenced with dllimport linkage" while IRIX
    // mipsPro 7.4 warns about "function declared inline after being called".
    inline const wchar_t* AsWChar() const;
    operator const wchar_t*() const { return AsWChar(); }

    inline const char* AsChar() const;
    const unsigned char* AsUnsignedChar() const
        { return (const unsigned char *) AsChar(); }
    operator const char*() const { return AsChar(); }
    operator const unsigned char*() const { return AsUnsignedChar(); }

    operator const void*() const { return AsChar(); }

    // returns buffers that are valid as long as the associated wxString exists
    const wxScopedCharBuffer AsCharBuf() const
    {
        return wxScopedCharBuffer::CreateNonOwned(AsChar());
    }

    const wxScopedWCharBuffer AsWCharBuf() const
    {
        return wxScopedWCharBuffer::CreateNonOwned(AsWChar());
    }

    inline wxString AsString() const;

    // returns the value as C string in internal representation (equivalent
    // to AsString().wx_str(), but more efficient)
    const wxStringCharType *AsInternal() const;

    // allow expressions like "c_str()[0]":
    inline wxUniChar operator[](size_t n) const;
    wxUniChar operator[](int n) const { return operator[](size_t(n)); }
    wxUniChar operator[](long n) const { return operator[](size_t(n)); }
#ifndef wxSIZE_T_IS_UINT
    wxUniChar operator[](unsigned int n) const { return operator[](size_t(n)); }
#endif // size_t != unsigned int

    // These operators are needed to emulate the pointer semantics of c_str():
    // expressions like "wxChar *p = str.c_str() + 1;" should continue to work
    // (we need both versions to resolve ambiguities). Note that this means
    // the 'n' value is interpreted as addition to char*/wchar_t* pointer, it
    // is *not* number of Unicode characters in wxString.
    wxCStrData operator+(int n) const
        { return wxCStrData(m_str, m_offset + n, m_owned); }
    wxCStrData operator+(long n) const
        { return wxCStrData(m_str, m_offset + n, m_owned); }
    wxCStrData operator+(size_t n) const
        { return wxCStrData(m_str, m_offset + n, m_owned); }

    // and these for "str.c_str() + (p2 - p1)" (it also works for any integer
    // expression but it must be ptrdiff_t and not e.g. int to work in this
    // example):
    wxCStrData operator-(ptrdiff_t n) const
    {
        wxASSERT_MSG( n <= (ptrdiff_t)m_offset,
                      wxT("attempt to construct address before the beginning of the string") );
        return wxCStrData(m_str, m_offset - n, m_owned);
    }

    // this operator is needed to make expressions like "*c_str()" or
    // "*(c_str() + 2)" work
    inline wxUniChar operator*() const;

private:
    // the wxString this object was returned for
    const wxString *m_str;
    // Offset into c_str() return value. Note that this is *not* offset in
    // m_str in Unicode characters. Instead, it is index into the
    // char*/wchar_t* buffer returned by c_str(). It's interpretation depends
    // on how is the wxCStrData instance used: if it is eventually cast to
    // const char*, m_offset will be in bytes form string's start; if it is
    // cast to const wchar_t*, it will be in wchar_t values.
    size_t m_offset;
    // should m_str be deleted, i.e. is it owned by us?
    bool m_owned;

    friend class WXDLLIMPEXP_FWD_BASE wxString;
};

// ----------------------------------------------------------------------------
// wxStringPrintfMixin
// ---------------------------------------------------------------------------

// NB: VC6 has a bug that causes linker errors if you have template methods
//     in a class using __declspec(dllimport). The solution is to split such
//     class into two classes, one that contains the template methods and does
//     *not* use WXDLLIMPEXP_BASE and another class that contains the rest
//     (with DLL linkage).
//
//     We only do this for VC6 here, because the code is less efficient
//     (Printf() has to use dynamic_cast<>) and because OpenWatcom compiler
//     cannot compile this code.

#if defined(__VISUALC__) && __VISUALC__ < 1300
    #define wxNEEDS_WXSTRING_PRINTF_MIXIN
#endif

#ifdef wxNEEDS_WXSTRING_PRINTF_MIXIN
// this class contains implementation of wxString's vararg methods, it's
// exported from wxBase DLL
class WXDLLIMPEXP_BASE wxStringPrintfMixinBase
{
protected:
    wxStringPrintfMixinBase() {}

#if !wxUSE_UTF8_LOCALE_ONLY
    int DoPrintfWchar(const wxChar *format, ...);
    static wxString DoFormatWchar(const wxChar *format, ...);
#endif
#if wxUSE_UNICODE_UTF8
    int DoPrintfUtf8(const char *format, ...);
    static wxString DoFormatUtf8(const char *format, ...);
#endif
};

// this class contains template wrappers for wxString's vararg methods, it's
// intentionally *not* exported from the DLL in order to fix the VC6 bug
// described above
class wxStringPrintfMixin : public wxStringPrintfMixinBase
{
private:
    // to further complicate things, we can't return wxString from
    // wxStringPrintfMixin::Format() because wxString is not yet declared at
    // this point; the solution is to use this fake type trait template - this
    // way the compiler won't know the return type until Format() is used
    // (this doesn't compile with Watcom, but VC6 compiles it just fine):
    template<typename T> struct StringReturnType
    {
        typedef wxString type;
    };

public:
    // these are duplicated wxString methods, they're also declared below
    // if !wxNEEDS_WXSTRING_PRINTF_MIXIN:

    // static wxString Format(const wString& format, ...) WX_ATTRIBUTE_PRINTF_1;
    WX_DEFINE_VARARG_FUNC_SANS_N0(static typename StringReturnType<T1>::type,
                                  Format, 1, (const wxFormatString&),
                                  DoFormatWchar, DoFormatUtf8)
    // We have to implement the version without template arguments manually
    // because of the StringReturnType<> hack, although WX_DEFINE_VARARG_FUNC
    // normally does it itself. It has to be a template so that we can use
    // the hack, even though there's no real template parameter. We can't move
    // it to wxStrig, because it would shadow these versions of Format() then.
    template<typename T>
    inline static typename StringReturnType<T>::type
    Format(const T& fmt)
    {
        // NB: this doesn't compile if T is not (some form of) a string;
        //     this makes Format's prototype equivalent to
        //     Format(const wxFormatString& fmt)
        return DoFormatWchar(wxFormatString(fmt));
    }

    // int Printf(const wxString& format, ...);
    WX_DEFINE_VARARG_FUNC(int, Printf, 1, (const wxFormatString&),
                          DoPrintfWchar, DoPrintfUtf8)
    // int sprintf(const wxString& format, ...) WX_ATTRIBUTE_PRINTF_2;
    WX_DEFINE_VARARG_FUNC(int, sprintf, 1, (const wxFormatString&),
                          DoPrintfWchar, DoPrintfUtf8)

protected:
    wxStringPrintfMixin() : wxStringPrintfMixinBase() {}
};
#endif // wxNEEDS_WXSTRING_PRINTF_MIXIN


// ----------------------------------------------------------------------------
// wxString: string class trying to be compatible with std::string, MFC
//           CString and wxWindows 1.x wxString all at once
// ---------------------------------------------------------------------------

#ifdef wxNEEDS_WXSTRING_PRINTF_MIXIN
    // "non dll-interface class 'wxStringPrintfMixin' used as base interface
    // for dll-interface class 'wxString'" -- this is OK in our case
    #pragma warning (push)
    #pragma warning (disable:4275)
#endif

#if wxUSE_UNICODE_UTF8
// see the comment near wxString::iterator for why we need this
class WXDLLIMPEXP_BASE wxStringIteratorNode
{
public:
    wxStringIteratorNode()
        : m_str(NULL), m_citer(NULL), m_iter(NULL), m_prev(NULL), m_next(NULL) {}
    wxStringIteratorNode(const wxString *str,
                          wxStringImpl::const_iterator *citer)
        { DoSet(str, citer, NULL); }
    wxStringIteratorNode(const wxString *str, wxStringImpl::iterator *iter)
        { DoSet(str, NULL, iter); }
    ~wxStringIteratorNode()
        { clear(); }

    inline void set(const wxString *str, wxStringImpl::const_iterator *citer)
        { clear(); DoSet(str, citer, NULL); }
    inline void set(const wxString *str, wxStringImpl::iterator *iter)
        { clear(); DoSet(str, NULL, iter); }

    const wxString *m_str;
    wxStringImpl::const_iterator *m_citer;
    wxStringImpl::iterator *m_iter;
    wxStringIteratorNode *m_prev, *m_next;

private:
    inline void clear();
    inline void DoSet(const wxString *str,
                      wxStringImpl::const_iterator *citer,
                      wxStringImpl::iterator *iter);

    // the node belongs to a particular iterator instance, it's not copied
    // when a copy of the iterator is made
    wxDECLARE_NO_COPY_CLASS(wxStringIteratorNode);
};
#endif // wxUSE_UNICODE_UTF8

class WXDLLIMPEXP_BASE wxString
#ifdef wxNEEDS_WXSTRING_PRINTF_MIXIN
                                : public wxStringPrintfMixin
#endif
{
  // NB: special care was taken in arranging the member functions in such order
  //     that all inline functions can be effectively inlined, verify that all
  //     performance critical functions are still inlined if you change order!
public:
  // an 'invalid' value for string index, moved to this place due to a CW bug
  static const size_t npos;

private:
  // if we hadn't made these operators private, it would be possible to
  // compile "wxString s; s = 17;" without any warnings as 17 is implicitly
  // converted to char in C and we do have operator=(char)
  //
  // NB: we don't need other versions (short/long and unsigned) as attempt
  //     to assign another numeric type to wxString will now result in
  //     ambiguity between operator=(char) and operator=(int)
  wxString& operator=(int);

  // these methods are not implemented - there is _no_ conversion from int to
  // string, you're doing something wrong if the compiler wants to call it!
  //
  // try `s << i' or `s.Printf("%d", i)' instead
  wxString(int);


  // buffer for holding temporary substring when using any of the methods
  // that take (char*,size_t) or (wchar_t*,size_t) arguments:
  template<typename T>
  struct SubstrBufFromType
  {
      T data;
      size_t len;

      SubstrBufFromType(const T& data_, size_t len_)
          : data(data_), len(len_)
      {
          wxASSERT_MSG( len != npos, "must have real length" );
      }
  };

#if wxUSE_UNICODE_UTF8
  // even char* -> char* needs conversion, from locale charset to UTF-8
  typedef SubstrBufFromType<wxScopedCharBuffer>    SubstrBufFromWC;
  typedef SubstrBufFromType<wxScopedCharBuffer>    SubstrBufFromMB;
#elif wxUSE_UNICODE_WCHAR
  typedef SubstrBufFromType<const wchar_t*>        SubstrBufFromWC;
  typedef SubstrBufFromType<wxScopedWCharBuffer>   SubstrBufFromMB;
#else
  typedef SubstrBufFromType<const char*>           SubstrBufFromMB;
  typedef SubstrBufFromType<wxScopedCharBuffer>    SubstrBufFromWC;
#endif


  // Functions implementing primitive operations on string data; wxString
  // methods and iterators are implemented in terms of it. The differences
  // between UTF-8 and wchar_t* representations of the string are mostly
  // contained here.

#if wxUSE_UNICODE_UTF8
  static SubstrBufFromMB ConvertStr(const char *psz, size_t nLength,
                                    const wxMBConv& conv);
  static SubstrBufFromWC ConvertStr(const wchar_t *pwz, size_t nLength,
                                    const wxMBConv& conv);
#elif wxUSE_UNICODE_WCHAR
  static SubstrBufFromMB ConvertStr(const char *psz, size_t nLength,
                                    const wxMBConv& conv);
#else
  static SubstrBufFromWC ConvertStr(const wchar_t *pwz, size_t nLength,
                                    const wxMBConv& conv);
#endif

#if !wxUSE_UNICODE_UTF8 // wxUSE_UNICODE_WCHAR or !wxUSE_UNICODE
  // returns C string encoded as the implementation expects:
  #if wxUSE_UNICODE
  static const wchar_t* ImplStr(const wchar_t* str)
    { return str ? str : wxT(""); }
  static const SubstrBufFromWC ImplStr(const wchar_t* str, size_t n)
    { return SubstrBufFromWC(str, (str && n == npos) ? wxWcslen(str) : n); }
  static wxScopedWCharBuffer ImplStr(const char* str,
                                     const wxMBConv& conv = wxConvLibc)
    { return ConvertStr(str, npos, conv).data; }
  static SubstrBufFromMB ImplStr(const char* str, size_t n,
                                 const wxMBConv& conv = wxConvLibc)
    { return ConvertStr(str, n, conv); }
  #else
  static const char* ImplStr(const char* str,
                             const wxMBConv& WXUNUSED(conv) = wxConvLibc)
    { return str ? str : ""; }
  static const SubstrBufFromMB ImplStr(const char* str, size_t n,
                                       const wxMBConv& WXUNUSED(conv) = wxConvLibc)
    { return SubstrBufFromMB(str, (str && n == npos) ? wxStrlen(str) : n); }
  static wxScopedCharBuffer ImplStr(const wchar_t* str)
    { return ConvertStr(str, npos, wxConvLibc).data; }
  static SubstrBufFromWC ImplStr(const wchar_t* str, size_t n)
    { return ConvertStr(str, n, wxConvLibc); }
  #endif

  // translates position index in wxString to/from index in underlying
  // wxStringImpl:
  static size_t PosToImpl(size_t pos) { return pos; }
  static void PosLenToImpl(size_t pos, size_t len,
                           size_t *implPos, size_t *implLen)
    { *implPos = pos; *implLen = len; }
  static size_t LenToImpl(size_t len) { return len; }
  static size_t PosFromImpl(size_t pos) { return pos; }

  // we don't want to define these as empty inline functions as it could
  // result in noticeable (and quite unnecessary in non-UTF-8 build) slowdown
  // in debug build where the inline functions are not effectively inlined
  #define wxSTRING_INVALIDATE_CACHE()
  #define wxSTRING_INVALIDATE_CACHED_LENGTH()
  #define wxSTRING_UPDATE_CACHED_LENGTH(n)
  #define wxSTRING_SET_CACHED_LENGTH(n)

#else // wxUSE_UNICODE_UTF8

  static wxScopedCharBuffer ImplStr(const char* str,
                                    const wxMBConv& conv = wxConvLibc)
    { return ConvertStr(str, npos, conv).data; }
  static SubstrBufFromMB ImplStr(const char* str, size_t n,
                                 const wxMBConv& conv = wxConvLibc)
    { return ConvertStr(str, n, conv); }

  static wxScopedCharBuffer ImplStr(const wchar_t* str)
    { return ConvertStr(str, npos, wxMBConvUTF8()).data; }
  static SubstrBufFromWC ImplStr(const wchar_t* str, size_t n)
    { return ConvertStr(str, n, wxMBConvUTF8()); }

#if wxUSE_STRING_POS_CACHE
  // this is an extremely simple cache used by PosToImpl(): each cache element
  // contains the string it applies to and the index corresponding to the last
  // used position in this wxString in its m_impl string
  //
  // NB: notice that this struct (and nested Element one) must be a POD or we
  //     wouldn't be able to use a thread-local variable of this type, in
  //     particular it should have no ctor -- we rely on statics being
  //     initialized to 0 instead
  struct Cache
  {
      enum { SIZE = 8 };

      struct Element
      {
          const wxString *str;  // the string to which this element applies
          size_t pos,           // the cached index in this string
                 impl,          // the corresponding position in its m_impl
                 len;           // cached length or npos if unknown

          // reset cached index to 0
          void ResetPos() { pos = impl = 0; }

          // reset position and length
          void Reset() { ResetPos(); len = npos; }
      };

      // cache the indices mapping for the last few string used
      Element cached[SIZE];

      // the last used index
      unsigned lastUsed;
  };

#ifndef wxHAS_COMPILER_TLS
  // we must use an accessor function and not a static variable when the TLS
  // variables support is implemented in the library (and not by the compiler)
  // because the global s_cache variable could be not yet initialized when a
  // ctor of another global object is executed and if that ctor uses any
  // wxString methods, bad things happen
  //
  // however notice that this approach does not work when compiler TLS is used,
  // at least not with g++ 4.1.2 under amd64 as it apparently compiles code
  // using this accessor incorrectly when optimizations are enabled (-O2 is
  // enough) -- luckily we don't need it then neither as static __thread
  // variables are initialized by 0 anyhow then and so we can use the variable
  // directly
  WXEXPORT static Cache& GetCache()
  {
      static wxTLS_TYPE(Cache) s_cache;

      return wxTLS_VALUE(s_cache);
  }

  // this helper struct is used to ensure that GetCache() is called during
  // static initialization time, i.e. before any threads creation, as otherwise
  // the static s_cache construction inside GetCache() wouldn't be MT-safe
  friend struct wxStrCacheInitializer;
#else // wxHAS_COMPILER_TLS
  static wxTLS_TYPE(Cache) ms_cache;
  static Cache& GetCache() { return wxTLS_VALUE(ms_cache); }
#endif // !wxHAS_COMPILER_TLS/wxHAS_COMPILER_TLS

  static Cache::Element *GetCacheBegin() { return GetCache().cached; }
  static Cache::Element *GetCacheEnd() { return GetCacheBegin() + Cache::SIZE; }
  static unsigned& LastUsedCacheElement() { return GetCache().lastUsed; }

  // this is used in debug builds only to provide a convenient function,
  // callable from a debugger, to show the cache contents
  friend struct wxStrCacheDumper;

  // uncomment this to have access to some profiling statistics on program
  // termination
  //#define wxPROFILE_STRING_CACHE

#ifdef wxPROFILE_STRING_CACHE
  static struct PosToImplCacheStats
  {
      unsigned postot,  // total non-trivial calls to PosToImpl
               poshits, // cache hits from PosToImpl()
               mishits, // cached position beyond the needed one
               sumpos,  // sum of all positions, used to compute the
                        // average position after dividing by postot
               sumofs,  // sum of all offsets after using the cache, used to
                        // compute the average after dividing by hits
               lentot,  // number of total calls to length()
               lenhits; // number of cache hits in length()
  } ms_cacheStats;

  friend struct wxStrCacheStatsDumper;

  #define wxCACHE_PROFILE_FIELD_INC(field) ms_cacheStats.field++
  #define wxCACHE_PROFILE_FIELD_ADD(field, val) ms_cacheStats.field += (val)
#else // !wxPROFILE_STRING_CACHE
  #define wxCACHE_PROFILE_FIELD_INC(field)
  #define wxCACHE_PROFILE_FIELD_ADD(field, val)
#endif // wxPROFILE_STRING_CACHE/!wxPROFILE_STRING_CACHE

  // note: it could seem that the functions below shouldn't be inline because
  // they are big, contain loops and so the compiler shouldn't be able to
  // inline them anyhow, however moving them into string.cpp does decrease the
  // code performance by ~5%, at least when using g++ 4.1 so do keep them here
  // unless tests show that it's not advantageous any more

  // return the pointer to the cache element for this string or NULL if not
  // cached
  Cache::Element *FindCacheElement() const
  {
      // profiling seems to show a small but consistent gain if we use this
      // simple loop instead of starting from the last used element (there are
      // a lot of misses in this function...)
      Cache::Element * const cacheBegin = GetCacheBegin();
#ifndef wxHAS_COMPILER_TLS
      // during destruction tls calls may return NULL, in this case return NULL
      // immediately without accessing anything else
      if ( cacheBegin == NULL )
        return NULL;
#endif
      Cache::Element * const cacheEnd = GetCacheEnd();
      for ( Cache::Element *c = cacheBegin; c != cacheEnd; c++ )
      {
          if ( c->str == this )
              return c;
      }

      return NULL;
  }

  // unlike FindCacheElement(), this one always returns a valid pointer to the
  // cache element for this string, it may have valid last cached position and
  // its corresponding index in the byte string or not
  Cache::Element *GetCacheElement() const
  {
      Cache::Element * const cacheBegin = GetCacheBegin();
      Cache::Element * const cacheEnd = GetCacheEnd();
      Cache::Element * const cacheStart = cacheBegin + LastUsedCacheElement();

      // check the last used first, this does no (measurable) harm for a miss
      // but does help for simple loops addressing the same string all the time
      if ( cacheStart->str == this )
          return cacheStart;

      // notice that we're going to check cacheStart again inside this call but
      // profiling shows that it's still faster to use a simple loop like
      // inside FindCacheElement() than manually looping with wrapping starting
      // from the cache entry after the start one
      Cache::Element *c = FindCacheElement();
      if ( !c )
      {
          // claim the next cache entry for this string
          c = cacheStart;
          if ( ++c == cacheEnd )
              c = cacheBegin;

          c->str = this;
          c->Reset();

          // and remember the last used element
          LastUsedCacheElement() = c - cacheBegin;
      }

      return c;
  }

  size_t DoPosToImpl(size_t pos) const
  {
      wxCACHE_PROFILE_FIELD_INC(postot);

      // NB: although the case of pos == 1 (and offset from cached position
      //     equal to 1) are common, nothing is gained by writing special code
      //     for handling them, the compiler (at least g++ 4.1 used) seems to
      //     optimize the code well enough on its own

      wxCACHE_PROFILE_FIELD_ADD(sumpos, pos);

      Cache::Element * const cache = GetCacheElement();

      // cached position can't be 0 so if it is, it means that this entry was
      // used for length caching only so far, i.e. it doesn't count as a hit
      // from our point of view
      if ( cache->pos )
      {
          wxCACHE_PROFILE_FIELD_INC(poshits);
      }

      if ( pos == cache->pos )
          return cache->impl;

      // this seems to happen only rarely so just reset the cache in this case
      // instead of complicating code even further by seeking backwards in this
      // case
      if ( cache->pos > pos )
      {
          wxCACHE_PROFILE_FIELD_INC(mishits);

          cache->ResetPos();
      }

      wxCACHE_PROFILE_FIELD_ADD(sumofs, pos - cache->pos);


      wxStringImpl::const_iterator i(m_impl.begin() + cache->impl);
      for ( size_t n = cache->pos; n < pos; n++ )
          wxStringOperations::IncIter(i);

      cache->pos = pos;
      cache->impl = i - m_impl.begin();

      wxSTRING_CACHE_ASSERT(
          (int)cache->impl == (begin() + pos).impl() - m_impl.begin() );

      return cache->impl;
  }

  void InvalidateCache()
  {
      Cache::Element * const cache = FindCacheElement();
      if ( cache )
          cache->Reset();
  }

  void InvalidateCachedLength()
  {
      Cache::Element * const cache = FindCacheElement();
      if ( cache )
          cache->len = npos;
  }

  void SetCachedLength(size_t len)
  {
      // we optimistically cache the length here even if the string wasn't
      // present in the cache before, this seems to do no harm and the
      // potential for avoiding length recomputation for long strings looks
      // interesting
      GetCacheElement()->len = len;
  }

  void UpdateCachedLength(ptrdiff_t delta)
  {
      Cache::Element * const cache = FindCacheElement();
      if ( cache && cache->len != npos )
      {
          wxSTRING_CACHE_ASSERT( (ptrdiff_t)cache->len + delta >= 0 );

          cache->len += delta;
      }
  }

  #define wxSTRING_INVALIDATE_CACHE() InvalidateCache()
  #define wxSTRING_INVALIDATE_CACHED_LENGTH() InvalidateCachedLength()
  #define wxSTRING_UPDATE_CACHED_LENGTH(n) UpdateCachedLength(n)
  #define wxSTRING_SET_CACHED_LENGTH(n) SetCachedLength(n)
#else // !wxUSE_STRING_POS_CACHE
  size_t DoPosToImpl(size_t pos) const
  {
      return (begin() + pos).impl() - m_impl.begin();
  }

  #define wxSTRING_INVALIDATE_CACHE()
  #define wxSTRING_INVALIDATE_CACHED_LENGTH()
  #define wxSTRING_UPDATE_CACHED_LENGTH(n)
  #define wxSTRING_SET_CACHED_LENGTH(n)
#endif // wxUSE_STRING_POS_CACHE/!wxUSE_STRING_POS_CACHE

  size_t PosToImpl(size_t pos) const
  {
      return pos == 0 || pos == npos ? pos : DoPosToImpl(pos);
  }

  void PosLenToImpl(size_t pos, size_t len, size_t *implPos, size_t *implLen) const;

  size_t LenToImpl(size_t len) const
  {
      size_t pos, len2;
      PosLenToImpl(0, len, &pos, &len2);
      return len2;
  }

  size_t PosFromImpl(size_t pos) const
  {
      if ( pos == 0 || pos == npos )
          return pos;
      else
          return const_iterator(this, m_impl.begin() + pos) - begin();
  }
#endif // !wxUSE_UNICODE_UTF8/wxUSE_UNICODE_UTF8

public:
  // standard types
  typedef wxUniChar value_type;
  typedef wxUniChar char_type;
  typedef wxUniCharRef reference;
  typedef wxChar* pointer;
  typedef const wxChar* const_pointer;

  typedef size_t size_type;
  typedef wxUniChar const_reference;

#if wxUSE_STD_STRING
  #if wxUSE_UNICODE_UTF8
    // random access is not O(1), as required by Random Access Iterator
    #define WX_STR_ITERATOR_TAG std::bidirectional_iterator_tag
  #else
    #define WX_STR_ITERATOR_TAG std::random_access_iterator_tag
  #endif
  #define WX_DEFINE_ITERATOR_CATEGORY(cat) typedef cat iterator_category;
#else
  // not defining iterator_category at all in this case is better than defining
  // it as some dummy type -- at least it results in more intelligible error
  // messages
  #define WX_DEFINE_ITERATOR_CATEGORY(cat)
#endif

  #define WX_STR_ITERATOR_IMPL(iterator_name, pointer_type, reference_type) \
      private:                                                              \
          typedef wxStringImpl::iterator_name underlying_iterator;          \
      public:                                                               \
          WX_DEFINE_ITERATOR_CATEGORY(WX_STR_ITERATOR_TAG)                  \
          typedef wxUniChar value_type;                                     \
          typedef ptrdiff_t difference_type;                                \
          typedef reference_type reference;                                 \
          typedef pointer_type pointer;                                     \
                                                                            \
          reference operator[](size_t n) const { return *(*this + n); }     \
                                                                            \
          iterator_name& operator++()                                       \
            { wxStringOperations::IncIter(m_cur); return *this; }           \
          iterator_name& operator--()                                       \
            { wxStringOperations::DecIter(m_cur); return *this; }           \
          iterator_name operator++(int)                                     \
          {                                                                 \
              iterator_name tmp = *this;                                    \
              wxStringOperations::IncIter(m_cur);                           \
              return tmp;                                                   \
          }                                                                 \
          iterator_name operator--(int)                                     \
          {                                                                 \
              iterator_name tmp = *this;                                    \
              wxStringOperations::DecIter(m_cur);                           \
              return tmp;                                                   \
          }                                                                 \
                                                                            \
          iterator_name& operator+=(ptrdiff_t n)                            \
          {                                                                 \
              m_cur = wxStringOperations::AddToIter(m_cur, n);              \
              return *this;                                                 \
          }                                                                 \
          iterator_name& operator-=(ptrdiff_t n)                            \
          {                                                                 \
              m_cur = wxStringOperations::AddToIter(m_cur, -n);             \
              return *this;                                                 \
          }                                                                 \
                                                                            \
          difference_type operator-(const iterator_name& i) const           \
            { return wxStringOperations::DiffIters(m_cur, i.m_cur); }       \
                                                                            \
          bool operator==(const iterator_name& i) const                     \
            { return m_cur == i.m_cur; }                                    \
          bool operator!=(const iterator_name& i) const                     \
            { return m_cur != i.m_cur; }                                    \
                                                                            \
          bool operator<(const iterator_name& i) const                      \
            { return m_cur < i.m_cur; }                                     \
          bool operator>(const iterator_name& i) const                      \
            { return m_cur > i.m_cur; }                                     \
          bool operator<=(const iterator_name& i) const                     \
            { return m_cur <= i.m_cur; }                                    \
          bool operator>=(const iterator_name& i) const                     \
            { return m_cur >= i.m_cur; }                                    \
                                                                            \
      private:                                                              \
          /* for internal wxString use only: */                             \
          underlying_iterator impl() const { return m_cur; }                \
                                                                            \
          friend class wxString;                                            \
          friend class wxCStrData;                                          \
                                                                            \
      private:                                                              \
          underlying_iterator m_cur

  class WXDLLIMPEXP_FWD_BASE const_iterator;

#if wxUSE_UNICODE_UTF8
  // NB: In UTF-8 build, (non-const) iterator needs to keep reference
  //     to the underlying wxStringImpl, because UTF-8 is variable-length
  //     encoding and changing the value pointer to by an iterator (using
  //     its operator*) requires calling wxStringImpl::replace() if the old
  //     and new values differ in their encoding's length.
  //
  //     Furthermore, the replace() call may invalid all iterators for the
  //     string, so we have to keep track of outstanding iterators and update
  //     them if replace() happens.
  //
  //     This is implemented by maintaining linked list of iterators for every
  //     string and traversing it in wxUniCharRef::operator=(). Head of the
  //     list is stored in wxString. (FIXME-UTF8)

  class WXDLLIMPEXP_BASE iterator
  {
      WX_STR_ITERATOR_IMPL(iterator, wxChar*, wxUniCharRef);

  public:
      iterator() {}
      iterator(const iterator& i)
          : m_cur(i.m_cur), m_node(i.str(), &m_cur) {}
      iterator& operator=(const iterator& i)
      {
          if (&i != this)
          {
              m_cur = i.m_cur;
              m_node.set(i.str(), &m_cur);
          }
          return *this;
      }

      reference operator*()
        { return wxUniCharRef::CreateForString(*str(), m_cur); }

      iterator operator+(ptrdiff_t n) const
        { return iterator(str(), wxStringOperations::AddToIter(m_cur, n)); }
      iterator operator-(ptrdiff_t n) const
        { return iterator(str(), wxStringOperations::AddToIter(m_cur, -n)); }

      // Normal iterators need to be comparable with the const_iterators so
      // declare the comparison operators and implement them below after the
      // full const_iterator declaration.
      bool operator==(const const_iterator& i) const;
      bool operator!=(const const_iterator& i) const;
      bool operator<(const const_iterator& i) const;
      bool operator>(const const_iterator& i) const;
      bool operator<=(const const_iterator& i) const;
      bool operator>=(const const_iterator& i) const;

  private:
      iterator(wxString *wxstr, underlying_iterator ptr)
          : m_cur(ptr), m_node(wxstr, &m_cur) {}

      wxString* str() const { return const_cast<wxString*>(m_node.m_str); }

      wxStringIteratorNode m_node;

      friend class const_iterator;
  };

  class WXDLLIMPEXP_BASE const_iterator
  {
      // NB: reference_type is intentionally value, not reference, the character
      //     may be encoded differently in wxString data:
      WX_STR_ITERATOR_IMPL(const_iterator, const wxChar*, wxUniChar);

  public:
      const_iterator() {}
      const_iterator(const const_iterator& i)
          : m_cur(i.m_cur), m_node(i.str(), &m_cur) {}
      const_iterator(const iterator& i)
          : m_cur(i.m_cur), m_node(i.str(), &m_cur) {}

      const_iterator& operator=(const const_iterator& i)
      {
          if (&i != this)
          {
              m_cur = i.m_cur;
              m_node.set(i.str(), &m_cur);
          }
          return *this;
      }
      const_iterator& operator=(const iterator& i)
        { m_cur = i.m_cur; m_node.set(i.str(), &m_cur); return *this; }

      reference operator*() const
        { return wxStringOperations::DecodeChar(m_cur); }

      const_iterator operator+(ptrdiff_t n) const
        { return const_iterator(str(), wxStringOperations::AddToIter(m_cur, n)); }
      const_iterator operator-(ptrdiff_t n) const
        { return const_iterator(str(), wxStringOperations::AddToIter(m_cur, -n)); }

      // Notice that comparison operators taking non-const iterator are not
      // needed here because of the implicit conversion from non-const iterator
      // to const ones ensure that the versions for const_iterator declared
      // inside WX_STR_ITERATOR_IMPL can be used.

  private:
      // for internal wxString use only:
      const_iterator(const wxString *wxstr, underlying_iterator ptr)
          : m_cur(ptr), m_node(wxstr, &m_cur) {}

      const wxString* str() const { return m_node.m_str; }

      wxStringIteratorNode m_node;
  };

  size_t IterToImplPos(wxString::iterator i) const
    { return wxStringImpl::const_iterator(i.impl()) - m_impl.begin(); }

  iterator GetIterForNthChar(size_t n)
    { return iterator(this, m_impl.begin() + PosToImpl(n)); }
  const_iterator GetIterForNthChar(size_t n) const
    { return const_iterator(this, m_impl.begin() + PosToImpl(n)); }
#else // !wxUSE_UNICODE_UTF8

  class WXDLLIMPEXP_BASE iterator
  {
      WX_STR_ITERATOR_IMPL(iterator, wxChar*, wxUniCharRef);

  public:
      iterator() {}
      iterator(const iterator& i) : m_cur(i.m_cur) {}

      reference operator*()
        { return wxUniCharRef::CreateForString(m_cur); }

      iterator operator+(ptrdiff_t n) const
        { return iterator(wxStringOperations::AddToIter(m_cur, n)); }
      iterator operator-(ptrdiff_t n) const
        { return iterator(wxStringOperations::AddToIter(m_cur, -n)); }

      // As in UTF-8 case above, define comparison operators taking
      // const_iterator too.
      bool operator==(const const_iterator& i) const;
      bool operator!=(const const_iterator& i) const;
      bool operator<(const const_iterator& i) const;
      bool operator>(const const_iterator& i) const;
      bool operator<=(const const_iterator& i) const;
      bool operator>=(const const_iterator& i) const;

  private:
      // for internal wxString use only:
      iterator(underlying_iterator ptr) : m_cur(ptr) {}
      iterator(wxString *WXUNUSED(str), underlying_iterator ptr) : m_cur(ptr) {}

      friend class const_iterator;
  };

  class WXDLLIMPEXP_BASE const_iterator
  {
      // NB: reference_type is intentionally value, not reference, the character
      //     may be encoded differently in wxString data:
      WX_STR_ITERATOR_IMPL(const_iterator, const wxChar*, wxUniChar);

  public:
      const_iterator() {}
      const_iterator(const const_iterator& i) : m_cur(i.m_cur) {}
      const_iterator(const iterator& i) : m_cur(i.m_cur) {}

      reference operator*() const
        { return wxStringOperations::DecodeChar(m_cur); }

      const_iterator operator+(ptrdiff_t n) const
        { return const_iterator(wxStringOperations::AddToIter(m_cur, n)); }
      const_iterator operator-(ptrdiff_t n) const
        { return const_iterator(wxStringOperations::AddToIter(m_cur, -n)); }

      // As in UTF-8 case above, we don't need comparison operators taking
      // iterator because we have an implicit conversion from iterator to
      // const_iterator so the operators declared by WX_STR_ITERATOR_IMPL will
      // be used.

  private:
      // for internal wxString use only:
      const_iterator(underlying_iterator ptr) : m_cur(ptr) {}
      const_iterator(const wxString *WXUNUSED(str), underlying_iterator ptr)
          : m_cur(ptr) {}
  };

  iterator GetIterForNthChar(size_t n) { return begin() + n; }
  const_iterator GetIterForNthChar(size_t n) const { return begin() + n; }
#endif // wxUSE_UNICODE_UTF8/!wxUSE_UNICODE_UTF8

  #undef WX_STR_ITERATOR_TAG
  #undef WX_STR_ITERATOR_IMPL

  // This method is mostly used by wxWidgets itself and return the offset of
  // the given iterator in bytes relative to the start of the buffer
  // representing the current string contents in the current locale encoding.
  //
  // It is inefficient as it involves converting part of the string to this
  // encoding (and also unsafe as it simply returns 0 if the conversion fails)
  // and so should be avoided if possible, wx itself only uses it to implement
  // backwards-compatible API.
  ptrdiff_t IterOffsetInMBStr(const const_iterator& i) const
  {
      const wxString str(begin(), i);

      // This is logically equivalent to strlen(str.mb_str()) but avoids
      // actually converting the string to multibyte and just computes the
      // length that it would have after conversion.
      size_t ofs = wxConvLibc.FromWChar(NULL, 0, str.wc_str(), str.length());
      return ofs == wxCONV_FAILED ? 0 : static_cast<ptrdiff_t>(ofs);
  }

  friend class iterator;
  friend class const_iterator;

  template <typename T>
  class reverse_iterator_impl
  {
  public:
      typedef T iterator_type;

      WX_DEFINE_ITERATOR_CATEGORY(typename T::iterator_category)
      typedef typename T::value_type value_type;
      typedef typename T::difference_type difference_type;
      typedef typename T::reference reference;
      typedef typename T::pointer *pointer;

      reverse_iterator_impl() {}
      reverse_iterator_impl(iterator_type i) : m_cur(i) {}
      reverse_iterator_impl(const reverse_iterator_impl& ri)
          : m_cur(ri.m_cur) {}

      iterator_type base() const { return m_cur; }

      reference operator*() const { return *(m_cur-1); }
      reference operator[](size_t n) const { return *(*this + n); }

      reverse_iterator_impl& operator++()
        { --m_cur; return *this; }
      reverse_iterator_impl operator++(int)
        { reverse_iterator_impl tmp = *this; --m_cur; return tmp; }
      reverse_iterator_impl& operator--()
        { ++m_cur; return *this; }
      reverse_iterator_impl operator--(int)
        { reverse_iterator_impl tmp = *this; ++m_cur; return tmp; }

      // NB: explicit <T> in the functions below is to keep BCC 5.5 happy
      reverse_iterator_impl operator+(ptrdiff_t n) const
        { return reverse_iterator_impl<T>(m_cur - n); }
      reverse_iterator_impl operator-(ptrdiff_t n) const
        { return reverse_iterator_impl<T>(m_cur + n); }
      reverse_iterator_impl operator+=(ptrdiff_t n)
        { m_cur -= n; return *this; }
      reverse_iterator_impl operator-=(ptrdiff_t n)
        { m_cur += n; return *this; }

      unsigned operator-(const reverse_iterator_impl& i) const
        { return i.m_cur - m_cur; }

      bool operator==(const reverse_iterator_impl& ri) const
        { return m_cur == ri.m_cur; }
      bool operator!=(const reverse_iterator_impl& ri) const
        { return !(*this == ri); }

      bool operator<(const reverse_iterator_impl& i) const
        { return m_cur > i.m_cur; }
      bool operator>(const reverse_iterator_impl& i) const
        { return m_cur < i.m_cur; }
      bool operator<=(const reverse_iterator_impl& i) const
        { return m_cur >= i.m_cur; }
      bool operator>=(const reverse_iterator_impl& i) const
        { return m_cur <= i.m_cur; }

  private:
      iterator_type m_cur;
  };

  typedef reverse_iterator_impl<iterator> reverse_iterator;
  typedef reverse_iterator_impl<const_iterator> const_reverse_iterator;

private:
  // used to transform an expression built using c_str() (and hence of type
  // wxCStrData) to an iterator into the string
  static const_iterator CreateConstIterator(const wxCStrData& data)
  {
      return const_iterator(data.m_str,
                            (data.m_str->begin() + data.m_offset).impl());
  }

  // in UTF-8 STL build, creation from std::string requires conversion under
  // non-UTF8 locales, so we can't have and use wxString(wxStringImpl) ctor;
  // instead we define dummy type that lets us have wxString ctor for creation
  // from wxStringImpl that couldn't be used by user code (in all other builds,
  // "standard" ctors can be used):
#if wxUSE_UNICODE_UTF8 && wxUSE_STL_BASED_WXSTRING
  struct CtorFromStringImplTag {};

  wxString(CtorFromStringImplTag* WXUNUSED(dummy), const wxStringImpl& src)
      : m_impl(src) {}

  static wxString FromImpl(const wxStringImpl& src)
      { return wxString((CtorFromStringImplTag*)NULL, src); }
#else
  #if !wxUSE_STL_BASED_WXSTRING
  wxString(const wxStringImpl& src) : m_impl(src) { }
  // else: already defined as wxString(wxStdString) below
  #endif
  static wxString FromImpl(const wxStringImpl& src) { return wxString(src); }
#endif

public:
  // constructors and destructor
    // ctor for an empty string
  wxString() {}

    // copy ctor
  wxString(const wxString& stringSrc) : m_impl(stringSrc.m_impl) { }

    // string containing nRepeat copies of ch
  wxString(wxUniChar ch, size_t nRepeat = 1 )
    { assign(nRepeat, ch); }
  wxString(size_t nRepeat, wxUniChar ch)
    { assign(nRepeat, ch); }
  wxString(wxUniCharRef ch, size_t nRepeat = 1)
    { assign(nRepeat, ch); }
  wxString(size_t nRepeat, wxUniCharRef ch)
    { assign(nRepeat, ch); }
  wxString(char ch, size_t nRepeat = 1)
    { assign(nRepeat, ch); }
  wxString(size_t nRepeat, char ch)
    { assign(nRepeat, ch); }
  wxString(wchar_t ch, size_t nRepeat = 1)
    { assign(nRepeat, ch); }
  wxString(size_t nRepeat, wchar_t ch)
    { assign(nRepeat, ch); }

    // ctors from char* strings:
  wxString(const char *psz)
    : m_impl(ImplStr(psz)) {}
  wxString(const char *psz, const wxMBConv& conv)
    : m_impl(ImplStr(psz, conv)) {}
  wxString(const char *psz, size_t nLength)
    { assign(psz, nLength); }
  wxString(const char *psz, const wxMBConv& conv, size_t nLength)
  {
    SubstrBufFromMB str(ImplStr(psz, nLength, conv));
    m_impl.assign(str.data, str.len);
  }

    // and unsigned char*:
  wxString(const unsigned char *psz)
    : m_impl(ImplStr((const char*)psz)) {}
  wxString(const unsigned char *psz, const wxMBConv& conv)
    : m_impl(ImplStr((const char*)psz, conv)) {}
  wxString(const unsigned char *psz, size_t nLength)
    { assign((const char*)psz, nLength); }
  wxString(const unsigned char *psz, const wxMBConv& conv, size_t nLength)
  {
    SubstrBufFromMB str(ImplStr((const char*)psz, nLength, conv));
    m_impl.assign(str.data, str.len);
  }

    // ctors from wchar_t* strings:
  wxString(const wchar_t *pwz)
    : m_impl(ImplStr(pwz)) {}
  wxString(const wchar_t *pwz, const wxMBConv& WXUNUSED(conv))
    : m_impl(ImplStr(pwz)) {}
  wxString(const wchar_t *pwz, size_t nLength)
    { assign(pwz, nLength); }
  wxString(const wchar_t *pwz, const wxMBConv& WXUNUSED(conv), size_t nLength)
    { assign(pwz, nLength); }

  wxString(const wxScopedCharBuffer& buf)
    { assign(buf.data(), buf.length()); }
  wxString(const wxScopedWCharBuffer& buf)
    { assign(buf.data(), buf.length()); }

    // NB: this version uses m_impl.c_str() to force making a copy of the
    //     string, so that "wxString(str.c_str())" idiom for passing strings
    //     between threads works
  wxString(const wxCStrData& cstr)
      : m_impl(cstr.AsString().m_impl.c_str()) { }

    // as we provide both ctors with this signature for both char and unsigned
    // char string, we need to provide one for wxCStrData to resolve ambiguity
  wxString(const wxCStrData& cstr, size_t nLength)
      : m_impl(cstr.AsString().Mid(0, nLength).m_impl) {}

    // and because wxString is convertible to wxCStrData and const wxChar *
    // we also need to provide this one
  wxString(const wxString& str, size_t nLength)
    { assign(str, nLength); }


#if wxUSE_STRING_POS_CACHE
  ~wxString()
  {
      // we need to invalidate our cache entry as another string could be
      // recreated at the same address (unlikely, but still possible, with the
      // heap-allocated strings but perfectly common with stack-allocated ones)
      InvalidateCache();
  }
#endif // wxUSE_STRING_POS_CACHE

  // even if we're not built with wxUSE_STD_STRING_CONV_IN_WXSTRING == 1 it is
  // very convenient to allow implicit conversions from std::string to wxString
  // and vice verse as this allows to use the same strings in non-GUI and GUI
  // code, however we don't want to unconditionally add this ctor as it would
  // make wx lib dependent on libstdc++ on some Linux versions which is bad, so
  // instead we ask the client code to define this wxUSE_STD_STRING symbol if
  // they need it
#if wxUSE_STD_STRING
  #if wxUSE_UNICODE_WCHAR
    wxString(const wxStdWideString& str) : m_impl(str) {}
  #else // UTF-8 or ANSI
    wxString(const wxStdWideString& str)
        { assign(str.c_str(), str.length()); }
  #endif

  #if !wxUSE_UNICODE // ANSI build
    // FIXME-UTF8: do this in UTF8 build #if wxUSE_UTF8_LOCALE_ONLY, too
    wxString(const std::string& str) : m_impl(str) {}
  #else // Unicode
    wxString(const std::string& str)
        { assign(str.c_str(), str.length()); }
  #endif
#endif // wxUSE_STD_STRING

  // Also always provide explicit conversions to std::[w]string in any case,
  // see below for the implicit ones.
#if wxUSE_STD_STRING
  // We can avoid a copy if we already use this string type internally,
  // otherwise we create a copy on the fly:
  #if wxUSE_UNICODE_WCHAR && wxUSE_STL_BASED_WXSTRING
    #define wxStringToStdWstringRetType const wxStdWideString&
    const wxStdWideString& ToStdWstring() const { return m_impl; }
  #else
    // wxStringImpl is either not std::string or needs conversion
    #define wxStringToStdWstringRetType wxStdWideString
    wxStdWideString ToStdWstring() const
    {
#if wxUSE_UNICODE_WCHAR
        wxScopedWCharBuffer buf =
            wxScopedWCharBuffer::CreateNonOwned(m_impl.c_str(), m_impl.length());
#else // !wxUSE_UNICODE_WCHAR
        wxScopedWCharBuffer buf(wc_str());
#endif

        return wxStdWideString(buf.data(), buf.length());
    }
  #endif

  #if (!wxUSE_UNICODE || wxUSE_UTF8_LOCALE_ONLY) && wxUSE_STL_BASED_WXSTRING
    // wxStringImpl is std::string in the encoding we want
    #define wxStringToStdStringRetType const std::string&
    const std::string& ToStdString() const { return m_impl; }
  #else
    // wxStringImpl is either not std::string or needs conversion
    #define wxStringToStdStringRetType std::string
    std::string ToStdString() const
    {
        wxScopedCharBuffer buf(mb_str());
        return std::string(buf.data(), buf.length());
    }
  #endif

#if wxUSE_STD_STRING_CONV_IN_WXSTRING
    // Implicit conversions to std::[w]string are not provided by default as
    // they conflict with the implicit conversions to "const char/wchar_t *"
    // which we use for backwards compatibility but do provide them if
    // explicitly requested.
  operator wxStringToStdStringRetType() const { return ToStdString(); }
  operator wxStringToStdWstringRetType() const { return ToStdWstring(); }
#endif // wxUSE_STD_STRING_CONV_IN_WXSTRING

#undef wxStringToStdStringRetType
#undef wxStringToStdWstringRetType

#endif // wxUSE_STD_STRING

  wxString Clone() const
  {
      // make a deep copy of the string, i.e. the returned string will have
      // ref count = 1 with refcounted implementation
      return wxString::FromImpl(wxStringImpl(m_impl.c_str(), m_impl.length()));
  }

  // first valid index position
  const_iterator begin() const { return const_iterator(this, m_impl.begin()); }
  iterator begin() { return iterator(this, m_impl.begin()); }
  // position one after the last valid one
  const_iterator end() const { return const_iterator(this, m_impl.end()); }
  iterator end() { return iterator(this, m_impl.end()); }

  // first element of the reversed string
  const_reverse_iterator rbegin() const
    { return const_reverse_iterator(end()); }
  reverse_iterator rbegin()
    { return reverse_iterator(end()); }
  // one beyond the end of the reversed string
  const_reverse_iterator rend() const
    { return const_reverse_iterator(begin()); }
  reverse_iterator rend()
    { return reverse_iterator(begin()); }

  // std::string methods:
#if wxUSE_UNICODE_UTF8
  size_t length() const
  {
#if wxUSE_STRING_POS_CACHE
      wxCACHE_PROFILE_FIELD_INC(lentot);

      Cache::Element * const cache = GetCacheElement();

      if ( cache->len == npos )
      {
          // it's probably not worth trying to be clever and using cache->pos
          // here as it's probably 0 anyhow -- you usually call length() before
          // starting to index the string
          cache->len = end() - begin();
      }
      else
      {
          wxCACHE_PROFILE_FIELD_INC(lenhits);

          wxSTRING_CACHE_ASSERT( (int)cache->len == end() - begin() );
      }

      return cache->len;
#else // !wxUSE_STRING_POS_CACHE
      return end() - begin();
#endif // wxUSE_STRING_POS_CACHE/!wxUSE_STRING_POS_CACHE
  }
#else
  size_t length() const { return m_impl.length(); }
#endif

  size_type size() const { return length(); }
  size_type max_size() const { return npos; }

  bool empty() const { return m_impl.empty(); }

  // NB: these methods don't have a well-defined meaning in UTF-8 case
  size_type capacity() const { return m_impl.capacity(); }
  void reserve(size_t sz) { m_impl.reserve(sz); }

  void resize(size_t nSize, wxUniChar ch = wxT('\0'))
  {
    const size_t len = length();
    if ( nSize == len)
        return;

#if wxUSE_UNICODE_UTF8
    if ( nSize < len )
    {
        wxSTRING_INVALIDATE_CACHE();

        // we can't use wxStringImpl::resize() for truncating the string as it
        // counts in bytes, not characters
        erase(nSize);
        return;
    }

    // we also can't use (presumably more efficient) resize() if we have to
    // append characters taking more than one byte
    if ( !ch.IsAscii() )
    {
        append(nSize - len, ch);
    }
    else // can use (presumably faster) resize() version
#endif // wxUSE_UNICODE_UTF8
    {
        wxSTRING_INVALIDATE_CACHED_LENGTH();

        m_impl.resize(nSize, (wxStringCharType)ch);
    }
  }

  wxString substr(size_t nStart = 0, size_t nLen = npos) const
  {
    size_t pos, len;
    PosLenToImpl(nStart, nLen, &pos, &len);
    return FromImpl(m_impl.substr(pos, len));
  }

  // generic attributes & operations
    // as standard strlen()
  size_t Len() const { return length(); }
    // string contains any characters?
  bool IsEmpty() const { return empty(); }
    // empty string is "false", so !str will return true
  bool operator!() const { return empty(); }
    // truncate the string to given length
  wxString& Truncate(size_t uiLen);
    // empty string contents
  void Empty() { clear(); }
    // empty the string and free memory
  void Clear() { clear(); }

  // contents test
    // Is an ascii value
  bool IsAscii() const;
    // Is a number
  bool IsNumber() const;
    // Is a word
  bool IsWord() const;

  // data access (all indexes are 0 based)
    // read access
    wxUniChar at(size_t n) const
      { return wxStringOperations::DecodeChar(m_impl.begin() + PosToImpl(n)); }
    wxUniChar GetChar(size_t n) const
      { return at(n); }
    // read/write access
    wxUniCharRef at(size_t n)
      { return *GetIterForNthChar(n); }
    wxUniCharRef GetWritableChar(size_t n)
      { return at(n); }
    // write access
    void SetChar(size_t n, wxUniChar ch)
      { at(n) = ch; }

    // get last character
    wxUniChar Last() const
    {
      wxASSERT_MSG( !empty(), wxT("wxString: index out of bounds") );
      return *rbegin();
    }

    // get writable last character
    wxUniCharRef Last()
    {
      wxASSERT_MSG( !empty(), wxT("wxString: index out of bounds") );
      return *rbegin();
    }

    /*
       Note that we we must define all of the overloads below to avoid
       ambiguity when using str[0].
     */
    wxUniChar operator[](int n) const
      { return at(n); }
    wxUniChar operator[](long n) const
      { return at(n); }
    wxUniChar operator[](size_t n) const
      { return at(n); }
#ifndef wxSIZE_T_IS_UINT
    wxUniChar operator[](unsigned int n) const
      { return at(n); }
#endif // size_t != unsigned int

    // operator versions of GetWriteableChar()
    wxUniCharRef operator[](int n)
      { return at(n); }
    wxUniCharRef operator[](long n)
      { return at(n); }
    wxUniCharRef operator[](size_t n)
      { return at(n); }
#ifndef wxSIZE_T_IS_UINT
    wxUniCharRef operator[](unsigned int n)
      { return at(n); }
#endif // size_t != unsigned int


    /*
        Overview of wxString conversions, implicit and explicit:

        - wxString has a std::[w]string-like c_str() method, however it does
          not return a C-style string directly but instead returns wxCStrData
          helper object which is convertible to either "char *" narrow string
          or "wchar_t *" wide string. Usually the correct conversion will be
          applied by the compiler automatically but if this doesn't happen you
          need to explicitly choose one using wxCStrData::AsChar() or AsWChar()
          methods or another wxString conversion function.

        - One of the places where the conversion does *NOT* happen correctly is
          when c_str() is passed to a vararg function such as printf() so you
          must *NOT* use c_str() with them. Either use wxPrintf() (all wx
          functions do handle c_str() correctly, even if they appear to be
          vararg (but they're not, really)) or add an explicit AsChar() or, if
          compatibility with previous wxWidgets versions is important, add a
          cast to "const char *".

        - In non-STL mode only, wxString is also implicitly convertible to
          wxCStrData. The same warning as above applies.

        - c_str() is polymorphic as it can be converted to either narrow or
          wide string. If you explicitly need one or the other, choose to use
          mb_str() (for narrow) or wc_str() (for wide) instead. Notice that
          these functions can return either the pointer to string directly (if
          this is what the string uses internally) or a temporary buffer
          containing the string and convertible to it. Again, conversion will
          usually be done automatically by the compiler but beware of the
          vararg functions: you need an explicit cast when using them.

        - There are also non-const versions of mb_str() and wc_str() called
          char_str() and wchar_str(). They are only meant to be used with
          non-const-correct functions and they always return buffers.

        - Finally wx_str() returns whatever string representation is used by
          wxString internally. It may be either a narrow or wide string
          depending on wxWidgets build mode but it will always be a raw pointer
          (and not a buffer).
     */

    // explicit conversion to wxCStrData
    wxCStrData c_str() const { return wxCStrData(this); }
    wxCStrData data() const { return c_str(); }

    // implicit conversion to wxCStrData
    operator wxCStrData() const { return c_str(); }

    // the first two operators conflict with operators for conversion to
    // std::string and they must be disabled if those conversions are enabled;
    // the next one only makes sense if conversions to char* are also defined
    // and not defining it in STL build also helps us to get more clear error
    // messages for the code which relies on implicit conversion to char* in
    // STL build
#if !wxUSE_STD_STRING_CONV_IN_WXSTRING
    operator const char*() const { return c_str(); }
    operator const wchar_t*() const { return c_str(); }

    // implicit conversion to untyped pointer for compatibility with previous
    // wxWidgets versions: this is the same as conversion to const char * so it
    // may fail!
    operator const void*() const { return c_str(); }
#endif // !wxUSE_STD_STRING_CONV_IN_WXSTRING

    // identical to c_str(), for MFC compatibility
    const wxCStrData GetData() const { return c_str(); }

    // explicit conversion to C string in internal representation (char*,
    // wchar_t*, UTF-8-encoded char*, depending on the build):
    const wxStringCharType *wx_str() const { return m_impl.c_str(); }

    // conversion to *non-const* multibyte or widestring buffer; modifying
    // returned buffer won't affect the string, these methods are only useful
    // for passing values to const-incorrect functions
    wxWritableCharBuffer char_str(const wxMBConv& conv = wxConvLibc) const
        { return mb_str(conv); }
    wxWritableWCharBuffer wchar_str() const { return wc_str(); }

    // conversion to the buffer of the given type T (= char or wchar_t) and
    // also optionally return the buffer length
    //
    // this is mostly/only useful for the template functions
    //
    // FIXME-VC6: the second argument only exists for VC6 which doesn't support
    //            explicit template function selection, do not use it unless
    //            you must support VC6!
    template <typename T>
    wxCharTypeBuffer<T> tchar_str(size_t *len = NULL,
                                  T * WXUNUSED(dummy) = NULL) const
    {
#if wxUSE_UNICODE
        // we need a helper dispatcher depending on type
        return wxPrivate::wxStringAsBufHelper<T>::Get(*this, len);
#else // ANSI
        // T can only be char in ANSI build
        if ( len )
            *len = length();

        return wxCharTypeBuffer<T>::CreateNonOwned(wx_str(), length());
#endif // Unicode build kind
    }

    // conversion to/from plain (i.e. 7 bit) ASCII: this is useful for
    // converting numbers or strings which are certain not to contain special
    // chars (typically system functions, X atoms, environment variables etc.)
    //
    // the behaviour of these functions with the strings containing anything
    // else than 7 bit ASCII characters is undefined, use at your own risk.
#if wxUSE_UNICODE
    static wxString FromAscii(const char *ascii, size_t len);
    static wxString FromAscii(const char *ascii);
    static wxString FromAscii(char ascii);
    const wxScopedCharBuffer ToAscii() const;
#else // ANSI
    static wxString FromAscii(const char *ascii) { return wxString( ascii ); }
    static wxString FromAscii(const char *ascii, size_t len)
        { return wxString( ascii, len ); }
    static wxString FromAscii(char ascii) { return wxString( ascii ); }
    const char *ToAscii() const { return c_str(); }
#endif // Unicode/!Unicode

    // also provide unsigned char overloads as signed/unsigned doesn't matter
    // for 7 bit ASCII characters
    static wxString FromAscii(const unsigned char *ascii)
        { return FromAscii((const char *)ascii); }
    static wxString FromAscii(const unsigned char *ascii, size_t len)
        { return FromAscii((const char *)ascii, len); }

    // conversion to/from UTF-8:
#if wxUSE_UNICODE_UTF8
    static wxString FromUTF8Unchecked(const char *utf8)
    {
      if ( !utf8 )
          return wxEmptyString;

      wxASSERT( wxStringOperations::IsValidUtf8String(utf8) );
      return FromImpl(wxStringImpl(utf8));
    }
    static wxString FromUTF8Unchecked(const char *utf8, size_t len)
    {
      if ( !utf8 )
          return wxEmptyString;
      if ( len == npos )
          return FromUTF8Unchecked(utf8);

      wxASSERT( wxStringOperations::IsValidUtf8String(utf8, len) );
      return FromImpl(wxStringImpl(utf8, len));
    }

    static wxString FromUTF8(const char *utf8)
    {
        if ( !utf8 || !wxStringOperations::IsValidUtf8String(utf8) )
            return "";

        return FromImpl(wxStringImpl(utf8));
    }
    static wxString FromUTF8(const char *utf8, size_t len)
    {
        if ( len == npos )
            return FromUTF8(utf8);

        if ( !utf8 || !wxStringOperations::IsValidUtf8String(utf8, len) )
            return "";

        return FromImpl(wxStringImpl(utf8, len));
    }

    const wxScopedCharBuffer utf8_str() const
        { return wxCharBuffer::CreateNonOwned(m_impl.c_str(), m_impl.length()); }

    // this function exists in UTF-8 build only and returns the length of the
    // internal UTF-8 representation
    size_t utf8_length() const { return m_impl.length(); }
#elif wxUSE_UNICODE_WCHAR
    static wxString FromUTF8(const char *utf8, size_t len = npos)
      { return wxString(utf8, wxMBConvUTF8(), len); }
    static wxString FromUTF8Unchecked(const char *utf8, size_t len = npos)
    {
        const wxString s(utf8, wxMBConvUTF8(), len);
        wxASSERT_MSG( !utf8 || !*utf8 || !s.empty(),
                      "string must be valid UTF-8" );
        return s;
    }
    const wxScopedCharBuffer utf8_str() const { return mb_str(wxMBConvUTF8()); }
#else // ANSI
    static wxString FromUTF8(const char *utf8)
      { return wxString(wxMBConvUTF8().cMB2WC(utf8)); }
    static wxString FromUTF8(const char *utf8, size_t len)
    {
        size_t wlen;
        wxScopedWCharBuffer buf(wxMBConvUTF8().cMB2WC(utf8, len == npos ? wxNO_LEN : len, &wlen));
        return wxString(buf.data(), wlen);
    }
    static wxString FromUTF8Unchecked(const char *utf8, size_t len = npos)
    {
        size_t wlen;
        wxScopedWCharBuffer buf
                            (
                              wxMBConvUTF8().cMB2WC
                                             (
                                               utf8,
                                               len == npos ? wxNO_LEN : len,
                                               &wlen
                                             )
                            );
        wxASSERT_MSG( !utf8 || !*utf8 || wlen,
                      "string must be valid UTF-8" );

        return wxString(buf.data(), wlen);
    }
    const wxScopedCharBuffer utf8_str() const
      { return wxMBConvUTF8().cWC2MB(wc_str()); }
#endif

    const wxScopedCharBuffer ToUTF8() const { return utf8_str(); }

    // functions for storing binary data in wxString:
#if wxUSE_UNICODE
    static wxString From8BitData(const char *data, size_t len)
      { return wxString(data, wxConvISO8859_1, len); }
    // version for NUL-terminated data:
    static wxString From8BitData(const char *data)
      { return wxString(data, wxConvISO8859_1); }
    const wxScopedCharBuffer To8BitData() const
        { return mb_str(wxConvISO8859_1); }
#else // ANSI
    static wxString From8BitData(const char *data, size_t len)
      { return wxString(data, len); }
    // version for NUL-terminated data:
    static wxString From8BitData(const char *data)
      { return wxString(data); }
    const wxScopedCharBuffer To8BitData() const
        { return wxScopedCharBuffer::CreateNonOwned(wx_str(), length()); }
#endif // Unicode/ANSI

    // conversions with (possible) format conversions: have to return a
    // buffer with temporary data
    //
    // the functions defined (in either Unicode or ANSI) mode are mb_str() to
    // return an ANSI (multibyte) string, wc_str() to return a wide string and
    // fn_str() to return a string which should be used with the OS APIs
    // accepting the file names. The return value is always the same, but the
    // type differs because a function may either return pointer to the buffer
    // directly or have to use intermediate buffer for translation.

#if wxUSE_UNICODE

    // this is an optimization: even though using mb_str(wxConvLibc) does the
    // same thing (i.e. returns pointer to internal representation as locale is
    // always an UTF-8 one) in wxUSE_UTF8_LOCALE_ONLY case, we can avoid the
    // extra checks and the temporary buffer construction by providing a
    // separate mb_str() overload
#if wxUSE_UTF8_LOCALE_ONLY
    const char* mb_str() const { return wx_str(); }
    const wxScopedCharBuffer mb_str(const wxMBConv& conv) const
    {
        return AsCharBuf(conv);
    }
#else // !wxUSE_UTF8_LOCALE_ONLY
    const wxScopedCharBuffer mb_str(const wxMBConv& conv = wxConvLibc) const
    {
        return AsCharBuf(conv);
    }
#endif // wxUSE_UTF8_LOCALE_ONLY/!wxUSE_UTF8_LOCALE_ONLY

    const wxWX2MBbuf mbc_str() const { return mb_str(*wxConvCurrent); }

#if wxUSE_UNICODE_WCHAR
    const wchar_t* wc_str() const { return wx_str(); }
#elif wxUSE_UNICODE_UTF8
    const wxScopedWCharBuffer wc_str() const
        { return AsWCharBuf(wxMBConvStrictUTF8()); }
#endif
    // for compatibility with !wxUSE_UNICODE version
    const wxWX2WCbuf wc_str(const wxMBConv& WXUNUSED(conv)) const
      { return wc_str(); }

#if wxMBFILES
    const wxScopedCharBuffer fn_str() const { return mb_str(wxConvFile); }
#else // !wxMBFILES
    const wxWX2WCbuf fn_str() const { return wc_str(); }
#endif // wxMBFILES/!wxMBFILES

#else // ANSI
    const char* mb_str() const { return wx_str(); }

    // for compatibility with wxUSE_UNICODE version
    const char* mb_str(const wxMBConv& WXUNUSED(conv)) const { return wx_str(); }

    const wxWX2MBbuf mbc_str() const { return mb_str(); }

    const wxScopedWCharBuffer wc_str(const wxMBConv& conv = wxConvLibc) const
        { return AsWCharBuf(conv); }

    const wxScopedCharBuffer fn_str() const
        { return wxConvFile.cWC2WX( wc_str( wxConvLibc ) ); }
#endif // Unicode/ANSI

#if wxUSE_UNICODE_UTF8
    const wxScopedWCharBuffer t_str() const { return wc_str(); }
#elif wxUSE_UNICODE_WCHAR
    const wchar_t* t_str() const { return wx_str(); }
#else
    const char* t_str() const { return wx_str(); }
#endif


  // overloaded assignment
    // from another wxString
  wxString& operator=(const wxString& stringSrc)
  {
    if ( this != &stringSrc )
    {
        wxSTRING_INVALIDATE_CACHE();

        m_impl = stringSrc.m_impl;
    }

    return *this;
  }

  wxString& operator=(const wxCStrData& cstr)
    { return *this = cstr.AsString(); }
    // from a character
  wxString& operator=(wxUniChar ch)
  {
    wxSTRING_INVALIDATE_CACHE();

#if wxUSE_UNICODE_UTF8
    if ( !ch.IsAscii() )
        m_impl = wxStringOperations::EncodeChar(ch);
    else
#endif // wxUSE_UNICODE_UTF8
        m_impl = (wxStringCharType)ch;
    return *this;
  }

  wxString& operator=(wxUniCharRef ch)
    { return operator=((wxUniChar)ch); }
  wxString& operator=(char ch)
    { return operator=(wxUniChar(ch)); }
  wxString& operator=(unsigned char ch)
    { return operator=(wxUniChar(ch)); }
  wxString& operator=(wchar_t ch)
    { return operator=(wxUniChar(ch)); }
    // from a C string - STL probably will crash on NULL,
    // so we need to compensate in that case
#if wxUSE_STL_BASED_WXSTRING
  wxString& operator=(const char *psz)
  {
      wxSTRING_INVALIDATE_CACHE();

      if ( psz )
          m_impl = ImplStr(psz);
      else
          clear();

      return *this;
  }

  wxString& operator=(const wchar_t *pwz)
  {
      wxSTRING_INVALIDATE_CACHE();

      if ( pwz )
          m_impl = ImplStr(pwz);
      else
          clear();

      return *this;
  }
#else // !wxUSE_STL_BASED_WXSTRING
  wxString& operator=(const char *psz)
  {
      wxSTRING_INVALIDATE_CACHE();

      m_impl = ImplStr(psz);

      return *this;
  }

  wxString& operator=(const wchar_t *pwz)
  {
      wxSTRING_INVALIDATE_CACHE();

      m_impl = ImplStr(pwz);

      return *this;
  }
#endif // wxUSE_STL_BASED_WXSTRING/!wxUSE_STL_BASED_WXSTRING

  wxString& operator=(const unsigned char *psz)
    { return operator=((const char*)psz); }

    // from wxScopedWCharBuffer
  wxString& operator=(const wxScopedWCharBuffer& s)
    { return assign(s); }
    // from wxScopedCharBuffer
  wxString& operator=(const wxScopedCharBuffer& s)
    { return assign(s); }

  // string concatenation
    // in place concatenation
    /*
        Concatenate and return the result. Note that the left to right
        associativity of << allows to write things like "str << str1 << str2
        << ..." (unlike with +=)
     */
      // string += string
  wxString& operator<<(const wxString& s)
  {
#if WXWIN_COMPATIBILITY_2_8 && !wxUSE_STL_BASED_WXSTRING && !wxUSE_UNICODE_UTF8
    wxASSERT_MSG( s.IsValid(),
                  wxT("did you forget to call UngetWriteBuf()?") );
#endif

    append(s);
    return *this;
  }
      // string += C string
  wxString& operator<<(const char *psz)
    { append(psz); return *this; }
  wxString& operator<<(const wchar_t *pwz)
    { append(pwz); return *this; }
  wxString& operator<<(const wxCStrData& psz)
    { append(psz.AsString()); return *this; }
      // string += char
  wxString& operator<<(wxUniChar ch) { append(1, ch); return *this; }
  wxString& operator<<(wxUniCharRef ch) { append(1, ch); return *this; }
  wxString& operator<<(char ch) { append(1, ch); return *this; }
  wxString& operator<<(unsigned char ch) { append(1, ch); return *this; }
  wxString& operator<<(wchar_t ch) { append(1, ch); return *this; }

      // string += buffer (i.e. from wxGetString)
  wxString& operator<<(const wxScopedWCharBuffer& s)
    { return append(s); }
  wxString& operator<<(const wxScopedCharBuffer& s)
    { return append(s); }

    // string += C string
  wxString& Append(const wxString& s)
    {
        // test for empty() to share the string if possible
        if ( empty() )
            *this = s;
        else
            append(s);
        return *this;
    }
  wxString& Append(const char* psz)
    { append(psz); return *this; }
  wxString& Append(const wchar_t* pwz)
    { append(pwz); return *this; }
  wxString& Append(const wxCStrData& psz)
    { append(psz); return *this; }
  wxString& Append(const wxScopedCharBuffer& psz)
    { append(psz); return *this; }
  wxString& Append(const wxScopedWCharBuffer& psz)
    { append(psz); return *this; }
  wxString& Append(const char* psz, size_t nLen)
    { append(psz, nLen); return *this; }
  wxString& Append(const wchar_t* pwz, size_t nLen)
    { append(pwz, nLen); return *this; }
  wxString& Append(const wxCStrData& psz, size_t nLen)
    { append(psz, nLen); return *this; }
  wxString& Append(const wxScopedCharBuffer& psz, size_t nLen)
    { append(psz, nLen); return *this; }
  wxString& Append(const wxScopedWCharBuffer& psz, size_t nLen)
    { append(psz, nLen); return *this; }
    // append count copies of given character
  wxString& Append(wxUniChar ch, size_t count = 1u)
    { append(count, ch); return *this; }
  wxString& Append(wxUniCharRef ch, size_t count = 1u)
    { append(count, ch); return *this; }
  wxString& Append(char ch, size_t count = 1u)
    { append(count, ch); return *this; }
  wxString& Append(unsigned char ch, size_t count = 1u)
    { append(count, ch); return *this; }
  wxString& Append(wchar_t ch, size_t count = 1u)
    { append(count, ch); return *this; }

    // prepend a string, return the string itself
  wxString& Prepend(const wxString& str)
    { *this = str + *this; return *this; }

    // non-destructive concatenation
      // two strings
  friend wxString WXDLLIMPEXP_BASE operator+(const wxString& string1,
                                             const wxString& string2);
      // string with a single char
  friend wxString WXDLLIMPEXP_BASE operator+(const wxString& string, wxUniChar ch);
      // char with a string
  friend wxString WXDLLIMPEXP_BASE operator+(wxUniChar ch, const wxString& string);
      // string with C string
  friend wxString WXDLLIMPEXP_BASE operator+(const wxString& string,
                                             const char *psz);
  friend wxString WXDLLIMPEXP_BASE operator+(const wxString& string,
                                             const wchar_t *pwz);
      // C string with string
  friend wxString WXDLLIMPEXP_BASE operator+(const char *psz,
                                             const wxString& string);
  friend wxString WXDLLIMPEXP_BASE operator+(const wchar_t *pwz,
                                             const wxString& string);

  // stream-like functions
      // insert an int into string
  wxString& operator<<(int i)
    { return (*this) << Format(wxT("%d"), i); }
      // insert an unsigned int into string
  wxString& operator<<(unsigned int ui)
    { return (*this) << Format(wxT("%u"), ui); }
      // insert a long into string
  wxString& operator<<(long l)
    { return (*this) << Format(wxT("%ld"), l); }
      // insert an unsigned long into string
  wxString& operator<<(unsigned long ul)
    { return (*this) << Format(wxT("%lu"), ul); }
#ifdef wxHAS_LONG_LONG_T_DIFFERENT_FROM_LONG
      // insert a long long if they exist and aren't longs
  wxString& operator<<(wxLongLong_t ll)
    {
      return (*this) << Format("%" wxLongLongFmtSpec "d", ll);
    }
      // insert an unsigned long long
  wxString& operator<<(wxULongLong_t ull)
    {
      return (*this) << Format("%" wxLongLongFmtSpec "u" , ull);
    }
#endif // wxHAS_LONG_LONG_T_DIFFERENT_FROM_LONG
      // insert a float into string
  wxString& operator<<(float f)
    { return (*this) << Format(wxT("%f"), f); }
      // insert a double into string
  wxString& operator<<(double d)
    { return (*this) << Format(wxT("%g"), d); }

  // string comparison
    // case-sensitive comparison (returns a value < 0, = 0 or > 0)
  int Cmp(const char *psz) const
    { return compare(psz); }
  int Cmp(const wchar_t *pwz) const
    { return compare(pwz); }
  int Cmp(const wxString& s) const
    { return compare(s); }
  int Cmp(const wxCStrData& s) const
    { return compare(s); }
  int Cmp(const wxScopedCharBuffer& s) const
    { return compare(s); }
  int Cmp(const wxScopedWCharBuffer& s) const
    { return compare(s); }
    // same as Cmp() but not case-sensitive
  int CmpNoCase(const wxString& s) const;

    // test for the string equality, either considering case or not
    // (if compareWithCase then the case matters)
  bool IsSameAs(const wxString& str, bool compareWithCase = true) const
  {
#if !wxUSE_UNICODE_UTF8
      // in UTF-8 build, length() is O(n) and doing this would be _slower_
      if ( length() != str.length() )
          return false;
#endif
      return (compareWithCase ? Cmp(str) : CmpNoCase(str)) == 0;
  }
  bool IsSameAs(const char *str, bool compareWithCase = true) const
    { return (compareWithCase ? Cmp(str) : CmpNoCase(str)) == 0; }
  bool IsSameAs(const wchar_t *str, bool compareWithCase = true) const
    { return (compareWithCase ? Cmp(str) : CmpNoCase(str)) == 0; }

  bool IsSameAs(const wxCStrData& str, bool compareWithCase = true) const
    { return IsSameAs(str.AsString(), compareWithCase); }
  bool IsSameAs(const wxScopedCharBuffer& str, bool compareWithCase = true) const
    { return IsSameAs(str.data(), compareWithCase); }
  bool IsSameAs(const wxScopedWCharBuffer& str, bool compareWithCase = true) const
    { return IsSameAs(str.data(), compareWithCase); }
    // comparison with a single character: returns true if equal
  bool IsSameAs(wxUniChar c, bool compareWithCase = true) const;
  // FIXME-UTF8: remove these overloads
  bool IsSameAs(wxUniCharRef c, bool compareWithCase = true) const
    { return IsSameAs(wxUniChar(c), compareWithCase); }
  bool IsSameAs(char c, bool compareWithCase = true) const
    { return IsSameAs(wxUniChar(c), compareWithCase); }
  bool IsSameAs(unsigned char c, bool compareWithCase = true) const
    { return IsSameAs(wxUniChar(c), compareWithCase); }
  bool IsSameAs(wchar_t c, bool compareWithCase = true) const
    { return IsSameAs(wxUniChar(c), compareWithCase); }
  bool IsSameAs(int c, bool compareWithCase = true) const
    { return IsSameAs(wxUniChar(c), compareWithCase); }

  // simple sub-string extraction
      // return substring starting at nFirst of length nCount (or till the end
      // if nCount = default value)
  wxString Mid(size_t nFirst, size_t nCount = npos) const;

      // operator version of Mid()
  wxString  operator()(size_t start, size_t len) const
    { return Mid(start, len); }

      // check if the string starts with the given prefix and return the rest
      // of the string in the provided pointer if it is not NULL; otherwise
      // return false
  bool StartsWith(const wxString& prefix, wxString *rest = NULL) const;
      // check if the string ends with the given suffix and return the
      // beginning of the string before the suffix in the provided pointer if
      // it is not NULL; otherwise return false
  bool EndsWith(const wxString& suffix, wxString *rest = NULL) const;

      // get first nCount characters
  wxString Left(size_t nCount) const;
      // get last nCount characters
  wxString Right(size_t nCount) const;
      // get all characters before the first occurrence of ch
      // (returns the whole string if ch not found) and also put everything
      // following the first occurrence of ch into rest if it's non-NULL
  wxString BeforeFirst(wxUniChar ch, wxString *rest = NULL) const;
      // get all characters before the last occurrence of ch
      // (returns empty string if ch not found) and also put everything
      // following the last occurrence of ch into rest if it's non-NULL
  wxString BeforeLast(wxUniChar ch, wxString *rest = NULL) const;
      // get all characters after the first occurrence of ch
      // (returns empty string if ch not found)
  wxString AfterFirst(wxUniChar ch) const;
      // get all characters after the last occurrence of ch
      // (returns the whole string if ch not found)
  wxString AfterLast(wxUniChar ch) const;

    // for compatibility only, use more explicitly named functions above
  wxString Before(wxUniChar ch) const { return BeforeLast(ch); }
  wxString After(wxUniChar ch) const { return AfterFirst(ch); }

  // case conversion
      // convert to upper case in place, return the string itself
  wxString& MakeUpper();
      // convert to upper case, return the copy of the string
  wxString Upper() const { return wxString(*this).MakeUpper(); }
      // convert to lower case in place, return the string itself
  wxString& MakeLower();
      // convert to lower case, return the copy of the string
  wxString Lower() const { return wxString(*this).MakeLower(); }
      // convert the first character to the upper case and the rest to the
      // lower one, return the modified string itself
  wxString& MakeCapitalized();
      // convert the first character to the upper case and the rest to the
      // lower one, return the copy of the string
  wxString Capitalize() const { return wxString(*this).MakeCapitalized(); }

  // trimming/padding whitespace (either side) and truncating
      // remove spaces from left or from right (default) side
  wxString& Trim(bool bFromRight = true);
      // add nCount copies chPad in the beginning or at the end (default)
  wxString& Pad(size_t nCount, wxUniChar chPad = wxT(' '), bool bFromRight = true);

  // searching and replacing
      // searching (return starting index, or -1 if not found)
  int Find(wxUniChar ch, bool bFromEnd = false) const;   // like strchr/strrchr
  int Find(wxUniCharRef ch, bool bFromEnd = false) const
    { return Find(wxUniChar(ch), bFromEnd); }
  int Find(char ch, bool bFromEnd = false) const
    { return Find(wxUniChar(ch), bFromEnd); }
  int Find(unsigned char ch, bool bFromEnd = false) const
    { return Find(wxUniChar(ch), bFromEnd); }
  int Find(wchar_t ch, bool bFromEnd = false) const
    { return Find(wxUniChar(ch), bFromEnd); }
      // searching (return starting index, or -1 if not found)
  int Find(const wxString& sub) const               // like strstr
  {
    size_type idx = find(sub);
    return (idx == npos) ? wxNOT_FOUND : (int)idx;
  }
  int Find(const char *sub) const               // like strstr
  {
    size_type idx = find(sub);
    return (idx == npos) ? wxNOT_FOUND : (int)idx;
  }
  int Find(const wchar_t *sub) const               // like strstr
  {
    size_type idx = find(sub);
    return (idx == npos) ? wxNOT_FOUND : (int)idx;
  }

  int Find(const wxCStrData& sub) const
    { return Find(sub.AsString()); }
  int Find(const wxScopedCharBuffer& sub) const
    { return Find(sub.data()); }
  int Find(const wxScopedWCharBuffer& sub) const
    { return Find(sub.data()); }

      // replace first (or all of bReplaceAll) occurrences of substring with
      // another string, returns the number of replacements made
  size_t Replace(const wxString& strOld,
                 const wxString& strNew,
                 bool bReplaceAll = true);

    // check if the string contents matches a mask containing '*' and '?'
  bool Matches(const wxString& mask) const;

  // conversion to numbers: all functions return true only if the whole
  // string is a number and put the value of this number into the pointer
  // provided, the base is the numeric base in which the conversion should be
  // done and must be comprised between 2 and 36 or be 0 in which case the
  // standard C rules apply (leading '0' => octal, "0x" => hex)
      // convert to a signed integer
  bool ToLong(long *val, int base = 10) const;
      // convert to an unsigned integer
  bool ToULong(unsigned long *val, int base = 10) const;
      // convert to wxLongLong
#if defined(wxLongLong_t)
  bool ToLongLong(wxLongLong_t *val, int base = 10) const;
      // convert to wxULongLong
  bool ToULongLong(wxULongLong_t *val, int base = 10) const;
#endif // wxLongLong_t
      // convert to a double
  bool ToDouble(double *val) const;

  // conversions to numbers using C locale
      // convert to a signed integer
  bool ToCLong(long *val, int base = 10) const;
      // convert to an unsigned integer
  bool ToCULong(unsigned long *val, int base = 10) const;
      // convert to a double
  bool ToCDouble(double *val) const;

  // create a string representing the given floating point number with the
  // default (like %g) or fixed (if precision >=0) precision
    // in the current locale
  static wxString FromDouble(double val, int precision = -1);
    // in C locale
  static wxString FromCDouble(double val, int precision = -1);

#ifndef wxNEEDS_WXSTRING_PRINTF_MIXIN
  // formatted input/output
    // as sprintf(), returns the number of characters written or < 0 on error
    // (take 'this' into account in attribute parameter count)
  // int Printf(const wxString& format, ...);
  WX_DEFINE_VARARG_FUNC(int, Printf, 1, (const wxFormatString&),
                        DoPrintfWchar, DoPrintfUtf8)
#ifdef __WATCOMC__
  // workaround for http://bugzilla.openwatcom.org/show_bug.cgi?id=351
  WX_VARARG_WATCOM_WORKAROUND(int, Printf, 1, (const wxString&),
                              (wxFormatString(f1)));
  WX_VARARG_WATCOM_WORKAROUND(int, Printf, 1, (const wxCStrData&),
                              (wxFormatString(f1)));
  WX_VARARG_WATCOM_WORKAROUND(int, Printf, 1, (const char*),
                              (wxFormatString(f1)));
  WX_VARARG_WATCOM_WORKAROUND(int, Printf, 1, (const wchar_t*),
                              (wxFormatString(f1)));
#endif
#endif // !wxNEEDS_WXSTRING_PRINTF_MIXIN
    // as vprintf(), returns the number of characters written or < 0 on error
  int PrintfV(const wxString& format, va_list argptr);

#ifndef wxNEEDS_WXSTRING_PRINTF_MIXIN
    // returns the string containing the result of Printf() to it
  // static wxString Format(const wxString& format, ...) WX_ATTRIBUTE_PRINTF_1;
  WX_DEFINE_VARARG_FUNC(static wxString, Format, 1, (const wxFormatString&),
                        DoFormatWchar, DoFormatUtf8)
#ifdef __WATCOMC__
  // workaround for http://bugzilla.openwatcom.org/show_bug.cgi?id=351
  WX_VARARG_WATCOM_WORKAROUND(static wxString, Format, 1, (const wxString&),
                              (wxFormatString(f1)));
  WX_VARARG_WATCOM_WORKAROUND(static wxString, Format, 1, (const wxCStrData&),
                              (wxFormatString(f1)));
  WX_VARARG_WATCOM_WORKAROUND(static wxString, Format, 1, (const char*),
                              (wxFormatString(f1)));
  WX_VARARG_WATCOM_WORKAROUND(static wxString, Format, 1, (const wchar_t*),
                              (wxFormatString(f1)));
#endif
#endif
    // the same as above, but takes a va_list
  static wxString FormatV(const wxString& format, va_list argptr);

  // raw access to string memory
    // ensure that string has space for at least nLen characters
    // only works if the data of this string is not shared
  bool Alloc(size_t nLen) { reserve(nLen); return capacity() >= nLen; }
    // minimize the string's memory
    // only works if the data of this string is not shared
  bool Shrink();
#if WXWIN_COMPATIBILITY_2_8 && !wxUSE_STL_BASED_WXSTRING && !wxUSE_UNICODE_UTF8
    // These are deprecated, use wxStringBuffer or wxStringBufferLength instead
    //
    // get writable buffer of at least nLen bytes. Unget() *must* be called
    // a.s.a.p. to put string back in a reasonable state!
  wxDEPRECATED( wxStringCharType *GetWriteBuf(size_t nLen) );
    // call this immediately after GetWriteBuf() has been used
  wxDEPRECATED( void UngetWriteBuf() );
  wxDEPRECATED( void UngetWriteBuf(size_t nLen) );
#endif // WXWIN_COMPATIBILITY_2_8 && !wxUSE_STL_BASED_WXSTRING && wxUSE_UNICODE_UTF8

  // wxWidgets version 1 compatibility functions

  // use Mid()
  wxString SubString(size_t from, size_t to) const
      { return Mid(from, (to - from + 1)); }
    // values for second parameter of CompareTo function
  enum caseCompare {exact, ignoreCase};
    // values for first parameter of Strip function
  enum stripType {leading = 0x1, trailing = 0x2, both = 0x3};

#ifndef wxNEEDS_WXSTRING_PRINTF_MIXIN
  // use Printf()
  // (take 'this' into account in attribute parameter count)
  // int sprintf(const wxString& format, ...) WX_ATTRIBUTE_PRINTF_2;
  WX_DEFINE_VARARG_FUNC(int, sprintf, 1, (const wxFormatString&),
                        DoPrintfWchar, DoPrintfUtf8)
#ifdef __WATCOMC__
  // workaround for http://bugzilla.openwatcom.org/show_bug.cgi?id=351
  WX_VARARG_WATCOM_WORKAROUND(int, sprintf, 1, (const wxString&),
                              (wxFormatString(f1)));
  WX_VARARG_WATCOM_WORKAROUND(int, sprintf, 1, (const wxCStrData&),
                              (wxFormatString(f1)));
  WX_VARARG_WATCOM_WORKAROUND(int, sprintf, 1, (const char*),
                              (wxFormatString(f1)));
  WX_VARARG_WATCOM_WORKAROUND(int, sprintf, 1, (const wchar_t*),
                              (wxFormatString(f1)));
#endif
#endif // wxNEEDS_WXSTRING_PRINTF_MIXIN

    // use Cmp()
  int CompareTo(const wxChar* psz, caseCompare cmp = exact) const
    { return cmp == exact ? Cmp(psz) : CmpNoCase(psz); }

    // use length()
  size_t Length() const { return length(); }
    // Count the number of characters
  int Freq(wxUniChar ch) const;
    // use MakeLower
  void LowerCase() { MakeLower(); }
    // use MakeUpper
  void UpperCase() { MakeUpper(); }
    // use Trim except that it doesn't change this string
  wxString Strip(stripType w = trailing) const;

    // use Find (more general variants not yet supported)
  size_t Index(const wxChar* psz) const { return Find(psz); }
  size_t Index(wxUniChar ch)         const { return Find(ch);  }
    // use Truncate
  wxString& Remove(size_t pos) { return Truncate(pos); }
  wxString& RemoveLast(size_t n = 1) { return Truncate(length() - n); }

  wxString& Remove(size_t nStart, size_t nLen)
      { return (wxString&)erase( nStart, nLen ); }

    // use Find()
  int First( wxUniChar ch ) const { return Find(ch); }
  int First( wxUniCharRef ch ) const { return Find(ch); }
  int First( char ch ) const { return Find(ch); }
  int First( unsigned char ch ) const { return Find(ch); }
  int First( wchar_t ch ) const { return Find(ch); }
  int First( const wxString& str ) const { return Find(str); }
  int Last( wxUniChar ch ) const { return Find(ch, true); }
  bool Contains(const wxString& str) const { return Find(str) != wxNOT_FOUND; }

    // use empty()
  bool IsNull() const { return empty(); }

  // std::string compatibility functions

    // take nLen chars starting at nPos
  wxString(const wxString& str, size_t nPos, size_t nLen)
      { assign(str, nPos, nLen); }
    // take all characters from first to last
  wxString(const_iterator first, const_iterator last)
      : m_impl(first.impl(), last.impl()) { }
#if WXWIN_COMPATIBILITY_STRING_PTR_AS_ITER
    // the 2 overloads below are for compatibility with the existing code using
    // pointers instead of iterators
  wxString(const char *first, const char *last)
  {
      SubstrBufFromMB str(ImplStr(first, last - first));
      m_impl.assign(str.data, str.len);
  }
  wxString(const wchar_t *first, const wchar_t *last)
  {
      SubstrBufFromWC str(ImplStr(first, last - first));
      m_impl.assign(str.data, str.len);
  }
    // and this one is needed to compile code adding offsets to c_str() result
  wxString(const wxCStrData& first, const wxCStrData& last)
      : m_impl(CreateConstIterator(first).impl(),
               CreateConstIterator(last).impl())
  {
      wxASSERT_MSG( first.m_str == last.m_str,
                    wxT("pointers must be into the same string") );
  }
#endif // WXWIN_COMPATIBILITY_STRING_PTR_AS_ITER

  // lib.string.modifiers
    // append elements str[pos], ..., str[pos+n]
  wxString& append(const wxString& str, size_t pos, size_t n)
  {
      wxSTRING_UPDATE_CACHED_LENGTH(n);

      size_t from, len;
      str.PosLenToImpl(pos, n, &from, &len);
      m_impl.append(str.m_impl, from, len);
      return *this;
  }
    // append a string
  wxString& append(const wxString& str)
  {
      wxSTRING_UPDATE_CACHED_LENGTH(str.length());

      m_impl.append(str.m_impl);
      return *this;
  }

    // append first n (or all if n == npos) characters of sz
  wxString& append(const char *sz)
  {
      wxSTRING_INVALIDATE_CACHED_LENGTH();

      m_impl.append(ImplStr(sz));
      return *this;
  }

  wxString& append(const wchar_t *sz)
  {
      wxSTRING_INVALIDATE_CACHED_LENGTH();

      m_impl.append(ImplStr(sz));
      return *this;
  }

  wxString& append(const char *sz, size_t n)
  {
      wxSTRING_INVALIDATE_CACHED_LENGTH();

      SubstrBufFromMB str(ImplStr(sz, n));
      m_impl.append(str.data, str.len);
      return *this;
  }
  wxString& append(const wchar_t *sz, size_t n)
  {
      wxSTRING_UPDATE_CACHED_LENGTH(n);

      SubstrBufFromWC str(ImplStr(sz, n));
      m_impl.append(str.data, str.len);
      return *this;
  }

  wxString& append(const wxCStrData& str)
    { return append(str.AsString()); }
  wxString& append(const wxScopedCharBuffer& str)
    { return append(str.data(), str.length()); }
  wxString& append(const wxScopedWCharBuffer& str)
    { return append(str.data(), str.length()); }
  wxString& append(const wxCStrData& str, size_t n)
    { return append(str.AsString(), 0, n); }
  wxString& append(const wxScopedCharBuffer& str, size_t n)
    { return append(str.data(), n); }
  wxString& append(const wxScopedWCharBuffer& str, size_t n)
    { return append(str.data(), n); }

    // append n copies of ch
  wxString& append(size_t n, wxUniChar ch)
  {
#if wxUSE_UNICODE_UTF8
      if ( !ch.IsAscii() )
      {
          wxSTRING_INVALIDATE_CACHED_LENGTH();

          m_impl.append(wxStringOperations::EncodeNChars(n, ch));
      }
      else // ASCII
#endif
      {
          wxSTRING_UPDATE_CACHED_LENGTH(n);

          m_impl.append(n, (wxStringCharType)ch);
      }

      return *this;
  }

  wxString& append(size_t n, wxUniCharRef ch)
    { return append(n, wxUniChar(ch)); }
  wxString& append(size_t n, char ch)
    { return append(n, wxUniChar(ch)); }
  wxString& append(size_t n, unsigned char ch)
    { return append(n, wxUniChar(ch)); }
  wxString& append(size_t n, wchar_t ch)
    { return append(n, wxUniChar(ch)); }

    // append from first to last
  wxString& append(const_iterator first, const_iterator last)
  {
      wxSTRING_INVALIDATE_CACHED_LENGTH();

      m_impl.append(first.impl(), last.impl());
      return *this;
  }
#if WXWIN_COMPATIBILITY_STRING_PTR_AS_ITER
  wxString& append(const char *first, const char *last)
    { return append(first, last - first); }
  wxString& append(const wchar_t *first, const wchar_t *last)
    { return append(first, last - first); }
  wxString& append(const wxCStrData& first, const wxCStrData& last)
    { return append(CreateConstIterator(first), CreateConstIterator(last)); }
#endif // WXWIN_COMPATIBILITY_STRING_PTR_AS_ITER

    // same as `this_string = str'
  wxString& assign(const wxString& str)
  {
      wxSTRING_SET_CACHED_LENGTH(str.length());

      m_impl = str.m_impl;

      return *this;
  }

    // This is a non-standard-compliant overload taking the first "len"
    // characters of the source string.
  wxString& assign(const wxString& str, size_t len)
  {
#if wxUSE_STRING_POS_CACHE
      // It is legal to pass len > str.length() to wxStringImpl::assign() but
      // by restricting it here we save some work for that function so it's not
      // really less efficient and, at the same time, ensure that we don't
      // cache invalid length.
      const size_t lenSrc = str.length();
      if ( len > lenSrc )
          len = lenSrc;

      wxSTRING_SET_CACHED_LENGTH(len);
#endif // wxUSE_STRING_POS_CACHE

      m_impl.assign(str.m_impl, 0, str.LenToImpl(len));

      return *this;
  }

    // same as ` = str[pos..pos + n]
  wxString& assign(const wxString& str, size_t pos, size_t n)
  {
      size_t from, len;
      str.PosLenToImpl(pos, n, &from, &len);
      m_impl.assign(str.m_impl, from, len);

      // it's important to call this after PosLenToImpl() above in case str is
      // the same string as this one
      wxSTRING_SET_CACHED_LENGTH(n);

      return *this;
  }

    // same as `= first n (or all if n == npos) characters of sz'
  wxString& assign(const char *sz)
  {
      wxSTRING_INVALIDATE_CACHE();

      m_impl.assign(ImplStr(sz));

      return *this;
  }

  wxString& assign(const wchar_t *sz)
  {
      wxSTRING_INVALIDATE_CACHE();

      m_impl.assign(ImplStr(sz));

      return *this;
  }

  wxString& assign(const char *sz, size_t n)
  {
      wxSTRING_INVALIDATE_CACHE();

      SubstrBufFromMB str(ImplStr(sz, n));
      m_impl.assign(str.data, str.len);

      return *this;
  }

  wxString& assign(const wchar_t *sz, size_t n)
  {
      wxSTRING_SET_CACHED_LENGTH(n);

      SubstrBufFromWC str(ImplStr(sz, n));
      m_impl.assign(str.data, str.len);

      return *this;
  }

  wxString& assign(const wxCStrData& str)
    { return assign(str.AsString()); }
  wxString& assign(const wxScopedCharBuffer& str)
    { return assign(str.data(), str.length()); }
  wxString& assign(const wxScopedWCharBuffer& str)
    { return assign(str.data(), str.length()); }
  wxString& assign(const wxCStrData& str, size_t len)
    { return assign(str.AsString(), len); }
  wxString& assign(const wxScopedCharBuffer& str, size_t len)
    { return assign(str.data(), len); }
  wxString& assign(const wxScopedWCharBuffer& str, size_t len)
    { return assign(str.data(), len); }

    // same as `= n copies of ch'
  wxString& assign(size_t n, wxUniChar ch)
  {
      wxSTRING_SET_CACHED_LENGTH(n);

#if wxUSE_UNICODE_UTF8
      if ( !ch.IsAscii() )
          m_impl.assign(wxStringOperations::EncodeNChars(n, ch));
      else
#endif
          m_impl.assign(n, (wxStringCharType)ch);

      return *this;
  }

  wxString& assign(size_t n, wxUniCharRef ch)
    { return assign(n, wxUniChar(ch)); }
  wxString& assign(size_t n, char ch)
    { return assign(n, wxUniChar(ch)); }
  wxString& assign(size_t n, unsigned char ch)
    { return assign(n, wxUniChar(ch)); }
  wxString& assign(size_t n, wchar_t ch)
    { return assign(n, wxUniChar(ch)); }

    // assign from first to last
  wxString& assign(const_iterator first, const_iterator last)
  {
      wxSTRING_INVALIDATE_CACHE();

      m_impl.assign(first.impl(), last.impl());

      return *this;
  }
#if WXWIN_COMPATIBILITY_STRING_PTR_AS_ITER
  wxString& assign(const char *first, const char *last)
    { return assign(first, last - first); }
  wxString& assign(const wchar_t *first, const wchar_t *last)
    { return assign(first, last - first); }
  wxString& assign(const wxCStrData& first, const wxCStrData& last)
    { return assign(CreateConstIterator(first), CreateConstIterator(last)); }
#endif // WXWIN_COMPATIBILITY_STRING_PTR_AS_ITER

    // string comparison
  int compare(const wxString& str) const;
  int compare(const char* sz) const;
  int compare(const wchar_t* sz) const;
  int compare(const wxCStrData& str) const
    { return compare(str.AsString()); }
  int compare(const wxScopedCharBuffer& str) const
    { return compare(str.data()); }
  int compare(const wxScopedWCharBuffer& str) const
    { return compare(str.data()); }
    // comparison with a substring
  int compare(size_t nStart, size_t nLen, const wxString& str) const;
    // comparison of 2 substrings
  int compare(size_t nStart, size_t nLen,
              const wxString& str, size_t nStart2, size_t nLen2) const;
    // substring comparison with first nCount characters of sz
  int compare(size_t nStart, size_t nLen,
              const char* sz, size_t nCount = npos) const;
  int compare(size_t nStart, size_t nLen,
              const wchar_t* sz, size_t nCount = npos) const;

    // insert another string
  wxString& insert(size_t nPos, const wxString& str)
    { insert(GetIterForNthChar(nPos), str.begin(), str.end()); return *this; }
    // insert n chars of str starting at nStart (in str)
  wxString& insert(size_t nPos, const wxString& str, size_t nStart, size_t n)
  {
      wxSTRING_UPDATE_CACHED_LENGTH(n);

      size_t from, len;
      str.PosLenToImpl(nStart, n, &from, &len);
      m_impl.insert(PosToImpl(nPos), str.m_impl, from, len);

      return *this;
  }

    // insert first n (or all if n == npos) characters of sz
  wxString& insert(size_t nPos, const char *sz)
  {
      wxSTRING_INVALIDATE_CACHE();

      m_impl.insert(PosToImpl(nPos), ImplStr(sz));

      return *this;
  }

  wxString& insert(size_t nPos, const wchar_t *sz)
  {
      wxSTRING_INVALIDATE_CACHE();

      m_impl.insert(PosToImpl(nPos), ImplStr(sz)); return *this;
  }

  wxString& insert(size_t nPos, const char *sz, size_t n)
  {
      wxSTRING_UPDATE_CACHED_LENGTH(n);

      SubstrBufFromMB str(ImplStr(sz, n));
      m_impl.insert(PosToImpl(nPos), str.data, str.len);

      return *this;
  }

  wxString& insert(size_t nPos, const wchar_t *sz, size_t n)
  {
      wxSTRING_UPDATE_CACHED_LENGTH(n);

      SubstrBufFromWC str(ImplStr(sz, n));
      m_impl.insert(PosToImpl(nPos), str.data, str.len);

      return *this;
  }

    // insert n copies of ch
  wxString& insert(size_t nPos, size_t n, wxUniChar ch)
  {
      wxSTRING_UPDATE_CACHED_LENGTH(n);

#if wxUSE_UNICODE_UTF8
      if ( !ch.IsAscii() )
          m_impl.insert(PosToImpl(nPos), wxStringOperations::EncodeNChars(n, ch));
      else
#endif
          m_impl.insert(PosToImpl(nPos), n, (wxStringCharType)ch);
      return *this;
  }

  iterator insert(iterator it, wxUniChar ch)
  {
      wxSTRING_UPDATE_CACHED_LENGTH(1);

#if wxUSE_UNICODE_UTF8
      if ( !ch.IsAscii() )
      {
          size_t pos = IterToImplPos(it);
          m_impl.insert(pos, wxStringOperations::EncodeChar(ch));
          return iterator(this, m_impl.begin() + pos);
      }
      else
#endif
          return iterator(this, m_impl.insert(it.impl(), (wxStringCharType)ch));
  }

  void insert(iterator it, const_iterator first, const_iterator last)
  {
      wxSTRING_INVALIDATE_CACHE();

      m_impl.insert(it.impl(), first.impl(), last.impl());
  }

#if WXWIN_COMPATIBILITY_STRING_PTR_AS_ITER
  void insert(iterator it, const char *first, const char *last)
    { insert(it - begin(), first, last - first); }
  void insert(iterator it, const wchar_t *first, const wchar_t *last)
    { insert(it - begin(), first, last - first); }
  void insert(iterator it, const wxCStrData& first, const wxCStrData& last)
    { insert(it, CreateConstIterator(first), CreateConstIterator(last)); }
#endif // WXWIN_COMPATIBILITY_STRING_PTR_AS_ITER

  void insert(iterator it, size_type n, wxUniChar ch)
  {
      wxSTRING_UPDATE_CACHED_LENGTH(n);

#if wxUSE_UNICODE_UTF8
      if ( !ch.IsAscii() )
          m_impl.insert(IterToImplPos(it), wxStringOperations::EncodeNChars(n, ch));
      else
#endif
          m_impl.insert(it.impl(), n, (wxStringCharType)ch);
  }

    // delete characters from nStart to nStart + nLen
  wxString& erase(size_type pos = 0, size_type n = npos)
  {
      wxSTRING_INVALIDATE_CACHE();

      size_t from, len;
      PosLenToImpl(pos, n, &from, &len);
      m_impl.erase(from, len);

      return *this;
  }

    // delete characters from first up to last
  iterator erase(iterator first, iterator last)
  {
      wxSTRING_INVALIDATE_CACHE();

      return iterator(this, m_impl.erase(first.impl(), last.impl()));
  }

  iterator erase(iterator first)
  {
      wxSTRING_UPDATE_CACHED_LENGTH(-1);

      return iterator(this, m_impl.erase(first.impl()));
  }

#ifdef wxSTRING_BASE_HASNT_CLEAR
  void clear() { erase(); }
#else
  void clear()
  {
      wxSTRING_SET_CACHED_LENGTH(0);

      m_impl.clear();
  }
#endif

    // replaces the substring of length nLen starting at nStart
  wxString& replace(size_t nStart, size_t nLen, const char* sz)
  {
      wxSTRING_INVALIDATE_CACHE();

      size_t from, len;
      PosLenToImpl(nStart, nLen, &from, &len);
      m_impl.replace(from, len, ImplStr(sz));

      return *this;
  }

  wxString& replace(size_t nStart, size_t nLen, const wchar_t* sz)
  {
      wxSTRING_INVALIDATE_CACHE();

      size_t from, len;
      PosLenToImpl(nStart, nLen, &from, &len);
      m_impl.replace(from, len, ImplStr(sz));

      return *this;
  }

    // replaces the substring of length nLen starting at nStart
  wxString& replace(size_t nStart, size_t nLen, const wxString& str)
  {
      wxSTRING_INVALIDATE_CACHE();

      size_t from, len;
      PosLenToImpl(nStart, nLen, &from, &len);
      m_impl.replace(from, len, str.m_impl);

      return *this;
  }

    // replaces the substring with nCount copies of ch
  wxString& replace(size_t nStart, size_t nLen, size_t nCount, wxUniChar ch)
  {
      wxSTRING_INVALIDATE_CACHE();

      size_t from, len;
      PosLenToImpl(nStart, nLen, &from, &len);
#if wxUSE_UNICODE_UTF8
      if ( !ch.IsAscii() )
          m_impl.replace(from, len, wxStringOperations::EncodeNChars(nCount, ch));
      else
#endif
          m_impl.replace(from, len, nCount, (wxStringCharType)ch);

      return *this;
  }

    // replaces a substring with another substring
  wxString& replace(size_t nStart, size_t nLen,
                    const wxString& str, size_t nStart2, size_t nLen2)
  {
      wxSTRING_INVALIDATE_CACHE();

      size_t from, len;
      PosLenToImpl(nStart, nLen, &from, &len);

      size_t from2, len2;
      str.PosLenToImpl(nStart2, nLen2, &from2, &len2);

      m_impl.replace(from, len, str.m_impl, from2, len2);

      return *this;
  }

     // replaces the substring with first nCount chars of sz
  wxString& replace(size_t nStart, size_t nLen,
                    const char* sz, size_t nCount)
  {
      wxSTRING_INVALIDATE_CACHE();

      size_t from, len;
      PosLenToImpl(nStart, nLen, &from, &len);

      SubstrBufFromMB str(ImplStr(sz, nCount));

      m_impl.replace(from, len, str.data, str.len);

      return *this;
  }

  wxString& replace(size_t nStart, size_t nLen,
                    const wchar_t* sz, size_t nCount)
  {
      wxSTRING_INVALIDATE_CACHE();

      size_t from, len;
      PosLenToImpl(nStart, nLen, &from, &len);

      SubstrBufFromWC str(ImplStr(sz, nCount));

      m_impl.replace(from, len, str.data, str.len);

      return *this;
  }

  wxString& replace(size_t nStart, size_t nLen,
                    const wxString& s, size_t nCount)
  {
      wxSTRING_INVALIDATE_CACHE();

      size_t from, len;
      PosLenToImpl(nStart, nLen, &from, &len);
      m_impl.replace(from, len, s.m_impl.c_str(), s.LenToImpl(nCount));

      return *this;
  }

  wxString& replace(iterator first, iterator last, const char* s)
  {
      wxSTRING_INVALIDATE_CACHE();

      m_impl.replace(first.impl(), last.impl(), ImplStr(s));

      return *this;
  }

  wxString& replace(iterator first, iterator last, const wchar_t* s)
  {
      wxSTRING_INVALIDATE_CACHE();

      m_impl.replace(first.impl(), last.impl(), ImplStr(s));

      return *this;
  }

  wxString& replace(iterator first, iterator last, const char* s, size_type n)
  {
      wxSTRING_INVALIDATE_CACHE();

      SubstrBufFromMB str(ImplStr(s, n));
      m_impl.replace(first.impl(), last.impl(), str.data, str.len);

      return *this;
  }

  wxString& replace(iterator first, iterator last, const wchar_t* s, size_type n)
  {
      wxSTRING_INVALIDATE_CACHE();

      SubstrBufFromWC str(ImplStr(s, n));
      m_impl.replace(first.impl(), last.impl(), str.data, str.len);

      return *this;
  }

  wxString& replace(iterator first, iterator last, const wxString& s)
  {
      wxSTRING_INVALIDATE_CACHE();

      m_impl.replace(first.impl(), last.impl(), s.m_impl);

      return *this;
  }

  wxString& replace(iterator first, iterator last, size_type n, wxUniChar ch)
  {
      wxSTRING_INVALIDATE_CACHE();

#if wxUSE_UNICODE_UTF8
      if ( !ch.IsAscii() )
          m_impl.replace(first.impl(), last.impl(),
                  wxStringOperations::EncodeNChars(n, ch));
      else
#endif
          m_impl.replace(first.impl(), last.impl(), n, (wxStringCharType)ch);

      return *this;
  }

  wxString& replace(iterator first, iterator last,
                    const_iterator first1, const_iterator last1)
  {
      wxSTRING_INVALIDATE_CACHE();

      m_impl.replace(first.impl(), last.impl(), first1.impl(), last1.impl());

      return *this;
  }

  wxString& replace(iterator first, iterator last,
                    const char *first1, const char *last1)
    { replace(first, last, first1, last1 - first1); return *this; }
  wxString& replace(iterator first, iterator last,
                    const wchar_t *first1, const wchar_t *last1)
    { replace(first, last, first1, last1 - first1); return *this; }

  // swap two strings
  void swap(wxString& str)
  {
#if wxUSE_STRING_POS_CACHE
      // we modify not only this string but also the other one directly so we
      // need to invalidate cache for both of them (we could also try to
      // exchange their cache entries but it seems unlikely to be worth it)
      InvalidateCache();
      str.InvalidateCache();
#endif // wxUSE_STRING_POS_CACHE

      m_impl.swap(str.m_impl);
  }

    // find a substring
  size_t find(const wxString& str, size_t nStart = 0) const
    { return PosFromImpl(m_impl.find(str.m_impl, PosToImpl(nStart))); }

    // find first n characters of sz
  size_t find(const char* sz, size_t nStart = 0, size_t n = npos) const
  {
      SubstrBufFromMB str(ImplStr(sz, n));
      return PosFromImpl(m_impl.find(str.data, PosToImpl(nStart), str.len));
  }
  size_t find(const wchar_t* sz, size_t nStart = 0, size_t n = npos) const
  {
      SubstrBufFromWC str(ImplStr(sz, n));
      return PosFromImpl(m_impl.find(str.data, PosToImpl(nStart), str.len));
  }
  size_t find(const wxScopedCharBuffer& s, size_t nStart = 0, size_t n = npos) const
    { return find(s.data(), nStart, n); }
  size_t find(const wxScopedWCharBuffer& s, size_t nStart = 0, size_t n = npos) const
    { return find(s.data(), nStart, n); }
  size_t find(const wxCStrData& s, size_t nStart = 0, size_t n = npos) const
    { return find(s.AsWChar(), nStart, n); }

    // find the first occurrence of character ch after nStart
  size_t find(wxUniChar ch, size_t nStart = 0) const
  {
#if wxUSE_UNICODE_UTF8
    if ( !ch.IsAscii() )
        return PosFromImpl(m_impl.find(wxStringOperations::EncodeChar(ch),
                                       PosToImpl(nStart)));
    else
#endif
        return PosFromImpl(m_impl.find((wxStringCharType)ch,
                                       PosToImpl(nStart)));

  }
  size_t find(wxUniCharRef ch, size_t nStart = 0) const
    {  return find(wxUniChar(ch), nStart); }
  size_t find(char ch, size_t nStart = 0) const
    {  return find(wxUniChar(ch), nStart); }
  size_t find(unsigned char ch, size_t nStart = 0) const
    {  return find(wxUniChar(ch), nStart); }
  size_t find(wchar_t ch, size_t nStart = 0) const
    {  return find(wxUniChar(ch), nStart); }

    // rfind() family is exactly like find() but works right to left

    // as find, but from the end
  size_t rfind(const wxString& str, size_t nStart = npos) const
    { return PosFromImpl(m_impl.rfind(str.m_impl, PosToImpl(nStart))); }

    // as find, but from the end
  size_t rfind(const char* sz, size_t nStart = npos, size_t n = npos) const
  {
      SubstrBufFromMB str(ImplStr(sz, n));
      return PosFromImpl(m_impl.rfind(str.data, PosToImpl(nStart), str.len));
  }
  size_t rfind(const wchar_t* sz, size_t nStart = npos, size_t n = npos) const
  {
      SubstrBufFromWC str(ImplStr(sz, n));
      return PosFromImpl(m_impl.rfind(str.data, PosToImpl(nStart), str.len));
  }
  size_t rfind(const wxScopedCharBuffer& s, size_t nStart = npos, size_t n = npos) const
    { return rfind(s.data(), nStart, n); }
  size_t rfind(const wxScopedWCharBuffer& s, size_t nStart = npos, size_t n = npos) const
    { return rfind(s.data(), nStart, n); }
  size_t rfind(const wxCStrData& s, size_t nStart = npos, size_t n = npos) const
    { return rfind(s.AsWChar(), nStart, n); }
    // as find, but from the end
  size_t rfind(wxUniChar ch, size_t nStart = npos) const
  {
#if wxUSE_UNICODE_UTF8
    if ( !ch.IsAscii() )
        return PosFromImpl(m_impl.rfind(wxStringOperations::EncodeChar(ch),
                                        PosToImpl(nStart)));
    else
#endif
        return PosFromImpl(m_impl.rfind((wxStringCharType)ch,
                                        PosToImpl(nStart)));
  }
  size_t rfind(wxUniCharRef ch, size_t nStart = npos) const
    {  return rfind(wxUniChar(ch), nStart); }
  size_t rfind(char ch, size_t nStart = npos) const
    {  return rfind(wxUniChar(ch), nStart); }
  size_t rfind(unsigned char ch, size_t nStart = npos) const
    {  return rfind(wxUniChar(ch), nStart); }
  size_t rfind(wchar_t ch, size_t nStart = npos) const
    {  return rfind(wxUniChar(ch), nStart); }

  // find first/last occurrence of any character (not) in the set:
#if wxUSE_STL_BASED_WXSTRING && !wxUSE_UNICODE_UTF8
  // FIXME-UTF8: this is not entirely correct, because it doesn't work if
  //             sizeof(wchar_t)==2 and surrogates are present in the string;
  //             should we care? Probably not.
  size_t find_first_of(const wxString& str, size_t nStart = 0) const
    { return m_impl.find_first_of(str.m_impl, nStart); }
  size_t find_first_of(const char* sz, size_t nStart = 0) const
    { return m_impl.find_first_of(ImplStr(sz), nStart); }
  size_t find_first_of(const wchar_t* sz, size_t nStart = 0) const
    { return m_impl.find_first_of(ImplStr(sz), nStart); }
  size_t find_first_of(const char* sz, size_t nStart, size_t n) const
    { return m_impl.find_first_of(ImplStr(sz), nStart, n); }
  size_t find_first_of(const wchar_t* sz, size_t nStart, size_t n) const
    { return m_impl.find_first_of(ImplStr(sz), nStart, n); }
  size_t find_first_of(wxUniChar c, size_t nStart = 0) const
    { return m_impl.find_first_of((wxChar)c, nStart); }

  size_t find_last_of(const wxString& str, size_t nStart = npos) const
    { return m_impl.find_last_of(str.m_impl, nStart); }
  size_t find_last_of(const char* sz, size_t nStart = npos) const
    { return m_impl.find_last_of(ImplStr(sz), nStart); }
  size_t find_last_of(const wchar_t* sz, size_t nStart = npos) const
    { return m_impl.find_last_of(ImplStr(sz), nStart); }
  size_t find_last_of(const char* sz, size_t nStart, size_t n) const
    { return m_impl.find_last_of(ImplStr(sz), nStart, n); }
  size_t find_last_of(const wchar_t* sz, size_t nStart, size_t n) const
    { return m_impl.find_last_of(ImplStr(sz), nStart, n); }
  size_t find_last_of(wxUniChar c, size_t nStart = npos) const
    { return m_impl.find_last_of((wxChar)c, nStart); }

  size_t find_first_not_of(const wxString& str, size_t nStart = 0) const
    { return m_impl.find_first_not_of(str.m_impl, nStart); }
  size_t find_first_not_of(const char* sz, size_t nStart = 0) const
    { return m_impl.find_first_not_of(ImplStr(sz), nStart); }
  size_t find_first_not_of(const wchar_t* sz, size_t nStart = 0) const
    { return m_impl.find_first_not_of(ImplStr(sz), nStart); }
  size_t find_first_not_of(const char* sz, size_t nStart, size_t n) const
    { return m_impl.find_first_not_of(ImplStr(sz), nStart, n); }
  size_t find_first_not_of(const wchar_t* sz, size_t nStart, size_t n) const
    { return m_impl.find_first_not_of(ImplStr(sz), nStart, n); }
  size_t find_first_not_of(wxUniChar c, size_t nStart = 0) const
    { return m_impl.find_first_not_of((wxChar)c, nStart); }

  size_t find_last_not_of(const wxString& str, size_t nStart = npos) const
    { return m_impl.find_last_not_of(str.m_impl, nStart); }
  size_t find_last_not_of(const char* sz, size_t nStart = npos) const
    { return m_impl.find_last_not_of(ImplStr(sz), nStart); }
  size_t find_last_not_of(const wchar_t* sz, size_t nStart = npos) const
    { return m_impl.find_last_not_of(ImplStr(sz), nStart); }
  size_t find_last_not_of(const char* sz, size_t nStart, size_t n) const
    { return m_impl.find_last_not_of(ImplStr(sz), nStart, n); }
  size_t find_last_not_of(const wchar_t* sz, size_t nStart, size_t n) const
    { return m_impl.find_last_not_of(ImplStr(sz), nStart, n); }
  size_t find_last_not_of(wxUniChar c, size_t nStart = npos) const
    { return m_impl.find_last_not_of((wxChar)c, nStart); }
#else
  // we can't use std::string implementation in UTF-8 build, because the
  // character sets would be interpreted wrongly:

    // as strpbrk() but starts at nStart, returns npos if not found
  size_t find_first_of(const wxString& str, size_t nStart = 0) const
#if wxUSE_UNICODE // FIXME-UTF8: temporary
    { return find_first_of(str.wc_str(), nStart); }
#else
    { return find_first_of(str.mb_str(), nStart); }
#endif
    // same as above
  size_t find_first_of(const char* sz, size_t nStart = 0) const;
  size_t find_first_of(const wchar_t* sz, size_t nStart = 0) const;
  size_t find_first_of(const char* sz, size_t nStart, size_t n) const;
  size_t find_first_of(const wchar_t* sz, size_t nStart, size_t n) const;
    // same as find(char, size_t)
  size_t find_first_of(wxUniChar c, size_t nStart = 0) const
    { return find(c, nStart); }
    // find the last (starting from nStart) char from str in this string
  size_t find_last_of (const wxString& str, size_t nStart = npos) const
#if wxUSE_UNICODE // FIXME-UTF8: temporary
    { return find_last_of(str.wc_str(), nStart); }
#else
    { return find_last_of(str.mb_str(), nStart); }
#endif
    // same as above
  size_t find_last_of (const char* sz, size_t nStart = npos) const;
  size_t find_last_of (const wchar_t* sz, size_t nStart = npos) const;
  size_t find_last_of(const char* sz, size_t nStart, size_t n) const;
  size_t find_last_of(const wchar_t* sz, size_t nStart, size_t n) const;
    // same as above
  size_t find_last_of(wxUniChar c, size_t nStart = npos) const
    { return rfind(c, nStart); }

    // find first/last occurrence of any character not in the set

    // as strspn() (starting from nStart), returns npos on failure
  size_t find_first_not_of(const wxString& str, size_t nStart = 0) const
#if wxUSE_UNICODE // FIXME-UTF8: temporary
    { return find_first_not_of(str.wc_str(), nStart); }
#else
    { return find_first_not_of(str.mb_str(), nStart); }
#endif
    // same as above
  size_t find_first_not_of(const char* sz, size_t nStart = 0) const;
  size_t find_first_not_of(const wchar_t* sz, size_t nStart = 0) const;
  size_t find_first_not_of(const char* sz, size_t nStart, size_t n) const;
  size_t find_first_not_of(const wchar_t* sz, size_t nStart, size_t n) const;
    // same as above
  size_t find_first_not_of(wxUniChar ch, size_t nStart = 0) const;
    //  as strcspn()
  size_t find_last_not_of(const wxString& str, size_t nStart = npos) const
#if wxUSE_UNICODE // FIXME-UTF8: temporary
    { return find_last_not_of(str.wc_str(), nStart); }
#else
    { return find_last_not_of(str.mb_str(), nStart); }
#endif
    // same as above
  size_t find_last_not_of(const char* sz, size_t nStart = npos) const;
  size_t find_last_not_of(const wchar_t* sz, size_t nStart = npos) const;
  size_t find_last_not_of(const char* sz, size_t nStart, size_t n) const;
  size_t find_last_not_of(const wchar_t* sz, size_t nStart, size_t n) const;
    // same as above
  size_t find_last_not_of(wxUniChar ch, size_t nStart = npos) const;
#endif // wxUSE_STL_BASED_WXSTRING && !wxUSE_UNICODE_UTF8 or not

  // provide char/wchar_t/wxUniCharRef overloads for char-finding functions
  // above to resolve ambiguities:
  size_t find_first_of(wxUniCharRef ch, size_t nStart = 0) const
    {  return find_first_of(wxUniChar(ch), nStart); }
  size_t find_first_of(char ch, size_t nStart = 0) const
    {  return find_first_of(wxUniChar(ch), nStart); }
  size_t find_first_of(unsigned char ch, size_t nStart = 0) const
    {  return find_first_of(wxUniChar(ch), nStart); }
  size_t find_first_of(wchar_t ch, size_t nStart = 0) const
    {  return find_first_of(wxUniChar(ch), nStart); }
  size_t find_last_of(wxUniCharRef ch, size_t nStart = npos) const
    {  return find_last_of(wxUniChar(ch), nStart); }
  size_t find_last_of(char ch, size_t nStart = npos) const
    {  return find_last_of(wxUniChar(ch), nStart); }
  size_t find_last_of(unsigned char ch, size_t nStart = npos) const
    {  return find_last_of(wxUniChar(ch), nStart); }
  size_t find_last_of(wchar_t ch, size_t nStart = npos) const
    {  return find_last_of(wxUniChar(ch), nStart); }
  size_t find_first_not_of(wxUniCharRef ch, size_t nStart = 0) const
    {  return find_first_not_of(wxUniChar(ch), nStart); }
  size_t find_first_not_of(char ch, size_t nStart = 0) const
    {  return find_first_not_of(wxUniChar(ch), nStart); }
  size_t find_first_not_of(unsigned char ch, size_t nStart = 0) const
    {  return find_first_not_of(wxUniChar(ch), nStart); }
  size_t find_first_not_of(wchar_t ch, size_t nStart = 0) const
    {  return find_first_not_of(wxUniChar(ch), nStart); }
  size_t find_last_not_of(wxUniCharRef ch, size_t nStart = npos) const
    {  return find_last_not_of(wxUniChar(ch), nStart); }
  size_t find_last_not_of(char ch, size_t nStart = npos) const
    {  return find_last_not_of(wxUniChar(ch), nStart); }
  size_t find_last_not_of(unsigned char ch, size_t nStart = npos) const
    {  return find_last_not_of(wxUniChar(ch), nStart); }
  size_t find_last_not_of(wchar_t ch, size_t nStart = npos) const
    {  return find_last_not_of(wxUniChar(ch), nStart); }

  // and additional overloads for the versions taking strings:
  size_t find_first_of(const wxCStrData& sz, size_t nStart = 0) const
    { return find_first_of(sz.AsString(), nStart); }
  size_t find_first_of(const wxScopedCharBuffer& sz, size_t nStart = 0) const
    { return find_first_of(sz.data(), nStart); }
  size_t find_first_of(const wxScopedWCharBuffer& sz, size_t nStart = 0) const
    { return find_first_of(sz.data(), nStart); }
  size_t find_first_of(const wxCStrData& sz, size_t nStart, size_t n) const
    { return find_first_of(sz.AsWChar(), nStart, n); }
  size_t find_first_of(const wxScopedCharBuffer& sz, size_t nStart, size_t n) const
    { return find_first_of(sz.data(), nStart, n); }
  size_t find_first_of(const wxScopedWCharBuffer& sz, size_t nStart, size_t n) const
    { return find_first_of(sz.data(), nStart, n); }

  size_t find_last_of(const wxCStrData& sz, size_t nStart = 0) const
    { return find_last_of(sz.AsString(), nStart); }
  size_t find_last_of(const wxScopedCharBuffer& sz, size_t nStart = 0) const
    { return find_last_of(sz.data(), nStart); }
  size_t find_last_of(const wxScopedWCharBuffer& sz, size_t nStart = 0) const
    { return find_last_of(sz.data(), nStart); }
  size_t find_last_of(const wxCStrData& sz, size_t nStart, size_t n) const
    { return find_last_of(sz.AsWChar(), nStart, n); }
  size_t find_last_of(const wxScopedCharBuffer& sz, size_t nStart, size_t n) const
    { return find_last_of(sz.data(), nStart, n); }
  size_t find_last_of(const wxScopedWCharBuffer& sz, size_t nStart, size_t n) const
    { return find_last_of(sz.data(), nStart, n); }

  size_t find_first_not_of(const wxCStrData& sz, size_t nStart = 0) const
    { return find_first_not_of(sz.AsString(), nStart); }
  size_t find_first_not_of(const wxScopedCharBuffer& sz, size_t nStart = 0) const
    { return find_first_not_of(sz.data(), nStart); }
  size_t find_first_not_of(const wxScopedWCharBuffer& sz, size_t nStart = 0) const
    { return find_first_not_of(sz.data(), nStart); }
  size_t find_first_not_of(const wxCStrData& sz, size_t nStart, size_t n) const
    { return find_first_not_of(sz.AsWChar(), nStart, n); }
  size_t find_first_not_of(const wxScopedCharBuffer& sz, size_t nStart, size_t n) const
    { return find_first_not_of(sz.data(), nStart, n); }
  size_t find_first_not_of(const wxScopedWCharBuffer& sz, size_t nStart, size_t n) const
    { return find_first_not_of(sz.data(), nStart, n); }

  size_t find_last_not_of(const wxCStrData& sz, size_t nStart = 0) const
    { return find_last_not_of(sz.AsString(), nStart); }
  size_t find_last_not_of(const wxScopedCharBuffer& sz, size_t nStart = 0) const
    { return find_last_not_of(sz.data(), nStart); }
  size_t find_last_not_of(const wxScopedWCharBuffer& sz, size_t nStart = 0) const
    { return find_last_not_of(sz.data(), nStart); }
  size_t find_last_not_of(const wxCStrData& sz, size_t nStart, size_t n) const
    { return find_last_not_of(sz.AsWChar(), nStart, n); }
  size_t find_last_not_of(const wxScopedCharBuffer& sz, size_t nStart, size_t n) const
    { return find_last_not_of(sz.data(), nStart, n); }
  size_t find_last_not_of(const wxScopedWCharBuffer& sz, size_t nStart, size_t n) const
    { return find_last_not_of(sz.data(), nStart, n); }

      // string += string
  wxString& operator+=(const wxString& s)
  {
      wxSTRING_INVALIDATE_CACHED_LENGTH();

      m_impl += s.m_impl;
      return *this;
  }
      // string += C string
  wxString& operator+=(const char *psz)
  {
      wxSTRING_INVALIDATE_CACHED_LENGTH();

      m_impl += ImplStr(psz);
      return *this;
  }
  wxString& operator+=(const wchar_t *pwz)
  {
      wxSTRING_INVALIDATE_CACHED_LENGTH();

      m_impl += ImplStr(pwz);
      return *this;
  }
  wxString& operator+=(const wxCStrData& s)
  {
      wxSTRING_INVALIDATE_CACHED_LENGTH();

      m_impl += s.AsString().m_impl;
      return *this;
  }
  wxString& operator+=(const wxScopedCharBuffer& s)
    { return append(s); }
  wxString& operator+=(const wxScopedWCharBuffer& s)
    { return append(s); }
      // string += char
  wxString& operator+=(wxUniChar ch)
  {
      wxSTRING_UPDATE_CACHED_LENGTH(1);

#if wxUSE_UNICODE_UTF8
      if ( !ch.IsAscii() )
          m_impl += wxStringOperations::EncodeChar(ch);
      else
#endif
          m_impl += (wxStringCharType)ch;
      return *this;
  }
  wxString& operator+=(wxUniCharRef ch) { return *this += wxUniChar(ch); }
  wxString& operator+=(int ch) { return *this += wxUniChar(ch); }
  wxString& operator+=(char ch) { return *this += wxUniChar(ch); }
  wxString& operator+=(unsigned char ch) { return *this += wxUniChar(ch); }
  wxString& operator+=(wchar_t ch) { return *this += wxUniChar(ch); }

private:
#if !wxUSE_STL_BASED_WXSTRING
  // helpers for wxStringBuffer and wxStringBufferLength
  wxStringCharType *DoGetWriteBuf(size_t nLen)
  {
      return m_impl.DoGetWriteBuf(nLen);
  }

  void DoUngetWriteBuf()
  {
      wxSTRING_INVALIDATE_CACHE();

      m_impl.DoUngetWriteBuf();
  }

  void DoUngetWriteBuf(size_t nLen)
  {
      wxSTRING_INVALIDATE_CACHE();

      m_impl.DoUngetWriteBuf(nLen);
  }
#endif // !wxUSE_STL_BASED_WXSTRING

#ifndef wxNEEDS_WXSTRING_PRINTF_MIXIN
  #if !wxUSE_UTF8_LOCALE_ONLY
  int DoPrintfWchar(const wxChar *format, ...);
  static wxString DoFormatWchar(const wxChar *format, ...);
  #endif
  #if wxUSE_UNICODE_UTF8
  int DoPrintfUtf8(const char *format, ...);
  static wxString DoFormatUtf8(const char *format, ...);
  #endif
#endif

#if !wxUSE_STL_BASED_WXSTRING
  // check string's data validity
  bool IsValid() const { return m_impl.GetStringData()->IsValid(); }
#endif

private:
  wxStringImpl m_impl;

  // buffers for compatibility conversion from (char*)c_str() and
  // (wchar_t*)c_str(): the pointers returned by these functions should remain
  // valid until the string itself is modified for compatibility with the
  // existing code and consistency with std::string::c_str() so returning a
  // temporary buffer won't do and we need to cache the conversion results

  // TODO-UTF8: benchmark various approaches to keeping compatibility buffers
  template<typename T>
  struct ConvertedBuffer
  {
      // notice that there is no need to initialize m_len here as it's unused
      // as long as m_str is NULL
      ConvertedBuffer() : m_str(NULL) {}
      ~ConvertedBuffer()
          { free(m_str); }

      bool Extend(size_t len)
      {
          // add extra 1 for the trailing NUL
          void * const str = realloc(m_str, sizeof(T)*(len + 1));
          if ( !str )
              return false;

          m_str = static_cast<T *>(str);
          m_len = len;

          return true;
      }

      const wxScopedCharTypeBuffer<T> AsScopedBuffer() const
      {
          return wxScopedCharTypeBuffer<T>::CreateNonOwned(m_str, m_len);
      }

      T *m_str;     // pointer to the string data
      size_t m_len; // length, not size, i.e. in chars and without last NUL
  };


#if wxUSE_UNICODE
  // common mb_str() and wxCStrData::AsChar() helper: performs the conversion
  // and returns either m_convertedToChar.m_str (in which case its m_len is
  // also updated) or NULL if it failed
  //
  // there is an important exception: in wxUSE_UNICODE_UTF8 build if conv is a
  // UTF-8 one, we return m_impl.c_str() directly, without doing any conversion
  // as optimization and so the caller needs to check for this before using
  // m_convertedToChar
  //
  // NB: AsChar() returns char* in any build, unlike mb_str()
  const char *AsChar(const wxMBConv& conv) const;

  // mb_str() implementation helper
  wxScopedCharBuffer AsCharBuf(const wxMBConv& conv) const
  {
#if wxUSE_UNICODE_UTF8
      // avoid conversion if we can
      if ( conv.IsUTF8() )
      {
          return wxScopedCharBuffer::CreateNonOwned(m_impl.c_str(),
                  m_impl.length());
      }
#endif // wxUSE_UNICODE_UTF8

      // call this solely in order to fill in m_convertedToChar as AsChar()
      // updates it as a side effect: this is a bit ugly but it's a completely
      // internal function so the users of this class shouldn't care or know
      // about it and doing it like this, i.e. having a separate AsChar(),
      // allows us to avoid the creation and destruction of a temporary buffer
      // when using wxCStrData without duplicating any code
      if ( !AsChar(conv) )
      {
          // although it would be probably more correct to return NULL buffer
          // from here if the conversion fails, a lot of existing code doesn't
          // expect mb_str() (or wc_str()) to ever return NULL so return an
          // empty string otherwise to avoid crashes in it
          //
          // also, some existing code does check for the conversion success and
          // so asserting here would be bad too -- even if it does mean that
          // silently losing data is possible for badly written code
          return wxScopedCharBuffer::CreateNonOwned("", 0);
      }

      return m_convertedToChar.AsScopedBuffer();
  }

  ConvertedBuffer<char> m_convertedToChar;
#endif // !wxUSE_UNICODE

#if !wxUSE_UNICODE_WCHAR
  // common wc_str() and wxCStrData::AsWChar() helper for both UTF-8 and ANSI
  // builds: converts the string contents into m_convertedToWChar and returns
  // NULL if the conversion failed (this can only happen in ANSI build)
  //
  // NB: AsWChar() returns wchar_t* in any build, unlike wc_str()
  const wchar_t *AsWChar(const wxMBConv& conv) const;

  // wc_str() implementation helper
  wxScopedWCharBuffer AsWCharBuf(const wxMBConv& conv) const
  {
      if ( !AsWChar(conv) )
          return wxScopedWCharBuffer::CreateNonOwned(L"", 0);

      return m_convertedToWChar.AsScopedBuffer();
  }

  ConvertedBuffer<wchar_t> m_convertedToWChar;
#endif // !wxUSE_UNICODE_WCHAR

#if wxUSE_UNICODE_UTF8
  // FIXME-UTF8: (try to) move this elsewhere (TLS) or solve differently
  //             assigning to character pointer to by wxString::iterator may
  //             change the underlying wxStringImpl iterator, so we have to
  //             keep track of all iterators and update them as necessary:
  struct wxStringIteratorNodeHead
  {
      wxStringIteratorNodeHead() : ptr(NULL) {}
      wxStringIteratorNode *ptr;

      // copying is disallowed as it would result in more than one pointer into
      // the same linked list
      wxDECLARE_NO_COPY_CLASS(wxStringIteratorNodeHead);
  };

  wxStringIteratorNodeHead m_iterators;

  friend class WXDLLIMPEXP_FWD_BASE wxStringIteratorNode;
  friend class WXDLLIMPEXP_FWD_BASE wxUniCharRef;
#endif // wxUSE_UNICODE_UTF8

  friend class WXDLLIMPEXP_FWD_BASE wxCStrData;
  friend class wxStringInternalBuffer;
  friend class wxStringInternalBufferLength;
};

#ifdef wxNEEDS_WXSTRING_PRINTF_MIXIN
    #pragma warning (pop)
#endif

// string iterator operators that satisfy STL Random Access Iterator
// requirements:
inline wxString::iterator operator+(ptrdiff_t n, wxString::iterator i)
  { return i + n; }
inline wxString::const_iterator operator+(ptrdiff_t n, wxString::const_iterator i)
  { return i + n; }
inline wxString::reverse_iterator operator+(ptrdiff_t n, wxString::reverse_iterator i)
  { return i + n; }
inline wxString::const_reverse_iterator operator+(ptrdiff_t n, wxString::const_reverse_iterator i)
  { return i + n; }

// notice that even though for many compilers the friend declarations above are
// enough, from the point of view of C++ standard we must have the declarations
// here as friend ones are not injected in the enclosing namespace and without
// them the code fails to compile with conforming compilers such as xlC or g++4
wxString WXDLLIMPEXP_BASE operator+(const wxString& string1, const wxString& string2);
wxString WXDLLIMPEXP_BASE operator+(const wxString& string, const char *psz);
wxString WXDLLIMPEXP_BASE operator+(const wxString& string, const wchar_t *pwz);
wxString WXDLLIMPEXP_BASE operator+(const char *psz, const wxString& string);
wxString WXDLLIMPEXP_BASE operator+(const wchar_t *pwz, const wxString& string);

wxString WXDLLIMPEXP_BASE operator+(const wxString& string, wxUniChar ch);
wxString WXDLLIMPEXP_BASE operator+(wxUniChar ch, const wxString& string);

inline wxString operator+(const wxString& string, wxUniCharRef ch)
    { return string + (wxUniChar)ch; }
inline wxString operator+(const wxString& string, char ch)
    { return string + wxUniChar(ch); }
inline wxString operator+(const wxString& string, wchar_t ch)
    { return string + wxUniChar(ch); }
inline wxString operator+(wxUniCharRef ch, const wxString& string)
    { return (wxUniChar)ch + string; }
inline wxString operator+(char ch, const wxString& string)
    { return wxUniChar(ch) + string; }
inline wxString operator+(wchar_t ch, const wxString& string)
    { return wxUniChar(ch) + string; }


#define wxGetEmptyString() wxString()

// ----------------------------------------------------------------------------
// helper functions which couldn't be defined inline
// ----------------------------------------------------------------------------

namespace wxPrivate
{

#if wxUSE_UNICODE_WCHAR

template <>
struct wxStringAsBufHelper<char>
{
    static wxScopedCharBuffer Get(const wxString& s, size_t *len)
    {
        wxScopedCharBuffer buf(s.mb_str());
        if ( len )
            *len = buf ? strlen(buf) : 0;
        return buf;
    }
};

template <>
struct wxStringAsBufHelper<wchar_t>
{
    static wxScopedWCharBuffer Get(const wxString& s, size_t *len)
    {
        const size_t length = s.length();
        if ( len )
            *len = length;
        return wxScopedWCharBuffer::CreateNonOwned(s.wx_str(), length);
    }
};

#elif wxUSE_UNICODE_UTF8

template <>
struct wxStringAsBufHelper<char>
{
    static wxScopedCharBuffer Get(const wxString& s, size_t *len)
    {
        const size_t length = s.utf8_length();
        if ( len )
            *len = length;
        return wxScopedCharBuffer::CreateNonOwned(s.wx_str(), length);
    }
};

template <>
struct wxStringAsBufHelper<wchar_t>
{
    static wxScopedWCharBuffer Get(const wxString& s, size_t *len)
    {
        wxScopedWCharBuffer wbuf(s.wc_str());
        if ( len )
            *len = wxWcslen(wbuf);
        return wbuf;
    }
};

#endif // Unicode build kind

} // namespace wxPrivate

// ----------------------------------------------------------------------------
// wxStringBuffer: a tiny class allowing to get a writable pointer into string
// ----------------------------------------------------------------------------

#if !wxUSE_STL_BASED_WXSTRING
// string buffer for direct access to string data in their native
// representation:
class wxStringInternalBuffer
{
public:
    typedef wxStringCharType CharType;

    wxStringInternalBuffer(wxString& str, size_t lenWanted = 1024)
        : m_str(str), m_buf(NULL)
        { m_buf = m_str.DoGetWriteBuf(lenWanted); }

    ~wxStringInternalBuffer() { m_str.DoUngetWriteBuf(); }

    operator wxStringCharType*() const { return m_buf; }

private:
    wxString&         m_str;
    wxStringCharType *m_buf;

    wxDECLARE_NO_COPY_CLASS(wxStringInternalBuffer);
};

class wxStringInternalBufferLength
{
public:
    typedef wxStringCharType CharType;

    wxStringInternalBufferLength(wxString& str, size_t lenWanted = 1024)
        : m_str(str), m_buf(NULL), m_len(0), m_lenSet(false)
    {
        m_buf = m_str.DoGetWriteBuf(lenWanted);
        wxASSERT(m_buf != NULL);
    }

    ~wxStringInternalBufferLength()
    {
        wxASSERT(m_lenSet);
        m_str.DoUngetWriteBuf(m_len);
    }

    operator wxStringCharType*() const { return m_buf; }
    void SetLength(size_t length) { m_len = length; m_lenSet = true; }

private:
    wxString&         m_str;
    wxStringCharType *m_buf;
    size_t            m_len;
    bool              m_lenSet;

    wxDECLARE_NO_COPY_CLASS(wxStringInternalBufferLength);
};

#endif // !wxUSE_STL_BASED_WXSTRING

template<typename T>
class wxStringTypeBufferBase
{
public:
    typedef T CharType;

    wxStringTypeBufferBase(wxString& str, size_t lenWanted = 1024)
        : m_str(str), m_buf(lenWanted)
    {
        // for compatibility with old wxStringBuffer which provided direct
        // access to wxString internal buffer, initialize ourselves with the
        // string initial contents

        // FIXME-VC6: remove the ugly (CharType *)NULL and use normal
        //            tchar_str<CharType>
        size_t len;
        const wxCharTypeBuffer<CharType> buf(str.tchar_str(&len, (CharType *)NULL));
        if ( buf )
        {
            if ( len > lenWanted )
            {
                // in this case there is not enough space for terminating NUL,
                // ensure that we still put it there
                m_buf.data()[lenWanted] = 0;
                len = lenWanted - 1;
            }

            memcpy(m_buf.data(), buf, (len + 1)*sizeof(CharType));
        }
        //else: conversion failed, this can happen when trying to get Unicode
        //      string contents into a char string
    }

    operator CharType*() { return m_buf.data(); }

protected:
    wxString& m_str;
    wxCharTypeBuffer<CharType> m_buf;
};

template<typename T>
class wxStringTypeBufferLengthBase : public wxStringTypeBufferBase<T>
{
public:
    wxStringTypeBufferLengthBase(wxString& str, size_t lenWanted = 1024)
        : wxStringTypeBufferBase<T>(str, lenWanted),
          m_len(0),
          m_lenSet(false)
        { }

    ~wxStringTypeBufferLengthBase()
    {
        wxASSERT_MSG( this->m_lenSet, "forgot to call SetLength()" );
    }

    void SetLength(size_t length) { m_len = length; m_lenSet = true; }

protected:
    size_t m_len;
    bool m_lenSet;
};

template<typename T>
class wxStringTypeBuffer : public wxStringTypeBufferBase<T>
{
public:
    wxStringTypeBuffer(wxString& str, size_t lenWanted = 1024)
        : wxStringTypeBufferBase<T>(str, lenWanted)
        { }

    ~wxStringTypeBuffer()
    {
        this->m_str.assign(this->m_buf.data());
    }

    wxDECLARE_NO_COPY_CLASS(wxStringTypeBuffer);
};

template<typename T>
class wxStringTypeBufferLength : public wxStringTypeBufferLengthBase<T>
{
public:
    wxStringTypeBufferLength(wxString& str, size_t lenWanted = 1024)
        : wxStringTypeBufferLengthBase<T>(str, lenWanted)
        { }

    ~wxStringTypeBufferLength()
    {
        this->m_str.assign(this->m_buf.data(), this->m_len);
    }

    wxDECLARE_NO_COPY_CLASS(wxStringTypeBufferLength);
};

#if wxUSE_STL_BASED_WXSTRING

WXDLLIMPEXP_TEMPLATE_INSTANCE_BASE( wxStringTypeBufferBase<wxStringCharType> )

class wxStringInternalBuffer : public wxStringTypeBufferBase<wxStringCharType>
{
public:
    wxStringInternalBuffer(wxString& str, size_t lenWanted = 1024)
        : wxStringTypeBufferBase<wxStringCharType>(str, lenWanted) {}
    ~wxStringInternalBuffer()
        { m_str.m_impl.assign(m_buf.data()); }

    wxDECLARE_NO_COPY_CLASS(wxStringInternalBuffer);
};

WXDLLIMPEXP_TEMPLATE_INSTANCE_BASE(
    wxStringTypeBufferLengthBase<wxStringCharType> )

class wxStringInternalBufferLength
    : public wxStringTypeBufferLengthBase<wxStringCharType>
{
public:
    wxStringInternalBufferLength(wxString& str, size_t lenWanted = 1024)
        : wxStringTypeBufferLengthBase<wxStringCharType>(str, lenWanted) {}

    ~wxStringInternalBufferLength()
    {
        m_str.m_impl.assign(m_buf.data(), m_len);
    }

    wxDECLARE_NO_COPY_CLASS(wxStringInternalBufferLength);
};

#endif // wxUSE_STL_BASED_WXSTRING


#if wxUSE_STL_BASED_WXSTRING || wxUSE_UNICODE_UTF8
typedef wxStringTypeBuffer<wxChar>        wxStringBuffer;
typedef wxStringTypeBufferLength<wxChar>  wxStringBufferLength;
#else // if !wxUSE_STL_BASED_WXSTRING && !wxUSE_UNICODE_UTF8
typedef wxStringInternalBuffer                wxStringBuffer;
typedef wxStringInternalBufferLength          wxStringBufferLength;
#endif // !wxUSE_STL_BASED_WXSTRING && !wxUSE_UNICODE_UTF8

#if wxUSE_UNICODE_UTF8
typedef wxStringInternalBuffer                wxUTF8StringBuffer;
typedef wxStringInternalBufferLength          wxUTF8StringBufferLength;
#elif wxUSE_UNICODE_WCHAR

WXDLLIMPEXP_TEMPLATE_INSTANCE_BASE( wxStringTypeBufferBase<char> )

// Note about inlined dtors in the classes below: this is done not for
// performance reasons but just to avoid linking errors in the MSVC DLL build
// under Windows: if a class has non-inline methods it must be declared as
// being DLL-exported but, due to an extremely interesting feature of MSVC 7
// and later, any template class which is used as a base of a DLL-exported
// class is implicitly made DLL-exported too, as explained at the bottom of
// http://msdn.microsoft.com/en-us/library/twa2aw10.aspx (just to confirm: yes,
// _inheriting_ from a class can change whether it is being exported from DLL)
//
// But this results in link errors because the base template class is not DLL-
// exported, whether it is declared with WXDLLIMPEXP_BASE or not, because it
// does have only inline functions. So the simplest fix is to just make all the
// functions of these classes inline too.

class wxUTF8StringBuffer : public wxStringTypeBufferBase<char>
{
public:
    wxUTF8StringBuffer(wxString& str, size_t lenWanted = 1024)
        : wxStringTypeBufferBase<char>(str, lenWanted) {}
    ~wxUTF8StringBuffer()
    {
        wxMBConvStrictUTF8 conv;
        size_t wlen = conv.ToWChar(NULL, 0, m_buf);
        wxCHECK_RET( wlen != wxCONV_FAILED, "invalid UTF-8 data in string buffer?" );

        wxStringInternalBuffer wbuf(m_str, wlen);
        conv.ToWChar(wbuf, wlen, m_buf);
    }

    wxDECLARE_NO_COPY_CLASS(wxUTF8StringBuffer);
};

WXDLLIMPEXP_TEMPLATE_INSTANCE_BASE( wxStringTypeBufferLengthBase<char> )

class wxUTF8StringBufferLength : public wxStringTypeBufferLengthBase<char>
{
public:
    wxUTF8StringBufferLength(wxString& str, size_t lenWanted = 1024)
        : wxStringTypeBufferLengthBase<char>(str, lenWanted) {}
    ~wxUTF8StringBufferLength()
    {
        wxCHECK_RET(m_lenSet, "length not set");

        wxMBConvStrictUTF8 conv;
        size_t wlen = conv.ToWChar(NULL, 0, m_buf, m_len);
        wxCHECK_RET( wlen != wxCONV_FAILED, "invalid UTF-8 data in string buffer?" );

        wxStringInternalBufferLength wbuf(m_str, wlen);
        conv.ToWChar(wbuf, wlen, m_buf, m_len);
        wbuf.SetLength(wlen);
    }

    wxDECLARE_NO_COPY_CLASS(wxUTF8StringBufferLength);
};
#endif // wxUSE_UNICODE_UTF8/wxUSE_UNICODE_WCHAR


// ---------------------------------------------------------------------------
// wxString comparison functions: operator versions are always case sensitive
// ---------------------------------------------------------------------------

#define wxCMP_WXCHAR_STRING(p, s, op) 0 op s.Cmp(p)

wxDEFINE_ALL_COMPARISONS(const wxChar *, const wxString&, wxCMP_WXCHAR_STRING)

#undef wxCMP_WXCHAR_STRING

inline bool operator==(const wxString& s1, const wxString& s2)
    { return s1.IsSameAs(s2); }
inline bool operator!=(const wxString& s1, const wxString& s2)
    { return !s1.IsSameAs(s2); }
inline bool operator< (const wxString& s1, const wxString& s2)
    { return s1.Cmp(s2) < 0; }
inline bool operator> (const wxString& s1, const wxString& s2)
    { return s1.Cmp(s2) >  0; }
inline bool operator<=(const wxString& s1, const wxString& s2)
    { return s1.Cmp(s2) <= 0; }
inline bool operator>=(const wxString& s1, const wxString& s2)
    { return s1.Cmp(s2) >= 0; }

inline bool operator==(const wxString& s1, const wxCStrData& s2)
    { return s1 == s2.AsString(); }
inline bool operator==(const wxCStrData& s1, const wxString& s2)
    { return s1.AsString() == s2; }
inline bool operator!=(const wxString& s1, const wxCStrData& s2)
    { return s1 != s2.AsString(); }
inline bool operator!=(const wxCStrData& s1, const wxString& s2)
    { return s1.AsString() != s2; }

inline bool operator==(const wxString& s1, const wxScopedWCharBuffer& s2)
    { return (s1.Cmp((const wchar_t *)s2) == 0); }
inline bool operator==(const wxScopedWCharBuffer& s1, const wxString& s2)
    { return (s2.Cmp((const wchar_t *)s1) == 0); }
inline bool operator!=(const wxString& s1, const wxScopedWCharBuffer& s2)
    { return (s1.Cmp((const wchar_t *)s2) != 0); }
inline bool operator!=(const wxScopedWCharBuffer& s1, const wxString& s2)
    { return (s2.Cmp((const wchar_t *)s1) != 0); }

inline bool operator==(const wxString& s1, const wxScopedCharBuffer& s2)
    { return (s1.Cmp((const char *)s2) == 0); }
inline bool operator==(const wxScopedCharBuffer& s1, const wxString& s2)
    { return (s2.Cmp((const char *)s1) == 0); }
inline bool operator!=(const wxString& s1, const wxScopedCharBuffer& s2)
    { return (s1.Cmp((const char *)s2) != 0); }
inline bool operator!=(const wxScopedCharBuffer& s1, const wxString& s2)
    { return (s2.Cmp((const char *)s1) != 0); }

inline wxString operator+(const wxString& string, const wxScopedWCharBuffer& buf)
    { return string + (const wchar_t *)buf; }
inline wxString operator+(const wxScopedWCharBuffer& buf, const wxString& string)
    { return (const wchar_t *)buf + string; }

inline wxString operator+(const wxString& string, const wxScopedCharBuffer& buf)
    { return string + (const char *)buf; }
inline wxString operator+(const wxScopedCharBuffer& buf, const wxString& string)
    { return (const char *)buf + string; }

// comparison with char
inline bool operator==(const wxUniChar& c, const wxString& s) { return s.IsSameAs(c); }
inline bool operator==(const wxUniCharRef& c, const wxString& s) { return s.IsSameAs(c); }
inline bool operator==(char c, const wxString& s) { return s.IsSameAs(c); }
inline bool operator==(wchar_t c, const wxString& s) { return s.IsSameAs(c); }
inline bool operator==(int c, const wxString& s) { return s.IsSameAs(c); }
inline bool operator==(const wxString& s, const wxUniChar& c) { return s.IsSameAs(c); }
inline bool operator==(const wxString& s, const wxUniCharRef& c) { return s.IsSameAs(c); }
inline bool operator==(const wxString& s, char c) { return s.IsSameAs(c); }
inline bool operator==(const wxString& s, wchar_t c) { return s.IsSameAs(c); }
inline bool operator!=(const wxUniChar& c, const wxString& s) { return !s.IsSameAs(c); }
inline bool operator!=(const wxUniCharRef& c, const wxString& s) { return !s.IsSameAs(c); }
inline bool operator!=(char c, const wxString& s) { return !s.IsSameAs(c); }
inline bool operator!=(wchar_t c, const wxString& s) { return !s.IsSameAs(c); }
inline bool operator!=(int c, const wxString& s) { return !s.IsSameAs(c); }
inline bool operator!=(const wxString& s, const wxUniChar& c) { return !s.IsSameAs(c); }
inline bool operator!=(const wxString& s, const wxUniCharRef& c) { return !s.IsSameAs(c); }
inline bool operator!=(const wxString& s, char c) { return !s.IsSameAs(c); }
inline bool operator!=(const wxString& s, wchar_t c) { return !s.IsSameAs(c); }


// wxString iterators comparisons
inline bool wxString::iterator::operator==(const const_iterator& i) const
    { return i == *this; }
inline bool wxString::iterator::operator!=(const const_iterator& i) const
    { return i != *this; }
inline bool wxString::iterator::operator<(const const_iterator& i) const
    { return i > *this; }
inline bool wxString::iterator::operator>(const const_iterator& i) const
    { return i < *this; }
inline bool wxString::iterator::operator<=(const const_iterator& i) const
    { return i >= *this; }
inline bool wxString::iterator::operator>=(const const_iterator& i) const
    { return i <= *this; }

// comparison with C string in Unicode build
#if wxUSE_UNICODE

#define wxCMP_CHAR_STRING(p, s, op) wxString(p) op s

wxDEFINE_ALL_COMPARISONS(const char *, const wxString&, wxCMP_CHAR_STRING)

#undef wxCMP_CHAR_STRING

#endif // wxUSE_UNICODE

// we also need to provide the operators for comparison with wxCStrData to
// resolve ambiguity between operator(const wxChar *,const wxString &) and
// operator(const wxChar *, const wxChar *) for "p == s.c_str()"
//
// notice that these are (shallow) pointer comparisons, not (deep) string ones
#define wxCMP_CHAR_CSTRDATA(p, s, op) p op s.AsChar()
#define wxCMP_WCHAR_CSTRDATA(p, s, op) p op s.AsWChar()

wxDEFINE_ALL_COMPARISONS(const wchar_t *, const wxCStrData&, wxCMP_WCHAR_CSTRDATA)
wxDEFINE_ALL_COMPARISONS(const char *, const wxCStrData&, wxCMP_CHAR_CSTRDATA)

#undef wxCMP_CHAR_CSTRDATA
#undef wxCMP_WCHAR_CSTRDATA

// ---------------------------------------------------------------------------
// Implementation only from here until the end of file
// ---------------------------------------------------------------------------

#if wxUSE_STD_IOSTREAM

#include "wx/iosfwrap.h"

WXDLLIMPEXP_BASE wxSTD ostream& operator<<(wxSTD ostream&, const wxString&);
WXDLLIMPEXP_BASE wxSTD ostream& operator<<(wxSTD ostream&, const wxCStrData&);
WXDLLIMPEXP_BASE wxSTD ostream& operator<<(wxSTD ostream&, const wxScopedCharBuffer&);
#ifndef __BORLANDC__
WXDLLIMPEXP_BASE wxSTD ostream& operator<<(wxSTD ostream&, const wxScopedWCharBuffer&);
#endif

#if wxUSE_UNICODE && defined(HAVE_WOSTREAM)

WXDLLIMPEXP_BASE wxSTD wostream& operator<<(wxSTD wostream&, const wxString&);
WXDLLIMPEXP_BASE wxSTD wostream& operator<<(wxSTD wostream&, const wxCStrData&);
WXDLLIMPEXP_BASE wxSTD wostream& operator<<(wxSTD wostream&, const wxScopedWCharBuffer&);

#endif  // wxUSE_UNICODE && defined(HAVE_WOSTREAM)

#endif  // wxUSE_STD_IOSTREAM

// ---------------------------------------------------------------------------
// wxCStrData implementation
// ---------------------------------------------------------------------------

inline wxCStrData::wxCStrData(char *buf)
    : m_str(new wxString(buf)), m_offset(0), m_owned(true) {}
inline wxCStrData::wxCStrData(wchar_t *buf)
    : m_str(new wxString(buf)), m_offset(0), m_owned(true) {}

inline wxCStrData::wxCStrData(const wxCStrData& data)
    : m_str(data.m_owned ? new wxString(*data.m_str) : data.m_str),
      m_offset(data.m_offset),
      m_owned(data.m_owned)
{
}

inline wxCStrData::~wxCStrData()
{
    if ( m_owned )
        delete const_cast<wxString*>(m_str); // cast to silence warnings
}

// AsChar() and AsWChar() implementations simply forward to wxString methods

inline const wchar_t* wxCStrData::AsWChar() const
{
    const wchar_t * const p =
#if wxUSE_UNICODE_WCHAR
        m_str->wc_str();
#elif wxUSE_UNICODE_UTF8
        m_str->AsWChar(wxMBConvStrictUTF8());
#else
        m_str->AsWChar(wxConvLibc);
#endif

    // in Unicode build the string always has a valid Unicode representation
    // and even if a conversion is needed (as in UTF8 case) it can't fail
    //
    // but in ANSI build the string contents might be not convertible to
    // Unicode using the current locale encoding so we do need to check for
    // errors
#if !wxUSE_UNICODE
    if ( !p )
    {
        // if conversion fails, return empty string and not NULL to avoid
        // crashes in code written with either wxWidgets 2 wxString or
        // std::string behaviour in mind: neither of them ever returns NULL
        // from its c_str() and so we shouldn't neither
        //
        // notice that the same is done in AsChar() below and
        // wxString::wc_str() and mb_str() for the same reasons
        return L"";
    }
#endif // !wxUSE_UNICODE

    return p + m_offset;
}

inline const char* wxCStrData::AsChar() const
{
#if wxUSE_UNICODE && !wxUSE_UTF8_LOCALE_ONLY
    const char * const p = m_str->AsChar(wxConvLibc);
    if ( !p )
        return "";
#else // !wxUSE_UNICODE || wxUSE_UTF8_LOCALE_ONLY
    const char * const p = m_str->mb_str();
#endif // wxUSE_UNICODE && !wxUSE_UTF8_LOCALE_ONLY

    return p + m_offset;
}

inline wxString wxCStrData::AsString() const
{
    if ( m_offset == 0 )
        return *m_str;
    else
        return m_str->Mid(m_offset);
}

inline const wxStringCharType *wxCStrData::AsInternal() const
{
#if wxUSE_UNICODE_UTF8
    return wxStringOperations::AddToIter(m_str->wx_str(), m_offset);
#else
    return m_str->wx_str() + m_offset;
#endif
}

inline wxUniChar wxCStrData::operator*() const
{
    if ( m_str->empty() )
        return wxUniChar(wxT('\0'));
    else
        return (*m_str)[m_offset];
}

inline wxUniChar wxCStrData::operator[](size_t n) const
{
    // NB: we intentionally use operator[] and not at() here because the former
    //     works for the terminating NUL while the latter does not
    return (*m_str)[m_offset + n];
}

// ----------------------------------------------------------------------------
// more wxCStrData operators
// ----------------------------------------------------------------------------

// we need to define those to allow "size_t pos = p - s.c_str()" where p is
// some pointer into the string
inline size_t operator-(const char *p, const wxCStrData& cs)
{
    return p - cs.AsChar();
}

inline size_t operator-(const wchar_t *p, const wxCStrData& cs)
{
    return p - cs.AsWChar();
}

// ----------------------------------------------------------------------------
// implementation of wx[W]CharBuffer inline methods using wxCStrData
// ----------------------------------------------------------------------------

// FIXME-UTF8: move this to buffer.h
inline wxCharBuffer::wxCharBuffer(const wxCStrData& cstr)
                    : wxCharTypeBufferBase(cstr.AsCharBuf())
{
}

inline wxWCharBuffer::wxWCharBuffer(const wxCStrData& cstr)
                    : wxCharTypeBufferBase(cstr.AsWCharBuf())
{
}

#if wxUSE_UNICODE_UTF8
// ----------------------------------------------------------------------------
// implementation of wxStringIteratorNode inline methods
// ----------------------------------------------------------------------------

void wxStringIteratorNode::DoSet(const wxString *str,
                                 wxStringImpl::const_iterator *citer,
                                 wxStringImpl::iterator *iter)
{
    m_prev = NULL;
    m_iter = iter;
    m_citer = citer;
    m_str = str;
    if ( str )
    {
        m_next = str->m_iterators.ptr;
        const_cast<wxString*>(m_str)->m_iterators.ptr = this;
        if ( m_next )
            m_next->m_prev = this;
    }
    else
    {
        m_next = NULL;
    }
}

void wxStringIteratorNode::clear()
{
    if ( m_next )
        m_next->m_prev = m_prev;
    if ( m_prev )
        m_prev->m_next = m_next;
    else if ( m_str ) // first in the list
        const_cast<wxString*>(m_str)->m_iterators.ptr = m_next;

    m_next = m_prev = NULL;
    m_citer = NULL;
    m_iter = NULL;
    m_str = NULL;
}
#endif // wxUSE_UNICODE_UTF8

#if WXWIN_COMPATIBILITY_2_8
    // lot of code out there doesn't explicitly include wx/crt.h, but uses
    // CRT wrappers that are now declared in wx/wxcrt.h and wx/wxcrtvararg.h,
    // so let's include this header now that wxString is defined and it's safe
    // to do it:
    #include "wx/crt.h"
#endif

// ----------------------------------------------------------------------------
// Checks on wxString characters
// ----------------------------------------------------------------------------

template<bool (T)(const wxUniChar& c)>
    inline bool wxStringCheck(const wxString& val)
    {
        for ( wxString::const_iterator i = val.begin();
              i != val.end();
              ++i )
            if (T(*i) == 0)
                return false;
        return true;
    }

#endif  // _WX_WXSTRING_H_
