/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/printdlg.h
// Purpose:     wxPrintDialog, wxPageSetupDialog classes
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRINTDLG_H_
#define _WX_PRINTDLG_H_

#if wxUSE_PRINTING_ARCHITECTURE

#include "wx/dialog.h"
#include "wx/cmndata.h"
#include "wx/prntbase.h"
#include "wx/printdlg.h"

class WXDLLIMPEXP_FWD_CORE wxDC;
class WinPrinter;

//----------------------------------------------------------------------------
// wxWindowsPrintNativeData
//----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxWindowsPrintNativeData: public wxPrintNativeDataBase
{
public:
    wxWindowsPrintNativeData();
    virtual ~wxWindowsPrintNativeData();

    virtual bool TransferTo( wxPrintData &data );
    virtual bool TransferFrom( const wxPrintData &data );

    virtual bool Ok() const { return IsOk(); }
    virtual bool IsOk() const;

    void InitializeDevMode(const wxString &printerName = wxEmptyString, WinPrinter* printer = NULL);
    void* GetDevMode() const { return m_devMode; }
    void SetDevMode(void* data) { m_devMode = data; }
    void* GetDevNames() const { return m_devNames; }
    void SetDevNames(void* data) { m_devNames = data; }

private:
    void* m_devMode;
    void* m_devNames;

    short m_customWindowsPaperId;

private:
    DECLARE_DYNAMIC_CLASS(wxWindowsPrintNativeData)
};

// ---------------------------------------------------------------------------
// wxWindowsPrintDialog: the MSW dialog for printing
// ---------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxWindowsPrintDialog : public wxPrintDialogBase
{
public:
    wxWindowsPrintDialog(wxWindow *parent, wxPrintDialogData* data = NULL);
    wxWindowsPrintDialog(wxWindow *parent, wxPrintData* data);
    virtual ~wxWindowsPrintDialog();

    bool Create(wxWindow *parent, wxPrintDialogData* data = NULL);
    virtual int ShowModal();

    wxPrintDialogData& GetPrintDialogData() { return m_printDialogData; }
    wxPrintData& GetPrintData() { return m_printDialogData.GetPrintData(); }

    virtual wxDC *GetPrintDC();

private:
    wxPrintDialogData m_printDialogData;
    wxPrinterDC*      m_printerDC;
    bool              m_destroyDC;
    wxWindow*         m_dialogParent;

private:
    bool ConvertToNative( wxPrintDialogData &data );
    bool ConvertFromNative( wxPrintDialogData &data );

    // holds MSW handle
    void*             m_printDlg;

private:
    wxDECLARE_NO_COPY_CLASS(wxWindowsPrintDialog);
    DECLARE_CLASS(wxWindowsPrintDialog)
};

// ---------------------------------------------------------------------------
// wxWindowsPageSetupDialog: the MSW page setup dialog
// ---------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxWindowsPageSetupDialog: public wxPageSetupDialogBase
{
public:
    wxWindowsPageSetupDialog();
    wxWindowsPageSetupDialog(wxWindow *parent, wxPageSetupDialogData *data = NULL);
    virtual ~wxWindowsPageSetupDialog();

    bool Create(wxWindow *parent, wxPageSetupDialogData *data = NULL);
    virtual int ShowModal();
    bool ConvertToNative( wxPageSetupDialogData &data );
    bool ConvertFromNative( wxPageSetupDialogData &data );

    virtual wxPageSetupDialogData& GetPageSetupDialogData() { return m_pageSetupData; }

private:
    wxPageSetupDialogData   m_pageSetupData;
    wxWindow*               m_dialogParent;

    // holds MSW handle
    void*                   m_pageDlg;

private:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxWindowsPageSetupDialog)
};

#endif // wxUSE_PRINTING_ARCHITECTURE

#endif
    // _WX_PRINTDLG_H_
