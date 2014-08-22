/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/radiobox.h
// Purpose:     wxRadioBox class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_RADIOBOX_H_
#define _WX_RADIOBOX_H_

#include "wx/statbox.h"

class WXDLLIMPEXP_FWD_CORE wxSubwindows;

// ----------------------------------------------------------------------------
// wxRadioBox
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxRadioBox : public wxStaticBox, public wxRadioBoxBase
{
public:
    wxRadioBox() { Init(); }

    wxRadioBox(wxWindow *parent,
               wxWindowID id,
               const wxString& title,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               int n = 0, const wxString choices[] = NULL,
               int majorDim = 0,
               long style = wxRA_SPECIFY_COLS,
               const wxValidator& val = wxDefaultValidator,
               const wxString& name = wxRadioBoxNameStr)
    {
        Init();

        (void)Create(parent, id, title, pos, size, n, choices, majorDim,
                     style, val, name);
    }

    wxRadioBox(wxWindow *parent,
               wxWindowID id,
               const wxString& title,
               const wxPoint& pos,
               const wxSize& size,
               const wxArrayString& choices,
               int majorDim = 0,
               long style = wxRA_SPECIFY_COLS,
               const wxValidator& val = wxDefaultValidator,
               const wxString& name = wxRadioBoxNameStr)
    {
        Init();

        (void)Create(parent, id, title, pos, size, choices, majorDim,
                     style, val, name);
    }

    virtual ~wxRadioBox();

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                int n = 0, const wxString choices[] = NULL,
                int majorDim = 0,
                long style = wxRA_SPECIFY_COLS,
                const wxValidator& val = wxDefaultValidator,
                const wxString& name = wxRadioBoxNameStr);
    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& title,
                const wxPoint& pos,
                const wxSize& size,
                const wxArrayString& choices,
                int majorDim = 0,
                long style = wxRA_SPECIFY_COLS,
                const wxValidator& val = wxDefaultValidator,
                const wxString& name = wxRadioBoxNameStr);

    // implement the radiobox interface
    virtual void SetSelection(int n);
    virtual int GetSelection() const { return m_selectedButton; }
    virtual unsigned int GetCount() const;
    virtual wxString GetString(unsigned int n) const;
    virtual void SetString(unsigned int n, const wxString& label);
    virtual bool Enable(unsigned int n, bool enable = true);
    virtual bool Show(unsigned int n, bool show = true);
    virtual bool IsItemEnabled(unsigned int n) const;
    virtual bool IsItemShown(unsigned int n) const;
    virtual int GetItemFromPoint(const wxPoint& pt) const;

    // override some base class methods
    virtual bool Show(bool show = true);
    virtual bool Enable(bool enable = true);
    virtual bool CanBeFocused() const;
    virtual void SetFocus();
    virtual bool SetFont(const wxFont& font);
    virtual bool ContainsHWND(WXHWND hWnd) const;
#if wxUSE_TOOLTIPS
    virtual bool HasToolTips() const;
#endif // wxUSE_TOOLTIPS
#if wxUSE_HELP
    // override virtual function with a platform-independent implementation
    virtual wxString GetHelpTextAtPoint(const wxPoint & pt, wxHelpEvent::Origin origin) const
    {
        return wxRadioBoxBase::DoGetHelpTextAtPoint( this, pt, origin );
    }
#endif // wxUSE_HELP

    virtual bool Reparent(wxWindowBase *newParent);

    // returns true if the platform should explicitly apply a theme border
    virtual bool CanApplyThemeBorder() const { return false; }

    void SetLabelFont(const wxFont& WXUNUSED(font)) {}
    void SetButtonFont(const wxFont& font) { SetFont(font); }


    // implementation only from now on
    // -------------------------------

    // This function can be used to check if the given radio button HWND
    // belongs to one of our radio boxes. If it doesn't, NULL is returned.
    static wxRadioBox *GetFromRadioButtonHWND(WXHWND hwnd);

    virtual bool MSWCommand(WXUINT param, WXWORD id);
    void Command(wxCommandEvent& event);

    void SendNotificationEvent();

protected:
    // common part of all ctors
    void Init();

    // subclass one radio button
    void SubclassRadioButton(WXHWND hWndBtn);

    // get the max size of radio buttons
    wxSize GetMaxButtonSize() const;

    // get the total size occupied by the radio box buttons
    wxSize GetTotalButtonSize(const wxSize& sizeBtn) const;

    // Adjust all the buttons to the new window size.
    void PositionAllButtons(int x, int y, int width, int height);

    virtual void DoSetSize(int x, int y,
                           int width, int height,
                           int sizeFlags = wxSIZE_AUTO);
    virtual void DoMoveWindow(int x, int y, int width, int height);
    virtual wxSize DoGetBestSize() const;

#if wxUSE_TOOLTIPS
    virtual void DoSetItemToolTip(unsigned int n, wxToolTip * tooltip);
#endif

#ifndef __WXWINCE__
    virtual WXHRGN MSWGetRegionWithoutChildren();
#endif // __WXWINCE__

    // resolve ambiguity in base classes
    virtual wxBorder GetDefaultBorder() const { return wxRadioBoxBase::GetDefaultBorder(); }

    // the buttons we contain
    wxSubwindows *m_radioButtons;

    // and the special dummy button used only as a tab group boundary
    WXHWND m_dummyHwnd;
    wxWindowIDRef m_dummyId;

    // array of widths and heights of the buttons, may be wxDefaultCoord if the
    // corresponding quantity should be computed
    int *m_radioWidth;
    int *m_radioHeight;

    // currently selected button or wxNOT_FOUND if none
    int m_selectedButton;

private:
    DECLARE_DYNAMIC_CLASS(wxRadioBox)
    wxDECLARE_NO_COPY_CLASS(wxRadioBox);
};

#endif
    // _WX_RADIOBOX_H_
