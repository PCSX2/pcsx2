/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/printdlg.h
// Purpose:     wxPrintDialog, wxPageSetupDialog classes.
//              Use generic, PostScript version if no
//              platform-specific implementation.
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRINTDLG_H_
#define _WX_PRINTDLG_H_

#include "wx/dialog.h"
#include "wx/cmndata.h"
#include "wx/printdlg.h"
#include "wx/prntbase.h"

/*
 * wxMacPrintDialog
 * The Mac dialog for printing
 */

class WXDLLIMPEXP_FWD_CORE wxDC;
class WXDLLIMPEXP_CORE wxMacPrintDialog: public wxPrintDialogBase
{
public:
    wxMacPrintDialog();
    wxMacPrintDialog(wxWindow *parent, wxPrintDialogData* data = NULL);
    wxMacPrintDialog(wxWindow *parent, wxPrintData* data );
    virtual ~wxMacPrintDialog();

    bool Create(wxWindow *parent, wxPrintDialogData* data = NULL);
    virtual int ShowModal();

    virtual wxPrintDialogData& GetPrintDialogData() { return m_printDialogData; }
    virtual wxPrintData& GetPrintData() { return m_printDialogData.GetPrintData(); }
    virtual wxDC *GetPrintDC();

private:
    wxPrintDialogData   m_printDialogData;
    wxDC*               m_printerDC;
    bool                m_destroyDC;
    wxWindow*           m_dialogParent;

private:
    DECLARE_DYNAMIC_CLASS(wxPrintDialog)
};

/*
 * wxMacPageSetupDialog
 * The Mac page setup dialog
 */

class WXDLLIMPEXP_CORE wxMacPageSetupDialog: public wxPageSetupDialogBase
{
public:
    wxMacPageSetupDialog(wxWindow *parent, wxPageSetupDialogData *data = NULL);
    virtual ~wxMacPageSetupDialog();

    virtual wxPageSetupDialogData& GetPageSetupDialogData();

    bool Create(wxWindow *parent, wxPageSetupDialogData *data = NULL);
    virtual int ShowModal();

private:
    wxPageSetupDialogData   m_pageSetupData;
    wxWindow*               m_dialogParent;

private:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxMacPageSetupDialog)
};

class WXDLLIMPEXP_FWD_CORE wxTextCtrl;

/*
* wxMacPageMarginsDialog
* A Mac dialog for setting the page margins separately from page setup since
* (native) wxMacPageSetupDialog doesn't let you set margins.
*/

class WXDLLIMPEXP_CORE wxMacPageMarginsDialog : public wxDialog
{
public:
    wxMacPageMarginsDialog(wxFrame* parent, wxPageSetupDialogData* data);
    bool TransferToWindow();
    bool TransferDataFromWindow();

    virtual wxPageSetupDialogData& GetPageSetupDialogData() { return *m_pageSetupDialogData; }

private:
    wxPageSetupDialogData* m_pageSetupDialogData;

    wxPoint m_MinMarginTopLeft;
    wxPoint m_MinMarginBottomRight;
    wxTextCtrl *m_LeftMargin;
    wxTextCtrl *m_TopMargin;
    wxTextCtrl *m_RightMargin;
    wxTextCtrl *m_BottomMargin;

    void GetMinMargins();
    bool CheckValue(wxTextCtrl* textCtrl, int *value, int minValue, const wxString& name);

private:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxMacPageMarginsDialog)
};


#endif    // _WX_PRINTDLG_H_
