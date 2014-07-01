/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/print.h
// Purpose:     wxPrinter, wxPrintPreview classes
// Author:      David Webster
// Modified by:
// Created:     10/14/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRINT_H_
#define _WX_PRINT_H_

#include "wx/prntbase.h"

/*
 * Represents the printer: manages printing a wxPrintout object
 */

class WXDLLIMPEXP_CORE wxOS2Printer: public wxPrinterBase
{
  DECLARE_DYNAMIC_CLASS(wxPrinter)

 public:
  wxOS2Printer(wxPrintData *data = NULL);
  virtual ~wxOS2Printer();

  virtual bool Print(wxWindow *parent, wxPrintout *printout, bool prompt = TRUE);
  virtual wxDC* PrintDialog(wxWindow *parent);
  virtual bool Setup(wxWindow *parent);
private:
};

/*
 * wxPrintPreview
 * Programmer creates an object of this class to preview a wxPrintout.
 */

class WXDLLIMPEXP_CORE wxOS2PrintPreview: public wxPrintPreviewBase
{
  DECLARE_CLASS(wxPrintPreview)

 public:
  wxOS2PrintPreview(wxPrintout *printout, wxPrintout *printoutForPrinting = NULL, wxPrintData *data = NULL);
  virtual ~wxOS2PrintPreview();

  virtual bool Print(bool interactive);
  virtual void DetermineScaling();
};

#endif
    // _WX_PRINT_H_
