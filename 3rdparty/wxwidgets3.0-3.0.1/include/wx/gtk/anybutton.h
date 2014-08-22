/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/anybutton.h
// Purpose:     wxGTK wxAnyButton class declaration
// Author:      Robert Roebling
// Created:     1998-05-20 (extracted from button.h)
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_ANYBUTTON_H_
#define _WX_GTK_ANYBUTTON_H_

//-----------------------------------------------------------------------------
// wxAnyButton
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxAnyButton : public wxAnyButtonBase
{
public:
    wxAnyButton()
    {
        m_isCurrent =
        m_isPressed = false;
    }

    virtual bool Enable( bool enable = true );

    // implementation
    // --------------

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    // called from GTK callbacks: they update the button state and call
    // GTKUpdateBitmap()
    void GTKMouseEnters();
    void GTKMouseLeaves();
    void GTKPressed();
    void GTKReleased();

protected:
    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const;

    virtual wxBitmap DoGetBitmap(State which) const;
    virtual void DoSetBitmap(const wxBitmap& bitmap, State which);
    virtual void DoSetBitmapPosition(wxDirection dir);

private:
    typedef wxAnyButtonBase base_type;

    // focus event handler: calls GTKUpdateBitmap()
    void GTKOnFocus(wxFocusEvent& event);

    // update the bitmap to correspond to the current button state
    void GTKUpdateBitmap();

    // return the current button state from m_isXXX flags (which means that it
    // might not correspond to the real current state as e.g. m_isCurrent will
    // never be true if we don't have a valid current bitmap)
    State GTKGetCurrentState() const;

    // show the given bitmap (must be valid)
    void GTKDoShowBitmap(const wxBitmap& bitmap);


    // the bitmaps for the different state of the buttons, all of them may be
    // invalid and the button only shows a bitmap at all if State_Normal bitmap
    // is valid
    wxBitmap m_bitmaps[State_Max];

    // true iff mouse is currently over the button
    bool m_isCurrent;

    // true iff the button is in pressed state
    bool m_isPressed;

    wxDECLARE_NO_COPY_CLASS(wxAnyButton);
};

#endif // _WX_GTK_ANYBUTTON_H_
