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
class wxSpinButton;
class wxBoxSizer;
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
    DECLARE_DYNAMIC_CLASS( wxRichTextFontPage )
    DECLARE_EVENT_TABLE()
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

////@begin wxRichTextFontPage event handler declarations

    /// wxEVT_IDLE event handler for ID_RICHTEXTFONTPAGE
    void OnIdle( wxIdleEvent& event );

    /// wxEVT_TEXT event handler for ID_RICHTEXTFONTPAGE_FACETEXTCTRL
    void OnFaceTextCtrlUpdated( wxCommandEvent& event );

    /// wxEVT_TEXT event handler for ID_RICHTEXTFONTPAGE_SIZETEXTCTRL
    void OnSizeTextCtrlUpdated( wxCommandEvent& event );

    /// wxEVT_SCROLL_LINEUP event handler for ID_RICHTEXTFONTPAGE_SPINBUTTONS
    void OnRichtextfontpageSpinbuttonsUp( wxSpinEvent& event );

    /// wxEVT_SCROLL_LINEDOWN event handler for ID_RICHTEXTFONTPAGE_SPINBUTTONS
    void OnRichtextfontpageSpinbuttonsDown( wxSpinEvent& event );

    /// wxEVT_CHOICE event handler for ID_RICHTEXTFONTPAGE_SIZE_UNITS
    void OnRichtextfontpageSizeUnitsSelected( wxCommandEvent& event );

    /// wxEVT_LISTBOX event handler for ID_RICHTEXTFONTPAGE_SIZELISTBOX
    void OnSizeListBoxSelected( wxCommandEvent& event );

    /// wxEVT_COMBOBOX event handler for ID_RICHTEXTFONTPAGE_STYLECTRL
    void OnStyleCtrlSelected( wxCommandEvent& event );

    /// wxEVT_COMBOBOX event handler for ID_RICHTEXTFONTPAGE_WEIGHTCTRL
    void OnWeightCtrlSelected( wxCommandEvent& event );

    /// wxEVT_COMBOBOX event handler for ID_RICHTEXTFONTPAGE_UNDERLINING_CTRL
    void OnUnderliningCtrlSelected( wxCommandEvent& event );

    /// wxEVT_CHECKBOX event handler for ID_RICHTEXTFONTPAGE_STRIKETHROUGHCTRL
    void OnStrikethroughctrlClick( wxCommandEvent& event );

    /// wxEVT_CHECKBOX event handler for ID_RICHTEXTFONTPAGE_CAPSCTRL
    void OnCapsctrlClick( wxCommandEvent& event );

    /// wxEVT_CHECKBOX event handler for ID_RICHTEXTFONTPAGE_SUPERSCRIPT
    void OnRichtextfontpageSuperscriptClick( wxCommandEvent& event );

    /// wxEVT_CHECKBOX event handler for ID_RICHTEXTFONTPAGE_SUBSCRIPT
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
        ID_RICHTEXTFONTPAGE_PREVIEWCTRL = 10019
    };
////@end wxRichTextFontPage member variables

    bool m_dontUpdate;
    bool m_colourPresent;
    bool m_bgColourPresent;
};

#endif
    // _RICHTEXTFONTPAGE_H_
