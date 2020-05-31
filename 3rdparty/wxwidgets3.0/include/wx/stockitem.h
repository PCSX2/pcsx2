/////////////////////////////////////////////////////////////////////////////
// Name:        wx/stockitem.h
// Purpose:     stock items helpers (privateh header)
// Author:      Vaclav Slavik
// Modified by:
// Created:     2004-08-15
// Copyright:   (c) Vaclav Slavik, 2004
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_STOCKITEM_H_
#define _WX_STOCKITEM_H_

#include "wx/defs.h"
#include "wx/chartype.h"
#include "wx/string.h"
#include "wx/accel.h"

// ----------------------------------------------------------------------------
// Helper functions for stock items handling:
// ----------------------------------------------------------------------------

// Returns true if the ID is in the list of recognized stock actions
WXDLLIMPEXP_CORE bool wxIsStockID(wxWindowID id);

// Returns true of the label is empty or label of a stock button with
// given ID
WXDLLIMPEXP_CORE bool wxIsStockLabel(wxWindowID id, const wxString& label);

enum wxStockLabelQueryFlag
{
    wxSTOCK_NOFLAGS = 0,

    wxSTOCK_WITH_MNEMONIC = 1,
    wxSTOCK_WITH_ACCELERATOR = 2,

    // by default, stock items text is returned with ellipsis, if appropriate,
    // this flag allows to avoid having it
    wxSTOCK_WITHOUT_ELLIPSIS = 4,

    // return label for button, not menu item: buttons should always use
    // mnemonics and never use ellipsis
    wxSTOCK_FOR_BUTTON = wxSTOCK_WITHOUT_ELLIPSIS | wxSTOCK_WITH_MNEMONIC
};

// Returns label that should be used for given stock UI element (e.g. "&OK"
// for wxSTOCK_OK); if wxSTOCK_WITH_MNEMONIC is given, the & character
// is included; if wxSTOCK_WITH_ACCELERATOR is given, the stock accelerator
// for given ID is concatenated to the label using \t as separator
WXDLLIMPEXP_CORE wxString wxGetStockLabel(wxWindowID id,
                                     long flags = wxSTOCK_WITH_MNEMONIC);

#if wxUSE_ACCEL

    // Returns the accelerator that should be used for given stock UI element
    // (e.g. "Ctrl+x" for wxSTOCK_EXIT)
    WXDLLIMPEXP_CORE wxAcceleratorEntry wxGetStockAccelerator(wxWindowID id);

#endif

// wxStockHelpStringClient conceptually works like wxArtClient: it gives a hint to
// wxGetStockHelpString() about the context where the help string is to be used
enum wxStockHelpStringClient
{
    wxSTOCK_MENU        // help string to use for menu items
};

// Returns an help string for the given stock UI element and for the given "context".
WXDLLIMPEXP_CORE wxString wxGetStockHelpString(wxWindowID id,
                                          wxStockHelpStringClient client = wxSTOCK_MENU);


#ifdef __WXGTK20__

// Translates stock ID to GTK+'s stock item string identifier:
WXDLLIMPEXP_CORE const char *wxGetStockGtkID(wxWindowID id);

#endif

#endif // _WX_STOCKITEM_H_
