/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/dialog.h
// Purpose:     wxDialog class
// Author:      David Webster
// Modified by:
// Created:     10/14/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DIALOG_H_
#define _WX_DIALOG_H_

#include "wx/panel.h"

WXDLLIMPEXP_DATA_CORE(extern const char) wxDialogNameStr[];

class WXDLLIMPEXP_FWD_CORE wxDialogModalData;

//
// Dialog boxes
//
class WXDLLIMPEXP_CORE wxDialog: public wxDialogBase
{
public:

  inline wxDialog() { Init(); }

    // full ctor
    wxDialog(wxWindow *parent, wxWindowID id,
             const wxString& title,
             const wxPoint& pos = wxDefaultPosition,
             const wxSize& size = wxDefaultSize,
             long style = wxDEFAULT_DIALOG_STYLE,
             const wxString& name = wxDialogNameStr)
    {
        Init();

        (void)Create(parent, id, title, pos, size, style, name);
    }

    bool Create( wxWindow*       pParent
                ,wxWindowID      vId
                ,const wxString& rsTitle
                ,const wxPoint&  rPos = wxDefaultPosition
                ,const wxSize&   rSize = wxDefaultSize
                ,long            lStyle = wxDEFAULT_DIALOG_STYLE
                ,const wxString& rsName = wxDialogNameStr
               );
    virtual ~wxDialog();

    // return true if we're showing the dialog modally
    virtual bool IsModal() const { return m_modalData != NULL; }

    // show the dialog modally and return the value passed to EndModal()
    virtual int ShowModal();

    // may be called to terminate the dialog with the given return code
    virtual void EndModal(int retCode);

    // implementation only from now on
    // -------------------------------

    // override some base class virtuals
    virtual bool Show(bool show = true);

    //
    // Callbacks
    //
    virtual MRESULT OS2WindowProc( WXUINT   uMessage
                                  ,WXWPARAM wParam
                                  ,WXLPARAM lParam
                                 );
#if WXWIN_COMPATIBILITY_2_6

    // Constructor with a modal flag, but no window id - the old convention
    wxDEPRECATED( wxDialog( wxWindow*       pParent
                           ,const wxString& rsTitle
                           ,bool            bModal
                           ,int             nX = -1
                           ,int             nY = -1
                           ,int             nWidth = 500
                           ,int             nHeight = 500
                           ,long            lStyle = wxDEFAULT_DIALOG_STYLE
                           ,const wxString& rsName = wxDialogNameStr
                          ) );

    // just call Show() or ShowModal()
    wxDEPRECATED( void SetModal(bool bFlag) );

    // use IsModal()
    wxDEPRECATED( bool IsModalShowing() const );

#endif // WXWIN_COMPATIBILITY_2_6

protected:
    //
    // Common part of all ctors
    //
    void Init(void);

private:
    wxWindow*                       m_pOldFocus;
    bool        m_endModalCalled; // allow for closing within InitDialog

    // this pointer is non-NULL only while the modal event loop is running
    wxDialogModalData *m_modalData;

    //
    // While we are showing a modal dialog we disable the other windows using
    // this object
    //
    class wxWindowDisabler*         m_pWindowDisabler;

    DECLARE_DYNAMIC_CLASS(wxDialog)
    wxDECLARE_NO_COPY_CLASS(wxDialog);
}; // end of CLASS wxDialog

#endif // _WX_DIALOG_H_
