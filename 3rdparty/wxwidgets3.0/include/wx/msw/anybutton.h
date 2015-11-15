/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/anybutton.h
// Purpose:     wxAnyButton class
// Author:      Julian Smart
// Created:     1997-02-01 (extracted from button.h)
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_ANYBUTTON_H_
#define _WX_MSW_ANYBUTTON_H_

// ----------------------------------------------------------------------------
// Common button functionality
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxAnyButton : public wxAnyButtonBase
{
public:
    wxAnyButton()
    {
        m_imageData = NULL;
#if wxUSE_MARKUP
        m_markupText = NULL;
#endif // wxUSE_MARKUP
    }

    virtual ~wxAnyButton();

    // overridden base class methods
    virtual void SetLabel(const wxString& label);
    virtual bool SetBackgroundColour(const wxColour &colour);
    virtual bool SetForegroundColour(const wxColour &colour);

    // implementation from now on
    virtual WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);

    virtual bool MSWOnDraw(WXDRAWITEMSTRUCT *item);
    virtual State GetNormalState() const { return State_Normal; }

    // returns true if the platform should explicitly apply a theme border
    virtual bool CanApplyThemeBorder() const { return false; }

protected:
    // usually overridden base class virtuals
    virtual wxSize DoGetBestSize() const;

    virtual wxBitmap DoGetBitmap(State which) const;
    virtual void DoSetBitmap(const wxBitmap& bitmap, State which);
    virtual wxSize DoGetBitmapMargins() const;
    virtual void DoSetBitmapMargins(wxCoord x, wxCoord y);
    virtual void DoSetBitmapPosition(wxDirection dir);

#if wxUSE_MARKUP
    virtual bool DoSetLabelMarkup(const wxString& markup);
#endif // wxUSE_MARKUP

    // Increases the passed in size to account for the button image.
    //
    // Should only be called if we do have a button, i.e. if m_imageData is
    // non-NULL.
    void AdjustForBitmapSize(wxSize& size) const;

    class wxButtonImageData *m_imageData;

#if wxUSE_MARKUP
    class wxMarkupText *m_markupText;
#endif // wxUSE_MARKUP

    // Switches button into owner-drawn mode: this is used if we need to draw
    // something not supported by the native control, such as using non default
    // colours or a bitmap on pre-XP systems.
    void MakeOwnerDrawn();
    bool IsOwnerDrawn() const;

private:
    wxDECLARE_NO_COPY_CLASS(wxAnyButton);
};

#endif // _WX_MSW_ANYBUTTON_H_
