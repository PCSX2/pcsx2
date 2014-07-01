///////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/infobar.h
// Purpose:     generic wxInfoBar class declaration
// Author:      Vadim Zeitlin
// Created:     2009-07-28
// Copyright:   (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_INFOBAR_H_
#define _WX_GENERIC_INFOBAR_H_

class WXDLLIMPEXP_FWD_CORE wxBitmapButton;
class WXDLLIMPEXP_FWD_CORE wxStaticBitmap;
class WXDLLIMPEXP_FWD_CORE wxStaticText;

// ----------------------------------------------------------------------------
// wxInfoBar
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxInfoBarGeneric : public wxInfoBarBase
{
public:
    // the usual ctors and Create() but remember that info bar is created
    // hidden
    wxInfoBarGeneric() { Init(); }

    wxInfoBarGeneric(wxWindow *parent, wxWindowID winid = wxID_ANY)
    {
        Init();
        Create(parent, winid);
    }

    bool Create(wxWindow *parent, wxWindowID winid = wxID_ANY);


    // implement base class methods
    // ----------------------------

    virtual void ShowMessage(const wxString& msg,
                             int flags = wxICON_INFORMATION);

    virtual void Dismiss();

    virtual void AddButton(wxWindowID btnid, const wxString& label = wxString());

    virtual void RemoveButton(wxWindowID btnid);

    // methods specific to this version
    // --------------------------------

    // set the effect(s) to use when showing/hiding the bar, may be
    // wxSHOW_EFFECT_NONE to disable any effects entirely
    //
    // by default, slide to bottom/top is used when it's positioned on the top
    // of the window for showing/hiding it and top/bottom when it's positioned
    // at the bottom
    void SetShowHideEffects(wxShowEffect showEffect, wxShowEffect hideEffect)
    {
        m_showEffect = showEffect;
        m_hideEffect = hideEffect;
    }

    // get effect used when showing/hiding the window
    wxShowEffect GetShowEffect() const;
    wxShowEffect GetHideEffect() const;

    // set the duration of animation used when showing/hiding the bar, in ms
    void SetEffectDuration(int duration) { m_effectDuration = duration; }

    // get the currently used effect animation duration
    int GetEffectDuration() const { return m_effectDuration; }


    // overridden base class methods
    // -----------------------------

    // setting the font of this window sets it for the text control inside it
    // (default font is a larger and bold version of the normal one)
    virtual bool SetFont(const wxFont& font);

#if wxABI_VERSION >= 30001
    // same thing with the colour: this affects the text colour
    virtual bool SetForegroundColour(const wxColor& colour);
#endif // 3.0.1+

protected:
    // info bar shouldn't have any border by default, the colour difference
    // between it and the main window separates it well enough
    virtual wxBorder GetDefaultBorder() const { return wxBORDER_NONE; }


    // update the parent to take our new or changed size into account (notably
    // should be called when we're shown or hidden)
    void UpdateParent();

private:
    // common part of all ctors
    void Init();

    // handler for the close button
    void OnButton(wxCommandEvent& event);

    // show/hide the bar
    void DoShow();
    void DoHide();

    // determine the placement of the bar from its position in the containing
    // sizer
    enum BarPlacement
    {
        BarPlacement_Top,
        BarPlacement_Bottom,
        BarPlacement_Unknown
    };

    BarPlacement GetBarPlacement() const;


    // different controls making up the bar
    wxStaticBitmap *m_icon;
    wxStaticText *m_text;
    wxBitmapButton *m_button;

    // the effects to use when showing/hiding and duration for them: by default
    // the effect is determined by the info bar automatically depending on its
    // position and the default duration is used
    wxShowEffect m_showEffect,
                 m_hideEffect;
    int m_effectDuration;

    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxInfoBarGeneric);
};

#endif // _WX_GENERIC_INFOBAR_H_

