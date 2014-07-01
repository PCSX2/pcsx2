/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/fontdlg.h
// Purpose:     wxFontDialog
// Author:      Robert Roebling
// Created:
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_FONTDLG_H_
#define _WX_GTK_FONTDLG_H_

//-----------------------------------------------------------------------------
// wxFontDialog
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFontDialog : public wxFontDialogBase
{
public:
    wxFontDialog() : wxFontDialogBase() { /* must be Create()d later */ }
    wxFontDialog(wxWindow *parent)
        : wxFontDialogBase(parent) { Create(parent); }
    wxFontDialog(wxWindow *parent, const wxFontData& data)
        : wxFontDialogBase(parent, data) { Create(parent, data); }

    virtual ~wxFontDialog();

#if WXWIN_COMPATIBILITY_2_6
    // deprecated interface, don't use
    wxDEPRECATED( wxFontDialog(wxWindow *parent, const wxFontData *data) );
#endif // WXWIN_COMPATIBILITY_2_6

protected:
    // create the GTK dialog
    virtual bool DoCreate(wxWindow *parent);

    DECLARE_DYNAMIC_CLASS(wxFontDialog)
};

#if WXWIN_COMPATIBILITY_2_6
    // deprecated interface, don't use
inline wxFontDialog::wxFontDialog(wxWindow *parent, const wxFontData *data)
        : wxFontDialogBase(parent) { InitFontData(data); Create(parent); }
#endif // WXWIN_COMPATIBILITY_2_6

#endif
