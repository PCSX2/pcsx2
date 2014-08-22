/////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/dialog.h
// Purpose:     wxDialog class
// Author:      Julian Smart
// Modified by:
// Created:     17/09/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DIALOG_H_
#define _WX_DIALOG_H_

class WXDLLIMPEXP_FWD_CORE wxEventLoop;

// Dialog boxes
class WXDLLIMPEXP_CORE wxDialog : public wxDialogBase
{
public:
    wxDialog();

    wxDialog(wxWindow *parent, wxWindowID id,
        const wxString& title,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE,
        const wxString& name = wxDialogNameStr)
    {
        Create(parent, id, title, pos, size, style, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
        const wxString& title,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxDEFAULT_DIALOG_STYLE,
        const wxString& name = wxDialogNameStr);

    virtual ~wxDialog();

    virtual bool Destroy();

    virtual bool Show(bool show = true);

    void SetTitle(const wxString& title);

    void SetModal(bool flag);

    virtual bool IsModal() const
    { return m_modalShowing; }

    virtual int ShowModal();
    virtual void EndModal(int retCode);

    // Implementation
    virtual void ChangeFont(bool keepOriginalSize = true);
    virtual void ChangeBackgroundColour();
    virtual void ChangeForegroundColour();
    WXWidget GetTopWidget() const { return m_mainWidget; }
    WXWidget GetClientWidget() const { return m_mainWidget; }

private:
    virtual bool XmDoCreateTLW(wxWindow* parent,
                               wxWindowID id,
                               const wxString& title,
                               const wxPoint& pos,
                               const wxSize& size,
                               long style,
                               const wxString& name);


    //// Motif-specific
    bool          m_modalShowing;
    wxEventLoop*  m_eventLoop;

protected:
    virtual void DoSetSize(int x, int y,
                           int width, int height,
                           int sizeFlags = wxSIZE_AUTO);

    virtual void DoSetClientSize(int width, int height);


private:
    DECLARE_DYNAMIC_CLASS(wxDialog)
};

#endif // _WX_DIALOG_H_
