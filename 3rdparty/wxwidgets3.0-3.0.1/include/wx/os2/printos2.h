/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/printos2.h
// Purpose:     wxOS2Printer, wxOS2PrintPreview classes
// Author:      David Webster
// Modified by:
// Created:     10/14/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRINT_H_
#define _WX_PRINT_H_

#include "wx/prntbase.h"

#if wxUSE_PRINTING_ARCHITECTURE

/*
 * Represents the printer: manages printing a wxPrintout object
 */

class WXDLLIMPEXP_CORE wxOS2Printer: public wxPrinterBase
{
    DECLARE_DYNAMIC_CLASS(wxOS2Printer)

public:
    wxOS2Printer(wxPrintDialogData *data = NULL);
    virtual ~wxOS2Printer();

    virtual bool Print(wxWindow *parent, wxPrintout *printout, bool prompt = true);
    virtual wxDC* PrintDialog(wxWindow *parent);
    virtual bool Setup(wxWindow *parent);
private:
};

/*
 * wxOS2PrintPreview
 * Programmer creates an object of this class to preview a wxPrintout.
 */

class WXDLLIMPEXP_CORE wxOS2PrintPreview: public wxPrintPreviewBase
{
    DECLARE_CLASS(wxOS2PrintPreview)

public:
    wxOS2PrintPreview(wxPrintout *printout, wxPrintout *printoutForPrinting = NULL, wxPrintDialogData *data = NULL);
    wxOS2PrintPreview(wxPrintout *printout, wxPrintout *printoutForPrinting, wxPrintData *data);
    virtual ~wxOS2PrintPreview();

    virtual bool Print(bool interactive);
    virtual void DetermineScaling();
};

#endif // wxUSE_PRINTING_ARCHITECTURE

#endif
    // _WX_PRINT_H_
