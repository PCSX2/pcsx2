/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/bmpbuttn.cpp
// Purpose:     wxBitmapButton
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_BMPBUTTON

#include "wx/bmpbuttn.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/dcmemory.h"
    #include "wx/image.h"
#endif

#include "wx/msw/private.h"
#include "wx/msw/dc.h"          // for wxDCTemp

#include "wx/msw/uxtheme.h"

#if wxUSE_UXTHEME
    // no need to include tmschema.h
    #ifndef BP_PUSHBUTTON
        #define BP_PUSHBUTTON 1

        #define PBS_NORMAL    1
        #define PBS_HOT       2
        #define PBS_PRESSED   3
        #define PBS_DISABLED  4
        #define PBS_DEFAULTED 5

        #define TMT_CONTENTMARGINS 3602
    #endif
#endif // wxUSE_UXTHEME

#ifndef ODS_NOFOCUSRECT
    #define ODS_NOFOCUSRECT     0x0200
#endif

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

wxBEGIN_EVENT_TABLE(wxBitmapButton, wxBitmapButtonBase)
    EVT_SYS_COLOUR_CHANGED(wxBitmapButton::OnSysColourChanged)
wxEND_EVENT_TABLE()

/*
TODO PROPERTIES :

long "style" , wxBU_AUTODRAW
bool "default" , 0
bitmap "selected" ,
bitmap "focus" ,
bitmap "disabled" ,
*/

bool wxBitmapButton::Create(wxWindow *parent,
                            wxWindowID id,
                            const wxBitmap& bitmap,
                            const wxPoint& pos,
                            const wxSize& size, long style,
                            const wxValidator& validator,
                            const wxString& name)
{
    if ( !wxBitmapButtonBase::Create(parent, id, pos, size, style,
                                     validator, name) )
        return false;

    if ( bitmap.IsOk() )
        SetBitmapLabel(bitmap);

    if ( !size.IsFullySpecified() )
    {
        // As our bitmap has just changed, our best size has changed as well so
        // reset the initial size using the new value.
        SetInitialSize(size);
    }

    return true;
}

#endif // wxUSE_BMPBUTTON
