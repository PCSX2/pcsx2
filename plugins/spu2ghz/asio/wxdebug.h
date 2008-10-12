//==========================================================================;
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
//  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
//  PURPOSE.
//
//  Copyright (c) 1992 - 1996  Microsoft Corporation.  All Rights Reserved.
//
//--------------------------------------------------------------------------;

// Debugging facilities, January 1995

#ifndef __WXDEBUG__
#define __WXDEBUG__

// Avoid conflict with MFC
#undef ASSERT

// This library provides fairly straight forward debugging functionality, this
// is split into two main sections. The first is assertion handling, there are
// three types of assertions provided here. The most commonly used one is the
// ASSERT(condition) macro which will pop up a message box including the file
// and line number if the condition evaluates to FALSE. Then there is the
// EXECUTE_ASSERT macro which is the same as ASSERT except the condition will
// still be executed in NON debug builds. The final type of assertion is the
// KASSERT macro which is more suitable for pure (perhaps kernel) filters as
// the condition is printed onto the debugger rather than in a message box.
//
// The other part of the debug module facilties is general purpose logging.
// This is accessed by calling DbgLog(). The function takes a type and level
// field which define the type of informational string you are presenting and
// it's relative importance. The type field can be a combination (one or more)
// of LOG_TIMING, LOG_TRACE, LOG_MEMORY, LOG_LOCKING and LOG_ERROR. The level
// is a DWORD value where zero defines highest important. Use of zero as the
// debug logging level is to be encouraged ONLY for major errors or events as
// they will ALWAYS be displayed on the debugger. Other debug output has it's
// level matched against the current debug output level stored in the registry
// for this module and if less than the current setting it will be displayed.
//
// Each module or executable has it's own debug output level for each of the
// five types. These are read in when the DbgInitialise function is called
// for DLLs linking to STRMBASE.LIB this is done automatically when the DLL
// is loaded, executables must call it explicitely with the module instance
// handle given to them through the WINMAIN entry point. An executable must
// also call DbgTerminate when they have finished to clean up the resources
// the debug library uses, once again this is done automatically for DLLs

// These are the five different categories of logging information

enum {  LOG_TIMING = 0x01,    // Timing and performance measurements
        LOG_TRACE = 0x02,     // General step point call tracing
        LOG_MEMORY =  0x04,   // Memory and object allocation/destruction
        LOG_LOCKING = 0x08,   // Locking/unlocking of critical sections
        LOG_ERROR = 0x10 };   // Debug error notification

enum {  CDISP_HEX = 0x01,
        CDISP_DEC = 0x02};

// For each object created derived from CBaseObject (in debug builds) we
// create a descriptor that holds it's name (statically allocated memory)
// and a cookie we assign it. We keep a list of all the active objects
// we have registered so that we can dump a list of remaining objects

typedef struct tag_ObjectDesc {
    TCHAR *m_pName;
    DWORD m_dwCookie;
    tag_ObjectDesc *m_pNext;
} ObjectDesc;

#define DLLIMPORT __declspec(dllimport)
#define DLLEXPORT __declspec(dllexport)

#ifdef DEBUG

    #define NAME(x) TEXT(x)

    // These are used internally by the debug library (PRIVATE)

    void DbgInitKeyLevels(HKEY hKey);
    void DbgInitGlobalSettings();
    void DbgInitModuleSettings();
    void DbgInitModuleName();
    DWORD DbgRegisterObjectCreation(TCHAR *pObjectName);
    BOOL DbgRegisterObjectDestruction(DWORD dwCookie);

    // These are the PUBLIC entry points

    BOOL DbgCheckModuleLevel(DWORD Type,DWORD Level);
    void DbgSetModuleLevel(DWORD Type,DWORD Level);

    // Initialise the library with the module handle

    void DbgInitialise(HINSTANCE hInst);
    void DbgTerminate();

    void DbgDumpObjectRegister();

    // Display error and logging to the user

    void DbgAssert(const TCHAR *pCondition,const TCHAR *pFileName,INT iLine);
    void DbgBreakPoint(const TCHAR *pCondition,const TCHAR *pFileName,INT iLine);
    void DbgKernelAssert(const TCHAR *pCondition,const TCHAR *pFileName,INT iLine);
    void DbgLogInfo(DWORD Type,DWORD Level,const TCHAR *pFormat,...);
    void DbgOutString(LPCTSTR psz);

    //  Debug infinite wait stuff
    DWORD DbgWaitForSingleObject(HANDLE h);
    DWORD DbgWaitForMultipleObjects(DWORD nCount,
                                    CONST HANDLE *lpHandles,
                                    BOOL bWaitAll);
    void DbgSetWaitTimeout(DWORD dwTimeout);

