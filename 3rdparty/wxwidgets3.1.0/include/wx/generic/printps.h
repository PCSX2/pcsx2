/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/printps.h
// Purpose:     wxPostScriptPrinter, wxPostScriptPrintPreview
//              wxGenericPageSetupDialog
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __PRINTPSH__
#define __PRINTPSH__

#include "wx/prntbase.h"

#if wxUSE_PRINTING_ARCHITECTURE && wxUSE_POSTSCRIPT

// ----------------------------------------------------------------------------
// Represents the printer: manages printing a wxPrintout object
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPostScriptPrinter : public wxPrinterBase
{
public:
    wxPostScriptPrinter(wxPrintDialogData *data = NULL);
    virtual ~wxPostScriptPrinter();

    virtual bool Print(wxWindow *parent, wxPrintout *printout, bool prompt = true) wxOVERRIDE;
    virtual wxDC* PrintDialog(wxWindow *parent) wxOVERRIDE;
    virtual bool Setup(wxWindow *parent) wxOVERRIDE;

private:
    wxDECLARE_DYNAMIC_CLASS(wxPostScriptPrinter);
};

// ----------------------------------------------------------------------------
// wxPrintPreview: programmer creates an object of this class to preview a
// wxPrintout.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPostScriptPrintPreview : public wxPrintPreviewBase
{
public:
    wxPostScriptPrintPreview(wxPrintout *printout,
                             wxPrintout *printoutForPrinting = NULL,
                             wxPrintDialogData *data = NULL);
    wxPostScriptPrintPreview(wxPrintout *printout,
                             wxPrintout *printoutForPrinting,
                             wxPrintData *data);

    virtual ~wxPostScriptPrintPreview();

    virtual bool Print(bool interactive) wxOVERRIDE;
    virtual void DetermineScaling() wxOVERRIDE;

private:
    void Init(wxPrintout *printout, wxPrintout *printoutForPrinting);

private:
    wxDECLARE_CLASS(wxPostScriptPrintPreview);
};

#endif

#endif
// __PRINTPSH__
