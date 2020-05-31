/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/colordlg.h
// Purpose:     wxColourDialog class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COLORDLG_H_
#define _WX_COLORDLG_H_

#include "wx/dialog.h"

// ----------------------------------------------------------------------------
// wxColourDialog: dialog for choosing a colours
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxColourDialog : public wxDialog
{
public:
    wxColourDialog() { Init(); }
    wxColourDialog(wxWindow *parent, wxColourData *data = NULL)
    {
        Init();

        Create(parent, data);
    }

    bool Create(wxWindow *parent, wxColourData *data = NULL);

    wxColourData& GetColourData() { return m_colourData; }

    // override some base class virtuals
    virtual void SetTitle(const wxString& title);
    virtual wxString GetTitle() const;

    virtual int ShowModal();

    // wxMSW-specific implementation from now on
    // -----------------------------------------

    // called from the hook procedure on WM_INITDIALOG reception
    virtual void MSWOnInitDone(WXHWND hDlg);

protected:
    // common part of all ctors
    void Init();

#if !(defined(__SMARTPHONE__) && defined(__WXWINCE__))
    virtual void DoGetPosition( int *x, int *y ) const;
    virtual void DoGetSize(int *width, int *height) const;
    virtual void DoGetClientSize(int *width, int *height) const;
    virtual void DoMoveWindow(int x, int y, int width, int height);
    virtual void DoCentre(int dir);
#endif // !(__SMARTPHONE__ && __WXWINCE__)

    wxColourData        m_colourData;
    wxString            m_title;

    // indicates that the dialog should be centered in this direction if non 0
    // (set by DoCentre(), used by MSWOnInitDone())
    int m_centreDir;

    // true if DoMoveWindow() had been called
    bool m_movedWindow;


    DECLARE_DYNAMIC_CLASS_NO_COPY(wxColourDialog)
};

#endif // _WX_COLORDLG_H_
