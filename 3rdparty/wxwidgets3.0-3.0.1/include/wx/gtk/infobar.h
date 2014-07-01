///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/infobar.h
// Purpose:     native implementation of wxInfoBar for GTK+ 2.18 and later
// Author:      Vadim Zeitlin
// Created:     2009-09-26
// Copyright:   (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_INFOBAR_H_
#define _WX_GTK_INFOBAR_H_

#include "wx/generic/infobar.h"

// ----------------------------------------------------------------------------
// wxInfoBar for GTK+
// ----------------------------------------------------------------------------

// notice that the native GTK+ implementation is only available since
// (relatively recent) 2.18 so we inherit from the generic one to be able to
// fall back to it if GTK+ version is determined to be too old during run-time
class WXDLLIMPEXP_CORE wxInfoBar : public wxInfoBarGeneric
{
public:
    wxInfoBar() { Init(); }

    wxInfoBar(wxWindow *parent, wxWindowID winid = wxID_ANY)
    {
        Init();
        Create(parent, winid);
    }

    bool Create(wxWindow *parent, wxWindowID winid = wxID_ANY);

    virtual ~wxInfoBar();

    // implement base class methods
    // ----------------------------

    virtual void ShowMessage(const wxString& msg,
                             int flags = wxICON_INFORMATION);

    virtual void Dismiss();

    virtual void AddButton(wxWindowID btnid,
                           const wxString& label = wxString());

    virtual void RemoveButton(wxWindowID btnid);

    // implementation only
    // -------------------

    void GTKResponse(int btnid);

protected:
    virtual void DoApplyWidgetStyle(GtkRcStyle *style);

private:
    void Init() { m_impl = NULL; }

    // add a button with the given id/label and return its widget
    GtkWidget *GTKAddButton(wxWindowID btnid,
                            const wxString& label = wxString());


    // only used when the native implementation is really being used
    class wxInfoBarGTKImpl *m_impl;

    wxDECLARE_NO_COPY_CLASS(wxInfoBar);
};

#endif // _WX_GTK_INFOBAR_H_

