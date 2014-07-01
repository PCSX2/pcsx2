/////////////////////////////////////////////////////////////////////////////
// Name:        wx/richtext/richtextsizepage.h
// Purpose:
// Author:      Julian Smart
// Modified by:
// Created:     20/10/2010 10:23:24
// RCS-ID:
// Copyright:   (c) Julian Smart
// Licence:
/////////////////////////////////////////////////////////////////////////////

#ifndef _RICHTEXTSIZEPAGE_H_
#define _RICHTEXTSIZEPAGE_H_

/*!
 * Includes
 */

#include "wx/richtext/richtextdialogpage.h"
#include "wx/sizer.h"

////@begin includes
#include "wx/statline.h"
#include "wx/valgen.h"
////@end includes
#include "wx/stattext.h"

/*!
 * Forward declarations
 */


/*!
 * Control identifiers
 */

////@begin control identifiers
#define SYMBOL_WXRICHTEXTSIZEPAGE_STYLE wxTAB_TRAVERSAL
#define SYMBOL_WXRICHTEXTSIZEPAGE_TITLE wxEmptyString
#define SYMBOL_WXRICHTEXTSIZEPAGE_IDNAME ID_WXRICHTEXTSIZEPAGE
#define SYMBOL_WXRICHTEXTSIZEPAGE_SIZE wxSize(400, 300)
#define SYMBOL_WXRICHTEXTSIZEPAGE_POSITION wxDefaultPosition
////@end control identifiers


/*!
 * wxRichTextSizePage class declaration
 */

class WXDLLIMPEXP_RICHTEXT wxRichTextSizePage: public wxRichTextDialogPage
{
    DECLARE_DYNAMIC_CLASS( wxRichTextSizePage )
    DECLARE_EVENT_TABLE()
    DECLARE_HELP_PROVISION()

public:
    /// Constructors
    wxRichTextSizePage();
    wxRichTextSizePage( wxWindow* parent, wxWindowID id = SYMBOL_WXRICHTEXTSIZEPAGE_IDNAME, const wxPoint& pos = SYMBOL_WXRICHTEXTSIZEPAGE_POSITION, const wxSize& size = SYMBOL_WXRICHTEXTSIZEPAGE_SIZE, long style = SYMBOL_WXRICHTEXTSIZEPAGE_STYLE );

    /// Creation
    bool Create( wxWindow* parent, wxWindowID id = SYMBOL_WXRICHTEXTSIZEPAGE_IDNAME, const wxPoint& pos = SYMBOL_WXRICHTEXTSIZEPAGE_POSITION, const wxSize& size = SYMBOL_WXRICHTEXTSIZEPAGE_SIZE, long style = SYMBOL_WXRICHTEXTSIZEPAGE_STYLE );

    /// Destructor
    ~wxRichTextSizePage();

    /// Initialises member variables
    void Init();

    /// Creates the controls and sizers
    void CreateControls();

    /// Gets the attributes from the formatting dialog
    wxRichTextAttr* GetAttributes();

    /// Data transfer
    virtual bool TransferDataToWindow();
    virtual bool TransferDataFromWindow();

    /// Show/hide position controls
    static void ShowPositionControls(bool show) { sm_showPositionControls = show; }

    /// Show/hide minimum and maximum size controls
    static void ShowMinMaxSizeControls(bool show) { sm_showMinMaxSizeControls = show; }

    /// Show/hide position mode controls
    static void ShowPositionModeControls(bool show) { sm_showPositionModeControls = show; }

    /// Show/hide right/bottom position controls
    static void ShowRightBottomPositionControls(bool show) { sm_showRightBottomPositionControls = show; }

    /// Show/hide floating and alignment controls
    static void ShowFloatingAndAlignmentControls(bool show) { sm_showFloatingAndAlignmentControls = show; }

    /// Show/hide floating controls
    static void ShowFloatingControls(bool show) { sm_showFloatingControls = show; }

    /// Show/hide alignment controls
    static void ShowAlignmentControls(bool show) { sm_showAlignmentControls = show; }

    /// Enable the position and size units
    static void EnablePositionAndSizeUnits(bool enable) { sm_enablePositionAndSizeUnits = enable; }

