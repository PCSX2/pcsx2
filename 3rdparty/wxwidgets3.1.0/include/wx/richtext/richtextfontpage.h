/////////////////////////////////////////////////////////////////////////////
// Name:        wx/richtext/richtextfontpage.h
// Purpose:     Font page for wxRichTextFormattingDialog
// Author:      Julian Smart
// Modified by:
// Created:     2006-10-02
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _RICHTEXTFONTPAGE_H_
#define _RICHTEXTFONTPAGE_H_

/*!
 * Includes
 */

#include "wx/richtext/richtextdialogpage.h"

////@begin includes
#include "wx/spinbutt.h"
////@end includes

/*!
 * Forward declarations
 */

////@begin forward declarations
class wxBoxSizer;
class wxSpinButton;
class wxRichTextFontListBox;
class wxRichTextColourSwatchCtrl;
class wxRichTextFontPreviewCtrl;
////@end forward declarations

/*!
 * Control identifiers
 */

////@begin control identifiers
#define SYMBOL_WXRICHTEXTFONTPAGE_STYLE wxTAB_TRAVERSAL
#define SYMBOL_WXRICHTEXTFONTPAGE_TITLE wxEmptyString
#define SYMBOL_WXRICHTEXTFONTPAGE_IDNAME ID_RICHTEXTFONTPAGE
#define SYMBOL_WXRICHTEXTFONTPAGE_SIZE wxSize(200, 100)
#define SYMBOL_WXRICHTEXTFONTPAGE_POSITION wxDefaultPosition
////@end control identifiers

/*!
 * wxRichTextFontPage class declaration
 */

class WXDLLIMPEXP_RICHTEXT wxRichTextFontPage: public wxRichTextDialogPage
{
    wxDECLARE_DYNAMIC_CLASS(wxRichTextFontPage);
    wxDECLARE_EVENT_TABLE();
    DECLARE_HELP_PROVISION()

public:
    /// Constructors
    wxRichTextFontPage( );
    wxRichTextFontPage( wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = SYMBOL_WXRICHTEXTFONTPAGE_POSITION, const wxSize& size = SYMBOL_WXRICHTEXTFONTPAGE_SIZE, long style = SYMBOL_WXRICHTEXTFONTPAGE_STYLE );

    /// Initialise members
    void Init();

    /// Creation
    bool Create( wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = SYMBOL_WXRICHTEXTFONTPAGE_POSITION, const wxSize& size = SYMBOL_WXRICHTEXTFONTPAGE_SIZE, long style = SYMBOL_WXRICHTEXTFONTPAGE_STYLE );

    /// Creates the controls and sizers
    void CreateControls();

    /// Transfer data from/to window
    virtual bool TransferDataFromWindow();
    virtual bool TransferDataToWindow();

    /// Updates the font preview
    void UpdatePreview();

    void OnFaceListBoxSelected( wxCommandEvent& event );
    void OnColourClicked( wxCommandEvent& event );

    /// Gets the attributes associated with the main formatting dialog
    wxRichTextAttr* GetAttributes();

    /// Determines which text effect controls should be shown.
    /// Currently only wxTEXT_ATTR_EFFECT_RTL and wxTEXT_ATTR_EFFECT_SUPPRESS_HYPHENATION may
    /// be removed from the page. By default, these effects are not shown as they
    /// have no effect in the editor.
    static int GetAllowedTextEffects() { return sm_allowedTextEffects; }
    
    /// Sets the allowed text effects in the page.
    static void SetAllowedTextEffects(int allowed) { sm_allowedTextEffects = allowed; }

////@begin wxRichTextFontPage event handler declarations

    /// wxEVT_IDLE event handler for ID_RICHTEXTFONTPAGE
    void OnIdle( wxIdleEvent& event );

    /// wxEVT_COMMAND_TEXT_UPDATED event handler for ID_RICHTEXTFONTPAGE_FACETEXTCTRL
    void OnFaceTextCtrlUpdated( wxCommandEvent& event );

    /// wxEVT_COMMAND_TEXT_UPDATED event handler for ID_RICHTEXTFONTPAGE_SIZETEXTCTRL
    void OnSizeTextCtrlUpdated( wxCommandEvent& event );

    /// wxEVT_SCROLL_LINEUP event handler for ID_RICHTEXTFONTPAGE_SPINBUTTONS
    void OnRichtextfontpageSpinbuttonsUp( wxSpinEvent& event );

    /// wxEVT_SCROLL_LINEDOWN event handler for ID_RICHTEXTFONTPAGE_SPINBUTTONS
    void OnRichtextfontpageSpinbuttonsDown( wxSpinEvent& event );

    /// wxEVT_COMMAND_CHOICE_SELECTED event handler for ID_RICHTEXTFONTPAGE_SIZE_UNITS
    void OnRichtextfontpageSizeUnitsSelected( wxCommandEvent& event );

    /// wxEVT_COMMAND_LISTBOX_SELECTED event handler for ID_RICHTEXTFONTPAGE_SIZELISTBOX
    void OnSizeListBoxSelected( wxCommandEvent& event );

    /// wxEVT_COMMAND_COMBOBOX_SELECTED event handler for ID_RICHTEXTFONTPAGE_STYLECTRL
    void OnStyleCtrlSelected( wxCommandEvent& event );

