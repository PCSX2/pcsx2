///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/accelcmn.cpp
// Purpose:     implementation of platform-independent wxAcceleratorEntry parts
// Author:      Vadim Zeitlin
// Created:     2007-05-05
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_ACCEL

#ifndef WX_PRECOMP
    #include "wx/accel.h"
    #include "wx/string.h"
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/crt.h"
#endif //WX_PRECOMP

wxAcceleratorTable wxNullAcceleratorTable;

// ============================================================================
// wxAcceleratorEntry implementation
// ============================================================================

static const struct wxKeyName
{
    wxKeyCode code;
    const char *name;
} wxKeyNames[] =
{
    { WXK_DELETE, wxTRANSLATE("DEL") },
    { WXK_DELETE, wxTRANSLATE("DELETE") },
    { WXK_BACK, wxTRANSLATE("BACK") },
    { WXK_INSERT, wxTRANSLATE("INS") },
    { WXK_INSERT, wxTRANSLATE("INSERT") },
    { WXK_RETURN, wxTRANSLATE("ENTER") },
    { WXK_RETURN, wxTRANSLATE("RETURN") },
    { WXK_PAGEUP, wxTRANSLATE("PGUP") },
    { WXK_PAGEDOWN, wxTRANSLATE("PGDN") },
    { WXK_LEFT, wxTRANSLATE("LEFT") },
    { WXK_RIGHT, wxTRANSLATE("RIGHT") },
    { WXK_UP, wxTRANSLATE("UP") },
    { WXK_DOWN, wxTRANSLATE("DOWN") },
    { WXK_HOME, wxTRANSLATE("HOME") },
    { WXK_END, wxTRANSLATE("END") },
    { WXK_SPACE, wxTRANSLATE("SPACE") },
    { WXK_TAB, wxTRANSLATE("TAB") },
    { WXK_ESCAPE, wxTRANSLATE("ESC") },
    { WXK_ESCAPE, wxTRANSLATE("ESCAPE") },
    { WXK_CANCEL, wxTRANSLATE("CANCEL") },
    { WXK_CLEAR, wxTRANSLATE("CLEAR") },
    { WXK_MENU, wxTRANSLATE("MENU") },
    { WXK_PAUSE, wxTRANSLATE("PAUSE") },
    { WXK_CAPITAL, wxTRANSLATE("CAPITAL") },
    { WXK_SELECT, wxTRANSLATE("SELECT") },
    { WXK_PRINT, wxTRANSLATE("PRINT") },
    { WXK_EXECUTE, wxTRANSLATE("EXECUTE") },
    { WXK_SNAPSHOT, wxTRANSLATE("SNAPSHOT") },
    { WXK_HELP, wxTRANSLATE("HELP") },
    { WXK_ADD, wxTRANSLATE("ADD") },
    { WXK_SEPARATOR, wxTRANSLATE("SEPARATOR") },
    { WXK_SUBTRACT, wxTRANSLATE("SUBTRACT") },
    { WXK_DECIMAL, wxTRANSLATE("DECIMAL") },
    { WXK_DIVIDE, wxTRANSLATE("DIVIDE") },
    { WXK_NUMLOCK, wxTRANSLATE("NUM_LOCK") },
    { WXK_SCROLL, wxTRANSLATE("SCROLL_LOCK") },
    { WXK_PAGEUP, wxTRANSLATE("PAGEUP") },
    { WXK_PAGEDOWN, wxTRANSLATE("PAGEDOWN") },
    { WXK_NUMPAD_SPACE, wxTRANSLATE("KP_SPACE") },
    { WXK_NUMPAD_TAB, wxTRANSLATE("KP_TAB") },
    { WXK_NUMPAD_ENTER, wxTRANSLATE("KP_ENTER") },
    { WXK_NUMPAD_HOME, wxTRANSLATE("KP_HOME") },
    { WXK_NUMPAD_LEFT, wxTRANSLATE("KP_LEFT") },
    { WXK_NUMPAD_UP, wxTRANSLATE("KP_UP") },
    { WXK_NUMPAD_RIGHT, wxTRANSLATE("KP_RIGHT") },
    { WXK_NUMPAD_DOWN, wxTRANSLATE("KP_DOWN") },
    { WXK_NUMPAD_PAGEUP, wxTRANSLATE("KP_PRIOR") },
    { WXK_NUMPAD_PAGEUP, wxTRANSLATE("KP_PAGEUP") },
    { WXK_NUMPAD_PAGEDOWN, wxTRANSLATE("KP_NEXT") },
    { WXK_NUMPAD_PAGEDOWN, wxTRANSLATE("KP_PAGEDOWN") },
    { WXK_NUMPAD_END, wxTRANSLATE("KP_END") },
    { WXK_NUMPAD_BEGIN, wxTRANSLATE("KP_BEGIN") },
    { WXK_NUMPAD_INSERT, wxTRANSLATE("KP_INSERT") },
    { WXK_NUMPAD_DELETE, wxTRANSLATE("KP_DELETE") },
    { WXK_NUMPAD_EQUAL, wxTRANSLATE("KP_EQUAL") },
    { WXK_NUMPAD_MULTIPLY, wxTRANSLATE("KP_MULTIPLY") },
    { WXK_NUMPAD_ADD, wxTRANSLATE("KP_ADD") },
    { WXK_NUMPAD_SEPARATOR, wxTRANSLATE("KP_SEPARATOR") },
    { WXK_NUMPAD_SUBTRACT, wxTRANSLATE("KP_SUBTRACT") },
    { WXK_NUMPAD_DECIMAL, wxTRANSLATE("KP_DECIMAL") },
    { WXK_NUMPAD_DIVIDE, wxTRANSLATE("KP_DIVIDE") },
    { WXK_WINDOWS_LEFT, wxTRANSLATE("WINDOWS_LEFT") },
    { WXK_WINDOWS_RIGHT, wxTRANSLATE("WINDOWS_RIGHT") },
    { WXK_WINDOWS_MENU, wxTRANSLATE("WINDOWS_MENU") },
    { WXK_COMMAND, wxTRANSLATE("COMMAND") },
};

