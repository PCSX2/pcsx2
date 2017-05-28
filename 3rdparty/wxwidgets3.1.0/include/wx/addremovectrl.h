///////////////////////////////////////////////////////////////////////////////
// Name:        wx/addremovectrl.h
// Purpose:     wxAddRemoveCtrl declaration.
// Author:      Vadim Zeitlin
// Created:     2015-01-29
// Copyright:   (c) 2015 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ADDREMOVECTRL_H_
#define _WX_ADDREMOVECTRL_H_

#include "wx/panel.h"

#if wxUSE_ADDREMOVECTRL

extern WXDLLIMPEXP_DATA_ADV(const char) wxAddRemoveCtrlNameStr[];

// ----------------------------------------------------------------------------
// wxAddRemoveAdaptor: used by wxAddRemoveCtrl to work with the list control
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxAddRemoveAdaptor
{
public:
    // Default ctor and trivial but virtual dtor.
    wxAddRemoveAdaptor() { }
    virtual ~wxAddRemoveAdaptor() { }

    // Override to return the associated control.
    virtual wxWindow* GetItemsCtrl() const = 0;

    // Override to return whether a new item can be added to the control.
    virtual bool CanAdd() const = 0;

    // Override to return whether the currently selected item (if any) can be
    // removed from the control.
    virtual bool CanRemove() const = 0;

    // Called when an item should be added, can only be called if CanAdd()
    // currently returns true.
    virtual void OnAdd() = 0;

    // Called when the current item should be removed, can only be called if
    // CanRemove() currently returns true.
    virtual void OnRemove() = 0;

private:
    wxDECLARE_NO_COPY_CLASS(wxAddRemoveAdaptor);
};

// ----------------------------------------------------------------------------
// wxAddRemoveCtrl: a list-like control combined with add/remove buttons
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxAddRemoveCtrl : public wxPanel
{
public:
    wxAddRemoveCtrl()
    {
        Init();
    }

    wxAddRemoveCtrl(wxWindow* parent,
                    wxWindowID winid = wxID_ANY,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxDefaultSize,
                    long style = 0,
                    const wxString& name = wxAddRemoveCtrlNameStr)
    {
        Init();

        Create(parent, winid, pos, size, style, name);
    }

    bool Create(wxWindow* parent,
                wxWindowID winid = wxID_ANY,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxAddRemoveCtrlNameStr);

    virtual ~wxAddRemoveCtrl();

    // Must be called for the control to be usable, takes ownership of the
    // pointer.
    void SetAdaptor(wxAddRemoveAdaptor* adaptor);

    // Set tooltips to use for the add and remove buttons.
    void SetButtonsToolTips(const wxString& addtip, const wxString& removetip);

protected:
    virtual wxSize DoGetBestClientSize() const wxOVERRIDE;

private:
    // Common part of all ctors.
    void Init()
    {
        m_impl = NULL;
    }

    class wxAddRemoveImpl* m_impl;

    wxDECLARE_NO_COPY_CLASS(wxAddRemoveCtrl);
};

#endif // wxUSE_ADDREMOVECTRL

#endif // _WX_ADDREMOVECTRL_H_
