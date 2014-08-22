///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/private/button.h
// Purpose:     helper functions used with native BUTTON control
// Author:      Vadim Zeitlin
// Created:     2008-06-07
// Copyright:   (c) 2008 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_PRIVATE_BUTTON_H_
#define _WX_MSW_PRIVATE_BUTTON_H_

// define some standard button constants which may be missing in the headers
#ifndef BS_PUSHLIKE
    #define BS_PUSHLIKE 0x00001000L
#endif

#ifndef BST_UNCHECKED
    #define BST_UNCHECKED 0x0000
#endif

#ifndef BST_CHECKED
    #define BST_CHECKED 0x0001
#endif

#ifndef BST_INDETERMINATE
    #define BST_INDETERMINATE 0x0002
#endif

namespace wxMSWButton
{

// returns BS_MULTILINE if the label contains new lines or 0 otherwise
inline int GetMultilineStyle(const wxString& label)
{
    return label.find(wxT('\n')) == wxString::npos ? 0 : BS_MULTILINE;
}

// update the style of the specified HWND to include or exclude BS_MULTILINE
// depending on whether the label contains the new lines
void UpdateMultilineStyle(HWND hwnd, const wxString& label);

// flags for ComputeBestSize() and GetFittingSize()
enum
{
    Size_AuthNeeded = 1,
    Size_ExactFit   = 2
};

// NB: All the functions below are implemented in src/msw/button.cpp

// Compute the button size (as if wxBU_EXACTFIT were specified, i.e. without
// adjusting it to be of default size if it's smaller) for the given label size
WXDLLIMPEXP_CORE wxSize
GetFittingSize(wxWindow *win, const wxSize& sizeLabel, int flags = 0);

// Compute the button size (as if wxBU_EXACTFIT were specified) by computing
// its label size and then calling GetFittingSize().
wxSize ComputeBestFittingSize(wxControl *btn, int flags = 0);

// Increase the size passed as parameter to be at least the standard button
// size if the control doesn't have wxBU_EXACTFIT style and also cache it as
// the best size and return its value -- this is used in DoGetBestSize()
// implementation.
wxSize IncreaseToStdSizeAndCache(wxControl *btn, const wxSize& size);

// helper of wxToggleButton::DoGetBestSize()
inline wxSize ComputeBestSize(wxControl *btn, int flags = 0)
{
    return IncreaseToStdSizeAndCache(btn, ComputeBestFittingSize(btn, flags));
}

} // namespace wxMSWButton

#endif // _WX_MSW_PRIVATE_BUTTON_H_

