///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/ole/oleutils.cpp
// Purpose:     implementation of OLE helper functions
// Author:      Vadim Zeitlin
// Modified by:
// Created:     19.02.98
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// Declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if defined(__BORLANDC__)
    #pragma hdrstop
#endif

#if wxUSE_OLE

#ifndef WX_PRECOMP
    #include "wx/log.h"
#endif

#ifndef __CYGWIN10__

#include "wx/msw/private.h"

#ifdef __WXWINCE__
    #include <winreg.h>
    #include <ole2.h>

    #define GUID_DEFINED
    #define UUID_DEFINED
#endif

// OLE
#ifndef __WXWINCE__
#include  "wx/msw/ole/uuid.h"
#endif

#include  "wx/msw/ole/oleutils.h"
#include "wx/msw/ole/safearray.h"

#if defined(__VISUALC__) && (__VISUALC__ > 1000)
    #include  <docobj.h>
#endif

// ============================================================================
// Implementation
// ============================================================================

// return true if the iid is in the array
WXDLLEXPORT bool IsIidFromList(REFIID riid, const IID *aIids[], size_t nCount)
{
  for ( size_t i = 0; i < nCount; i++ ) {
    if ( riid == *aIids[i] )
      return true;
  }

  return false;
}

WXDLLEXPORT BSTR wxConvertStringToOle(const wxString& str)
{
    return wxBasicString(str).Get();
}

WXDLLEXPORT wxString wxConvertStringFromOle(BSTR bStr)
{
    // NULL BSTR is equivalent to an empty string (this is the convention used
    // by VB and hence we must follow it)
    if ( !bStr )
        return wxString();

    const int len = SysStringLen(bStr);

#if wxUSE_UNICODE
    wxString str(bStr, len);
#else
    wxString str;
    if (len)
    {
        wxStringBufferLength buf(str, len); // asserts if len == 0
        buf.SetLength(WideCharToMultiByte(CP_ACP, 0 /* no flags */,
                                  bStr, len /* not necessarily NUL-terminated */,
                                  buf, len,
                                  NULL, NULL /* no default char */));
    }
#endif

    return str;
}

// ----------------------------------------------------------------------------
// wxBasicString
// ----------------------------------------------------------------------------

wxBasicString::wxBasicString(const wxString& str)
{
    m_bstrBuf = SysAllocString(str.wc_str(*wxConvCurrent));
}

wxBasicString::wxBasicString(const wxBasicString& src)
{
    m_bstrBuf = src.Get();
}

wxBasicString& wxBasicString::operator=(const wxBasicString& src)
{
    SysReAllocString(&m_bstrBuf, src);
    return *this;
}

wxBasicString::~wxBasicString()
{
    SysFreeString(m_bstrBuf);
}


// ----------------------------------------------------------------------------
// Convert variants
// ----------------------------------------------------------------------------

#if wxUSE_VARIANT

// ----------------------------------------------------------------------------
// wxVariantDataCurrency
// ----------------------------------------------------------------------------


#if wxUSE_ANY

bool wxVariantDataCurrency::GetAsAny(wxAny* any) const
{
    *any = m_value;
    return true;
}

wxVariantData* wxVariantDataCurrency::VariantDataFactory(const wxAny& any)
{
    return new wxVariantDataCurrency(wxANY_AS(any, CURRENCY));
}

REGISTER_WXANY_CONVERSION(CURRENCY, wxVariantDataCurrency)

#endif // wxUSE_ANY

