///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/private/keyboard.h
// Purpose:     Helper keyboard-related functions.
// Author:      Vadim Zeitlin
// Created:     2010-09-09
// Copyright:   (c) 2010 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_PRIVATE_KEYBOARD_H_
#define _WX_MSW_PRIVATE_KEYBOARD_H_

#include "wx/defs.h"

namespace wxMSWKeyboard
{

// ----------------------------------------------------------------------------
// Functions for translating between MSW virtual keys codes and wx key codes
//
// These functions are currently implemented in src/msw/window.cpp.
// ----------------------------------------------------------------------------

// Translate MSW virtual key code to wx key code. lParam is used to distinguish
// between numpad and extended version of the keys, extended is assumed by
// default if lParam == 0.
//
// Returns WXK_NONE if translation couldn't be done at all (this happens e.g.
// for dead keys and in this case uc will be WXK_NONE too) or if the key
// corresponds to a non-Latin-1 character in which case uc is filled with its
// Unicode value.
WXDLLIMPEXP_CORE int VKToWX(WXWORD vk, WXLPARAM lParam = 0, wchar_t *uc = NULL);

// Translate wxKeyCode enum element (passed as int for compatibility reasons)
// to MSW virtual key code. isExtended is set to true if the key corresponds to
// a non-numpad version of a key that exists both on numpad and outside it.
WXDLLIMPEXP_CORE WXWORD WXToVK(int id, bool *isExtended = NULL);

} // namespace wxMSWKeyboard

#endif // _WX_MSW_PRIVATE_KEYBOARD_H_
