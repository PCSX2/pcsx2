///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/debughlp.h
// Purpose:     wraps dbghelp.h standard file
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2005-01-08 (extracted from msw/crashrpt.cpp)
// Copyright:   (c) 2003-2005 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_DEBUGHLPH_H_
#define _WX_MSW_DEBUGHLPH_H_

#include "wx/dynlib.h"

#include "wx/msw/wrapwin.h"
#ifndef __WXWINCE__
#include <imagehlp.h>
#endif // __WXWINCE__
#include "wx/msw/private.h"

// All known versions of imagehlp.h define API_VERSION_NUMBER but it's not
// documented, so deal with the possibility that it's not defined just in case.
#ifndef API_VERSION_NUMBER
    #define API_VERSION_NUMBER 0
#endif

// wxUSE_DBGHELP is a bit special as it is not defined in wx/setup.h and we try
// to auto-detect whether we should be using debug help API or not ourselves
// below. However if the auto-detection fails, you can always predefine it as 0
// to avoid even trying.
#ifndef wxUSE_DBGHELP
    // The version of imagehlp.h from VC6 (7) is too old and is missing some
    // required symbols while the version from VC7 (9) is good enough. As we
    // don't know anything about version 8, don't use it unless we can test it.
    #if API_VERSION_NUMBER >= 9
        #define wxUSE_DBGHELP 1
    #else
        #define wxUSE_DBGHELP 0
    #endif
#endif

#if wxUSE_DBGHELP

// ----------------------------------------------------------------------------
// wxDbgHelpDLL: dynamically load dbghelp.dll functions
// ----------------------------------------------------------------------------

// wrapper for some functions from dbghelp.dll
//
// MT note: this class is not MT safe and should be only used from a single
//          thread at a time (this is so because dbghelp.dll is not MT-safe
//          itself anyhow)
class wxDbgHelpDLL
{
public:
    // some useful constants not present in debughlp.h (stolen from DIA SDK)
    enum BasicType
    {
        BASICTYPE_NOTYPE = 0,
        BASICTYPE_VOID = 1,
        BASICTYPE_CHAR = 2,
        BASICTYPE_WCHAR = 3,
        BASICTYPE_INT = 6,
        BASICTYPE_UINT = 7,
        BASICTYPE_FLOAT = 8,
        BASICTYPE_BCD = 9,
        BASICTYPE_BOOL = 10,
        BASICTYPE_LONG = 13,
        BASICTYPE_ULONG = 14,
        BASICTYPE_CURRENCY = 25,
        BASICTYPE_DATE = 26,
        BASICTYPE_VARIANT = 27,
        BASICTYPE_COMPLEX = 28,
        BASICTYPE_BIT = 29,
        BASICTYPE_BSTR = 30,
        BASICTYPE_HRESULT = 31,
        BASICTYPE_MAX
    };

    enum SymbolTag
    {
        SYMBOL_TAG_NULL,
        SYMBOL_TAG_EXE,
        SYMBOL_TAG_COMPILAND,
        SYMBOL_TAG_COMPILAND_DETAILS,
        SYMBOL_TAG_COMPILAND_ENV,
        SYMBOL_TAG_FUNCTION,
        SYMBOL_TAG_BLOCK,
        SYMBOL_TAG_DATA,
        SYMBOL_TAG_ANNOTATION,
        SYMBOL_TAG_LABEL,
        SYMBOL_TAG_PUBLIC_SYMBOL,
        SYMBOL_TAG_UDT,
        SYMBOL_TAG_ENUM,
        SYMBOL_TAG_FUNCTION_TYPE,
        SYMBOL_TAG_POINTER_TYPE,
        SYMBOL_TAG_ARRAY_TYPE,
        SYMBOL_TAG_BASE_TYPE,
        SYMBOL_TAG_TYPEDEF,
        SYMBOL_TAG_BASE_CLASS,
        SYMBOL_TAG_FRIEND,
        SYMBOL_TAG_FUNCTION_ARG_TYPE,
        SYMBOL_TAG_FUNC_DEBUG_START,
        SYMBOL_TAG_FUNC_DEBUG_END,
        SYMBOL_TAG_USING_NAMESPACE,
        SYMBOL_TAG_VTABLE_SHAPE,
        SYMBOL_TAG_VTABLE,
        SYMBOL_TAG_CUSTOM,
        SYMBOL_TAG_THUNK,
        SYMBOL_TAG_CUSTOM_TYPE,
        SYMBOL_TAG_MANAGED_TYPE,
        SYMBOL_TAG_DIMENSION,
        SYMBOL_TAG_MAX
    };

    enum DataKind
    {
        DATA_UNKNOWN,
        DATA_LOCAL,
        DATA_STATIC_LOCAL,
        DATA_PARAM,
        DATA_OBJECT_PTR,                    // "this" pointer
        DATA_FILE_STATIC,
        DATA_GLOBAL,
        DATA_MEMBER,
        DATA_STATIC_MEMBER,
        DATA_CONSTANT,
        DATA_MAX
    };

    enum UdtKind
    {
        UDT_STRUCT,
        UDT_CLASS,
        UDT_UNION,
        UDT_MAX
    };


