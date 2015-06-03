///////////////////////////////////////////////////////////////////////////////
// Name:        wx/univ/statusbr.h
// Purpose:     wxStatusBarUniv: wxStatusBar for wxUniversal declaration
// Author:      Vadim Zeitlin
// Modified by:
// Created:     14.10.01
// Copyright:   (c) 2001 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIV_STATUSBR_H_
#define _WX_UNIV_STATUSBR_H_

#include "wx/univ/inpcons.h"
#include "wx/arrstr.h"

// ----------------------------------------------------------------------------
// wxStatusBarUniv: a window near the bottom of the frame used for status info
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxStatusBarUniv : public wxStatusBarBase
{
public:
    wxStatusBarUniv() { Init(); }

    wxStatusBarUniv(wxWindow *parent,
                    wxWindowID id = wxID_ANY,
                    long style = wxSTB_DEFAULT_STYLE,
                    const wxString& name = wxPanelNameStr)
    {
        Init();

        (void)Create(parent, id, style, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id = wxID_ANY,
                long style = wxSTB_DEFAULT_STYLE,
                const wxString& name = wxPanelNameStr);

    // implement base class methods
    virtual void SetFieldsCount(int number = 1, const int *widths = NULL) wxOVERRIDE;
    virtual void SetStatusWidths(int n, const int widths[]) wxOVERRIDE;

    virtual bool GetFieldRect(int i, wxRect& rect) const wxOVERRIDE;
    virtual void SetMinHeight(int height) wxOVERRIDE;

    virtual int GetBorderX() const wxOVERRIDE;
    virtual int GetBorderY() const wxOVERRIDE;

    // wxInputConsumer pure virtual
    virtual wxWindow *GetInputWindow() const wxOVERRIDE
        { return const_cast<wxStatusBar*>(this); }

protected:
    virtual void DoUpdateStatusText(int i) wxOVERRIDE;

    // recalculate the field widths
    void OnSize(wxSizeEvent& event);

    // draw the statusbar
    virtual void DoDraw(wxControlRenderer *renderer) wxOVERRIDE;

    // tell them about our preferred height
    virtual wxSize DoGetBestSize() const wxOVERRIDE;

    // override DoSetSize() to prevent the status bar height from changing
    virtual void DoSetSize(int x, int y,
                           int width, int height,
                           int sizeFlags = wxSIZE_AUTO) wxOVERRIDE;

    // get the (fixed) status bar height
    wxCoord GetHeight() const;

    // get the rectangle containing all the fields and the border between them
    //
    // also updates m_widthsAbs if necessary
    wxRect GetTotalFieldRect(wxCoord *borderBetweenFields);

    // get the rect for this field without ani side effects (see code)
    wxRect DoGetFieldRect(int n) const;

    // common part of all ctors
    void Init();

private:
    // the current status fields strings
    //wxArrayString m_statusText;

    // the absolute status fields widths
    wxArrayInt m_widthsAbs;

    wxDECLARE_DYNAMIC_CLASS(wxStatusBarUniv);
    wxDECLARE_EVENT_TABLE();
    WX_DECLARE_INPUT_CONSUMER()
};

#endif // _WX_UNIV_STATUSBR_H_

