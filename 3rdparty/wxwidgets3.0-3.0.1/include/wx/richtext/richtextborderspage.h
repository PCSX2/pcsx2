/////////////////////////////////////////////////////////////////////////////
// Name:        wx/richtext/richtextborderspage.h
// Purpose:     A border editing page for the wxRTC formatting dialog.
// Author:      Julian Smart
// Modified by:
// Created:     21/10/2010 11:34:24
// RCS-ID:
// Copyright:   (c) Julian Smart
// Licence:
/////////////////////////////////////////////////////////////////////////////

#ifndef _RICHTEXTBORDERSPAGE_H_
#define _RICHTEXTBORDERSPAGE_H_

/*!
 * Includes
 */

#include "wx/richtext/richtextdialogpage.h"

////@begin includes
#include "wx/notebook.h"
#include "wx/statline.h"
////@end includes

/*!
 * Forward declarations
 */

class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextColourSwatchCtrl;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextBorderPreviewCtrl;

/*!
 * Control identifiers
 */

////@begin control identifiers
#define SYMBOL_WXRICHTEXTBORDERSPAGE_STYLE wxTAB_TRAVERSAL
#define SYMBOL_WXRICHTEXTBORDERSPAGE_TITLE wxEmptyString
#define SYMBOL_WXRICHTEXTBORDERSPAGE_IDNAME ID_RICHTEXTBORDERSPAGE
#define SYMBOL_WXRICHTEXTBORDERSPAGE_SIZE wxSize(400, 300)
#define SYMBOL_WXRICHTEXTBORDERSPAGE_POSITION wxDefaultPosition
////@end control identifiers


/*!
 * wxRichTextBordersPage class declaration
 */

class WXDLLIMPEXP_RICHTEXT wxRichTextBordersPage: public wxRichTextDialogPage
{
    DECLARE_DYNAMIC_CLASS( wxRichTextBordersPage )
    DECLARE_EVENT_TABLE()
    DECLARE_HELP_PROVISION()

public:
    /// Constructors
    wxRichTextBordersPage();
    wxRichTextBordersPage( wxWindow* parent, wxWindowID id = SYMBOL_WXRICHTEXTBORDERSPAGE_IDNAME, const wxPoint& pos = SYMBOL_WXRICHTEXTBORDERSPAGE_POSITION, const wxSize& size = SYMBOL_WXRICHTEXTBORDERSPAGE_SIZE, long style = SYMBOL_WXRICHTEXTBORDERSPAGE_STYLE );

    /// Creation
    bool Create( wxWindow* parent, wxWindowID id = SYMBOL_WXRICHTEXTBORDERSPAGE_IDNAME, const wxPoint& pos = SYMBOL_WXRICHTEXTBORDERSPAGE_POSITION, const wxSize& size = SYMBOL_WXRICHTEXTBORDERSPAGE_SIZE, long style = SYMBOL_WXRICHTEXTBORDERSPAGE_STYLE );

    /// Destructor
    ~wxRichTextBordersPage();

    /// Initialises member variables
    void Init();

    /// Creates the controls and sizers
    void CreateControls();

    /// Gets the attributes from the formatting dialog
    wxRichTextAttr* GetAttributes();

    /// Data transfer
    virtual bool TransferDataToWindow();
    virtual bool TransferDataFromWindow();

    /// Updates the synchronization checkboxes to reflect the state of the attributes
    void UpdateSyncControls();

    /// Updates the preview
    void OnCommand(wxCommandEvent& event);

    /// Fill style combo
    virtual void FillStyleComboBox(wxComboBox* styleComboBox);

    /// Set the border controls
    static void SetBorderValue(wxTextAttrBorder& border, wxTextCtrl* widthValueCtrl, wxComboBox* widthUnitsCtrl, wxCheckBox* checkBox,
        wxComboBox* styleCtrl, wxRichTextColourSwatchCtrl* colourCtrl, const wxArrayInt& borderStyles);

    /// Get data from the border controls
    static void GetBorderValue(wxTextAttrBorder& border, wxTextCtrl* widthValueCtrl, wxComboBox* widthUnitsCtrl, wxCheckBox* checkBox,
        wxComboBox* styleCtrl, wxRichTextColourSwatchCtrl* colourCtrl, const wxArrayInt& borderStyles);

////@begin wxRichTextBordersPage event handler declarations

    /// wxEVT_COMMAND_CHECKBOX_CLICKED event handler for ID_RICHTEXT_BORDER_LEFT_CHECKBOX
    void OnRichtextBorderCheckboxClick( wxCommandEvent& event );

