///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk1/private/mnemonics.h
// Purpose:     helper functions for dealing with GTK+ mnemonics
// Author:      Vadim Zeitlin
// Created:     2007-11-12
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _GTK_PRIVATE_MNEMONICS_H_
#define _GTK_PRIVATE_MNEMONICS_H_

#if wxUSE_CONTROLS || wxUSE_MENUS

#include "wx/string.h"

// ----------------------------------------------------------------------------
// functions to convert between wxWidgets and GTK+ string containing mnemonics
// ----------------------------------------------------------------------------

// remove all mnemonics from a string
wxString wxGTKRemoveMnemonics(const wxString& label);

// convert a wx string with '&' to GTK+ string with '_'s
wxString wxConvertMnemonicsToGTK(const wxString& label);

// convert a wx string with '&' to indicate mnemonics as well as HTML entities
// to a GTK+ string with "&amp;" used instead of '&', i.e. suitable for use
// with GTK+ functions using markup strings
wxString wxConvertMnemonicsToGTKMarkup(const wxString& label);

// convert GTK+ string with '_'s to wx string with '&'s
wxString wxConvertMnemonicsFromGTK(const wxString& label);

#endif // wxUSE_CONTROLS || wxUSE_MENUS

#endif // _GTK_PRIVATE_MNEMONICS_H_

