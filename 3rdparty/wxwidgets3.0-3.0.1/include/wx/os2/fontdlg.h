/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/fontdlg.h
// Purpose:     wxFontDialog class. Use generic version if no
//              platform-specific implementation.
// Author:      David Webster
// Modified by:
// Created:     10/06/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FONTDLG_H_
#define _WX_FONTDLG_H_

#include "wx/dialog.h"

/*
 * Font dialog
 */

class WXDLLIMPEXP_CORE wxFontDialog: public wxFontDialogBase
{
public:
    wxFontDialog() : wxFontDialogBase() { /* must be Create()d later */ }
    wxFontDialog (wxWindow* pParent) : wxFontDialogBase(pParent) { Create(pParent); }
    wxFontDialog( wxWindow*         pParent
                 ,const wxFontData& rData
                )
                : wxFontDialogBase( pParent
                                   ,rData
                                  )
    {
        Create( pParent
               ,rData
              );
    }

    virtual int ShowModal();

#if WXWIN_COMPATIBILITY_2_6
    //
    // Deprecated interface, don't use
    //
    wxDEPRECATED( wxFontDialog( wxWindow* pParent, const wxFontData* pData ) );
#endif // WXWIN_COMPATIBILITY_2_6

protected:
    DECLARE_DYNAMIC_CLASS(wxFontDialog)
}; // end of CLASS wxFontDialog

#if WXWIN_COMPATIBILITY_2_6
    // deprecated interface, don't use
inline wxFontDialog::wxFontDialog(wxWindow *parent, const wxFontData *data)
        : wxFontDialogBase(parent) { InitFontData(data); Create(parent); }
#endif // WXWIN_COMPATIBILITY_2_6

#endif
    // _WX_FONTDLG_H_
