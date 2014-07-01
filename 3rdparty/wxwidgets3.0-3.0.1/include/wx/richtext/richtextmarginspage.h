/////////////////////////////////////////////////////////////////////////////
// Name:        wx/richtext/richtextmarginspage.h
// Purpose:     
// Author:      Julian Smart
// Modified by: 
// Created:     20/10/2010 10:27:34
// RCS-ID:      
// Copyright:   (c) Julian Smart
// Licence:     
/////////////////////////////////////////////////////////////////////////////

#ifndef _RICHTEXTMARGINSPAGE_H_
#define _RICHTEXTMARGINSPAGE_H_


/*!
 * Includes
 */

#include "wx/richtext/richtextdialogpage.h"

////@begin includes
#include "wx/statline.h"
////@end includes

/*!
 * Forward declarations
 */

////@begin forward declarations
////@end forward declarations

/*!
 * Control identifiers
 */

////@begin control identifiers
#define SYMBOL_WXRICHTEXTMARGINSPAGE_STYLE wxTAB_TRAVERSAL
#define SYMBOL_WXRICHTEXTMARGINSPAGE_TITLE wxEmptyString
#define SYMBOL_WXRICHTEXTMARGINSPAGE_IDNAME ID_WXRICHTEXTMARGINSPAGE
#define SYMBOL_WXRICHTEXTMARGINSPAGE_SIZE wxSize(400, 300)
#define SYMBOL_WXRICHTEXTMARGINSPAGE_POSITION wxDefaultPosition
////@end control identifiers


/*!
 * wxRichTextMarginsPage class declaration
 */

class WXDLLIMPEXP_RICHTEXT wxRichTextMarginsPage: public wxRichTextDialogPage
{    
    DECLARE_DYNAMIC_CLASS( wxRichTextMarginsPage )
    DECLARE_EVENT_TABLE()
    DECLARE_HELP_PROVISION()

public:
    /// Constructors
    wxRichTextMarginsPage();
    wxRichTextMarginsPage( wxWindow* parent, wxWindowID id = SYMBOL_WXRICHTEXTMARGINSPAGE_IDNAME, const wxPoint& pos = SYMBOL_WXRICHTEXTMARGINSPAGE_POSITION, const wxSize& size = SYMBOL_WXRICHTEXTMARGINSPAGE_SIZE, long style = SYMBOL_WXRICHTEXTMARGINSPAGE_STYLE );

    /// Creation
    bool Create( wxWindow* parent, wxWindowID id = SYMBOL_WXRICHTEXTMARGINSPAGE_IDNAME, const wxPoint& pos = SYMBOL_WXRICHTEXTMARGINSPAGE_POSITION, const wxSize& size = SYMBOL_WXRICHTEXTMARGINSPAGE_SIZE, long style = SYMBOL_WXRICHTEXTMARGINSPAGE_STYLE );

    /// Destructor
    ~wxRichTextMarginsPage();

    /// Initialises member variables
    void Init();

    /// Creates the controls and sizers
    void CreateControls();

    /// Gets the attributes from the formatting dialog
    wxRichTextAttr* GetAttributes();
    
