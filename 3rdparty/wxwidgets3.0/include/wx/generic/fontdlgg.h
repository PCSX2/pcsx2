/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/fontdlgg.h
// Purpose:     wxGenericFontDialog
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_FONTDLGG_H
#define _WX_GENERIC_FONTDLGG_H

#include "wx/gdicmn.h"
#include "wx/font.h"

#ifdef __WXWINCE__
#define USE_SPINCTRL_FOR_POINT_SIZE 1
class WXDLLIMPEXP_FWD_CORE wxSpinEvent;
#else
#define USE_SPINCTRL_FOR_POINT_SIZE 0
#endif

/*
 * FONT DIALOG
 */

class WXDLLIMPEXP_FWD_CORE wxChoice;
class WXDLLIMPEXP_FWD_CORE wxText;
class WXDLLIMPEXP_FWD_CORE wxCheckBox;
class WXDLLIMPEXP_FWD_CORE wxFontPreviewer;

enum
{
    wxID_FONT_UNDERLINE = 3000,
    wxID_FONT_STYLE,
    wxID_FONT_WEIGHT,
    wxID_FONT_FAMILY,
    wxID_FONT_COLOUR,
    wxID_FONT_SIZE
};

class WXDLLIMPEXP_CORE wxGenericFontDialog : public wxFontDialogBase
{
public:
    wxGenericFontDialog() { Init(); }
    wxGenericFontDialog(wxWindow *parent)
        : wxFontDialogBase(parent) { Init(); }
    wxGenericFontDialog(wxWindow *parent, const wxFontData& data)
        : wxFontDialogBase(parent, data) { Init(); }
    virtual ~wxGenericFontDialog();

    virtual int ShowModal();

#if WXWIN_COMPATIBILITY_2_6
    // deprecated, for backwards compatibility only
    wxDEPRECATED( wxGenericFontDialog(wxWindow *parent, const wxFontData *data) );
#endif // WXWIN_COMPATIBILITY_2_6

    // Internal functions
    void OnCloseWindow(wxCloseEvent& event);

    virtual void CreateWidgets();
    virtual void InitializeFont();

    void OnChangeFont(wxCommandEvent& event);

#if USE_SPINCTRL_FOR_POINT_SIZE
    void OnChangeSize(wxSpinEvent& event);
#endif

protected:

    virtual bool DoCreate(wxWindow *parent);

private:

    // common part of all ctors
    void Init();

    void DoChangeFont();

    wxFont m_dialogFont;

    wxChoice *m_familyChoice;
    wxChoice *m_styleChoice;
    wxChoice *m_weightChoice;
    wxChoice *m_colourChoice;
    wxCheckBox *m_underLineCheckBox;

#if !USE_SPINCTRL_FOR_POINT_SIZE
    wxChoice   *m_pointSizeChoice;
#endif

    wxFontPreviewer *m_previewer;
    bool       m_useEvents;

    //  static bool fontDialogCancelled;
    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxGenericFontDialog)
};

#if WXWIN_COMPATIBILITY_2_6
    // deprecated, for backwards compatibility only
inline wxGenericFontDialog::wxGenericFontDialog(wxWindow *parent, const wxFontData *data)
                           :wxFontDialogBase(parent) { Init(); InitFontData(data); Create(parent); }
#endif // WXWIN_COMPATIBILITY_2_6

#endif // _WX_GENERIC_FONTDLGG_H
