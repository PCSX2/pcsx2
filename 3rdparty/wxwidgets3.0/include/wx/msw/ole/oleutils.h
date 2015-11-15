///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/ole/oleutils.h
// Purpose:     OLE helper routines, OLE debugging support &c
// Author:      Vadim Zeitlin
// Modified by:
// Created:     19.02.1998
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef   _WX_OLEUTILS_H
#define   _WX_OLEUTILS_H

#include "wx/defs.h"

#if wxUSE_OLE

// ole2.h includes windows.h, so include wrapwin.h first
#include "wx/msw/wrapwin.h"
// get IUnknown, REFIID &c
#include <ole2.h>
#include "wx/intl.h"
#include "wx/log.h"
#include "wx/variant.h"

// ============================================================================
// General purpose functions and macros
// ============================================================================

// ----------------------------------------------------------------------------
// initialize/cleanup OLE
// ----------------------------------------------------------------------------

// call OleInitialize() or CoInitialize[Ex]() depending on the platform
//
// return true if ok, false otherwise
inline bool wxOleInitialize()
{
    HRESULT
#ifdef __WXWINCE__
     hr = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
#else
     hr = ::OleInitialize(NULL);
#endif

    // RPC_E_CHANGED_MODE indicates that OLE had been already initialized
    // before, albeit with different mode. Don't consider it to be an error as
    // we don't actually care ourselves about the mode used so this allows the
    // main application to call OleInitialize() on its own before we do if it
    // needs non-default mode.
    if ( hr != RPC_E_CHANGED_MODE && FAILED(hr) )
    {
        wxLogError(wxGetTranslation("Cannot initialize OLE"));

        return false;
    }

    return true;
}

inline void wxOleUninitialize()
{
#ifdef __WXWINCE__
    ::CoUninitialize();
#else
    ::OleUninitialize();
#endif
}

// ----------------------------------------------------------------------------
// misc helper functions/macros
// ----------------------------------------------------------------------------

// release the interface pointer (if !NULL)
inline void ReleaseInterface(IUnknown *pIUnk)
{
  if ( pIUnk != NULL )
    pIUnk->Release();
}

// release the interface pointer (if !NULL) and make it NULL
#define   RELEASE_AND_NULL(p)   if ( (p) != NULL ) { p->Release(); p = NULL; };

// return true if the iid is in the array
extern WXDLLIMPEXP_CORE bool IsIidFromList(REFIID riid, const IID *aIids[], size_t nCount);

// ============================================================================
// IUnknown implementation helpers
// ============================================================================

/*
   The most dumb implementation of IUnknown methods. We don't support
   aggregation nor containment, but for 99% of cases this simple
   implementation is quite enough.

   Usage is trivial: here is all you should have
   1) DECLARE_IUNKNOWN_METHODS in your (IUnknown derived!) class declaration
   2) BEGIN/END_IID_TABLE with ADD_IID in between for all interfaces you
      support (at least all for which you intent to return 'this' from QI,
      i.e. you should derive from IFoo if you have ADD_IID(Foo)) somewhere else
   3) IMPLEMENT_IUNKNOWN_METHODS somewhere also

   These macros are quite simple: AddRef and Release are trivial and QI does
   lookup in a static member array of IIDs and returns 'this' if it founds
   the requested interface in it or E_NOINTERFACE if not.
 */

/*
  wxAutoULong: this class is used for automatically initalising m_cRef to 0
*/
class wxAutoULong
{
public:
    wxAutoULong(ULONG value = 0) : m_Value(value) { }

    operator ULONG&() { return m_Value; }
    ULONG& operator=(ULONG value) { m_Value = value; return m_Value;  }

    wxAutoULong& operator++() { ++m_Value; return *this; }
    const wxAutoULong operator++( int ) { wxAutoULong temp = *this; ++m_Value; return temp; }

    wxAutoULong& operator--() { --m_Value; return *this; }
    const wxAutoULong operator--( int ) { wxAutoULong temp = *this; --m_Value; return temp; }

private:
    ULONG m_Value;
};

// declare the methods and the member variable containing reference count
// you must also define the ms_aIids array somewhere with BEGIN_IID_TABLE
// and friends (see below)

