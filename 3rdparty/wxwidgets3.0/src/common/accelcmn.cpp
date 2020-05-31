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

wxGCC_WARNING_SUPPRESS(missing-field-initializers)

static const struct wxKeyName
{
    wxKeyCode code;
    const char *name;
    const char *display_name;
} wxKeyNames[] =
{
    { WXK_DELETE,           /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Delete") },
    { WXK_DELETE,           /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Del") },
    { WXK_BACK,             /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Back"),          /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Backspace") },
    { WXK_INSERT,           /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Insert") },
    { WXK_INSERT,           /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Ins") },
    { WXK_RETURN,           /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Enter") },
    { WXK_RETURN,           /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Return") },
    { WXK_PAGEUP,           /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("PageUp"),        /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Page Up") },
    { WXK_PAGEDOWN,         /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("PageDown"),      /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Page Down") },
    { WXK_PAGEUP,           /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("PgUp") },
    { WXK_PAGEDOWN,         /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("PgDn") },
    { WXK_LEFT,             /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Left"),          /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Left") },
    { WXK_RIGHT,            /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Right"),         /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Right") },
    { WXK_UP,               /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Up"),            /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Up") },
    { WXK_DOWN,             /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Down"),          /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Down") },
    { WXK_HOME,             /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Home") },
    { WXK_END,              /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("End") },
    { WXK_SPACE,            /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Space") },
    { WXK_TAB,              /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Tab") },
    { WXK_ESCAPE,           /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Esc") },
    { WXK_ESCAPE,           /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Escape") },
    { WXK_CANCEL,           /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Cancel") },
    { WXK_CLEAR,            /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Clear") },
    { WXK_MENU,             /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Menu") },
    { WXK_PAUSE,            /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Pause") },
    { WXK_CAPITAL,          /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Capital") },
    { WXK_SELECT,           /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Select") },
    { WXK_PRINT,            /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Print") },
    { WXK_EXECUTE,          /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Execute") },
    { WXK_SNAPSHOT,         /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Snapshot") },
    { WXK_HELP,             /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Help") },
    { WXK_ADD,              /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Add") },
    { WXK_SEPARATOR,        /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Separator") },
    { WXK_SUBTRACT,         /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Subtract") },
    { WXK_DECIMAL,          /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Decimal") },
    { WXK_DIVIDE,           /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Divide") },
    { WXK_NUMLOCK,          /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num_lock"),      /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num Lock") },
    { WXK_SCROLL,           /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Scroll_lock"),   /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Scroll Lock") },
    { WXK_NUMPAD_SPACE,     /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_Space"),      /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num Space") },
    { WXK_NUMPAD_TAB,       /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_Tab"),        /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num Tab") },
    { WXK_NUMPAD_ENTER,     /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_Enter"),      /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num Enter") },
    { WXK_NUMPAD_HOME,      /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_Home"),       /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num Home") },
    { WXK_NUMPAD_LEFT,      /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_Left"),       /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num left") },
    { WXK_NUMPAD_UP,        /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_Up"),         /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num Up") },
    { WXK_NUMPAD_RIGHT,     /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_Right"),      /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num Right") },
    { WXK_NUMPAD_DOWN,      /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_Down"),       /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num Down") },
    { WXK_NUMPAD_PAGEUP,    /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_PageUp"),     /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num Page Up") },
    { WXK_NUMPAD_PAGEDOWN,  /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_PageDown"),   /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num Page Down") },
    { WXK_NUMPAD_PAGEUP,    /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_Prior") },
    { WXK_NUMPAD_PAGEDOWN,  /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_Next") },
    { WXK_NUMPAD_END,       /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_End"),        /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num End") },
    { WXK_NUMPAD_BEGIN,     /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_Begin"),      /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num Begin") },
    { WXK_NUMPAD_INSERT,    /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_Insert"),     /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num Insert") },
    { WXK_NUMPAD_DELETE,    /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_Delete"),     /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num Delete") },
    { WXK_NUMPAD_EQUAL,     /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_Equal"),      /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num =") },
    { WXK_NUMPAD_MULTIPLY,  /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_Multiply"),   /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num *") },
    { WXK_NUMPAD_ADD,       /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_Add"),        /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num +") },
    { WXK_NUMPAD_SEPARATOR, /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_Separator"),  /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num ,") },
    { WXK_NUMPAD_SUBTRACT,  /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_Subtract"),   /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num -") },
    { WXK_NUMPAD_DECIMAL,   /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_Decimal"),    /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num .") },
    { WXK_NUMPAD_DIVIDE,    /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("KP_Divide"),     /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Num /") },
    { WXK_WINDOWS_LEFT,     /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Windows_Left") },
    { WXK_WINDOWS_RIGHT,    /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Windows_Right") },
    { WXK_WINDOWS_MENU,     /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Windows_Menu") },
    { WXK_COMMAND,          /*TRANSLATORS: Name of keyboard key*/ wxTRANSLATE("Command") },
};

wxGCC_WARNING_RESTORE(missing-field-initializers)

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
                text << PossiblyLocalize(kn.display_name ? kn.display_name : kn.name, localized);
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