    /// Enable the checkboxes for position and size
    static void EnablePositionAndSizeCheckboxes(bool enable) { sm_enablePositionAndSizeCheckboxes = enable; }

////@begin wxRichTextSizePage event handler declarations

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_VERTICAL_ALIGNMENT_COMBOBOX
    void OnRichtextVerticalAlignmentComboboxUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_WIDTH
    void OnRichtextWidthUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_UNITS_W
    void OnRichtextWidthUnitsUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_HEIGHT
    void OnRichtextHeightUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_UNITS_H
    void OnRichtextHeightUnitsUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_MIN_WIDTH
    void OnRichtextMinWidthUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_MIN_HEIGHT
    void OnRichtextMinHeightUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_MAX_WIDTH
    void OnRichtextMaxWidthUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_MAX_HEIGHT
    void OnRichtextMaxHeightUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_LEFT
    void OnRichtextLeftUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_LEFT_UNITS
    void OnRichtextLeftUnitsUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_TOP
    void OnRichtextTopUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_TOP_UNITS
    void OnRichtextTopUnitsUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_RIGHT
    void OnRichtextRightUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_RIGHT_UNITS
    void OnRichtextRightUnitsUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_BOTTOM
    void OnRichtextBottomUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_BOTTOM_UNITS
    void OnRichtextBottomUnitsUpdate( wxUpdateUIEvent& event );

    /// wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_RICHTEXT_PARA_UP
    void OnRichtextParaUpClick( wxCommandEvent& event );

    /// wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_RICHTEXT_PARA_DOWN
    void OnRichtextParaDownClick( wxCommandEvent& event );

////@end wxRichTextSizePage event handler declarations

////@begin wxRichTextSizePage member function declarations

    int GetPositionMode() const { return m_positionMode ; }
    void SetPositionMode(int value) { m_positionMode = value ; }

    /// Retrieves bitmap resources
    wxBitmap GetBitmapResource( const wxString& name );

    /// Retrieves icon resources
    wxIcon GetIconResource( const wxString& name );
////@end wxRichTextSizePage member function declarations