#define   DECLARE_IUNKNOWN_METHODS                                            \
  public:                                                                     \
    STDMETHODIMP          QueryInterface(REFIID, void **);                    \
    STDMETHODIMP_(ULONG)  AddRef();                                           \
    STDMETHODIMP_(ULONG)  Release();                                          \
  private:                                                                    \
    static  const IID    *ms_aIids[];                                         \
    wxAutoULong           m_cRef

// macros for declaring supported interfaces
// NB: ADD_IID prepends IID_I whereas ADD_RAW_IID does not
#define BEGIN_IID_TABLE(cname)  const IID *cname::ms_aIids[] = {
#define ADD_IID(iid)                                             &IID_I##iid,
#define ADD_RAW_IID(iid)                                         &iid,
#define END_IID_TABLE                                          }

// implementation is as straightforward as possible
// Parameter:  classname - the name of the class
#define   IMPLEMENT_IUNKNOWN_METHODS(classname)                               \
  STDMETHODIMP classname::QueryInterface(REFIID riid, void **ppv)             \
  {                                                                           \
    wxLogQueryInterface(wxT(#classname), riid);                               \
                                                                              \
    if ( IsIidFromList(riid, ms_aIids, WXSIZEOF(ms_aIids)) ) {                \
      *ppv = this;                                                            \
      AddRef();                                                               \
                                                                              \
      return S_OK;                                                            \
    }                                                                         \
    else {                                                                    \
      *ppv = NULL;                                                            \
                                                                              \
      return (HRESULT) E_NOINTERFACE;                                         \
    }                                                                         \
  }                                                                           \
                                                                              \
  STDMETHODIMP_(ULONG) classname::AddRef()                                    \
  {                                                                           \
    wxLogAddRef(wxT(#classname), m_cRef);                                     \
                                                                              \
    return ++m_cRef;                                                          \
  }                                                                           \
                                                                              \
  STDMETHODIMP_(ULONG) classname::Release()                                   \
  {                                                                           \
    wxLogRelease(wxT(#classname), m_cRef);                                    \
                                                                              \
    if ( --m_cRef == wxAutoULong(0) ) {                                                    \
      delete this;                                                            \
      return 0;                                                               \
    }                                                                         \
    else                                                                      \
      return m_cRef;                                                          \
  }

// ============================================================================
// Debugging support
// ============================================================================

// VZ: I don't know it's not done for compilers other than VC++ but I leave it
//     as is. Please note, though, that tracing OLE interface calls may be
//     incredibly useful when debugging OLE programs.
#if defined(__WXDEBUG__) && (( defined(__VISUALC__) && (__VISUALC__ >= 1000) ))
// ----------------------------------------------------------------------------
// All OLE specific log functions have DebugTrace level (as LogTrace)
// ----------------------------------------------------------------------------

// tries to translate riid into a symbolic name, if possible
WXDLLIMPEXP_CORE void wxLogQueryInterface(const wxChar *szInterface, REFIID riid);

// these functions print out the new value of reference counter
WXDLLIMPEXP_CORE void wxLogAddRef (const wxChar *szInterface, ULONG cRef);
WXDLLIMPEXP_CORE void wxLogRelease(const wxChar *szInterface, ULONG cRef);

#else   //!__WXDEBUG__
  #define   wxLogQueryInterface(szInterface, riid)
  #define   wxLogAddRef(szInterface, cRef)
  #define   wxLogRelease(szInterface, cRef)
#endif  //__WXDEBUG__

// wrapper around BSTR type (by Vadim Zeitlin)

class WXDLLIMPEXP_CORE wxBasicString
{
public:
    // ctors & dtor
    wxBasicString(const wxString& str);
    wxBasicString(const wxBasicString& bstr);
    ~wxBasicString();

    wxBasicString& operator=(const wxBasicString& bstr);

    // accessors
        // just get the string
    operator BSTR() const { return m_bstrBuf; }
        // retrieve a copy of our string - caller must SysFreeString() it later!
    BSTR Get() const { return SysAllocString(m_bstrBuf); }

private:
    // actual string
    BSTR m_bstrBuf;
};

#if wxUSE_VARIANT
// Convert variants
class WXDLLIMPEXP_FWD_BASE wxVariant;

// wrapper for CURRENCY type used in VARIANT (VARIANT.vt == VT_CY)
class WXDLLIMPEXP_CORE wxVariantDataCurrency : public wxVariantData
{
public:
    wxVariantDataCurrency() { VarCyFromR8(0.0, &m_value); }
    wxVariantDataCurrency(CURRENCY value) { m_value = value; }

    CURRENCY GetValue() const { return m_value; }
    void SetValue(CURRENCY value) { m_value = value; }

    virtual bool Eq(wxVariantData& data) const;

#if wxUSE_STD_IOSTREAM
    virtual bool Write(wxSTD ostream& str) const;
#endif
    virtual bool Write(wxString& str) const;

    wxVariantData* Clone() const { return new wxVariantDataCurrency(m_value); }
    virtual wxString GetType() const { return wxS("currency"); }

    DECLARE_WXANY_CONVERSION()

private:
    CURRENCY m_value;
};


// wrapper for SCODE type used in VARIANT (VARIANT.vt == VT_ERROR)
class WXDLLIMPEXP_CORE wxVariantDataErrorCode : public wxVariantData
{
public:
    wxVariantDataErrorCode(SCODE value = S_OK) { m_value = value; }

    SCODE GetValue() const { return m_value; }
    void SetValue(SCODE value) { m_value = value; }

    virtual bool Eq(wxVariantData& data) const;

#if wxUSE_STD_IOSTREAM
    virtual bool Write(wxSTD ostream& str) const;
#endif
    virtual bool Write(wxString& str) const;

    wxVariantData* Clone() const { return new wxVariantDataErrorCode(m_value); }
    virtual wxString GetType() const { return wxS("errorcode"); }

    DECLARE_WXANY_CONVERSION()

private:
    SCODE m_value;
};

// wrapper for SAFEARRAY, used for passing multidimensional arrays in wxVariant
class WXDLLIMPEXP_CORE wxVariantDataSafeArray : public wxVariantData
{
public:
    wxEXPLICIT wxVariantDataSafeArray(SAFEARRAY* value = NULL)
    {
        m_value = value;
    }

    SAFEARRAY* GetValue() const { return m_value; }
    void SetValue(SAFEARRAY* value) { m_value = value; }

    virtual bool Eq(wxVariantData& data) const;

#if wxUSE_STD_IOSTREAM
    virtual bool Write(wxSTD ostream& str) const;
#endif
    virtual bool Write(wxString& str) const;

    wxVariantData* Clone() const { return new wxVariantDataSafeArray(m_value); }
    virtual wxString GetType() const { return wxS("safearray"); }

    DECLARE_WXANY_CONVERSION()

private:
    SAFEARRAY* m_value;
};

// Used by wxAutomationObject for its wxConvertOleToVariant() calls.
enum wxOleConvertVariantFlags
{
    wxOleConvertVariant_Default = 0,

    // If wxOleConvertVariant_ReturnSafeArrays  flag is set, SAFEARRAYs
    // contained in OLE VARIANTs will be returned as wxVariants
    // with wxVariantDataSafeArray type instead of wxVariants
    // with the list type containing the (flattened) SAFEARRAY's elements.
    wxOleConvertVariant_ReturnSafeArrays = 1
};

WXDLLIMPEXP_CORE
bool wxConvertVariantToOle(const wxVariant& variant, VARIANTARG& oleVariant);

WXDLLIMPEXP_CORE
bool wxConvertOleToVariant(const VARIANTARG& oleVariant, wxVariant& variant,
                           long flags = wxOleConvertVariant_Default);

#endif // wxUSE_VARIANT

// Convert string to Unicode
WXDLLIMPEXP_CORE BSTR wxConvertStringToOle(const wxString& str);

// Convert string from BSTR to wxString
WXDLLIMPEXP_CORE wxString wxConvertStringFromOle(BSTR bStr);

#else // !wxUSE_OLE

// ----------------------------------------------------------------------------
// stub functions to avoid #if wxUSE_OLE in the main code
// ----------------------------------------------------------------------------

inline bool wxOleInitialize() { return false; }
inline void wxOleUninitialize() { }

#endif // wxUSE_OLE/!wxUSE_OLE

// RAII class initializing OLE in its ctor and undoing it in its dtor.
class wxOleInitializer
{
public:
    wxOleInitializer()
        : m_ok(wxOleInitialize())
    {
    }

    bool IsOk() const
    {
        return m_ok;
    }

    ~wxOleInitializer()
    {
        if ( m_ok )
            wxOleUninitialize();
    }

private:
    const bool m_ok;

    wxDECLARE_NO_COPY_CLASS(wxOleInitializer);
};

#endif  //_WX_OLEUTILS_H
