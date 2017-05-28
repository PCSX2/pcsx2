///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/filectrlcmn.cpp
// Purpose:     Implementation for wxFileCtrlBase and other common functions used by
//              platform-specific wxFileCtrl's
// Author:      Diaa M. Sami
// Created:     2007-07-07
// Copyright:   (c) Diaa M. Sami
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#if wxUSE_FILECTRL

#include "wx/filectrl.h"

#ifndef WX_PRECOMP
#    include "wx/debug.h"
#endif

const char wxFileCtrlNameStr[] = "wxfilectrl";

wxDEFINE_EVENT( wxEVT_FILECTRL_SELECTIONCHANGED, wxFileCtrlEvent );
wxDEFINE_EVENT( wxEVT_FILECTRL_FILEACTIVATED, wxFileCtrlEvent );
wxDEFINE_EVENT( wxEVT_FILECTRL_FOLDERCHANGED, wxFileCtrlEvent );
wxDEFINE_EVENT( wxEVT_FILECTRL_FILTERCHANGED, wxFileCtrlEvent );

wxIMPLEMENT_DYNAMIC_CLASS( wxFileCtrlEvent, wxCommandEvent );

// some helper functions

void GenerateFilterChangedEvent( wxFileCtrlBase *fileCtrl, wxWindow *wnd )
{
    wxFileCtrlEvent event( wxEVT_FILECTRL_FILTERCHANGED, wnd, wnd->GetId() );

    event.SetFilterIndex( fileCtrl->GetFilterIndex() );

    wnd->GetEventHandler()->ProcessEvent( event );
}

void GenerateFolderChangedEvent( wxFileCtrlBase *fileCtrl, wxWindow *wnd )
{
    wxFileCtrlEvent event( wxEVT_FILECTRL_FOLDERCHANGED, wnd, wnd->GetId() );

    event.SetDirectory( fileCtrl->GetDirectory() );

    wnd->GetEventHandler()->ProcessEvent( event );
}

void GenerateSelectionChangedEvent( wxFileCtrlBase *fileCtrl, wxWindow *wnd)
{
    wxFileCtrlEvent event( wxEVT_FILECTRL_SELECTIONCHANGED, wnd, wnd->GetId() );
    event.SetDirectory( fileCtrl->GetDirectory() );

    wxArrayString filenames;
    fileCtrl->GetFilenames( filenames );
    event.SetFiles( filenames );

    wnd->GetEventHandler()->ProcessEvent( event );
}

void GenerateFileActivatedEvent( wxFileCtrlBase *fileCtrl, wxWindow *wnd, const wxString& filename )
{
    wxFileCtrlEvent event( wxEVT_FILECTRL_FILEACTIVATED, wnd, wnd->GetId() );
    event.SetDirectory( fileCtrl->GetDirectory() );

    wxArrayString filenames;

    if ( filename.empty() )
    {
        fileCtrl->GetFilenames( filenames );
    }
    else
    {
        filenames.Add( filename );
    }

    event.SetFiles( filenames );

    wnd->GetEventHandler()->ProcessEvent( event );
}

///////////////////////////////////////////////////////////////////////////////
// wxFileCtrlEvent implementation
///////////////////////////////////////////////////////////////////////////////

wxString wxFileCtrlEvent::GetFile() const
{
    wxASSERT_MSG( !wxDynamicCast( GetEventObject(), wxFileCtrl )->HasMultipleFileSelection(),
                  wxT( "Please use GetFiles() to get all files instead of this function" ) );

    wxString string;
    if (m_files.Count() != 0)
        string = m_files[0];
    return string;
}

#endif // wxUSE_FILECTRL
