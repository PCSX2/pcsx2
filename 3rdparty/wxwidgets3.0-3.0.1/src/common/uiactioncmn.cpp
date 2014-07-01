/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/uiactioncmn.cpp
// Purpose:     wxUIActionSimulator common implementation
// Author:      Kevin Ollivier, Steven Lamerton, Vadim Zeitlin
// Modified by:
// Created:     2010-03-06
// Copyright:   (c) Kevin Ollivier
//              (c) 2010 Steven Lamerton
//              (c) 2010 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_UIACTIONSIMULATOR

#include "wx/uiaction.h"

bool wxUIActionSimulator::MouseClick(int button)
{
    MouseDown(button);
    MouseUp(button);

    return true;
}

#ifndef __WXOSX__

bool wxUIActionSimulator::MouseDblClick(int button)
{
    MouseDown(button);
    MouseUp(button);
    MouseDown(button);
    MouseUp(button);

    return true;
}

bool wxUIActionSimulator::MouseDragDrop(long x1, long y1, long x2, long y2,
                                   int button)
{
    MouseMove(x1, y1);
    MouseDown(button);
    MouseMove(x2, y2);
    MouseUp(button);
    
    return true;
}

#endif

bool
wxUIActionSimulator::Key(int keycode, int modifiers, bool isDown)
{
    wxASSERT_MSG( (modifiers & wxMOD_ALTGR) != wxMOD_ALTGR,
        "wxMOD_ALTGR is not implemented" );
    wxASSERT_MSG( !(modifiers & wxMOD_META ),
        "wxMOD_META is not implemented" );
    wxASSERT_MSG( !(modifiers & wxMOD_WIN ),
        "wxMOD_WIN is not implemented" );

    if ( isDown )
        SimulateModifiers(modifiers, true);

    bool rc = DoKey(keycode, modifiers, isDown);

    if ( !isDown )
        SimulateModifiers(modifiers, false);

    return rc;
}

void wxUIActionSimulator::SimulateModifiers(int modifiers, bool isDown)
{
    if ( modifiers & wxMOD_SHIFT )
        DoKey(WXK_SHIFT, modifiers, isDown);
    if ( modifiers & wxMOD_ALT )
        DoKey(WXK_ALT, modifiers, isDown);
    if ( modifiers & wxMOD_CONTROL )
        DoKey(WXK_CONTROL, modifiers, isDown);
}

bool wxUIActionSimulator::Char(int keycode, int modifiers)
{
    switch(keycode)
    {
    case '0':
        keycode = '0';
        break;
    case '1':
        keycode = '1';
        break;
    case '2':
        keycode = '2';
        break;
    case '3':
        keycode = '3';
        break;
    case '4':
        keycode = '4';
        break;
    case '5':
        keycode = '5';
        break;
    case '6':
        keycode = '6';
        break;
    case '7':
        keycode = '7';
        break;
    case '8':
        keycode = '8';
        break;
    case '9':
        keycode = '9';
        break;
    case '+':
        keycode = '+';
        break;
    case '-':
        keycode = '-';
        break;
    case '.':
        keycode = '.';
        break;
    default:
        break;
    };

    Key(keycode, modifiers, true);
    Key(keycode, modifiers, false);

    return true;
}

bool wxUIActionSimulator::Text(const char *s)
{
    while ( *s != '\0' )
    {
        const char ch = *s++;
        if ( !Char(ch, isupper(ch) ? wxMOD_SHIFT : 0) )
            return false;
    }

    return true;
}

#endif // wxUSE_UIACTIONSIMULATOR
