/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/printmac.h
// Purpose:     wxWindowsPrinter, wxWindowsPrintPreview classes
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRINTWIN_H_
#define _WX_PRINTWIN_H_

#include "wx/prntbase.h"

/*
 * Represents the printer: manages printing a wxPrintout object
 */

class WXDLLIMPEXP_CORE wxMacPrinter: public wxPrinterBase
{
  DECLARE_DYNAMIC_CLASS(wxMacPrinter)

 public:
    wxMacPrinter(wxPrintDialogData *data = NULL);
    virtual ~wxMacPrinter();

    virtual bool Print(wxWindow *parent,
                       wxPrintout *printout,
                       bool prompt = true);
    virtual wxDC* PrintDialog(wxWindow *parent);
  virtual bool Setup(wxWindow *parent);

};

/*
 * wxPrintPreview
 * Programmer creates an object of this class to preview a wxPrintout.
 */

class WXDLLIMPEXP_CORE wxMacPrintPreview: public wxPrintPreviewBase
{
  DECLARE_CLASS(wxMacPrintPreview)

 public:
    wxMacPrintPreview(wxPrintout *printout,
                          wxPrintout *printoutForPrinting = NULL,
                          wxPrintDialogData *data = NULL);
    wxMacPrintPreview(wxPrintout *printout,
                          wxPrintout *printoutForPrinting,
                          wxPrintData *data);
    virtual ~wxMacPrintPreview();

  virtual bool Print(bool interactive);
    virtual void DetermineScaling();
};

#endif
    // _WX_PRINTWIN_H_
