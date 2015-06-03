///////////////////////////////////////////////////////////////////////////////
// Name:        src/xrc/xmladv.cpp
// Purpose:     Parts of wxXRC library depending on wxAdv: they must not be in
//              xmlres.cpp itself or it becomes impossible to use wxXRC without
//              linking wxAdv even if the latter is not used at all.
// Author:      Vadim Zeitlin (extracted from src/xrc/xmlres.cpp)
// Created:     2008-08-02
// Copyright:   (c) 2000 Vaclav Slavik
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_XRC

#include "wx/xrc/xmlres.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
#endif // WX_PRECOMP

#include "wx/animate.h"
#include "wx/scopedptr.h"

// ============================================================================
// implementation
// ============================================================================

#if wxUSE_ANIMATIONCTRL
wxAnimation* wxXmlResourceHandlerImpl::GetAnimation(const wxString& param)
{
    const wxString name = GetParamValue(param);
    if ( name.empty() )
        return NULL;

    // load the animation from file
    wxScopedPtr<wxAnimation> ani(new wxAnimation);
#if wxUSE_FILESYSTEM
    wxFSFile * const
        fsfile = GetCurFileSystem().OpenFile(name, wxFS_READ | wxFS_SEEKABLE);
    if ( fsfile )
    {
        ani->Load(*fsfile->GetStream());
        delete fsfile;
    }
#else
    ani->LoadFile(name);
#endif

    if ( !ani->IsOk() )
    {
        ReportParamError
        (
            param,
            wxString::Format("cannot create animation from \"%s\"", name)
        );
        return NULL;
    }

    return ani.release();
}
#endif // wxUSE_ANIMATIONCTRL

#endif // wxUSE_XRC




