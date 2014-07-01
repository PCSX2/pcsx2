/////////////////////////////////////////////////////////////////////////////
// Name:        wx/richtext/richtexttabspage.h
// Purpose:
// Author:      Julian Smart
// Modified by:
// Created:     10/4/2006 8:03:20 AM
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _RICHTEXTTABSPAGE_H_
#define _RICHTEXTTABSPAGE_H_

/*!
 * Includes
 */

#include "wx/richtext/richtextdialogpage.h"

////@begin includes
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
#define SYMBOL_WXRICHTEXTTABSPAGE_STYLE wxRESIZE_BORDER|wxTAB_TRAVERSAL
#define SYMBOL_WXRICHTEXTTABSPAGE_TITLE wxEmptyString
#define SYMBOL_WXRICHTEXTTABSPAGE_IDNAME ID_RICHTEXTTABSPAGE
#define SYMBOL_WXRICHTEXTTABSPAGE_SIZE wxSize(400, 300)
#define SYMBOL_WXRICHTEXTTABSPAGE_POSITION wxDefaultPosition
////@end control identifiers

/*!
 * wxRichTextTabsPage class declaration
 */

class WXDLLIMPEXP_RICHTEXT wxRichTextTabsPage: public wxRichTextDialogPage
{
    DECLARE_DYNAMIC_CLASS( wxRichTextTabsPage )
    DECLARE_EVENT_TABLE()
    DECLARE_HELP_PROVISION()

public:
    /// Constructors
    wxRichTextTabsPage( );
    wxRichTextTabsPage( wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = SYMBOL_WXRICHTEXTTABSPAGE_POSITION, const wxSize& size = SYMBOL_WXRICHTEXTTABSPAGE_SIZE, long style = SYMBOL_WXRICHTEXTTABSPAGE_STYLE );

    /// Creation
    bool Create( wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = SYMBOL_WXRICHTEXTTABSPAGE_POSITION, const wxSize& size = SYMBOL_WXRICHTEXTTABSPAGE_SIZE, long style = SYMBOL_WXRICHTEXTTABSPAGE_STYLE );

    /// Creates the controls and sizers
    void CreateControls();

    /// Initialise members
    void Init();

    /// Transfer data from/to window
    virtual bool TransferDataFromWindow();
    virtual bool TransferDataToWindow();

    /// Sorts the tab array
    virtual void SortTabs();

    /// Gets the attributes associated with the main formatting dialog
    wxRichTextAttr* GetAttributes();

////@begin wxRichTextTabsPage event handler declarations

    /// wxEVT_LISTBOX event handler for ID_RICHTEXTTABSPAGE_TABLIST
    void OnTablistSelected( wxCommandEvent& event );

    /// wxEVT_BUTTON event handler for ID_RICHTEXTTABSPAGE_NEW_TAB
    void OnNewTabClick( wxCommandEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXTTABSPAGE_NEW_TAB
    void OnNewTabUpdate( wxUpdateUIEvent& event );

    /// wxEVT_BUTTON event handler for ID_RICHTEXTTABSPAGE_DELETE_TAB
    void OnDeleteTabClick( wxCommandEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXTTABSPAGE_DELETE_TAB
    void OnDeleteTabUpdate( wxUpdateUIEvent& event );

    /// wxEVT_BUTTON event handler for ID_RICHTEXTTABSPAGE_DELETE_ALL_TABS
    void OnDeleteAllTabsClick( wxCommandEvent& event );

    /// wxEVT_UPDATE_UI event handler for ID_RICHTEXTTABSPAGE_DELETE_ALL_TABS
    void OnDeleteAllTabsUpdate( wxUpdateUIEvent& event );

////@end wxRichTextTabsPage event handler declarations

////@begin wxRichTextTabsPage member function declarations

    /// Retrieves bitmap resources
    wxBitmap GetBitmapResource( const wxString& name );

    /// Retrieves icon resources
    wxIcon GetIconResource( const wxString& name );
////@end wxRichTextTabsPage member function declarations

    /// Should we show tooltips?
    static bool ShowToolTips();

////@begin wxRichTextTabsPage member variables
    wxTextCtrl* m_tabEditCtrl;
    wxListBox* m_tabListCtrl;
    /// Control identifiers
    enum {
        ID_RICHTEXTTABSPAGE = 10200,
        ID_RICHTEXTTABSPAGE_TABEDIT = 10213,
        ID_RICHTEXTTABSPAGE_TABLIST = 10214,
        ID_RICHTEXTTABSPAGE_NEW_TAB = 10201,
        ID_RICHTEXTTABSPAGE_DELETE_TAB = 10202,
        ID_RICHTEXTTABSPAGE_DELETE_ALL_TABS = 10203
    };
////@end wxRichTextTabsPage member variables

    bool m_tabsPresent;
};

#endif
    // _RICHTEXTTABSPAGE_H_
