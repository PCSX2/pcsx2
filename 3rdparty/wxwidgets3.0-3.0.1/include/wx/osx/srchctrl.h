/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/srchctrl.h
// Purpose:     mac carbon wxSearchCtrl class
// Author:      Vince Harron
// Created:     2006-02-19
// Copyright:   Vince Harron
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SEARCHCTRL_H_
#define _WX_SEARCHCTRL_H_

#if wxUSE_SEARCHCTRL

class wxSearchWidgetImpl;

class WXDLLIMPEXP_CORE wxSearchCtrl : public wxSearchCtrlBase
{
public:
    // creation
    // --------

    wxSearchCtrl();
    wxSearchCtrl(wxWindow *parent, wxWindowID id,
               const wxString& value = wxEmptyString,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = 0,
               const wxValidator& validator = wxDefaultValidator,
               const wxString& name = wxSearchCtrlNameStr);

    virtual ~wxSearchCtrl();

    bool Create(wxWindow *parent, wxWindowID id,
                const wxString& value = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxSearchCtrlNameStr);

    // get/set search button menu
    // --------------------------
    virtual void SetMenu( wxMenu* menu );
    virtual wxMenu* GetMenu();

    // get/set search options
    // ----------------------
    virtual void ShowSearchButton( bool show );
    virtual bool IsSearchButtonVisible() const;

    virtual void ShowCancelButton( bool show );
    virtual bool IsCancelButtonVisible() const;

    // TODO: In 2.9 these should probably be virtual, and declared in the base class...
    void        SetDescriptiveText(const wxString& text);
    wxString    GetDescriptiveText() const;

    virtual bool    HandleSearchFieldSearchHit() ;
    virtual bool    HandleSearchFieldCancelHit() ;

    wxSearchWidgetImpl * GetSearchPeer() const;

protected:

    wxSize DoGetBestSize() const;

    void Init();

    wxMenu *m_menu;

    wxString m_descriptiveText;

private:
    DECLARE_DYNAMIC_CLASS(wxSearchCtrl)

    DECLARE_EVENT_TABLE()
};

#endif // wxUSE_SEARCHCTRL

#endif // _WX_SEARCHCTRL_H_