    /// wxEVT_COMMAND_COMBOBOX_SELECTED event handler for ID_RICHTEXTFONTPAGE_WEIGHTCTRL
    void OnWeightCtrlSelected( wxCommandEvent& event );

    /// wxEVT_COMMAND_COMBOBOX_SELECTED event handler for ID_RICHTEXTFONTPAGE_UNDERLINING_CTRL
    void OnUnderliningCtrlSelected( wxCommandEvent& event );

    /// wxEVT_COMMAND_CHECKBOX_CLICKED event handler for ID_RICHTEXTFONTPAGE_STRIKETHROUGHCTRL
    void OnStrikethroughctrlClick( wxCommandEvent& event );

    /// wxEVT_COMMAND_CHECKBOX_CLICKED event handler for ID_RICHTEXTFONTPAGE_CAPSCTRL
    void OnCapsctrlClick( wxCommandEvent& event );

    /// wxEVT_COMMAND_CHECKBOX_CLICKED event handler for ID_RICHTEXTFONTPAGE_SUPERSCRIPT
    void OnRichtextfontpageSuperscriptClick( wxCommandEvent& event );

    /// wxEVT_COMMAND_CHECKBOX_CLICKED event handler for ID_RICHTEXTFONTPAGE_SUBSCRIPT
    void OnRichtextfontpageSubscriptClick( wxCommandEvent& event );

////@end wxRichTextFontPage event handler declarations

////@begin wxRichTextFontPage member function declarations

    /// Retrieves bitmap resources
    wxBitmap GetBitmapResource( const wxString& name );

    /// Retrieves icon resources
    wxIcon GetIconResource( const wxString& name );
////@end wxRichTextFontPage member function declarations

    /// Should we show tooltips?
    static bool ShowToolTips();

////@begin wxRichTextFontPage member variables
    wxBoxSizer* m_innerSizer;
    wxTextCtrl* m_faceTextCtrl;
    wxTextCtrl* m_sizeTextCtrl;
    wxSpinButton* m_fontSizeSpinButtons;
    wxChoice* m_sizeUnitsCtrl;
    wxBoxSizer* m_fontListBoxParent;
    wxRichTextFontListBox* m_faceListBox;
    wxListBox* m_sizeListBox;
    wxComboBox* m_styleCtrl;
    wxComboBox* m_weightCtrl;
    wxComboBox* m_underliningCtrl;
    wxCheckBox* m_textColourLabel;
    wxRichTextColourSwatchCtrl* m_colourCtrl;
    wxCheckBox* m_bgColourLabel;
    wxRichTextColourSwatchCtrl* m_bgColourCtrl;
    wxCheckBox* m_strikethroughCtrl;
    wxCheckBox* m_capitalsCtrl;
    wxCheckBox* m_smallCapitalsCtrl;
    wxCheckBox* m_superscriptCtrl;
    wxCheckBox* m_subscriptCtrl;
    wxBoxSizer* m_rtlParentSizer;
    wxCheckBox* m_rtlCtrl;
    wxCheckBox* m_suppressHyphenationCtrl;
    wxRichTextFontPreviewCtrl* m_previewCtrl;
    /// Control identifiers
    enum {
        ID_RICHTEXTFONTPAGE = 10000,
        ID_RICHTEXTFONTPAGE_FACETEXTCTRL = 10001,
        ID_RICHTEXTFONTPAGE_SIZETEXTCTRL = 10002,
        ID_RICHTEXTFONTPAGE_SPINBUTTONS = 10003,
        ID_RICHTEXTFONTPAGE_SIZE_UNITS = 10004,
        ID_RICHTEXTFONTPAGE_FACELISTBOX = 10005,
        ID_RICHTEXTFONTPAGE_SIZELISTBOX = 10006,
        ID_RICHTEXTFONTPAGE_STYLECTRL = 10007,
        ID_RICHTEXTFONTPAGE_WEIGHTCTRL = 10008,
        ID_RICHTEXTFONTPAGE_UNDERLINING_CTRL = 10009,
        ID_RICHTEXTFONTPAGE_COLOURCTRL_LABEL = 10010,
        ID_RICHTEXTFONTPAGE_COLOURCTRL = 10011,
        ID_RICHTEXTFONTPAGE_BGCOLOURCTRL_LABEL = 10012,
        ID_RICHTEXTFONTPAGE_BGCOLOURCTRL = 10013,
        ID_RICHTEXTFONTPAGE_STRIKETHROUGHCTRL = 10014,
        ID_RICHTEXTFONTPAGE_CAPSCTRL = 10015,
        ID_RICHTEXTFONTPAGE_SMALLCAPSCTRL = 10016,
        ID_RICHTEXTFONTPAGE_SUPERSCRIPT = 10017,
        ID_RICHTEXTFONTPAGE_SUBSCRIPT = 10018,
        ID_RICHTEXTFONTPAGE_RIGHT_TO_LEFT = 10020,
        ID_RICHTEXTFONTPAGE_SUBSCRIPT_SUPPRESS_HYPHENATION = 10021,
        ID_RICHTEXTFONTPAGE_PREVIEWCTRL = 10019
    };
////@end wxRichTextFontPage member variables

    bool m_dontUpdate;
    bool m_colourPresent;
    bool m_bgColourPresent;
    static int sm_allowedTextEffects;
};

#endif
    // _RICHTEXTFONTPAGE_H_