    /// Should we show tooltips?
    static bool ShowToolTips();

////@begin wxRichTextSizePage member variables
    wxBoxSizer* m_parentSizer;
    wxBoxSizer* m_floatingAlignmentSizer;
    wxBoxSizer* m_floatingSizer;
    wxChoice* m_float;
    wxBoxSizer* m_alignmentSizer;
    wxCheckBox* m_verticalAlignmentCheckbox;
    wxChoice* m_verticalAlignmentComboBox;
    wxFlexGridSizer* m_sizeSizer;
    wxBoxSizer* m_widthSizer;
    wxCheckBox* m_widthCheckbox;
    wxStaticText* m_widthLabel;
    wxTextCtrl* m_width;
    wxComboBox* m_unitsW;
    wxBoxSizer* m_heightSizer;
    wxCheckBox* m_heightCheckbox;
    wxStaticText* m_heightLabel;
    wxTextCtrl* m_height;
    wxComboBox* m_unitsH;
    wxCheckBox* m_minWidthCheckbox;
    wxBoxSizer* m_minWidthSizer;
    wxTextCtrl* m_minWidth;
    wxComboBox* m_unitsMinW;
    wxCheckBox* m_minHeightCheckbox;
    wxBoxSizer* m_minHeightSizer;
    wxTextCtrl* m_minHeight;
    wxComboBox* m_unitsMinH;
    wxCheckBox* m_maxWidthCheckbox;
    wxBoxSizer* m_maxWidthSizer;
    wxTextCtrl* m_maxWidth;
    wxComboBox* m_unitsMaxW;
    wxCheckBox* m_maxHeightCheckbox;
    wxBoxSizer* m_maxHeightSizer;
    wxTextCtrl* m_maxHeight;
    wxComboBox* m_unitsMaxH;
    wxBoxSizer* m_positionControls;
    wxBoxSizer* m_moveObjectParentSizer;
    wxBoxSizer* m_positionModeSizer;
    wxChoice* m_positionModeCtrl;
    wxFlexGridSizer* m_positionGridSizer;
    wxBoxSizer* m_leftSizer;
    wxCheckBox* m_positionLeftCheckbox;
    wxStaticText* m_leftLabel;
    wxTextCtrl* m_left;
    wxComboBox* m_unitsLeft;
    wxBoxSizer* m_topSizer;
    wxCheckBox* m_positionTopCheckbox;
    wxStaticText* m_topLabel;
    wxTextCtrl* m_top;
    wxComboBox* m_unitsTop;
    wxBoxSizer* m_rightSizer;
    wxCheckBox* m_positionRightCheckbox;
    wxStaticText* m_rightLabel;
    wxBoxSizer* m_rightPositionSizer;
    wxTextCtrl* m_right;
    wxComboBox* m_unitsRight;
    wxBoxSizer* m_bottomSizer;
    wxCheckBox* m_positionBottomCheckbox;
    wxStaticText* m_bottomLabel;
    wxBoxSizer* m_bottomPositionSizer;
    wxTextCtrl* m_bottom;
    wxComboBox* m_unitsBottom;
    wxBoxSizer* m_moveObjectSizer;
    int m_positionMode;
    /// Control identifiers
    enum {
        ID_WXRICHTEXTSIZEPAGE = 10700,
        ID_RICHTEXT_FLOATING_MODE = 10701,
        ID_RICHTEXT_VERTICAL_ALIGNMENT_CHECKBOX = 10708,
        ID_RICHTEXT_VERTICAL_ALIGNMENT_COMBOBOX = 10709,
        ID_RICHTEXT_WIDTH_CHECKBOX = 10702,
        ID_RICHTEXT_WIDTH = 10703,
        ID_RICHTEXT_UNITS_W = 10704,
        ID_RICHTEXT_HEIGHT_CHECKBOX = 10705,
        ID_RICHTEXT_HEIGHT = 10706,
        ID_RICHTEXT_UNITS_H = 10707,
        ID_RICHTEXT_MIN_WIDTH_CHECKBOX = 10715,
        ID_RICHTEXT_MIN_WIDTH = 10716,
        ID_RICHTEXT_UNITS_MIN_W = 10717,
        ID_RICHTEXT_MIN_HEIGHT_CHECKBOX = 10718,
        ID_RICHTEXT_MIN_HEIGHT = 10719,
        ID_RICHTEXT_UNITS_MIN_H = 10720,
        ID_RICHTEXT_MAX_WIDTH_CHECKBOX = 10721,
        ID_RICHTEXT_MAX_WIDTH = 10722,
        ID_RICHTEXT_UNITS_MAX_W = 10723,
        ID_RICHTEXT_MAX_HEIGHT_CHECKBOX = 10724,
        ID_RICHTEXT_MAX_HEIGHT = 10725,
        ID_RICHTEXT_UNITS_MAX_H = 10726,
        ID_RICHTEXT_POSITION_MODE = 10735,
        ID_RICHTEXT_LEFT_CHECKBOX = 10710,
        ID_RICHTEXT_LEFT = 10711,
        ID_RICHTEXT_LEFT_UNITS = 10712,
        ID_RICHTEXT_TOP_CHECKBOX = 10710,
        ID_RICHTEXT_TOP = 10728,
        ID_RICHTEXT_TOP_UNITS = 10729,
        ID_RICHTEXT_RIGHT_CHECKBOX = 10727,
        ID_RICHTEXT_RIGHT = 10730,
        ID_RICHTEXT_RIGHT_UNITS = 10731,
        ID_RICHTEXT_BOTTOM_CHECKBOX = 10732,
        ID_RICHTEXT_BOTTOM = 10733,
        ID_RICHTEXT_BOTTOM_UNITS = 10734,
        ID_RICHTEXT_PARA_UP = 10713,
        ID_RICHTEXT_PARA_DOWN = 10714
    };
////@end wxRichTextSizePage member variables

    static bool sm_showFloatingControls;
    static bool sm_showPositionControls;
    static bool sm_showMinMaxSizeControls;
    static bool sm_showPositionModeControls;
    static bool sm_showRightBottomPositionControls;
    static bool sm_showAlignmentControls;
    static bool sm_showFloatingAndAlignmentControls;
    static bool sm_enablePositionAndSizeUnits;
    static bool sm_enablePositionAndSizeCheckboxes;
};

#endif
    // _RICHTEXTSIZEPAGE_H_