#ifdef __strmif_h__
    void DisplayType(LPSTR label, const AM_MEDIA_TYPE *pmtIn);
#endif

    #define KASSERT(_x_) if (!(_x_))         \
        DbgKernelAssert(TEXT(#_x_),TEXT(__FILE__),__LINE__)

    //  Break on the debugger without putting up a message box
    //  message goes to debugger instead

    #define KDbgBreak(_x_)                   \
        DbgKernelAssert(TEXT(#_x_),TEXT(__FILE__),__LINE__)

    #define ASSERT(_x_) if (!(_x_))         \
        DbgAssert(TEXT(#_x_),TEXT(__FILE__),__LINE__)

    //  Put up a message box informing the user of a halt
    //  condition in the program

    #define DbgBreak(_x_)                   \
        DbgBreakPoint(TEXT(#_x_),TEXT(__FILE__),__LINE__)

    #define EXECUTE_ASSERT(_x_) ASSERT(_x_)
    #define DbgLog(_x_) DbgLogInfo _x_

    // MFC style trace macros

    #define NOTE(_x_)             DbgLog((LOG_TRACE,5,TEXT(_x_)));
    #define NOTE1(_x_,a)          DbgLog((LOG_TRACE,5,TEXT(_x_),a));
    #define NOTE2(_x_,a,b)        DbgLog((LOG_TRACE,5,TEXT(_x_),a,b));
    #define NOTE3(_x_,a,b,c)      DbgLog((LOG_TRACE,5,TEXT(_x_),a,b,c));
    #define NOTE4(_x_,a,b,c,d)    DbgLog((LOG_TRACE,5,TEXT(_x_),a,b,c,d));
    #define NOTE5(_x_,a,b,c,d,e)  DbgLog((LOG_TRACE,5,TEXT(_x_),a,b,c,d,e));

#else

    // Retail builds make public debug functions inert  - WARNING the source
    // files do not define or build any of the entry points in debug builds
    // (public entry points compile to nothing) so if you go trying to call
    // any of the private entry points in your source they won't compile

    #define NAME(_x_) NULL

    #define DbgInitialise(hInst)
    #define DbgTerminate()
    #define DbgLog(_x_)
    #define DbgOutString(psz)

    #define DbgRegisterObjectCreation(pObjectName)
    #define DbgRegisterObjectDestruction(dwCookie)
    #define DbgDumpObjectRegister()

    #define DbgCheckModuleLevel(Type,Level)
    #define DbgSetModuleLevel(Type,Level)

    #define DbgWaitForSingleObject(h)  WaitForSingleObject(h, INFINITE)
    #define DbgWaitForMultipleObjects(nCount, lpHandles, bWaitAll)     \
               WaitForMultipleObjects(nCount, lpHandles, bWaitAll, INFINITE)
    #define DbgSetWaitTimeout(dwTimeout)

    #define KDbgBreak(_x_)
    #define DbgBreak(_x_)

    #define KASSERT(_x_)
    #define ASSERT(_x_)
    #define EXECUTE_ASSERT(_x_) _x_

    // MFC style trace macros

    #define NOTE(_x_)
    #define NOTE1(_x_,a)
    #define NOTE2(_x_,a,b)
    #define NOTE3(_x_,a,b,c)
    #define NOTE4(_x_,a,b,c,d)
    #define NOTE5(_x_,a,b,c,d,e)

    #define DisplayType(label, pmtIn)

#endif


// Checks a pointer which should be non NULL - can be used as follows.

#define CheckPointer(p,ret) {if((p)==NULL) return (ret);}

//   HRESULT Foo(VOID *pBar)
//   {
//       CheckPointer(pBar,E_INVALIDARG)
//   }
//
//   Or if the function returns a boolean
//
//   BOOL Foo(VOID *pBar)
//   {
//       CheckPointer(pBar,FALSE)
//   }

// These validate pointers when symbol VFWROBUST is defined
// This will normally be defined in debug not retail builds

#ifdef DEBUG
    #define VFWROBUST
#endif

#ifdef VFWROBUST

    #define ValidateReadPtr(p,cb) \
        {if(IsBadReadPtr((PVOID)p,cb) == TRUE) \
            DbgBreak("Invalid read pointer");}

    #define ValidateWritePtr(p,cb) \
        {if(IsBadWritePtr((PVOID)p,cb) == TRUE) \
            DbgBreak("Invalid write pointer");}

    #define ValidateReadWritePtr(p,cb) \
        {ValidateReadPtr(p,cb) ValidateWritePtr(p,cb)}

    #define ValidateStringPtr(p) \
        {if(IsBadStringPtr((LPCTSTR)p,INFINITE) == TRUE) \
            DbgBreak("Invalid string pointer");}

    #define ValidateStringPtrA(p) \
        {if(IsBadStringPtrA((LPCSTR)p,INFINITE) == TRUE) \
            DbgBreak("Invalid ANSII string pointer");}

    #define ValidateStringPtrW(p) \
        {if(IsBadStringPtrW((LPCWSTR)p,INFINITE) == TRUE) \
            DbgBreak("Invalid UNICODE string pointer");}

#else
    #define ValidateReadPtr(p,cb)
    #define ValidateWritePtr(p,cb)
    #define ValidateReadWritePtr(p,cb)
    #define ValidateStringPtr(p)
    #define ValidateStringPtrA(p)
    #define ValidateStringPtrW(p)
#endif


#ifdef _OBJBASE_H_

    //  Outputting GUID names.  If you want to include the name
    //  associated with a GUID (eg CLSID_...) then
    //
    //      GuidNames[yourGUID]
    //
    //  Returns the name defined in uuids.h as a string

    typedef struct {
        TCHAR   *szName;
        GUID    guid;
    } GUID_STRING_ENTRY;

    class CGuidNameList {
    public:
        TCHAR *operator [] (const GUID& guid);
    };

    extern CGuidNameList GuidNames;

#endif


//  REMIND macro - generates warning as reminder to complete coding
//  (eg) usage:
//
//  #pragma message (REMIND("Add automation support"))


#define QUOTE(x) #x
#define QQUOTE(y) QUOTE(y)
#define REMIND(str) __FILE__ "(" QQUOTE(__LINE__) ") :  " str


//  Hack to display objects in a useful format
//
//  eg If you want to display a LONGLONG ll in a debug string do (eg)
//
//  DbgLog((LOG_TRACE, n, TEXT("Value is %s"), (LPCTSTR)CDisp(ll, CDISP_HEX)));


class CDispBasic
{
public:
    CDispBasic() { m_pString = m_String; };
    ~CDispBasic();
protected:
    PTCHAR m_pString;  // normally points to m_String... unless too much data
    TCHAR m_String[50];
};
class CDisp : public CDispBasic
{
public:
    CDisp(LONGLONG ll, int Format = CDISP_HEX); // Display a LONGLONG in CDISP_HEX or CDISP_DEC form
    CDisp(REFCLSID clsid);      // Display a GUID
    CDisp(double d);            // Display a floating point number
#ifdef __strmif_h__
#ifdef __STREAMS__
    CDisp(CRefTime t);          // Display a Reference Time
#endif
    CDisp(IPin *pPin);          // Display a pin as {filter clsid}(pin name)
#endif // __strmif_h__
    ~CDisp();

    //  Implement cast to (LPCTSTR) as parameter to logger
    operator LPCTSTR()
    {
        return (LPCTSTR)m_pString;
    };
};

#endif // __WXDEBUG__

