/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/fontdlg.h
// Purpose:     wxFontDialog class using fonts window services (10.2+).
// Author:      Ryan Norton
// Modified by:
// Created:     2004-09-25
// Copyright:   (c) Ryan Norton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FONTDLG_H_
#define _WX_FONTDLG_H_

#include "wx/dialog.h"

/*
 * Font dialog
 */

/*
 * support old notation
 */
#ifdef wxMAC_USE_EXPERIMENTAL_FONTDIALOG
#define wxOSX_USE_EXPERIMENTAL_FONTDIALOG wxMAC_USE_EXPERIMENTAL_FONTDIALOG
#endif

#ifndef wxOSX_USE_EXPERIMENTAL_FONTDIALOG
#define wxOSX_USE_EXPERIMENTAL_FONTDIALOG 1
#endif

#if wxOSX_USE_EXPERIMENTAL_FONTDIALOG

class WXDLLIMPEXP_CORE wxFontDialog : public wxDialog
{
public:
    wxFontDialog();
    wxFontDialog(wxWindow *parent);
    wxFontDialog(wxWindow *parent, const wxFontData& data);
    virtual ~wxFontDialog();

    bool Create(wxWindow *parent);
    bool Create(wxWindow *parent, const wxFontData& data);

    int ShowModal();
    wxFontData& GetFontData() { return m_fontData; }

protected:
    wxFontData m_fontData;

    DECLARE_DYNAMIC_CLASS_NO_COPY(wxFontDialog)
};

extern "C" int RunMixedFontDialog(wxFontDialog* dialog) ;

#else // wxOSX_USE_EXPERIMENTAL_FONTDIALOG

#if !USE_NATIVE_FONT_DIALOG_FOR_MACOSX

/*!
 * Forward declarations
 */

class wxFontColourSwatchCtrl;
class wxFontPreviewCtrl;
class WXDLLIMPEXP_FWD_CORE wxSpinCtrl;
class WXDLLIMPEXP_FWD_CORE wxSpinEvent;
class WXDLLIMPEXP_FWD_CORE wxListBox;
class WXDLLIMPEXP_FWD_CORE wxChoice;
class WXDLLIMPEXP_FWD_CORE wxButton;
class WXDLLIMPEXP_FWD_CORE wxStaticText;
class WXDLLIMPEXP_FWD_CORE wxCheckBox;

/*!
 * Control identifiers
 */

#define wxID_FONTDIALOG_FACENAME 20001
#define wxID_FONTDIALOG_FONTSIZE 20002
#define wxID_FONTDIALOG_BOLD 20003
#define wxID_FONTDIALOG_ITALIC 20004
#define wxID_FONTDIALOG_UNDERLINED 20005
#define wxID_FONTDIALOG_COLOUR 20006
#define wxID_FONTDIALOG_PREVIEW 20007

#endif
    // !USE_NATIVE_FONT_DIALOG_FOR_MACOSX

class WXDLLIMPEXP_CORE wxFontDialog: public wxDialog
{
DECLARE_DYNAMIC_CLASS(wxFontDialog)

#if !USE_NATIVE_FONT_DIALOG_FOR_MACOSX
DECLARE_EVENT_TABLE()
#endif

public:
    wxFontDialog();
    wxFontDialog(wxWindow *parent, const wxFontData& data);
    virtual ~wxFontDialog();

    bool Create(wxWindow *parent, const wxFontData& data);

    int ShowModal();
    wxFontData& GetFontData() { return m_fontData; }
    bool IsShown() const;
    void OnPanelClose();
    void SetData(const wxFontData& data);

#if !USE_NATIVE_FONT_DIALOG_FOR_MACOSX

    /// Creates the controls and sizers
    void CreateControls();

    /// Initialize font
    void InitializeFont();

    /// Set controls according to current font
    void InitializeControls();

    /// Respond to font change
    void ChangeFont();

    /// Respond to colour change
    void OnColourChanged(wxCommandEvent& event);

    /// wxEVT_LISTBOX event handler for wxID_FONTDIALOG_FACENAME
    void OnFontdialogFacenameSelected( wxCommandEvent& event );

    /// wxEVT_SPINCTRL event handler for wxID_FONTDIALOG_FONTSIZE
    void OnFontdialogFontsizeUpdated( wxSpinEvent& event );

    /// wxEVT_TEXT event handler for wxID_FONTDIALOG_FONTSIZE
    void OnFontdialogFontsizeTextUpdated( wxCommandEvent& event );

    /// wxEVT_CHECKBOX event handler for wxID_FONTDIALOG_BOLD
    void OnFontdialogBoldClick( wxCommandEvent& event );

    /// wxEVT_CHECKBOX event handler for wxID_FONTDIALOG_ITALIC
    void OnFontdialogItalicClick( wxCommandEvent& event );

    /// wxEVT_CHECKBOX event handler for wxID_FONTDIALOG_UNDERLINED
    void OnFontdialogUnderlinedClick( wxCommandEvent& event );

    /// wxEVT_BUTTON event handler for wxID_OK
    void OnOkClick( wxCommandEvent& event );

    /// Should we show tooltips?
    static bool ShowToolTips();

    wxListBox* m_facenameCtrl;
    wxSpinCtrl* m_sizeCtrl;
    wxCheckBox* m_boldCtrl;
    wxCheckBox* m_italicCtrl;
    wxCheckBox* m_underlinedCtrl;
    wxFontColourSwatchCtrl* m_colourCtrl;
    wxFontPreviewCtrl* m_previewCtrl;

    wxFont      m_dialogFont;
    bool        m_suppressUpdates;

#endif
    // !USE_NATIVE_FONT_DIALOG_FOR_MACOSX

protected:
    wxWindow*   m_dialogParent;
    wxFontData  m_fontData;
    void*       m_pEventHandlerRef;
};

#endif

#endif
    // _WX_FONTDLG_H_