// return true if the 2 strings refer to the same accel
//
// as accels can be either translated or not, check for both possibilities and
// also compare case-insensitively as the key names case doesn't count
static inline bool CompareAccelString(const wxString& str, const char *accel)
{
    return str.CmpNoCase(accel) == 0
#if wxUSE_INTL
            || str.CmpNoCase(wxGetTranslation(accel)) == 0
#endif
            ;
}

// return prefixCode+number if the string is of the form "<prefix><number>" and
// 0 if it isn't
//
// first and last parameter specify the valid domain for "number" part
static int IsNumberedAccelKey(const wxString& str,
                              const char *prefix,
                              wxKeyCode prefixCode,
                              unsigned first,
                              unsigned last)
{
    const size_t lenPrefix = wxStrlen(prefix);
    if ( !CompareAccelString(str.Left(lenPrefix), prefix) )
        return 0;

    unsigned long num;
    if ( !str.Mid(lenPrefix).ToULong(&num) )
        return 0;

    if ( num < first || num > last )
    {
        // this must be a mistake, chances that this is a valid name of another
        // key are vanishingly small
        wxLogDebug(wxT("Invalid key string \"%s\""), str.c_str());
        return 0;
    }

    return prefixCode + num - first;
}

/* static */
bool
wxAcceleratorEntry::ParseAccel(const wxString& text, int *flagsOut, int *keyOut)
{
    // the parser won't like trailing spaces
    wxString label = text;
    label.Trim(true);

    // For compatibility with the old wx versions which accepted (and actually
    // even required) a TAB character in the string passed to this function we
    // ignore anything up to the first TAB. Notice however that the correct
    // input consists of just the accelerator itself and nothing else, this is
    // done for compatibility and compatibility only.
    int posTab = label.Find(wxT('\t'));
    if ( posTab == wxNOT_FOUND )
        posTab = 0;
    else
        posTab++;

    // parse the accelerator string
    int accelFlags = wxACCEL_NORMAL;
    wxString current;
    for ( size_t n = (size_t)posTab; n < label.length(); n++ )
    {
        if ( (label[n] == '+') || (label[n] == '-') )
        {
            if ( CompareAccelString(current, wxTRANSLATE("ctrl")) )
                accelFlags |= wxACCEL_CTRL;
            else if ( CompareAccelString(current, wxTRANSLATE("alt")) )
                accelFlags |= wxACCEL_ALT;
            else if ( CompareAccelString(current, wxTRANSLATE("shift")) )
                accelFlags |= wxACCEL_SHIFT;
            else if ( CompareAccelString(current, wxTRANSLATE("rawctrl")) )
                accelFlags |= wxACCEL_RAW_CTRL;
            else // not a recognized modifier name
            {
                // we may have "Ctrl-+", for example, but we still want to
                // catch typos like "Crtl-A" so only give the warning if we
                // have something before the current '+' or '-', else take
                // it as a literal symbol
                if ( current.empty() )
                {
                    current += label[n];

                    // skip clearing it below
                    continue;
                }
                else
                {
                    wxLogDebug(wxT("Unknown accel modifier: '%s'"),
                               current.c_str());
                }
            }

            current.clear();
        }
        else // not special character
        {
            current += (wxChar) wxTolower(label[n]);
        }
    }

    int keyCode;
    const size_t len = current.length();
    switch ( len )
    {
        case 0:
            wxLogDebug(wxT("No accel key found, accel string ignored."));
            return false;

        case 1:
            // it's just a letter
            keyCode = current[0U];

            // if the key is used with any modifiers, make it an uppercase one
            // because Ctrl-A and Ctrl-a are the same; but keep it as is if it's
            // used alone as 'a' and 'A' are different
            if ( accelFlags != wxACCEL_NORMAL )
                keyCode = wxToupper(keyCode);
            break;

        default:
            keyCode = IsNumberedAccelKey(current, wxTRANSLATE("F"),
                                         WXK_F1, 1, 12);
            if ( !keyCode )
            {
                for ( size_t n = 0; n < WXSIZEOF(wxKeyNames); n++ )
                {
                    const wxKeyName& kn = wxKeyNames[n];
                    if ( CompareAccelString(current, kn.name) )
                    {
                        keyCode = kn.code;
                        break;
                    }
                }
            }

            if ( !keyCode )
                keyCode = IsNumberedAccelKey(current, wxTRANSLATE("KP_"),
                                             WXK_NUMPAD0, 0, 9);
            if ( !keyCode )
                keyCode = IsNumberedAccelKey(current, wxTRANSLATE("SPECIAL"),
                                             WXK_SPECIAL1, 1, 20);

            if ( !keyCode )
            {
                wxLogDebug(wxT("Unrecognized accel key '%s', accel string ignored."),
                           current.c_str());
                return false;
            }
    }


    wxASSERT_MSG( keyCode, wxT("logic error: should have key code here") );

    if ( flagsOut )
        *flagsOut = accelFlags;
    if ( keyOut )
        *keyOut = keyCode;

    return true;
}

