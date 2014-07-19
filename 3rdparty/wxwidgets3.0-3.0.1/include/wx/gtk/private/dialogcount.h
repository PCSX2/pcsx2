///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/private/dialogcount.h
// Purpose:     Helper for temporarily changing wxOpenModalDialogsCount global.
// Author:      Vadim Zeitlin
// Created:     2013-03-21
// Copyright:   (c) 2013 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_PRIVATE_DIALOGCOUNT_H_
#define _WX_GTK_PRIVATE_DIALOGCOUNT_H_

#include "wx/defs.h"

// This global variable contains the number of currently opened modal dialogs.
// When it is non null, the global menu used in Ubuntu Unity needs to be
// explicitly disabled as this doesn't currently happen on its own due to a bug
// in Unity, see https://bugs.launchpad.net/indicator-appmenu/+bug/674605
//
// For this to work, all modal dialogs must use wxOpenModalDialogLocker class
// below to increment this variable while they are indeed being modally shown.
//
// This variable is defined in src/gtk/toplevel.cpp
extern int wxOpenModalDialogsCount;

// ----------------------------------------------------------------------------
// wxOpenModalDialogLocker: Create an object of this class to increment
//                          wxOpenModalDialogsCount during its lifetime.
// ----------------------------------------------------------------------------

class wxOpenModalDialogLocker
{
public:
    wxOpenModalDialogLocker()
    {
        wxOpenModalDialogsCount++;
    }

    ~wxOpenModalDialogLocker()
    {
        wxOpenModalDialogsCount--;
    }

private:
    wxDECLARE_NO_COPY_CLASS(wxOpenModalDialogLocker);
};

#endif // _WX_GTK_PRIVATE_DIALOGCOUNT_H_
