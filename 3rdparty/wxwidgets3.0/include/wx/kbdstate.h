///////////////////////////////////////////////////////////////////////////////
// Name:        wx/kbdstate.h
// Purpose:     Declaration of wxKeyboardState class
// Author:      Vadim Zeitlin
// Created:     2008-09-19
// Copyright:   (c) 2008 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_KBDSTATE_H_
#define _WX_KBDSTATE_H_

#include "wx/defs.h"

// ----------------------------------------------------------------------------
// wxKeyboardState stores the state of the keyboard modifier keys
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxKeyboardState
{
public:
    wxKeyboardState(bool controlDown = false,
                    bool shiftDown = false,
                    bool altDown = false,
                    bool metaDown = false)
        : m_controlDown(controlDown),
          m_shiftDown(shiftDown),
          m_altDown(altDown),
          m_metaDown(metaDown)
#ifdef __WXOSX__
          ,m_rawControlDown(false)
#endif
    {
    }

    // default copy ctor, assignment operator and dtor are ok


    // accessors for the various modifier keys
    // ---------------------------------------

    // should be used check if the key event has exactly the given modifiers:
    // "GetModifiers() = wxMOD_CONTROL" is easier to write than "ControlDown()
    // && !MetaDown() && !AltDown() && !ShiftDown()"
    int GetModifiers() const
    {
        return (m_controlDown ? wxMOD_CONTROL : 0) |
               (m_shiftDown ? wxMOD_SHIFT : 0) |
               (m_metaDown ? wxMOD_META : 0) |
#ifdef __WXOSX__
               (m_rawControlDown ? wxMOD_RAW_CONTROL : 0) |
#endif
               (m_altDown ? wxMOD_ALT : 0);
    }

    // returns true if any modifiers at all are pressed
    bool HasAnyModifiers() const { return GetModifiers() != wxMOD_NONE; }

    // returns true if any modifiers changing the usual key interpretation are
    // pressed, notably excluding Shift
    bool HasModifiers() const
    {
        return ControlDown() || RawControlDown() || AltDown();
    }

    // accessors for individual modifier keys
    bool ControlDown() const { return m_controlDown; }
    bool RawControlDown() const 
    { 
#ifdef __WXOSX__
        return m_rawControlDown; 
#else
        return m_controlDown;
#endif
    }
    bool ShiftDown() const { return m_shiftDown; }
    bool MetaDown() const { return m_metaDown; }
    bool AltDown() const { return m_altDown; }

    // "Cmd" is a pseudo key which is Control for PC and Unix platforms but
    // Apple ("Command") key under Macs: it makes often sense to use it instead
    // of, say, ControlDown() because Cmd key is used for the same thing under
    // Mac as Ctrl elsewhere (but Ctrl still exists, just not used for this
    // purpose under Mac)
    bool CmdDown() const
    {
        return ControlDown();
    }

    // these functions are mostly used by wxWidgets itself
    // ---------------------------------------------------

    void SetControlDown(bool down) { m_controlDown = down; }
    void SetRawControlDown(bool down) 
    { 
#ifdef __WXOSX__
        m_rawControlDown = down; 
#else
        m_controlDown = down; 
#endif
    }
    void SetShiftDown(bool down)   { m_shiftDown = down; }
    void SetAltDown(bool down)     { m_altDown = down; }
    void SetMetaDown(bool down)    { m_metaDown = down; }


    // for backwards compatibility with the existing code accessing these
    // members of wxKeyEvent directly, these variables are public, however you
    // should not use them in any new code, please use the accessors instead
public:
    bool m_controlDown     : 1;
    bool m_shiftDown       : 1;
    bool m_altDown         : 1;
    bool m_metaDown        : 1;
#ifdef __WXOSX__
    bool m_rawControlDown : 1;
#endif
};

#endif // _WX_KBDSTATE_H_