bool wxVariantDataCurrency::Eq(wxVariantData& data) const
{
    wxASSERT_MSG( (data.GetType() == wxS("currency")),
                  "wxVariantDataCurrency::Eq: argument mismatch" );

    wxVariantDataCurrency& otherData = (wxVariantDataCurrency&) data;

    return otherData.m_value.int64 == m_value.int64;
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataCurrency::Write(wxSTD ostream& str) const
{
    wxString s;
    Write(s);
    str << s;
    return true;
}
#endif

bool wxVariantDataCurrency::Write(wxString& str) const
{
    BSTR bStr = NULL;
    if ( SUCCEEDED(VarBstrFromCy(m_value, LOCALE_USER_DEFAULT, 0, &bStr)) )
    {
        str = wxConvertStringFromOle(bStr);
        SysFreeString(bStr);
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
// wxVariantDataErrorCode
// ----------------------------------------------------------------------------

#if wxUSE_ANY

bool wxVariantDataErrorCode::GetAsAny(wxAny* any) const
{
    *any = m_value;
    return true;
}

wxVariantData* wxVariantDataErrorCode::VariantDataFactory(const wxAny& any)
{
    return new wxVariantDataErrorCode(wxANY_AS(any, SCODE));
}

REGISTER_WXANY_CONVERSION(SCODE, wxVariantDataErrorCode)

#endif // wxUSE_ANY

bool wxVariantDataErrorCode::Eq(wxVariantData& data) const
{
    wxASSERT_MSG( (data.GetType() == wxS("errorcode")),
                  "wxVariantDataErrorCode::Eq: argument mismatch" );

    wxVariantDataErrorCode& otherData = (wxVariantDataErrorCode&) data;

    return otherData.m_value == m_value;
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataErrorCode::Write(wxSTD ostream& str) const
{
    wxString s;
    Write(s);
    str << s;
    return true;
}
#endif

bool wxVariantDataErrorCode::Write(wxString& str) const
{
    str << m_value;
    return true;
}


// ----------------------------------------------------------------------------
// wxVariantDataSafeArray
// ----------------------------------------------------------------------------

#if wxUSE_ANY

bool wxVariantDataSafeArray::GetAsAny(wxAny* any) const
{
    *any = m_value;
    return true;
}

wxVariantData* wxVariantDataSafeArray::VariantDataFactory(const wxAny& any)
{
    return new wxVariantDataSafeArray(wxANY_AS(any, SAFEARRAY*));
}

REGISTER_WXANY_CONVERSION(SAFEARRAY*, wxVariantDataSafeArray)

#endif // wxUSE_ANY

bool wxVariantDataSafeArray::Eq(wxVariantData& data) const
{
    wxASSERT_MSG( (data.GetType() == wxS("safearray")),
                  "wxVariantDataSafeArray::Eq: argument mismatch" );

    wxVariantDataSafeArray& otherData = (wxVariantDataSafeArray&) data;

    return otherData.m_value == m_value;
}

#if wxUSE_STD_IOSTREAM
bool wxVariantDataSafeArray::Write(wxSTD ostream& str) const
{
    wxString s;
    Write(s);
    str << s;
    return true;
}
#endif

bool wxVariantDataSafeArray::Write(wxString& str) const
{
    str.Printf(wxS("SAFEARRAY: %p"), (void*)m_value);
    return true;
}

WXDLLEXPORT bool wxConvertVariantToOle(const wxVariant& variant, VARIANTARG& oleVariant)
{
    VariantInit(&oleVariant);
    if (variant.IsNull())
    {
        oleVariant.vt = VT_NULL;
        return true;
    }

    wxString type(variant.GetType());

    if (type == wxT("errorcode"))
    {
        wxVariantDataErrorCode* const
            ec = wxStaticCastVariantData(variant.GetData(),
                                         wxVariantDataErrorCode);
        oleVariant.vt = VT_ERROR;
        oleVariant.scode = ec->GetValue();
    }
    else if (type == wxT("currency"))
    {
        wxVariantDataCurrency* const
            c = wxStaticCastVariantData(variant.GetData(),
                                        wxVariantDataCurrency);
        oleVariant.vt = VT_CY;
        oleVariant.cyVal = c->GetValue();
    }
    else if (type == wxT("safearray"))
    {
        wxVariantDataSafeArray* const
            vsa = wxStaticCastVariantData(variant.GetData(),
                                          wxVariantDataSafeArray);
        SAFEARRAY* psa = vsa->GetValue();
        VARTYPE vt;

        wxCHECK(psa, false);
        HRESULT hr = SafeArrayGetVartype(psa, &vt);
        if ( FAILED(hr) )
        {
            wxLogApiError(wxS("SafeArrayGetVartype()"), hr);
            SafeArrayDestroy(psa);
            return false;
        }
        oleVariant.vt = vt | VT_ARRAY;
        oleVariant.parray = psa;
    }
    else if (type == wxT("long"))
    {
        oleVariant.vt = VT_I4;
        oleVariant.lVal = variant.GetLong() ;
    }
    // Original VC6 came with SDK too old to contain VARIANT::llVal declaration
    // and there doesn't seem to be any way to test for it as Microsoft simply
    // added it to the later version of oaidl.h without changing anything else.
    // So assume it's not present for VC6, even though it might be if an
    // updated SDK is used. In this case the user would need to disable this
    // check himself.
#if wxUSE_LONGLONG && !defined(__VISUALC6__)
    else if (type == wxT("longlong"))
    {
        oleVariant.vt = VT_I8;
        oleVariant.llVal = variant.GetLongLong().GetValue();
    }
#endif
    else if (type == wxT("char"))
    {
        oleVariant.vt=VT_I1;            // Signed Char
        oleVariant.cVal=variant.GetChar();
    }
    else if (type == wxT("double"))
    {
        oleVariant.vt = VT_R8;
        oleVariant.dblVal = variant.GetDouble();
    }
    else if (type == wxT("bool"))
    {
        oleVariant.vt = VT_BOOL;
        oleVariant.boolVal = variant.GetBool() ? VARIANT_TRUE : VARIANT_FALSE;
    }
    else if (type == wxT("string"))
    {
        wxString str( variant.GetString() );
        oleVariant.vt = VT_BSTR;
        oleVariant.bstrVal = wxConvertStringToOle(str);
    }
#if wxUSE_DATETIME
    else if (type == wxT("datetime"))
    {
        wxDateTime date( variant.GetDateTime() );
        oleVariant.vt = VT_DATE;

        SYSTEMTIME st;
        date.GetAsMSWSysTime(&st);

        SystemTimeToVariantTime(&st, &oleVariant.date);
    }
#endif
    else if (type == wxT("void*"))
    {
        oleVariant.vt = VT_DISPATCH;
        oleVariant.pdispVal = (IDispatch*) variant.GetVoidPtr();
    }
    else if (type == wxT("list"))
    {
        wxSafeArray<VT_VARIANT> safeArray;
        if (!safeArray.CreateFromListVariant(variant))
            return false;

        oleVariant.vt = VT_VARIANT | VT_ARRAY;
        oleVariant.parray = safeArray.Detach();
    }
    else if (type == wxT("arrstring"))
    {
        wxSafeArray<VT_BSTR> safeArray;

        if (!safeArray.CreateFromArrayString(variant.GetArrayString()))
            return false;

        oleVariant.vt = VT_BSTR | VT_ARRAY;
        oleVariant.parray = safeArray.Detach();
    }
    else
    {
        oleVariant.vt = VT_NULL;
        return false;
    }
    return true;
}

#ifndef VT_TYPEMASK
#define VT_TYPEMASK 0xfff
#endif

WXDLLEXPORT bool
wxConvertOleToVariant(const VARIANTARG& oleVariant, wxVariant& variant, long flags)
{
    bool ok = true;
    if ( oleVariant.vt & VT_ARRAY )
    {
        if ( flags & wxOleConvertVariant_ReturnSafeArrays  )
        {
            variant.SetData(new wxVariantDataSafeArray(oleVariant.parray));
        }
        else
        {
            switch (oleVariant.vt & VT_TYPEMASK)
            {
                case VT_I2:
                    ok = wxSafeArray<VT_I2>::ConvertToVariant(oleVariant.parray, variant);
                    break;
                case VT_I4:
                    ok = wxSafeArray<VT_I4>::ConvertToVariant(oleVariant.parray, variant);
                    break;
                case VT_R4:
                    ok = wxSafeArray<VT_R4>::ConvertToVariant(oleVariant.parray, variant);
                    break;
                case VT_R8:
                    ok = wxSafeArray<VT_R8>::ConvertToVariant(oleVariant.parray, variant);
                    break;
                case VT_VARIANT:
                    ok = wxSafeArray<VT_VARIANT>::ConvertToVariant(oleVariant.parray, variant);
                    break;
                case VT_BSTR:
                    {
                        wxArrayString strings;
                        if ( wxSafeArray<VT_BSTR>::ConvertToArrayString(oleVariant.parray, strings) )
                            variant = strings;
                        else
                            ok = false;
                    }
                    break;
                default:
                    ok = false;
                    break;
            }
            if ( !ok )
            {
                wxLogDebug(wxT("unhandled VT_ARRAY type %x in wxConvertOleToVariant"),
                           oleVariant.vt & VT_TYPEMASK);
                variant = wxVariant();
            }
        }
    }
    else if ( oleVariant.vt & VT_BYREF )
    {
        switch ( oleVariant.vt & VT_TYPEMASK )
        {
            case VT_VARIANT:
                {
                    VARIANTARG& oleReference = *((LPVARIANT)oleVariant.byref);
                    if (!wxConvertOleToVariant(oleReference,variant))
                        return false;
                    break;
                }

            default:
                wxLogError(wxT("wxAutomationObject::ConvertOleToVariant: [as yet] unhandled reference %X"),
                            oleVariant.vt);
                return false;
        }
    }
    else // simply type (not array or reference)
    {
        switch ( oleVariant.vt & VT_TYPEMASK )
        {
            case VT_ERROR:
                variant.SetData(new wxVariantDataErrorCode(oleVariant.scode));
                break;

            case VT_CY:
                variant.SetData(new wxVariantDataCurrency(oleVariant.cyVal));
                break;

            case VT_BSTR:
                {
                    wxString str(wxConvertStringFromOle(oleVariant.bstrVal));
                    variant = str;
                }
                break;

            case VT_DATE:
#if wxUSE_DATETIME
                {
                    SYSTEMTIME st;
                    VariantTimeToSystemTime(oleVariant.date, &st);

                    wxDateTime date;
                    date.SetFromMSWSysTime(st);
                    variant = date;
                }
#endif // wxUSE_DATETIME
                break;

                // See the comment before the __VISUALC6__ test above.
#if wxUSE_LONGLONG && !defined(__VISUALC6__)
            case VT_I8:
                variant = wxLongLong(oleVariant.llVal);
                break;
#endif // wxUSE_LONGLONG

            case VT_I4:
                variant = (long) oleVariant.lVal;
                break;

            case VT_I2:
                variant = (long) oleVariant.iVal;
                break;

            case VT_BOOL:
                variant = oleVariant.boolVal != 0;
                break;

            case VT_R4:
                variant = oleVariant.fltVal;
                break;

            case VT_R8:
                variant = oleVariant.dblVal;
                break;

            case VT_DISPATCH:
                variant = (void*) oleVariant.pdispVal;
                break;

            case VT_NULL:
                variant.MakeNull();
                break;

            case VT_EMPTY:
                break;    // Ignore Empty Variant, used only during destruction of objects

            default:
                wxLogError(wxT("wxAutomationObject::ConvertOleToVariant: Unknown variant value type %X -> %X"),
                           oleVariant.vt,oleVariant.vt&VT_TYPEMASK);
                return false;
        }
    }

    return ok;
}

#endif // wxUSE_VARIANT


// ----------------------------------------------------------------------------
// Debug support
// ----------------------------------------------------------------------------

#if wxUSE_DATAOBJ

#if wxDEBUG_LEVEL && (( defined(__VISUALC__) && (__VISUALC__ > 1000) ))
static wxString GetIidName(REFIID riid)
{
  // an association between symbolic name and numeric value of an IID
  struct KNOWN_IID {
    const IID  *pIid;
    const wxChar *szName;
  };

  // construct the table containing all known interfaces
  #define ADD_KNOWN_IID(name) { &IID_I##name, wxT(#name) }

  static const KNOWN_IID aKnownIids[] = {
    ADD_KNOWN_IID(AdviseSink),
    ADD_KNOWN_IID(AdviseSink2),
    ADD_KNOWN_IID(BindCtx),
    ADD_KNOWN_IID(ClassFactory),
#if ( !defined( __VISUALC__) || (__VISUALC__!=1010) )
    ADD_KNOWN_IID(ContinueCallback),
    ADD_KNOWN_IID(EnumOleDocumentViews),
    ADD_KNOWN_IID(OleCommandTarget),
    ADD_KNOWN_IID(OleDocument),
    ADD_KNOWN_IID(OleDocumentSite),
    ADD_KNOWN_IID(OleDocumentView),
    ADD_KNOWN_IID(Print),
#endif
    ADD_KNOWN_IID(DataAdviseHolder),
    ADD_KNOWN_IID(DataObject),
    ADD_KNOWN_IID(Debug),
    ADD_KNOWN_IID(DebugStream),
    ADD_KNOWN_IID(DfReserved1),
    ADD_KNOWN_IID(DfReserved2),
    ADD_KNOWN_IID(DfReserved3),
    ADD_KNOWN_IID(Dispatch),
    ADD_KNOWN_IID(DropSource),
    ADD_KNOWN_IID(DropTarget),
    ADD_KNOWN_IID(EnumCallback),
    ADD_KNOWN_IID(EnumFORMATETC),
    ADD_KNOWN_IID(EnumGeneric),
    ADD_KNOWN_IID(EnumHolder),
    ADD_KNOWN_IID(EnumMoniker),
    ADD_KNOWN_IID(EnumOLEVERB),
    ADD_KNOWN_IID(EnumSTATDATA),
    ADD_KNOWN_IID(EnumSTATSTG),
    ADD_KNOWN_IID(EnumString),
    ADD_KNOWN_IID(EnumUnknown),
    ADD_KNOWN_IID(EnumVARIANT),
    ADD_KNOWN_IID(ExternalConnection),
    ADD_KNOWN_IID(InternalMoniker),
    ADD_KNOWN_IID(LockBytes),
    ADD_KNOWN_IID(Malloc),
    ADD_KNOWN_IID(Marshal),
    ADD_KNOWN_IID(MessageFilter),
    ADD_KNOWN_IID(Moniker),
    ADD_KNOWN_IID(OleAdviseHolder),
    ADD_KNOWN_IID(OleCache),
    ADD_KNOWN_IID(OleCache2),
    ADD_KNOWN_IID(OleCacheControl),
    ADD_KNOWN_IID(OleClientSite),
    ADD_KNOWN_IID(OleContainer),
    ADD_KNOWN_IID(OleInPlaceActiveObject),
    ADD_KNOWN_IID(OleInPlaceFrame),
    ADD_KNOWN_IID(OleInPlaceObject),
    ADD_KNOWN_IID(OleInPlaceSite),
    ADD_KNOWN_IID(OleInPlaceUIWindow),
    ADD_KNOWN_IID(OleItemContainer),
    ADD_KNOWN_IID(OleLink),
    ADD_KNOWN_IID(OleManager),
    ADD_KNOWN_IID(OleObject),
    ADD_KNOWN_IID(OlePresObj),
    ADD_KNOWN_IID(OleWindow),
    ADD_KNOWN_IID(PSFactory),
    ADD_KNOWN_IID(ParseDisplayName),
    ADD_KNOWN_IID(Persist),
    ADD_KNOWN_IID(PersistFile),
    ADD_KNOWN_IID(PersistStorage),
    ADD_KNOWN_IID(PersistStream),
    ADD_KNOWN_IID(ProxyManager),
    ADD_KNOWN_IID(RootStorage),
    ADD_KNOWN_IID(RpcChannel),
    ADD_KNOWN_IID(RpcProxy),
    ADD_KNOWN_IID(RpcStub),
    ADD_KNOWN_IID(RunnableObject),
    ADD_KNOWN_IID(RunningObjectTable),
    ADD_KNOWN_IID(StdMarshalInfo),
    ADD_KNOWN_IID(Storage),
    ADD_KNOWN_IID(Stream),
    ADD_KNOWN_IID(StubManager),
    ADD_KNOWN_IID(Unknown),
    ADD_KNOWN_IID(ViewObject),
    ADD_KNOWN_IID(ViewObject2),
  };

  // don't clobber preprocessor name space
  #undef ADD_KNOWN_IID

  // try to find the interface in the table
  for ( size_t ui = 0; ui < WXSIZEOF(aKnownIids); ui++ ) {
    if ( riid == *aKnownIids[ui].pIid ) {
      return aKnownIids[ui].szName;
    }
  }

#ifndef __WXWINCE__
  // unknown IID, just transform to string
  Uuid uuid(riid);
  return wxString((const wxChar *)uuid);
#else
  return wxEmptyString;
#endif
}

WXDLLEXPORT void wxLogQueryInterface(const wxChar *szInterface, REFIID riid)
{
  wxLogTrace(wxTRACE_OleCalls, wxT("%s::QueryInterface (iid = %s)"),
             szInterface, GetIidName(riid).c_str());
}

WXDLLEXPORT void wxLogAddRef(const wxChar *szInterface, ULONG cRef)
{
  wxLogTrace(wxTRACE_OleCalls, wxT("After %s::AddRef: m_cRef = %d"), szInterface, cRef + 1);
}

WXDLLEXPORT void wxLogRelease(const wxChar *szInterface, ULONG cRef)
{
  wxLogTrace(wxTRACE_OleCalls, wxT("After %s::Release: m_cRef = %d"), szInterface, cRef - 1);
}

#endif  // wxDEBUG_LEVEL

#endif // wxUSE_DATAOBJ

#endif // __CYGWIN10__

#endif // wxUSE_OLE
