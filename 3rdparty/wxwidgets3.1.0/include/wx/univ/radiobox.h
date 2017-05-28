///////////////////////////////////////////////////////////////////////////////
// Name:        wx/univ/radiobox.h
// Purpose:     wxRadioBox declaration
// Author:      Vadim Zeitlin
// Modified by:
// Created:     11.09.00
// Copyright:   (c) 2000 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIV_RADIOBOX_H_
#define _WX_UNIV_RADIOBOX_H_

class WXDLLIMPEXP_FWD_CORE wxRadioButton;

#include "wx/statbox.h"
#include "wx/dynarray.h"

WX_DEFINE_EXPORTED_ARRAY_PTR(wxRadioButton *, wxArrayRadioButtons);

// ----------------------------------------------------------------------------
// wxRadioBox: a box full of radio buttons
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxRadioBox : public wxStaticBox,
                               public wxRadioBoxBase
{
public:
    // wxRadioBox construction
    wxRadioBox() { Init(); }

    wxRadioBox(wxWindow *parent,
               wxWindowID id,
               const wxString& title,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               int n = 0, const wxString *choices = NULL,
               int majorDim = 0,
               long style = wxRA_SPECIFY_COLS,
               const wxValidator& val = wxDefaultValidator,
               const wxString& name = wxRadioBoxNameStr)
    {
        Init();

        (void)Create(parent, id, title, pos, size, n, choices,
                     majorDim, style, val, name);
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
               const wxString& name = wxRadioBoxNameStr);

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& title,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                int n = 0, const wxString *choices = NULL,
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

    virtual ~wxRadioBox();

    // implement wxRadioBox interface
    virtual void SetSelection(int n) wxOVERRIDE;
    virtual int GetSelection() const wxOVERRIDE;

    virtual unsigned int GetCount() const wxOVERRIDE
        { return (unsigned int)m_buttons.GetCount(); }

    virtual wxString GetString(unsigned int n) const wxOVERRIDE;
    virtual void SetString(unsigned int n, const wxString& label) wxOVERRIDE;

    virtual bool Enable(unsigned int n, bool enable = true) wxOVERRIDE;
    virtual bool Show(unsigned int n, bool show = true) wxOVERRIDE;

    virtual bool IsItemEnabled(unsigned int n) const wxOVERRIDE;
    virtual bool IsItemShown(unsigned int n) const wxOVERRIDE;

    // we also override the wxControl methods to avoid virtual function hiding
    virtual bool Enable(bool enable = true) wxOVERRIDE;
    virtual bool Show(bool show = true) wxOVERRIDE;
    virtual wxString GetLabel() const wxOVERRIDE;
    virtual void SetLabel(const wxString& label) wxOVERRIDE;

    // we inherit a version always returning false from wxStaticBox, override
    // it to behave normally
    virtual bool AcceptsFocus() const wxOVERRIDE { return wxControl::AcceptsFocus(); }

#if wxUSE_TOOLTIPS
    virtual void DoSetToolTip( wxToolTip *tip );
#endif // wxUSE_TOOLTIPS

    // wxUniversal-only methods

    // another Append() version
    void Append(int n, const wxString *choices);

    // implementation only: called by wxRadioHookHandler
    void OnRadioButton(wxEvent& event);
    bool OnKeyDown(wxKeyEvent& event);

protected:
    virtual wxBorder GetDefaultBorder() const wxOVERRIDE { return wxBORDER_NONE; }

    // override the base class methods dealing with window positioning/sizing
    // as we must move/size the buttons as well
    virtual void DoMoveWindow(int x, int y, int width, int height) wxOVERRIDE;
    virtual wxSize DoGetBestClientSize() const wxOVERRIDE;

    // generate a radiobutton click event for the current item
    void SendRadioEvent();

    // common part of all ctors
    void Init();

    // calculate the max size of all buttons
    wxSize GetMaxButtonSize() const;

    // the currently selected radio button or -1
    int m_selection;

    // all radio buttons
    wxArrayRadioButtons m_buttons;

    // the event handler which is used to translate radiobutton events into
    // radiobox one
    wxEvtHandler *m_evtRadioHook;

private:
    wxDECLARE_DYNAMIC_CLASS(wxRadioBox);
};

#endif // _WX_UNIV_RADIOBOX_H_
