/////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/spinbutt.h
// Purpose:     wxSpinButton class
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SPINBUTT_H_
#define _WX_SPINBUTT_H_

class WXDLLIMPEXP_FWD_CORE wxArrowButton; // internal

class WXDLLIMPEXP_CORE wxSpinButton : public wxSpinButtonBase
{
    DECLARE_DYNAMIC_CLASS(wxSpinButton)

public:
    wxSpinButton() : m_up( 0 ), m_down( 0 ), m_pos( 0 ) { }

    wxSpinButton(wxWindow *parent,
        wxWindowID id = wxID_ANY,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxSP_VERTICAL,
        const wxString& name = "wxSpinButton")
        : m_up( 0 ),
        m_down( 0 ),
        m_pos( 0 )
    {
        Create(parent, id, pos, size, style, name);
    }
    virtual ~wxSpinButton();

    bool Create(wxWindow *parent,
        wxWindowID id = wxID_ANY,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxSP_VERTICAL,
        const wxString& name = "wxSpinButton");

    // accessors
    int GetValue() const;
    int GetMin() const { return m_min; }
    int GetMax() const { return m_max; }

    // operations
    void SetValue(int val);
    void SetRange(int minVal, int maxVal);

    // Implementation
    virtual void Command(wxCommandEvent& event)
        { (void)ProcessCommand(event); }
    virtual void ChangeFont(bool keepOriginalSize = true);
    virtual void ChangeBackgroundColour();
    virtual void ChangeForegroundColour();
public:
    // implementation detail
    void Increment( int delta );

private:
    virtual void DoSetSize(int x, int y, int width, int height,
                           int sizeFlags = wxSIZE_AUTO);
    virtual void DoMoveWindow(int x, int y, int width, int height);
    virtual wxSize DoGetBestSize() const;

    wxArrowButton* m_up;
    wxArrowButton* m_down;
    int m_pos;
};

#endif
// _WX_SPINBUTT_H_