    /// Data transfer
    virtual bool TransferDataToWindow();
    virtual bool TransferDataFromWindow();
    
////@begin wxRichTextMarginsPage event handler declarations

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_LEFT_MARGIN
    void OnRichtextLeftMarginUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_RIGHT_MARGIN
    void OnRichtextRightMarginUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_TOP_MARGIN
    void OnRichtextTopMarginUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_BOTTOM_MARGIN
    void OnRichtextBottomMarginUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_LEFT_PADDING
    void OnRichtextLeftPaddingUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_RIGHT_PADDING
    void OnRichtextRightPaddingUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_TOP_PADDING
    void OnRichtextTopPaddingUpdate( wxUpdateUIEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXT_BOTTOM_PADDING
    void OnRichtextBottomPaddingUpdate( wxUpdateUIEvent& event );

////@end wxRichTextMarginsPage event handler declarations

////@begin wxRichTextMarginsPage member function declarations

    /// Retrieves bitmap resources
    wxBitmap GetBitmapResource( const wxString& name );

    /// Retrieves icon resources
    wxIcon GetIconResource( const wxString& name );
////@end wxRichTextMarginsPage member function declarations

    /// Should we show tooltips?
    static bool ShowToolTips();

////@begin wxRichTextMarginsPage member variables
    wxCheckBox* m_leftMarginCheckbox;
    wxTextCtrl* m_marginLeft;
    wxComboBox* m_unitsMarginLeft;
    wxCheckBox* m_rightMarginCheckbox;
    wxTextCtrl* m_marginRight;
    wxComboBox* m_unitsMarginRight;
    wxCheckBox* m_topMarginCheckbox;
    wxTextCtrl* m_marginTop;
    wxComboBox* m_unitsMarginTop;
    wxCheckBox* m_bottomMarginCheckbox;
    wxTextCtrl* m_marginBottom;
    wxComboBox* m_unitsMarginBottom;
    wxCheckBox* m_leftPaddingCheckbox;
    wxTextCtrl* m_paddingLeft;
    wxComboBox* m_unitsPaddingLeft;
    wxCheckBox* m_rightPaddingCheckbox;
    wxTextCtrl* m_paddingRight;
    wxComboBox* m_unitsPaddingRight;
    wxCheckBox* m_topPaddingCheckbox;
    wxTextCtrl* m_paddingTop;
    wxComboBox* m_unitsPaddingTop;
    wxCheckBox* m_bottomPaddingCheckbox;
    wxTextCtrl* m_paddingBottom;
    wxComboBox* m_unitsPaddingBottom;
    /// Control identifiers
    enum {
        ID_WXRICHTEXTMARGINSPAGE = 10750,
        ID_RICHTEXT_LEFT_MARGIN_CHECKBOX = 10751,
        ID_RICHTEXT_LEFT_MARGIN = 10752,
        ID_RICHTEXT_LEFT_MARGIN_UNITS = 10753,
        ID_RICHTEXT_RIGHT_MARGIN_CHECKBOX = 10754,
        ID_RICHTEXT_RIGHT_MARGIN = 10755,
        ID_RICHTEXT_RIGHT_MARGIN_UNITS = 10756,
        ID_RICHTEXT_TOP_MARGIN_CHECKBOX = 10757,
        ID_RICHTEXT_TOP_MARGIN = 10758,
        ID_RICHTEXT_TOP_MARGIN_UNITS = 10759,
        ID_RICHTEXT_BOTTOM_MARGIN_CHECKBOX = 10760,
        ID_RICHTEXT_BOTTOM_MARGIN = 10761,
        ID_RICHTEXT_BOTTOM_MARGIN_UNITS = 10762,
        ID_RICHTEXT_LEFT_PADDING_CHECKBOX = 10763,
        ID_RICHTEXT_LEFT_PADDING = 10764,
        ID_RICHTEXT_LEFT_PADDING_UNITS = 10765,
        ID_RICHTEXT_RIGHT_PADDING_CHECKBOX = 10766,
        ID_RICHTEXT_RIGHT_PADDING = 10767,
        ID_RICHTEXT_RIGHT_PADDING_UNITS = 10768,
        ID_RICHTEXT_TOP_PADDING_CHECKBOX = 10769,
        ID_RICHTEXT_TOP_PADDING = 10770,
        ID_RICHTEXT_TOP_PADDING_UNITS = 10771,
        ID_RICHTEXT_BOTTOM_PADDING_CHECKBOX = 10772,
        ID_RICHTEXT_BOTTOM_PADDING = 10773,
        ID_RICHTEXT_BOTTOM_PADDING_UNITS = 10774
    };
////@end wxRichTextMarginsPage member variables

    bool    m_ignoreUpdates;
};

#endif
    // _RICHTEXTMARGINSPAGE_H_
