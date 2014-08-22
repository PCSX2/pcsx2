/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/ole/automtn.h
// Purpose:     OLE automation utilities
// Author:      Julian Smart
// Modified by:
// Created:     11/6/98
// Copyright:   (c) 1998, Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_AUTOMTN_H_
#define _WX_AUTOMTN_H_

#include "wx/defs.h"

#if wxUSE_OLE_AUTOMATION

#include "wx/object.h"
#include "wx/variant.h"

typedef void            WXIDISPATCH;
typedef unsigned short* WXBSTR;
typedef unsigned long   WXLCID;

#ifdef GetObject
#undef GetObject
#endif

// Flags used with wxAutomationObject::GetInstance()
enum wxAutomationInstanceFlags
{
    // Only use the existing instance, never create a new one.
    wxAutomationInstance_UseExistingOnly = 0,

    // Create a new instance if there are no existing ones.
    wxAutomationInstance_CreateIfNeeded = 1,

    // Do not log errors if we failed to get the existing instance because none
    // is available.
    wxAutomationInstance_SilentIfNone = 2
};

/*
 * wxAutomationObject
 * Wraps up an IDispatch pointer and invocation; does variant conversion.
 */

class WXDLLIMPEXP_CORE wxAutomationObject: public wxObject
{
public:
    wxAutomationObject(WXIDISPATCH* dispatchPtr = NULL);
    virtual ~wxAutomationObject();

    // Set/get dispatch pointer
    void SetDispatchPtr(WXIDISPATCH* dispatchPtr) { m_dispatchPtr = dispatchPtr; }
    WXIDISPATCH* GetDispatchPtr() const { return m_dispatchPtr; }
    bool IsOk() const { return m_dispatchPtr != NULL; }

    // Get a dispatch pointer from the current object associated
    // with a ProgID, such as "Excel.Application"
    bool GetInstance(const wxString& progId,
                     int flags = wxAutomationInstance_CreateIfNeeded) const;

    // Get a dispatch pointer from a new instance of the class
    bool CreateInstance(const wxString& progId) const;

    // Low-level invocation function. Pass either an array of variants,
    // or an array of pointers to variants.
    bool Invoke(const wxString& member, int action,
        wxVariant& retValue, int noArgs, wxVariant args[], const wxVariant* ptrArgs[] = 0) const;

    // Invoke a member function
    wxVariant CallMethod(const wxString& method, int noArgs, wxVariant args[]);
    wxVariant CallMethodArray(const wxString& method, int noArgs, const wxVariant **args);

    // Convenience function
    wxVariant CallMethod(const wxString& method,
        const wxVariant& arg1 = wxNullVariant, const wxVariant& arg2 = wxNullVariant,
        const wxVariant& arg3 = wxNullVariant, const wxVariant& arg4 = wxNullVariant,
        const wxVariant& arg5 = wxNullVariant, const wxVariant& arg6 = wxNullVariant);

    // Get/Put property
    wxVariant GetProperty(const wxString& property, int noArgs = 0, wxVariant args[] = NULL) const;
    wxVariant GetPropertyArray(const wxString& property, int noArgs, const wxVariant **args) const;
    wxVariant GetProperty(const wxString& property,
        const wxVariant& arg1, const wxVariant& arg2 = wxNullVariant,
        const wxVariant& arg3 = wxNullVariant, const wxVariant& arg4 = wxNullVariant,
        const wxVariant& arg5 = wxNullVariant, const wxVariant& arg6 = wxNullVariant);

    bool PutPropertyArray(const wxString& property, int noArgs, const wxVariant **args);
    bool PutProperty(const wxString& property, int noArgs, wxVariant args[]) ;
    bool PutProperty(const wxString& property,
        const wxVariant& arg1, const wxVariant& arg2 = wxNullVariant,
        const wxVariant& arg3 = wxNullVariant, const wxVariant& arg4 = wxNullVariant,
        const wxVariant& arg5 = wxNullVariant, const wxVariant& arg6 = wxNullVariant);

    // Uses DISPATCH_PROPERTYGET
    // and returns a dispatch pointer. The calling code should call Release
    // on the pointer, though this could be implicit by constructing an wxAutomationObject
    // with it and letting the destructor call Release.
    WXIDISPATCH* GetDispatchProperty(const wxString& property, int noArgs, wxVariant args[]) const;
    WXIDISPATCH* GetDispatchProperty(const wxString& property, int noArgs, const wxVariant **args) const;

    // A way of initialising another wxAutomationObject with a dispatch object,
    // without having to deal with nasty IDispatch pointers.
    bool GetObject(wxAutomationObject& obj, const wxString& property, int noArgs = 0, wxVariant args[] = NULL) const;
    bool GetObject(wxAutomationObject& obj, const wxString& property, int noArgs, const wxVariant **args) const;

    // Returns the locale identifier used in automation calls. The default is
    // LOCALE_SYSTEM_DEFAULT. Objects obtained by GetObject() inherit the
    // locale identifier from the one that created them.
    WXLCID GetLCID() const;

    // Sets the locale identifier to be used in automation calls performed by
    // this object. The default is LOCALE_SYSTEM_DEFAULT.
    void SetLCID(WXLCID lcid);

    // Returns the flags used for conversions between wxVariant and OLE
    // VARIANT, see wxOleConvertVariantFlags. The default value is
    // wxOleConvertVariant_Default but all the objects obtained by GetObject()
    // inherit the flags from the one that created them.
    long GetConvertVariantFlags() const;

    // Sets the flags used for conversions between wxVariant and OLE VARIANT,
    // see wxOleConvertVariantFlags (default is wxOleConvertVariant_Default.
    void SetConvertVariantFlags(long flags);

public: // public for compatibility only, don't use m_dispatchPtr directly.
    WXIDISPATCH*  m_dispatchPtr;

private:
    WXLCID m_lcid;
    long   m_convertVariantFlags;

    wxDECLARE_NO_COPY_CLASS(wxAutomationObject);
};

#endif // wxUSE_OLE_AUTOMATION

#endif // _WX_AUTOMTN_H_
