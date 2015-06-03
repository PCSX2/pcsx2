/////////////////////////////////////////////////////////////////////////////
// Name:        include/wx/msw/taskbarbutton.h
// Purpose:     Defines wxTaskBarButtonImpl class.
// Author:      Chaobin Zhang <zhchbin@gmail.com>
// Created:     2014-06-01
// Copyright:   (c) 2014 wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef  _WX_MSW_TASKBARBUTTON_H_
#define  _WX_MSW_TASKBARBUTTON_H_

#include "wx/defs.h"

#if wxUSE_TASKBARBUTTON

#include "wx/vector.h"
#include "wx/taskbarbutton.h"

class WXDLLIMPEXP_FWD_CORE wxITaskbarList3;

class WXDLLIMPEXP_CORE wxTaskBarButtonImpl : public wxTaskBarButton
{
public:
    virtual ~wxTaskBarButtonImpl();

    virtual void SetProgressRange(int range) wxOVERRIDE;
    virtual void SetProgressValue(int value) wxOVERRIDE;
    virtual void PulseProgress() wxOVERRIDE;
    virtual void Show(bool show = true) wxOVERRIDE;
    virtual void Hide() wxOVERRIDE;
    virtual void SetThumbnailTooltip(const wxString& tooltip) wxOVERRIDE;
    virtual void SetProgressState(wxTaskBarButtonState state) wxOVERRIDE;
    virtual void SetOverlayIcon(const wxIcon& icon,
        const wxString& description = wxString()) wxOVERRIDE;
    virtual void SetThumbnailClip(const wxRect& rect) wxOVERRIDE;
    virtual void SetThumbnailContents(const wxWindow *child) wxOVERRIDE;
    virtual bool InsertThumbBarButton(size_t pos,
                                      wxThumbBarButton *button) wxOVERRIDE;
    virtual bool AppendThumbBarButton(wxThumbBarButton *button) wxOVERRIDE;
    virtual bool AppendSeparatorInThumbBar() wxOVERRIDE;
    virtual wxThumbBarButton* RemoveThumbBarButton(
        wxThumbBarButton *button) wxOVERRIDE;
    virtual wxThumbBarButton* RemoveThumbBarButton(int id) wxOVERRIDE;
    wxThumbBarButton* GetThumbBarButtonByIndex(size_t index);
    bool InitOrUpdateThumbBarButtons();
    virtual void Realize() wxOVERRIDE;

private:
    // This ctor is only used by wxTaskBarButton::New()
    wxTaskBarButtonImpl(wxITaskbarList3* taskbarList, wxWindow* parent);

    wxWindow* m_parent;
    wxITaskbarList3 *m_taskbarList;

    typedef wxVector<wxThumbBarButton*> wxThumbBarButtons;
    wxThumbBarButtons m_thumbBarButtons;

    int m_progressRange;
    int m_progressValue;
    wxTaskBarButtonState m_progressState;
    wxString m_thumbnailTooltip;
    wxIcon m_overlayIcon;
    wxString m_overlayIconDescription;
    wxRect m_thumbnailClipRect;
    bool m_hasInitThumbnailToolbar;

    friend wxTaskBarButton* wxTaskBarButton::New(wxWindow*);

    wxDECLARE_NO_COPY_CLASS(wxTaskBarButtonImpl);
};

#endif // wxUSE_TASKBARBUTTON

#endif  // _WX_MSW_TASKBARBUTTON_H_
