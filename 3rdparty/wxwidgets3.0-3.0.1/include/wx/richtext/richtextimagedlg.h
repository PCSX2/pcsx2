/////////////////////////////////////////////////////////////////////////////
// Name:        wx/richtext/richtextimagedlg.h
// Purpose:
// Author:      Mingquan Yang
// Modified by: Julian Smart
// Created:     Wed 02 Jun 2010 11:27:23 CST
// RCS-ID:
// Copyright:   (c) Mingquan Yang, Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/dialog.h"

#ifndef _RICHTEXTIMAGEDLG_H_
#define _RICHTEXTIMAGEDLG_H_

/*!
 * Forward declarations
 */

class WXDLLIMPEXP_FWD_CORE wxButton;
class WXDLLIMPEXP_FWD_CORE wxComboBox;
class WXDLLIMPEXP_FWD_CORE wxCheckBox;
class WXDLLIMPEXP_FWD_CORE wxTextCtrl;

/*!
 * Includes
 */

#include "wx/richtext/richtextbuffer.h"
#include "wx/richtext/richtextformatdlg.h"

/*!
 * Control identifiers
 */

#define SYMBOL_WXRICHTEXTOBJECTPROPERTIESDIALOG_STYLE wxDEFAULT_DIALOG_STYLE|wxTAB_TRAVERSAL
#define SYMBOL_WXRICHTEXTOBJECTPROPERTIESDIALOG_TITLE wxGetTranslation("Object Properties")
#define SYMBOL_WXRICHTEXTOBJECTPROPERTIESDIALOG_IDNAME ID_RICHTEXTOBJECTPROPERTIESDIALOG
#define SYMBOL_WXRICHTEXTOBJECTPROPERTIESDIALOG_SIZE wxSize(400, 300)
#define SYMBOL_WXRICHTEXTOBJECTPROPERTIESDIALOG_POSITION wxDefaultPosition

/*!
 * wxRichTextObjectPropertiesDialog class declaration
 */

class WXDLLIMPEXP_RICHTEXT wxRichTextObjectPropertiesDialog: public wxRichTextFormattingDialog
{    
    DECLARE_DYNAMIC_CLASS( wxRichTextObjectPropertiesDialog )
    DECLARE_EVENT_TABLE()

public:
    /// Constructors
    wxRichTextObjectPropertiesDialog();
    wxRichTextObjectPropertiesDialog( wxRichTextObject* obj, wxWindow* parent, wxWindowID id = SYMBOL_WXRICHTEXTOBJECTPROPERTIESDIALOG_IDNAME, const wxString& caption = SYMBOL_WXRICHTEXTOBJECTPROPERTIESDIALOG_TITLE, const wxPoint& pos = SYMBOL_WXRICHTEXTOBJECTPROPERTIESDIALOG_POSITION, const wxSize& size = SYMBOL_WXRICHTEXTOBJECTPROPERTIESDIALOG_SIZE, long style = SYMBOL_WXRICHTEXTOBJECTPROPERTIESDIALOG_STYLE );

    /// Creation
    bool Create( wxRichTextObject* obj, wxWindow* parent, wxWindowID id = SYMBOL_WXRICHTEXTOBJECTPROPERTIESDIALOG_IDNAME, const wxString& caption = SYMBOL_WXRICHTEXTOBJECTPROPERTIESDIALOG_TITLE, const wxPoint& pos = SYMBOL_WXRICHTEXTOBJECTPROPERTIESDIALOG_POSITION, const wxSize& size = SYMBOL_WXRICHTEXTOBJECTPROPERTIESDIALOG_SIZE, long style = SYMBOL_WXRICHTEXTOBJECTPROPERTIESDIALOG_STYLE );

    /// Destructor
    ~wxRichTextObjectPropertiesDialog();

    /// Initialises member variables
    void Init();

    /// Creates the controls and sizers
    void CreateControls();

////@begin wxRichTextObjectPropertiesDialog event handler declarations

////@end wxRichTextObjectPropertiesDialog event handler declarations

////@begin wxRichTextObjectPropertiesDialog member function declarations

    /// Retrieves bitmap resources
    wxBitmap GetBitmapResource( const wxString& name );

    /// Retrieves icon resources
    wxIcon GetIconResource( const wxString& name );
////@end wxRichTextObjectPropertiesDialog member function declarations

    /// Should we show tooltips?
    static bool ShowToolTips();

////@begin wxRichTextObjectPropertiesDialog member variables
    /// Control identifiers
    enum {
        ID_RICHTEXTOBJECTPROPERTIESDIALOG = 10650
    };
////@end wxRichTextObjectPropertiesDialog member variables
};

#endif
    // _RICHTEXTIMAGEDLG_H_
