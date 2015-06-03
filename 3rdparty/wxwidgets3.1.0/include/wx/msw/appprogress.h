/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/appprogress.h
// Purpose:     wxAppProgressIndicator interface.
// Author:      Chaobin Zhang <zhchbin@gmail.com>
// Created:     2014-09-05
// Copyright:   (c) 2014 wxWidgets development team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_APPPROG_H_
#define _WX_MSW_APPPROG_H_

#include "wx/vector.h"

class WXDLLIMPEXP_FWD_CORE wxTaskBarButton;

class WXDLLIMPEXP_CORE wxAppProgressIndicator
    : public wxAppProgressIndicatorBase
{
public:
    wxAppProgressIndicator(wxWindow* parent = NULL, int maxValue = 100);
    virtual ~wxAppProgressIndicator();

    virtual bool IsAvailable() const wxOVERRIDE;

    virtual void SetValue(int value) wxOVERRIDE;
    virtual void SetRange(int range) wxOVERRIDE;
    virtual void Pulse() wxOVERRIDE;
    virtual void Reset() wxOVERRIDE;

private:
    int m_maxValue;

    wxVector<wxTaskBarButton*> m_taskBarButtons;

    wxDECLARE_NO_COPY_CLASS(wxAppProgressIndicator);
};

#endif  // _WX_MSW_APPPROG_H_