    /// wxEVT_COMMAND_TEXT_UPDATED event handler for ID_RICHTEXT_BORDER_LEFT
    void OnRichtextBorderLeftValueTextUpdated( wxCommandEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_BORDER_LEFT
    void OnRichtextBorderLeftUpdate( wxUpdateUIEvent& event );

    /// wxEVT_COMMAND_COMBOBOX_SELECTED event handler for ID_RICHTEXT_BORDER_LEFT_UNITS
    void OnRichtextBorderLeftUnitsSelected( wxCommandEvent& event );

    /// wxEVT_COMMAND_COMBOBOX_SELECTED event handler for ID_RICHTEXT_BORDER_LEFT_STYLE
    void OnRichtextBorderLeftStyleSelected( wxCommandEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_BORDER_RIGHT_CHECKBOX
    void OnRichtextBorderOtherCheckboxUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_BORDER_RIGHT
    void OnRichtextBorderRightUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_BORDER_TOP
    void OnRichtextBorderTopUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_BORDER_BOTTOM
    void OnRichtextBorderBottomUpdate( wxUpdateUIEvent& event );

    /// wxEVT_COMMAND_CHECKBOX_CLICKED event handler for ID_RICHTEXT_BORDER_SYNCHRONIZE
    void OnRichtextBorderSynchronizeClick( wxCommandEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_BORDER_SYNCHRONIZE
    void OnRichtextBorderSynchronizeUpdate( wxUpdateUIEvent& event );

    /// wxEVT_COMMAND_TEXT_UPDATED event handler for ID_RICHTEXT_OUTLINE_LEFT
    void OnRichtextOutlineLeftTextUpdated( wxCommandEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_OUTLINE_LEFT
    void OnRichtextOutlineLeftUpdate( wxUpdateUIEvent& event );

    /// wxEVT_COMMAND_COMBOBOX_SELECTED event handler for ID_RICHTEXT_OUTLINE_LEFT_UNITS
    void OnRichtextOutlineLeftUnitsSelected( wxCommandEvent& event );

    /// wxEVT_COMMAND_COMBOBOX_SELECTED event handler for ID_RICHTEXT_OUTLINE_LEFT_STYLE
    void OnRichtextOutlineLeftStyleSelected( wxCommandEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_OUTLINE_RIGHT_CHECKBOX
    void OnRichtextOutlineOtherCheckboxUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_OUTLINE_RIGHT
    void OnRichtextOutlineRightUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_OUTLINE_TOP
    void OnRichtextOutlineTopUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_OUTLINE_BOTTOM
    void OnRichtextOutlineBottomUpdate( wxUpdateUIEvent& event );

    /// wxEVT_COMMAND_CHECKBOX_CLICKED event handler for ID_RICHTEXT_OUTLINE_SYNCHRONIZE
    void OnRichtextOutlineSynchronizeClick( wxCommandEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_OUTLINE_SYNCHRONIZE
    void OnRichtextOutlineSynchronizeUpdate( wxUpdateUIEvent& event );

////@end wxRichTextBordersPage event handler declarations

////@begin wxRichTextBordersPage member function declarations

    /// Retrieves bitmap resources
    wxBitmap GetBitmapResource( const wxString& name );

    /// Retrieves icon resources
    wxIcon GetIconResource( const wxString& name );
////@end wxRichTextBordersPage member function declarations

    /// Should we show tooltips?
    static bool ShowToolTips();

////@begin wxRichTextBordersPage member variables
    wxCheckBox* m_leftBorderCheckbox;
    wxTextCtrl* m_leftBorderWidth;
    wxComboBox* m_leftBorderWidthUnits;
    wxComboBox* m_leftBorderStyle;
    wxRichTextColourSwatchCtrl* m_leftBorderColour;
    wxCheckBox* m_rightBorderCheckbox;
    wxTextCtrl* m_rightBorderWidth;
    wxComboBox* m_rightBorderWidthUnits;
    wxComboBox* m_rightBorderStyle;
    wxRichTextColourSwatchCtrl* m_rightBorderColour;
    wxCheckBox* m_topBorderCheckbox;
    wxTextCtrl* m_topBorderWidth;
    wxComboBox* m_topBorderWidthUnits;
    wxComboBox* m_topBorderStyle;
    wxRichTextColourSwatchCtrl* m_topBorderColour;
    wxCheckBox* m_bottomBorderCheckbox;
    wxTextCtrl* m_bottomBorderWidth;
    wxComboBox* m_bottomBorderWidthUnits;
    wxComboBox* m_bottomBorderStyle;
    wxRichTextColourSwatchCtrl* m_bottomBorderColour;
    wxCheckBox* m_borderSyncCtrl;
    wxCheckBox* m_leftOutlineCheckbox;
    wxTextCtrl* m_leftOutlineWidth;
    wxComboBox* m_leftOutlineWidthUnits;
    wxComboBox* m_leftOutlineStyle;
    wxRichTextColourSwatchCtrl* m_leftOutlineColour;
    wxCheckBox* m_rightOutlineCheckbox;
    wxTextCtrl* m_rightOutlineWidth;
    wxComboBox* m_rightOutlineWidthUnits;
    wxComboBox* m_rightOutlineStyle;
    wxRichTextColourSwatchCtrl* m_rightOutlineColour;
    wxCheckBox* m_topOutlineCheckbox;
    wxTextCtrl* m_topOutlineWidth;
    wxComboBox* m_topOutlineWidthUnits;
    wxComboBox* m_topOutlineStyle;
    wxRichTextColourSwatchCtrl* m_topOutlineColour;
    wxCheckBox* m_bottomOutlineCheckbox;
    wxTextCtrl* m_bottomOutlineWidth;
    wxComboBox* m_bottomOutlineWidthUnits;
    wxComboBox* m_bottomOutlineStyle;
    wxRichTextColourSwatchCtrl* m_bottomOutlineColour;
    wxCheckBox* m_outlineSyncCtrl;
    wxRichTextBorderPreviewCtrl* m_borderPreviewCtrl;
    /// Control identifiers
    enum {
        ID_RICHTEXTBORDERSPAGE = 10800,
        ID_RICHTEXTBORDERSPAGE_NOTEBOOK = 10801,
        ID_RICHTEXTBORDERSPAGE_BORDERS = 10802,
        ID_RICHTEXT_BORDER_LEFT_CHECKBOX = 10803,
        ID_RICHTEXT_BORDER_LEFT = 10804,
        ID_RICHTEXT_BORDER_LEFT_UNITS = 10805,
        ID_RICHTEXT_BORDER_LEFT_STYLE = 10806,
        ID_RICHTEXT_BORDER_LEFT_COLOUR = 10807,
        ID_RICHTEXT_BORDER_RIGHT_CHECKBOX = 10808,
        ID_RICHTEXT_BORDER_RIGHT = 10809,
        ID_RICHTEXT_BORDER_RIGHT_UNITS = 10810,
        ID_RICHTEXT_BORDER_RIGHT_STYLE = 10811,
        ID_RICHTEXT_BORDER_RIGHT_COLOUR = 10812,
        ID_RICHTEXT_BORDER_TOP_CHECKBOX = 10813,
        ID_RICHTEXT_BORDER_TOP = 10814,
        ID_RICHTEXT_BORDER_TOP_UNITS = 10815,
        ID_RICHTEXT_BORDER_TOP_STYLE = 10816,
        ID_RICHTEXT_BORDER_TOP_COLOUR = 10817,
        ID_RICHTEXT_BORDER_BOTTOM_CHECKBOX = 10818,
        ID_RICHTEXT_BORDER_BOTTOM = 10819,
        ID_RICHTEXT_BORDER_BOTTOM_UNITS = 10820,
        ID_RICHTEXT_BORDER_BOTTOM_STYLE = 10821,
        ID_RICHTEXT_BORDER_BOTTOM_COLOUR = 10822,
        ID_RICHTEXT_BORDER_SYNCHRONIZE = 10845,
        ID_RICHTEXTBORDERSPAGE_OUTLINE = 10823,
        ID_RICHTEXT_OUTLINE_LEFT_CHECKBOX = 10824,
        ID_RICHTEXT_OUTLINE_LEFT = 10825,
        ID_RICHTEXT_OUTLINE_LEFT_UNITS = 10826,
        ID_RICHTEXT_OUTLINE_LEFT_STYLE = 10827,
        ID_RICHTEXT_OUTLINE_LEFT_COLOUR = 10828,
        ID_RICHTEXT_OUTLINE_RIGHT_CHECKBOX = 10829,
        ID_RICHTEXT_OUTLINE_RIGHT = 10830,
        ID_RICHTEXT_OUTLINE_RIGHT_UNITS = 10831,
        ID_RICHTEXT_OUTLINE_RIGHT_STYLE = 10832,
        ID_RICHTEXT_OUTLINE_RIGHT_COLOUR = 10833,
        ID_RICHTEXT_OUTLINE_TOP_CHECKBOX = 10834,
        ID_RICHTEXT_OUTLINE_TOP = 10835,
        ID_RICHTEXT_OUTLINE_TOP_UNITS = 10836,
        ID_RICHTEXT_OUTLINE_TOP_STYLE = 10837,
        ID_RICHTEXT_OUTLINE_TOP_COLOUR = 10838,
        ID_RICHTEXT_OUTLINE_BOTTOM_CHECKBOX = 10839,
        ID_RICHTEXT_OUTLINE_BOTTOM = 10840,
        ID_RICHTEXT_OUTLINE_BOTTOM_UNITS = 10841,
        ID_RICHTEXT_OUTLINE_BOTTOM_STYLE = 10842,
        ID_RICHTEXT_OUTLINE_BOTTOM_COLOUR = 10843,
        ID_RICHTEXT_OUTLINE_SYNCHRONIZE = 10846,
        ID_RICHTEXT_BORDER_PREVIEW = 10844
    };
////@end wxRichTextBordersPage member variables

    wxArrayInt m_borderStyles;
    wxArrayString m_borderStyleNames;
    bool m_ignoreUpdates;
};

class WXDLLIMPEXP_RICHTEXT wxRichTextBorderPreviewCtrl : public wxWindow
{
public:
    wxRichTextBorderPreviewCtrl(wxWindow *parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& sz = wxDefaultSize, long style = 0);

    void SetAttributes(wxRichTextAttr* attr) { m_attributes = attr; }
    wxRichTextAttr* GetAttributes() const { return m_attributes; }

private:
    wxRichTextAttr* m_attributes;

    void OnPaint(wxPaintEvent& event);
    DECLARE_EVENT_TABLE()
};

#endif
    // _RICHTEXTBORDERSPAGE_H_
