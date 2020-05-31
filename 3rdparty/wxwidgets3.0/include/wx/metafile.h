///////////////////////////////////////////////////////////////////////////////
// Name:        wx/metafile.h
// Purpose:     wxMetaFile class declaration
// Author:      wxWidgets team
// Modified by:
// Created:     13.01.00
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_METAFILE_H_BASE_
#define _WX_METAFILE_H_BASE_

#include "wx/defs.h"

#if wxUSE_METAFILE

// provide synonyms for all metafile classes
#define wxMetaFile wxMetafile
#define wxMetaFileDC wxMetafileDC
#define wxMetaFileDataObject wxMetafileDataObject

#define wxMakeMetaFilePlaceable wxMakeMetafilePlaceable

#if defined(__WXMSW__)
    #if wxUSE_ENH_METAFILE
        #include "wx/msw/enhmeta.h"

        #if wxUSE_WIN_METAFILES_ALWAYS
            // use normal metafiles as well
            #include "wx/msw/metafile.h"
        #else // also map all metafile classes to enh metafile
            typedef wxEnhMetaFile wxMetafile;
            typedef wxEnhMetaFileDC wxMetafileDC;
            #if wxUSE_DRAG_AND_DROP
                typedef wxEnhMetaFileDataObject wxMetafileDataObject;
            #endif

            // this flag will be set if wxMetafile class is wxEnhMetaFile
            #define wxMETAFILE_IS_ENH
        #endif // wxUSE_WIN_METAFILES_ALWAYS
    #else // !wxUSE_ENH_METAFILE
        #include "wx/msw/metafile.h"
    #endif
#elif defined(__WXPM__)
    #include "wx/os2/metafile.h"
#elif defined(__WXMAC__)
    #include "wx/osx/metafile.h"
#endif

#endif // wxUSE_METAFILE

#endif // _WX_METAFILE_H_BASE_