    // function types
    typedef DWORD (WINAPI *SymGetOptions_t)();
    typedef DWORD (WINAPI *SymSetOptions_t)(DWORD);
    typedef BOOL (WINAPI *SymInitialize_t)(HANDLE, LPSTR, BOOL);
    typedef BOOL (WINAPI *StackWalk_t)(DWORD, HANDLE, HANDLE, LPSTACKFRAME,
                                       LPVOID, PREAD_PROCESS_MEMORY_ROUTINE,
                                       PFUNCTION_TABLE_ACCESS_ROUTINE,
                                       PGET_MODULE_BASE_ROUTINE,
                                       PTRANSLATE_ADDRESS_ROUTINE);
    typedef BOOL (WINAPI *SymFromAddr_t)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
    typedef LPVOID (WINAPI *SymFunctionTableAccess_t)(HANDLE, DWORD_PTR);
    typedef DWORD_PTR (WINAPI *SymGetModuleBase_t)(HANDLE, DWORD_PTR);
    typedef BOOL (WINAPI *SymGetLineFromAddr_t)(HANDLE, DWORD_PTR,
                                                PDWORD, PIMAGEHLP_LINE);
    typedef BOOL (WINAPI *SymSetContext_t)(HANDLE, PIMAGEHLP_STACK_FRAME,
                                           PIMAGEHLP_CONTEXT);
    typedef BOOL (WINAPI *SymEnumSymbols_t)(HANDLE, ULONG64, PCSTR,
                                            PSYM_ENUMERATESYMBOLS_CALLBACK, PVOID);
    typedef BOOL (WINAPI *SymGetTypeInfo_t)(HANDLE, DWORD64, ULONG,
                                            IMAGEHLP_SYMBOL_TYPE_INFO, PVOID);
    typedef BOOL (WINAPI *SymCleanup_t)(HANDLE);
    typedef BOOL (WINAPI *EnumerateLoadedModules_t)(HANDLE, PENUMLOADED_MODULES_CALLBACK, PVOID);
    typedef BOOL (WINAPI *MiniDumpWriteDump_t)(HANDLE, DWORD, HANDLE,
                                               MINIDUMP_TYPE,
                                               CONST PMINIDUMP_EXCEPTION_INFORMATION,
                                               CONST PMINIDUMP_USER_STREAM_INFORMATION,
                                               CONST PMINIDUMP_CALLBACK_INFORMATION);

    // The macro called by wxDO_FOR_ALL_SYM_FUNCS() below takes 2 arguments:
    // the name of the function in the program code, which never has "64"
    // suffix, and the name of the function in the DLL which can have "64"
    // suffix in some cases. These 2 helper macros call the macro with the
    // correct arguments in both cases.
    #define wxSYM_CALL(what, name)  what(name, name)
#if defined(_M_AMD64)
    #define wxSYM_CALL_64(what, name)  what(name, name ## 64)

    // Also undo all the "helpful" definitions done by imagehlp.h that map 32
    // bit functions to 64 bit ones, we don't need this as we do it ourselves.
    #undef StackWalk
    #undef SymFunctionTableAccess
    #undef SymGetModuleBase
    #undef SymGetLineFromAddr
    #undef EnumerateLoadedModules
#else
    #define wxSYM_CALL_64(what, name)  what(name, name)
#endif

    #define wxDO_FOR_ALL_SYM_FUNCS(what)                                      \
        wxSYM_CALL_64(what, StackWalk);                                       \
        wxSYM_CALL_64(what, SymFunctionTableAccess);                          \
        wxSYM_CALL_64(what, SymGetModuleBase);                                \
        wxSYM_CALL_64(what, SymGetLineFromAddr);                              \
        wxSYM_CALL_64(what, EnumerateLoadedModules);                          \
                                                                              \
        wxSYM_CALL(what, SymGetOptions);                                      \
        wxSYM_CALL(what, SymSetOptions);                                      \
        wxSYM_CALL(what, SymInitialize);                                      \
        wxSYM_CALL(what, SymFromAddr);                                        \
        wxSYM_CALL(what, SymSetContext);                                      \
        wxSYM_CALL(what, SymEnumSymbols);                                     \
        wxSYM_CALL(what, SymGetTypeInfo);                                     \
        wxSYM_CALL(what, SymCleanup);                                         \
        wxSYM_CALL(what, MiniDumpWriteDump)

    #define wxDECLARE_SYM_FUNCTION(func, name) static func ## _t func

    wxDO_FOR_ALL_SYM_FUNCS(wxDECLARE_SYM_FUNCTION);

    #undef wxDECLARE_SYM_FUNCTION

    // load all functions from DLL, return true if ok
    static bool Init();

    // return the string with the error message explaining why Init() failed
    static const wxString& GetErrorMessage();

    // log error returned by the given function to debug output
    static void LogError(const wxChar *func);

    // return textual representation of the value of given symbol
    static wxString DumpSymbol(PSYMBOL_INFO pSymInfo, void *pVariable);

    // return the name of the symbol with given type index
    static wxString GetSymbolName(PSYMBOL_INFO pSymInfo);

private:
    // dereference the given symbol, i.e. return symbol which is not a
    // pointer/reference any more
    //
    // if ppData != NULL, dereference the pointer as many times as we
    // dereferenced the symbol
    //
    // return the tag of the dereferenced symbol
    static SymbolTag DereferenceSymbol(PSYMBOL_INFO pSymInfo, void **ppData);

    static wxString DumpField(PSYMBOL_INFO pSymInfo,
                              void *pVariable,
                              unsigned level);

    static wxString DumpBaseType(BasicType bt, DWORD64 length, void *pVariable);

    static wxString DumpUDT(PSYMBOL_INFO pSymInfo,
                            void *pVariable,
                            unsigned level = 0);
};

#endif // wxUSE_DBGHELP

#endif // _WX_MSW_DEBUGHLPH_H_

