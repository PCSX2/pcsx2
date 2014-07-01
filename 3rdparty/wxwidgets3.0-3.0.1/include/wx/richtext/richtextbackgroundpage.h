/////////////////////////////////////////////////////////////////////////////
// Name:        wx/richtext/richtextbackgroundpage.h
// Purpose:
// Author:      Julian Smart
// Modified by:
// Created:     13/11/2010 11:17:25
// RCS-ID:
// Copyright:   (c) Julian Smart
// Licence:
/////////////////////////////////////////////////////////////////////////////

#ifndef _RICHTEXTBACKGROUNDPAGE_H_
#define _RICHTEXTBACKGROUNDPAGE_H_

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

class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextColourSwatchCtrl;

/*!
 * Control identifiers
 */

////@begin control identifiers
#define SYMBOL_WXRICHTEXTBACKGROUNDPAGE_STYLE wxTAB_TRAVERSAL
#define SYMBOL_WXRICHTEXTBACKGROUNDPAGE_TITLE wxEmptyString
#define SYMBOL_WXRICHTEXTBACKGROUNDPAGE_IDNAME ID_RICHTEXTBACKGROUNDPAGE
#define SYMBOL_WXRICHTEXTBACKGROUNDPAGE_SIZE wxSize(400, 300)
#define SYMBOL_WXRICHTEXTBACKGROUNDPAGE_POSITION wxDefaultPosition
////@end control identifiers


/*!
 * wxRichTextBackgroundPage class declaration
 */

class WXDLLIMPEXP_RICHTEXT wxRichTextBackgroundPage: public wxRichTextDialogPage
{
    DECLARE_DYNAMIC_CLASS( wxRichTextBackgroundPage )
    DECLARE_EVENT_TABLE()
    DECLARE_HELP_PROVISION()

public:
    /// Constructors
    wxRichTextBackgroundPage();
    wxRichTextBackgroundPage( wxWindow* parent, wxWindowID id = SYMBOL_WXRICHTEXTBACKGROUNDPAGE_IDNAME, const wxPoint& pos = SYMBOL_WXRICHTEXTBACKGROUNDPAGE_POSITION, const wxSize& size = SYMBOL_WXRICHTEXTBACKGROUNDPAGE_SIZE, long style = SYMBOL_WXRICHTEXTBACKGROUNDPAGE_STYLE );

    /// Creation
    bool Create( wxWindow* parent, wxWindowID id = SYMBOL_WXRICHTEXTBACKGROUNDPAGE_IDNAME, const wxPoint& pos = SYMBOL_WXRICHTEXTBACKGROUNDPAGE_POSITION, const wxSize& size = SYMBOL_WXRICHTEXTBACKGROUNDPAGE_SIZE, long style = SYMBOL_WXRICHTEXTBACKGROUNDPAGE_STYLE );

    /// Destructor
    ~wxRichTextBackgroundPage();

    /// Initialises member variables
    void Init();

    /// Creates the controls and sizers
    void CreateControls();

    /// Gets the attributes from the formatting dialog
    wxRichTextAttr* GetAttributes();

    /// Data transfer
    virtual bool TransferDataToWindow();
    virtual bool TransferDataFromWindow();

    /// Respond to colour swatch click
    void OnColourSwatch(wxCommandEvent& event);

////@begin wxRichTextBackgroundPage event handler declarations

////@end wxRichTextBackgroundPage event handler declarations

////@begin wxRichTextBackgroundPage member function declarations

    /// Retrieves bitmap resources
    wxBitmap GetBitmapResource( const wxString& name );

    /// Retrieves icon resources
    wxIcon GetIconResource( const wxString& name );
////@end wxRichTextBackgroundPage member function declarations

    /// Should we show tooltips?
    static bool ShowToolTips();

////@begin wxRichTextBackgroundPage member variables
    wxCheckBox* m_backgroundColourCheckBox;
    wxRichTextColourSwatchCtrl* m_backgroundColourSwatch;
    /// Control identifiers
    enum {
        ID_RICHTEXTBACKGROUNDPAGE = 10845,
        ID_RICHTEXT_BACKGROUND_COLOUR_CHECKBOX = 10846,
        ID_RICHTEXT_BACKGROUND_COLOUR_SWATCH = 10847
    };
////@end wxRichTextBackgroundPage member variables
};

#endif
    // _RICHTEXTBACKGROUNDPAGE_H_
