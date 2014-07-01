///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/rcdefs.h
// Purpose:     Fallback for the generated rcdefs.h under the lib directory
// Author:      Mike Wetherell
// Copyright:   (c) 2005 Mike Wetherell
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_RCDEFS_H
#define _WX_RCDEFS_H

#ifdef __GNUC__
    // We must be using windres which uses gcc as its preprocessor. We do need
    // to generate the manifest then as gcc doesn't do it automatically and we
    // can define the architecture macro on our own as all the usual symbols
    // are available (unlike with Microsoft RC.EXE which doesn't predefine
    // anything useful at all).
    #ifndef wxUSE_RC_MANIFEST
        #define wxUSE_RC_MANIFEST 1
    #endif

    #if defined __i386__
        #ifndef WX_CPU_X86
            #define WX_CPU_X86
        #endif
    #elif defined __x86_64__
        #ifndef WX_CPU_AMD64
            #define WX_CPU_AMD64
        #endif
    #elif defined __ia64__
        #ifndef WX_CPU_IA64
            #define WX_CPU_IA64
        #endif
    #endif
#endif

// Don't do anything here for the other compilers, in particular don't define
// WX_CPU_X86 here as we used to do. If people define wxUSE_RC_MANIFEST, they
// must also define the architecture constant correctly.

#endif
