///////////////////////////////////////////////////////////////////////////////
// Name:        wx/univ/spinbutt.h
// Purpose:     universal version of wxSpinButton
// Author:      Vadim Zeitlin
// Modified by:
// Created:     21.01.01
// Copyright:   (c) 2001 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIV_SPINBUTT_H_
#define _WX_UNIV_SPINBUTT_H_

#include "wx/univ/scrarrow.h"

// ----------------------------------------------------------------------------
// wxSpinButton
// ----------------------------------------------------------------------------

// actions supported by this control
#define wxACTION_SPIN_INC    wxT("inc")
#define wxACTION_SPIN_DEC    wxT("dec")

class WXDLLIMPEXP_CORE wxSpinButton : public wxSpinButtonBase,
                                 public wxControlWithArrows
{
public:
    wxSpinButton();
    wxSpinButton(wxWindow *parent,
                 wxWindowID id = wxID_ANY,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = wxSP_VERTICAL | wxSP_ARROW_KEYS,
                 const wxString& name = wxSPIN_BUTTON_NAME);

    bool Create(wxWindow *parent,
                wxWindowID id = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxSP_VERTICAL | wxSP_ARROW_KEYS,
                const wxString& name = wxSPIN_BUTTON_NAME);

    // implement wxSpinButtonBase methods
    virtual int GetValue() const wxOVERRIDE;
    virtual void SetValue(int val) wxOVERRIDE;
    virtual void SetRange(int minVal, int maxVal) wxOVERRIDE;

    // implement wxControlWithArrows methods
    virtual wxRenderer *GetRenderer() const wxOVERRIDE { return m_renderer; }
    virtual wxWindow *GetWindow() wxOVERRIDE { return this; }
    virtual bool IsVertical() const wxOVERRIDE { return wxSpinButtonBase::IsVertical(); }
    virtual int GetArrowState(wxScrollArrows::Arrow arrow) const wxOVERRIDE;
    virtual void SetArrowFlag(wxScrollArrows::Arrow arrow, int flag, bool set) wxOVERRIDE;
    virtual bool OnArrow(wxScrollArrows::Arrow arrow) wxOVERRIDE;
    virtual wxScrollArrows::Arrow HitTestArrow(const wxPoint& pt) const wxOVERRIDE;

    // for wxStdSpinButtonInputHandler
    const wxScrollArrows& GetArrows() { return m_arrows; }

    virtual bool PerformAction(const wxControlAction& action,
                               long numArg = 0,
                               const wxString& strArg = wxEmptyString) wxOVERRIDE;

    static wxInputHandler *GetStdInputHandler(wxInputHandler *handlerDef);
    virtual wxInputHandler *DoGetStdInputHandler(wxInputHandler *handlerDef) wxOVERRIDE
    {
        return GetStdInputHandler(handlerDef);
    }

protected:
    virtual wxSize DoGetBestClientSize() const wxOVERRIDE;
    virtual void DoDraw(wxControlRenderer *renderer) wxOVERRIDE;
    virtual wxBorder GetDefaultBorder() const wxOVERRIDE { return wxBORDER_NONE; }

    // the common part of all ctors
    void Init();

    // normalize the value to fit into min..max range
    int NormalizeValue(int value) const;

    // change the value by +1/-1 and send the event, return true if value was
    // changed
    bool ChangeValue(int inc);

    // get the rectangles for our 2 arrows
    void CalcArrowRects(wxRect *rect1, wxRect *rect2) const;

    // the current controls value
    int m_value;

private:
    // the object which manages our arrows
    wxScrollArrows m_arrows;

    // the state (combination of wxCONTROL_XXX flags) of the arrows
    int m_arrowsState[wxScrollArrows::Arrow_Max];

    wxDECLARE_DYNAMIC_CLASS(wxSpinButton);
};

// ----------------------------------------------------------------------------
// wxStdSpinButtonInputHandler: manages clicks on them (use arrows like
// wxStdScrollBarInputHandler) and processes keyboard events too
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxStdSpinButtonInputHandler : public wxStdInputHandler
{
public:
    wxStdSpinButtonInputHandler(wxInputHandler *inphand);

    virtual bool HandleKey(wxInputConsumer *consumer,
                           const wxKeyEvent& event,
                           bool pressed) wxOVERRIDE;
    virtual bool HandleMouse(wxInputConsumer *consumer,
                             const wxMouseEvent& event) wxOVERRIDE;
    virtual bool HandleMouseMove(wxInputConsumer *consumer,
                                 const wxMouseEvent& event) wxOVERRIDE;
};

#endif // _WX_UNIV_SPINBUTT_H_

