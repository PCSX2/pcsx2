/////////////////////////////////////////////////////////////////////////////
// Name:        wx/univ/anybutton.h
// Purpose:     wxAnyButton class
// Author:      Vadim Zeitlin
// Created:     2000-08-15 (extracted from button.h)
// Copyright:   (c) 2000 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIV_ANYBUTTON_H_
#define _WX_UNIV_ANYBUTTON_H_
#include "wx/univ/inphand.h"
// ----------------------------------------------------------------------------
// Common button functionality
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxAnyButton : public wxAnyButtonBase
{
public:
    wxAnyButton() {}

    virtual ~wxAnyButton() {}

    // wxAnyButton actions
    virtual void Toggle();
    virtual void Press();
    virtual void Release();
    virtual void Click(){}

    virtual bool PerformAction(const wxControlAction& action,
                               long numArg = -1,
                               const wxString& strArg = wxEmptyString) wxOVERRIDE;

    static wxInputHandler *GetStdInputHandler(wxInputHandler *handlerDef);
    virtual wxInputHandler *DoGetStdInputHandler(wxInputHandler *handlerDef) wxOVERRIDE
    {
        return GetStdInputHandler(handlerDef);
    }

protected:
    // choose the default border for this window
    virtual wxBorder GetDefaultBorder() const wxOVERRIDE { return wxBORDER_STATIC; }

    virtual wxSize DoGetBestClientSize() const wxOVERRIDE;

    virtual bool DoDrawBackground(wxDC& dc) wxOVERRIDE;
    virtual void DoDraw(wxControlRenderer *renderer) wxOVERRIDE;
    // current state
    bool m_isPressed,
         m_isDefault;

    // the (optional) image to show and the margins around it
    wxBitmap m_bitmap;
    wxCoord  m_marginBmpX,
             m_marginBmpY;

private:
    wxDECLARE_NO_COPY_CLASS(wxAnyButton);
};

// ----------------------------------------------------------------------------
// wxStdAnyButtonInputHandler: translates SPACE and ENTER keys and the left mouse
// click into button press/release actions
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxStdAnyButtonInputHandler : public wxStdInputHandler
{
public:
    wxStdAnyButtonInputHandler(wxInputHandler *inphand);

    virtual bool HandleKey(wxInputConsumer *consumer,
                           const wxKeyEvent& event,
                           bool pressed) wxOVERRIDE;
    virtual bool HandleMouse(wxInputConsumer *consumer,
                             const wxMouseEvent& event) wxOVERRIDE;
    virtual bool HandleMouseMove(wxInputConsumer *consumer,
                                 const wxMouseEvent& event) wxOVERRIDE;
    virtual bool HandleFocus(wxInputConsumer *consumer,
                             const wxFocusEvent& event) wxOVERRIDE;
    virtual bool HandleActivation(wxInputConsumer *consumer, bool activated) wxOVERRIDE;

private:
    // the window (button) which has capture or NULL and the flag telling if
    // the mouse is inside the button which captured it or not
    wxWindow *m_winCapture;
    bool      m_winHasMouse;
};

#endif // _WX_UNIV_ANYBUTTON_H_