/* static */
wxAcceleratorEntry *wxAcceleratorEntry::Create(const wxString& str)
{
    const wxString accelStr = str.AfterFirst('\t');
    if ( accelStr.empty() )
    {
        // It's ok to pass strings not containing any accelerators at all to
        // this function, wxMenuItem code does it and we should just return
        // NULL in this case.
        return NULL;
    }

    int flags,
        keyCode;
    if ( !ParseAccel(accelStr, &flags, &keyCode) )
        return NULL;

    return new wxAcceleratorEntry(flags, keyCode);
}

bool wxAcceleratorEntry::FromString(const wxString& str)
{
    return ParseAccel(str, &m_flags, &m_keyCode);
}

namespace
{

wxString PossiblyLocalize(const wxString& str, bool localize)
{
    return localize ? wxGetTranslation(str) : str;
}

}

wxString wxAcceleratorEntry::AsPossiblyLocalizedString(bool localized) const
{
    wxString text;

    int flags = GetFlags();
    if ( flags & wxACCEL_ALT )
        text += PossiblyLocalize(wxTRANSLATE("Alt+"), localized);
    if ( flags & wxACCEL_CTRL )
        text += PossiblyLocalize(wxTRANSLATE("Ctrl+"), localized);
    if ( flags & wxACCEL_SHIFT )
        text += PossiblyLocalize(wxTRANSLATE("Shift+"), localized);
#if defined(__WXMAC__) || defined(__WXCOCOA__)
    if ( flags & wxACCEL_RAW_CTRL )
        text += PossiblyLocalize(wxTRANSLATE("RawCtrl+"), localized);
#endif
    
    const int code = GetKeyCode();

    if ( code >= WXK_F1 && code <= WXK_F12 )
        text << PossiblyLocalize(wxTRANSLATE("F"), localized)
             << code - WXK_F1 + 1;
    else if ( code >= WXK_NUMPAD0 && code <= WXK_NUMPAD9 )
        text << PossiblyLocalize(wxTRANSLATE("KP_"), localized)
             << code - WXK_NUMPAD0;
    else if ( code >= WXK_SPECIAL1 && code <= WXK_SPECIAL20 )
        text << PossiblyLocalize(wxTRANSLATE("SPECIAL"), localized)
             << code - WXK_SPECIAL1 + 1;
    else // check the named keys
    {
        size_t n;
        for ( n = 0; n < WXSIZEOF(wxKeyNames); n++ )
        {
            const wxKeyName& kn = wxKeyNames[n];
            if ( code == kn.code )
            {
                text << PossiblyLocalize(kn.name, localized);
                break;
            }
        }

        if ( n == WXSIZEOF(wxKeyNames) )
        {
            // must be a simple key
            if (
#if !wxUSE_UNICODE
                 // we can't call wxIsalnum() for non-ASCII characters in ASCII
                 // build as they're only defined for the ASCII range (or EOF)
                 wxIsascii(code) &&
#endif // ANSI
                    wxIsprint(code) )
            {
                text << (wxChar)code;
            }
            else
            {
                wxFAIL_MSG( wxT("unknown keyboard accelerator code") );
            }
        }
    }

    return text;
}

wxAcceleratorEntry *wxGetAccelFromString(const wxString& label)
{
    return wxAcceleratorEntry::Create(label);
}

#endif // wxUSE_ACCEL



