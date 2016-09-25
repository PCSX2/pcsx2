/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/prntbase.cpp
// Purpose:     Printing framework base class implementation
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_PRINTING_ARCHITECTURE

#include "wx/dcprint.h"

#ifndef WX_PRECOMP
    #if defined(__WXMSW__)
        #include "wx/msw/wrapcdlg.h"
    #endif // MSW
    #include "wx/utils.h"
    #include "wx/dc.h"
    #include "wx/app.h"
    #include "wx/math.h"
    #include "wx/msgdlg.h"
    #include "wx/layout.h"
    #include "wx/choice.h"
    #include "wx/button.h"
    #include "wx/bmpbuttn.h"
    #include "wx/settings.h"
    #include "wx/dcmemory.h"
    #include "wx/dcclient.h"
    #include "wx/stattext.h"
    #include "wx/intl.h"
    #include "wx/textdlg.h"
    #include "wx/sizer.h"
    #include "wx/module.h"
#endif // !WX_PRECOMP

#include "wx/prntbase.h"
#include "wx/printdlg.h"
#include "wx/print.h"
#include "wx/dcprint.h"
#include "wx/artprov.h"

#include <stdlib.h>
#include <string.h>

#if defined(__WXMSW__) && !defined(__WXUNIVERSAL__)
#include "wx/msw/printdlg.h"
#include "wx/msw/dcprint.h"
#elif defined(__WXMAC__)
#include "wx/osx/printdlg.h"
#include "wx/osx/private/print.h"
#include "wx/osx/dcprint.h"
#elif defined(__WXPM__)
#include "wx/os2/dcprint.h"
#include "wx/generic/prntdlgg.h"
#else
#include "wx/generic/prntdlgg.h"
#include "wx/dcps.h"
#endif

#ifdef __WXMSW__
    #ifndef __WIN32__
        #include <print.h>
    #endif
#endif // __WXMSW__

// The value traditionally used as the default max page number and meaning
// "infinitely many". It should probably be documented and exposed, but for now
// at least use it here instead of hardcoding the number.
static const int DEFAULT_MAX_PAGES = 32000;

//----------------------------------------------------------------------------
// wxPrintFactory
//----------------------------------------------------------------------------

wxPrintFactory *wxPrintFactory::m_factory = NULL;

void wxPrintFactory::SetPrintFactory( wxPrintFactory *factory )
{
    if (wxPrintFactory::m_factory)
        delete wxPrintFactory::m_factory;

    wxPrintFactory::m_factory = factory;
}

wxPrintFactory *wxPrintFactory::GetFactory()
{
    if (!wxPrintFactory::m_factory)
        wxPrintFactory::m_factory = new wxNativePrintFactory;

    return wxPrintFactory::m_factory;
}

//----------------------------------------------------------------------------
// wxNativePrintFactory
//----------------------------------------------------------------------------

wxPrinterBase *wxNativePrintFactory::CreatePrinter( wxPrintDialogData *data )
{
#if defined(__WXMSW__) && !defined(__WXUNIVERSAL__)
    return new wxWindowsPrinter( data );
#elif defined(__WXMAC__)
    return new wxMacPrinter( data );
#elif defined(__WXPM__)
    return new wxOS2Printer( data );
#else
    return new wxPostScriptPrinter( data );
#endif
}

wxPrintPreviewBase *wxNativePrintFactory::CreatePrintPreview( wxPrintout *preview,
    wxPrintout *printout, wxPrintDialogData *data )
{
#if defined(__WXMSW__) && !defined(__WXUNIVERSAL__)
    return new wxWindowsPrintPreview( preview, printout, data );
#elif defined(__WXMAC__)
    return new wxMacPrintPreview( preview, printout, data );
#elif defined(__WXPM__)
    return new wxOS2PrintPreview( preview, printout, data );
#else
    return new wxPostScriptPrintPreview( preview, printout, data );
#endif
}

wxPrintPreviewBase *wxNativePrintFactory::CreatePrintPreview( wxPrintout *preview,
    wxPrintout *printout, wxPrintData *data )
{
#if defined(__WXMSW__) && !defined(__WXUNIVERSAL__)
    return new wxWindowsPrintPreview( preview, printout, data );
#elif defined(__WXMAC__)
    return new wxMacPrintPreview( preview, printout, data );
#elif defined(__WXPM__)
    return new wxOS2PrintPreview( preview, printout, data );
#else
    return new wxPostScriptPrintPreview( preview, printout, data );
#endif
}

wxPrintDialogBase *wxNativePrintFactory::CreatePrintDialog( wxWindow *parent,
                                                  wxPrintDialogData *data )
{
#if defined(__WXMSW__) && !defined(__WXUNIVERSAL__)
    return new wxWindowsPrintDialog( parent, data );
#elif defined(__WXMAC__)
    return new wxMacPrintDialog( parent, data );
#else
    return new wxGenericPrintDialog( parent, data );
#endif
}

wxPrintDialogBase *wxNativePrintFactory::CreatePrintDialog( wxWindow *parent,
                                                  wxPrintData *data )
{
#if defined(__WXMSW__) && !defined(__WXUNIVERSAL__)
    return new wxWindowsPrintDialog( parent, data );
#elif defined(__WXMAC__)
    return new wxMacPrintDialog( parent, data );
#else
    return new wxGenericPrintDialog( parent, data );
#endif
}

wxPageSetupDialogBase *wxNativePrintFactory::CreatePageSetupDialog( wxWindow *parent,
                                                  wxPageSetupDialogData *data )
{
#if defined(__WXMSW__) && !defined(__WXUNIVERSAL__)
    return new wxWindowsPageSetupDialog( parent, data );
#elif defined(__WXMAC__)
    return new wxMacPageSetupDialog( parent, data );
#else
    return new wxGenericPageSetupDialog( parent, data );
#endif
}

bool wxNativePrintFactory::HasPrintSetupDialog()
{
#if defined(__WXMSW__) && !defined(__WXUNIVERSAL__)
    return false;
#elif defined(__WXMAC__)
    return false;
#else
    // Only here do we need to provide the print setup
    // dialog ourselves, the other platforms either have
    // none, don't make it accessible or let you configure
    // the printer from the wxPrintDialog anyway.
    return true;
#endif

}

wxDialog *wxNativePrintFactory::CreatePrintSetupDialog( wxWindow *parent,
                                                        wxPrintData *data )
{
#if defined(__WXMSW__) && !defined(__WXUNIVERSAL__)
    wxUnusedVar(parent);
    wxUnusedVar(data);
    return NULL;
#elif defined(__WXMAC__)
    wxUnusedVar(parent);
    wxUnusedVar(data);
    return NULL;
#else
    // Only here do we need to provide the print setup
    // dialog ourselves, the other platforms either have
    // none, don't make it accessible or let you configure
    // the printer from the wxPrintDialog anyway.
    return new wxGenericPrintSetupDialog( parent, data );
#endif
}

wxDCImpl* wxNativePrintFactory::CreatePrinterDCImpl( wxPrinterDC *owner, const wxPrintData& data )
{
#if defined(__WXGTK__) || defined(__WXMOTIF__) || ( defined(__WXUNIVERSAL__) && !defined(__WXMAC__) )
    return new wxPostScriptDCImpl( owner, data );
#else
    return new wxPrinterDCImpl( owner, data );
#endif
}

bool wxNativePrintFactory::HasOwnPrintToFile()
{
    // Only relevant for PostScript and here the
    // setup dialog provides no "print to file"
    // option. In the GNOME setup dialog, the
    // setup dialog has its own print to file.
    return false;
}

bool wxNativePrintFactory::HasPrinterLine()
{
    // Only relevant for PostScript for now
    return true;
}

wxString wxNativePrintFactory::CreatePrinterLine()
{
    // Only relevant for PostScript for now

    // We should query "lpstat -d" here
    return _("Generic PostScript");
}

bool wxNativePrintFactory::HasStatusLine()
{
    // Only relevant for PostScript for now
    return true;
}

wxString wxNativePrintFactory::CreateStatusLine()
{
    // Only relevant for PostScript for now

    // We should query "lpstat -r" or "lpstat -p" here
    return _("Ready");
}

wxPrintNativeDataBase *wxNativePrintFactory::CreatePrintNativeData()
{
#if defined(__WXMSW__) && !defined(__WXUNIVERSAL__)
    return new wxWindowsPrintNativeData;
#elif defined(__WXMAC__)
    return wxOSXCreatePrintData();
#else
    return new wxPostScriptPrintNativeData;
#endif
}

//----------------------------------------------------------------------------
// wxPrintNativeDataBase
//----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxPrintNativeDataBase, wxObject)

wxPrintNativeDataBase::wxPrintNativeDataBase()
{
    m_ref = 1;
}

//----------------------------------------------------------------------------
// wxPrintFactoryModule
//----------------------------------------------------------------------------

class wxPrintFactoryModule: public wxModule
{
public:
    wxPrintFactoryModule() {}
    bool OnInit() { return true; }
    void OnExit() { wxPrintFactory::SetPrintFactory( NULL ); }

private:
    DECLARE_DYNAMIC_CLASS(wxPrintFactoryModule)
};

IMPLEMENT_DYNAMIC_CLASS(wxPrintFactoryModule, wxModule)

//----------------------------------------------------------------------------
// wxPrinterBase
//----------------------------------------------------------------------------

IMPLEMENT_CLASS(wxPrinterBase, wxObject)

wxPrinterBase::wxPrinterBase(wxPrintDialogData *data)
{
    m_currentPrintout = NULL;
    sm_abortWindow = NULL;
    sm_abortIt = false;
    if (data)
        m_printDialogData = (*data);
    sm_lastError = wxPRINTER_NO_ERROR;
}

wxWindow *wxPrinterBase::sm_abortWindow = NULL;
bool wxPrinterBase::sm_abortIt = false;
wxPrinterError wxPrinterBase::sm_lastError = wxPRINTER_NO_ERROR;

wxPrinterBase::~wxPrinterBase()
{
}

wxPrintAbortDialog *wxPrinterBase::CreateAbortWindow(wxWindow *parent, wxPrintout * printout)
{
    return new wxPrintAbortDialog(parent, printout->GetTitle());
}

void wxPrinterBase::ReportError(wxWindow *parent, wxPrintout *WXUNUSED(printout), const wxString& message)
{
    wxMessageBox(message, _("Printing Error"), wxOK, parent);
}

wxPrintDialogData& wxPrinterBase::GetPrintDialogData() const
{
    return (wxPrintDialogData&) m_printDialogData;
}

//----------------------------------------------------------------------------
// wxPrinter
//----------------------------------------------------------------------------

IMPLEMENT_CLASS(wxPrinter, wxPrinterBase)

wxPrinter::wxPrinter(wxPrintDialogData *data)
{
    m_pimpl = wxPrintFactory::GetFactory()->CreatePrinter( data );
}

wxPrinter::~wxPrinter()
{
    delete m_pimpl;
}

wxPrintAbortDialog *wxPrinter::CreateAbortWindow(wxWindow *parent, wxPrintout *printout)
{
    return m_pimpl->CreateAbortWindow( parent, printout );
}

void wxPrinter::ReportError(wxWindow *parent, wxPrintout *printout, const wxString& message)
{
    m_pimpl->ReportError( parent, printout, message );
}

bool wxPrinter::Setup(wxWindow *parent)
{
    return m_pimpl->Setup( parent );
}

bool wxPrinter::Print(wxWindow *parent, wxPrintout *printout, bool prompt)
{
    if ( !prompt && m_printDialogData.GetToPage() == 0 )
    {
        // If the dialog is not shown, set the pages range to print everything
        // by default (as otherwise we wouldn't print anything at all which is
        // certainly not a reasonable default behaviour).
        int minPage, maxPage, selFrom, selTo;
        printout->GetPageInfo(&minPage, &maxPage, &selFrom, &selTo);

        wxPrintDialogData& pdd = m_pimpl->GetPrintDialogData();
        pdd.SetFromPage(minPage);
        pdd.SetToPage(maxPage);
    }

    return m_pimpl->Print( parent, printout, prompt );
}

wxDC* wxPrinter::PrintDialog(wxWindow *parent)
{
    return m_pimpl->PrintDialog( parent );
}

wxPrintDialogData& wxPrinter::GetPrintDialogData() const
{
    return m_pimpl->GetPrintDialogData();
}

// ---------------------------------------------------------------------------
// wxPrintDialogBase: the dialog for printing.
// ---------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxPrintDialogBase, wxDialog)

wxPrintDialogBase::wxPrintDialogBase(wxWindow *parent,
                                     wxWindowID id,
                                     const wxString &title,
                                     const wxPoint &pos,
                                     const wxSize &size,
                                     long style)
    : wxDialog( parent, id, title.empty() ? wxString(_("Print")) : title,
                pos, size, style )
{
}

// ---------------------------------------------------------------------------
// wxPrintDialog: the dialog for printing
// ---------------------------------------------------------------------------

IMPLEMENT_CLASS(wxPrintDialog, wxObject)

wxPrintDialog::wxPrintDialog(wxWindow *parent, wxPrintDialogData* data)
{
    m_pimpl = wxPrintFactory::GetFactory()->CreatePrintDialog( parent, data );
}

wxPrintDialog::wxPrintDialog(wxWindow *parent, wxPrintData* data)
{
    m_pimpl = wxPrintFactory::GetFactory()->CreatePrintDialog( parent, data );
}

wxPrintDialog::~wxPrintDialog()
{
    delete m_pimpl;
}

int wxPrintDialog::ShowModal()
{
    return m_pimpl->ShowModal();
}

wxPrintDialogData& wxPrintDialog::GetPrintDialogData()
{
    return m_pimpl->GetPrintDialogData();
}

wxPrintData& wxPrintDialog::GetPrintData()
{
    return m_pimpl->GetPrintData();
}

wxDC *wxPrintDialog::GetPrintDC()
{
    return m_pimpl->GetPrintDC();
}

// ---------------------------------------------------------------------------
// wxPageSetupDialogBase: the page setup dialog
// ---------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxPageSetupDialogBase, wxDialog)

wxPageSetupDialogBase::wxPageSetupDialogBase(wxWindow *parent,
                                     wxWindowID id,
                                     const wxString &title,
                                     const wxPoint &pos,
                                     const wxSize &size,
                                     long style)
    : wxDialog( parent, id, title.empty() ? wxString(_("Page setup")) : title,
                pos, size, style )
{
}

// ---------------------------------------------------------------------------
// wxPageSetupDialog: the page setup dialog
// ---------------------------------------------------------------------------

IMPLEMENT_CLASS(wxPageSetupDialog, wxObject)

wxPageSetupDialog::wxPageSetupDialog(wxWindow *parent, wxPageSetupDialogData *data )
{
    m_pimpl = wxPrintFactory::GetFactory()->CreatePageSetupDialog( parent, data );
}

wxPageSetupDialog::~wxPageSetupDialog()
{
    delete m_pimpl;
}

int wxPageSetupDialog::ShowModal()
{
    return m_pimpl->ShowModal();
}

wxPageSetupDialogData& wxPageSetupDialog::GetPageSetupDialogData()
{
    return m_pimpl->GetPageSetupDialogData();
}

// old name
wxPageSetupDialogData& wxPageSetupDialog::GetPageSetupData()
{
    return m_pimpl->GetPageSetupDialogData();
}

//----------------------------------------------------------------------------
// wxPrintAbortDialog
//----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxPrintAbortDialog, wxDialog)
    EVT_BUTTON(wxID_CANCEL, wxPrintAbortDialog::OnCancel)
END_EVENT_TABLE()

wxPrintAbortDialog::wxPrintAbortDialog(wxWindow *parent,
                                       const wxString& documentTitle,
                                       const wxPoint& pos,
                                       const wxSize& size,
                                       long style,
                                       const wxString& name)
    : wxDialog(parent, wxID_ANY, _("Printing"), pos, size, style, name)
{
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(new wxStaticText(this, wxID_ANY, _("Please wait while printing...")),
                   wxSizerFlags().Expand().DoubleBorder());

    wxFlexGridSizer *gridSizer = new wxFlexGridSizer(2, wxSize(20, 0));
    gridSizer->Add(new wxStaticText(this, wxID_ANY, _("Document:")));
    gridSizer->AddGrowableCol(1);
    gridSizer->Add(new wxStaticText(this, wxID_ANY, documentTitle));
    gridSizer->Add(new wxStaticText(this, wxID_ANY, _("Progress:")));
    m_progress = new wxStaticText(this, wxID_ANY, _("Preparing"));
    m_progress->SetMinSize(wxSize(250, -1));
    gridSizer->Add(m_progress);
    mainSizer->Add(gridSizer, wxSizerFlags().Expand().DoubleBorder(wxLEFT | wxRIGHT));

    mainSizer->Add(CreateStdDialogButtonSizer(wxCANCEL),
                   wxSizerFlags().Expand().DoubleBorder());

    SetSizerAndFit(mainSizer);
}

void wxPrintAbortDialog::SetProgress(int currentPage, int totalPages,
                                     int currentCopy, int totalCopies)
{
  wxString text;
  if ( totalPages == DEFAULT_MAX_PAGES )
  {
    // This means that the user has not supplied a total number of pages so it
    // is better not to show this value.
    text.Printf(_("Printing page %d"), currentPage);
  }
  else
  {
    // We have a valid total number of pages so we show it.
    text.Printf(_("Printing page %d of %d"), currentPage, totalPages);
  }
  if ( totalCopies > 1 )
      text += wxString::Format(_(" (copy %d of %d)"), currentCopy, totalCopies);
  m_progress->SetLabel(text);
}

void wxPrintAbortDialog::OnCancel(wxCommandEvent& WXUNUSED(event))
{
    wxCHECK_RET( wxPrinterBase::sm_abortWindow != NULL, "OnCancel called twice" );

    wxPrinterBase::sm_abortIt = true;
    wxPrinterBase::sm_abortWindow->Destroy();
    wxPrinterBase::sm_abortWindow = NULL;
}

//----------------------------------------------------------------------------
// wxPrintout
//----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxPrintout, wxObject)

wxPrintout::wxPrintout(const wxString& title)
{
    m_printoutTitle = title ;
    m_printoutDC = NULL;
    m_pageWidthMM = 0;
    m_pageHeightMM = 0;
    m_pageWidthPixels = 0;
    m_pageHeightPixels = 0;
    m_PPIScreenX = 0;
    m_PPIScreenY = 0;
    m_PPIPrinterX = 0;
    m_PPIPrinterY = 0;
    m_preview = NULL;
}

wxPrintout::~wxPrintout()
{
}

bool wxPrintout::OnBeginDocument(int WXUNUSED(startPage), int WXUNUSED(endPage))
{
    return GetDC()->StartDoc(_("Printing ") + m_printoutTitle);
}

void wxPrintout::OnEndDocument()
{
    GetDC()->EndDoc();
}

void wxPrintout::OnBeginPrinting()
{
}

void wxPrintout::OnEndPrinting()
{
}

bool wxPrintout::HasPage(int page)
{
    return (page == 1);
}

void wxPrintout::GetPageInfo(int *minPage, int *maxPage, int *fromPage, int *toPage)
{
    *minPage = 1;
    *maxPage = DEFAULT_MAX_PAGES;
    *fromPage = 1;
    *toPage = 1;
}

void wxPrintout::FitThisSizeToPaper(const wxSize& imageSize)
{
    // Set the DC scale and origin so that the given image size fits within the
    // entire page and the origin is at the top left corner of the page. Note
    // that with most printers, portions of the page will be non-printable. Use
    // this if you're managing your own page margins.
    if (!m_printoutDC) return;
    wxRect paperRect = GetPaperRectPixels();
    wxCoord pw, ph;
    GetPageSizePixels(&pw, &ph);
    wxCoord w, h;
    m_printoutDC->GetSize(&w, &h);
    float scaleX = ((float(paperRect.width) * w) / (float(pw) * imageSize.x));
    float scaleY = ((float(paperRect.height) * h) / (float(ph) * imageSize.y));
    float actualScale = wxMin(scaleX, scaleY);
    m_printoutDC->SetUserScale(actualScale, actualScale);
    m_printoutDC->SetDeviceOrigin(0, 0);
    wxRect logicalPaperRect = GetLogicalPaperRect();
    SetLogicalOrigin(logicalPaperRect.x, logicalPaperRect.y);
}

void wxPrintout::FitThisSizeToPage(const wxSize& imageSize)
{
    // Set the DC scale and origin so that the given image size fits within the
    // printable area of the page and the origin is at the top left corner of
    // the printable area.
    if (!m_printoutDC) return;
    int w, h;
    m_printoutDC->GetSize(&w, &h);
    float scaleX = float(w) / imageSize.x;
    float scaleY = float(h) / imageSize.y;
    float actualScale = wxMin(scaleX, scaleY);
    m_printoutDC->SetUserScale(actualScale, actualScale);
    m_printoutDC->SetDeviceOrigin(0, 0);
}

void wxPrintout::FitThisSizeToPageMargins(const wxSize& imageSize, const wxPageSetupDialogData& pageSetupData)
{
    // Set the DC scale and origin so that the given image size fits within the
    // page margins defined in the given wxPageSetupDialogData object and the
    // origin is at the top left corner of the page margins.
    if (!m_printoutDC) return;
    wxRect paperRect = GetPaperRectPixels();
    wxCoord pw, ph;
    GetPageSizePixels(&pw, &ph);
    wxPoint topLeft = pageSetupData.GetMarginTopLeft();
    wxPoint bottomRight = pageSetupData.GetMarginBottomRight();
    wxCoord mw, mh;
    GetPageSizeMM(&mw, &mh);
    float mmToDeviceX = float(pw) / mw;
    float mmToDeviceY = float(ph) / mh;
    wxRect pageMarginsRect(paperRect.x + wxRound(mmToDeviceX * topLeft.x),
        paperRect.y + wxRound(mmToDeviceY * topLeft.y),
        paperRect.width - wxRound(mmToDeviceX * (topLeft.x + bottomRight.x)),
        paperRect.height - wxRound(mmToDeviceY * (topLeft.y + bottomRight.y)));
    wxCoord w, h;
    m_printoutDC->GetSize(&w, &h);
    float scaleX = (float(pageMarginsRect.width) * w) / (float(pw) * imageSize.x);
    float scaleY = (float(pageMarginsRect.height) * h) / (float(ph) * imageSize.y);
    float actualScale = wxMin(scaleX, scaleY);
    m_printoutDC->SetUserScale(actualScale, actualScale);
    m_printoutDC->SetDeviceOrigin(0, 0);
    wxRect logicalPageMarginsRect = GetLogicalPageMarginsRect(pageSetupData);
    SetLogicalOrigin(logicalPageMarginsRect.x, logicalPageMarginsRect.y);
}

void wxPrintout::MapScreenSizeToPaper()
{
    // Set the DC scale so that an image on the screen is the same size on the
    // paper and the origin is at the top left of the paper. Note that with most
    // printers, portions of the page will be cut off. Use this if you're
    // managing your own page margins.
    if (!m_printoutDC) return;
    MapScreenSizeToPage();
    wxRect logicalPaperRect = GetLogicalPaperRect();
    SetLogicalOrigin(logicalPaperRect.x, logicalPaperRect.y);
}

void wxPrintout::MapScreenSizeToPage()
{
    // Set the DC scale and origin so that an image on the screen is the same
    // size on the paper and the origin is at the top left of the printable area.
    if (!m_printoutDC) return;
    int ppiScreenX, ppiScreenY;
    GetPPIScreen(&ppiScreenX, &ppiScreenY);
    int ppiPrinterX, ppiPrinterY;
    GetPPIPrinter(&ppiPrinterX, &ppiPrinterY);
    int w, h;
    m_printoutDC->GetSize(&w, &h);
    int pageSizePixelsX, pageSizePixelsY;
    GetPageSizePixels(&pageSizePixelsX, &pageSizePixelsY);
    float userScaleX = (float(ppiPrinterX) * w) / (float(ppiScreenX) * pageSizePixelsX);
    float userScaleY = (float(ppiPrinterY) * h) / (float(ppiScreenY) * pageSizePixelsY);
    m_printoutDC->SetUserScale(userScaleX, userScaleY);
    m_printoutDC->SetDeviceOrigin(0, 0);
}

void wxPrintout::MapScreenSizeToPageMargins(const wxPageSetupDialogData& pageSetupData)
{
    // Set the DC scale so that an image on the screen is the same size on the
    // paper and the origin is at the top left of the page margins defined by
    // the given wxPageSetupDialogData object.
    if (!m_printoutDC) return;
    MapScreenSizeToPage();
    wxRect logicalPageMarginsRect = GetLogicalPageMarginsRect(pageSetupData);
    SetLogicalOrigin(logicalPageMarginsRect.x, logicalPageMarginsRect.y);
}

void wxPrintout::MapScreenSizeToDevice()
{
    // Set the DC scale so that a screen pixel is the same size as a device
    // pixel and the origin is at the top left of the printable area.
    if (!m_printoutDC) return;
    int w, h;
    m_printoutDC->GetSize(&w, &h);
    int pageSizePixelsX, pageSizePixelsY;
    GetPageSizePixels(&pageSizePixelsX, &pageSizePixelsY);
    float userScaleX = float(w) / pageSizePixelsX;
    float userScaleY = float(h) / pageSizePixelsY;
    m_printoutDC->SetUserScale(userScaleX, userScaleY);
    m_printoutDC->SetDeviceOrigin(0, 0);
}

wxRect wxPrintout::GetLogicalPaperRect() const
{
    // Return the rectangle in logical units that corresponds to the paper
    // rectangle.
    wxRect paperRect = GetPaperRectPixels();
    wxCoord pw, ph;
    GetPageSizePixels(&pw, &ph);
    wxCoord w, h;
    m_printoutDC->GetSize(&w, &h);
    if (w == pw && h == ph) {
        // this DC matches the printed page, so no scaling
        return wxRect(m_printoutDC->DeviceToLogicalX(paperRect.x),
            m_printoutDC->DeviceToLogicalY(paperRect.y),
            m_printoutDC->DeviceToLogicalXRel(paperRect.width),
            m_printoutDC->DeviceToLogicalYRel(paperRect.height));
    }
    // This DC doesn't match the printed page, so we have to scale.
    float scaleX = float(w) / pw;
    float scaleY = float(h) / ph;
    return wxRect(m_printoutDC->DeviceToLogicalX(wxRound(paperRect.x * scaleX)),
        m_printoutDC->DeviceToLogicalY(wxRound(paperRect.y * scaleY)),
        m_printoutDC->DeviceToLogicalXRel(wxRound(paperRect.width * scaleX)),
        m_printoutDC->DeviceToLogicalYRel(wxRound(paperRect.height * scaleY)));
}

wxRect wxPrintout::GetLogicalPageRect() const
{
    // Return the rectangle in logical units that corresponds to the printable
    // area.
    int w, h;
    m_printoutDC->GetSize(&w, &h);
    return wxRect(m_printoutDC->DeviceToLogicalX(0),
        m_printoutDC->DeviceToLogicalY(0),
        m_printoutDC->DeviceToLogicalXRel(w),
        m_printoutDC->DeviceToLogicalYRel(h));
}

wxRect wxPrintout::GetLogicalPageMarginsRect(const wxPageSetupDialogData& pageSetupData) const
{
    // Return the rectangle in logical units that corresponds to the region
    // within the page margins as specified by the given wxPageSetupDialogData
    // object.

    // We get the paper size in device units and the margins in mm,
    // so we need to calculate the conversion with this trick
    wxCoord pw, ph;
    GetPageSizePixels(&pw, &ph);
    wxCoord mw, mh;
    GetPageSizeMM(&mw, &mh);
    float mmToDeviceX = float(pw) / mw;
    float mmToDeviceY = float(ph) / mh;

    // paper size in device units
    wxRect paperRect = GetPaperRectPixels();

    // margins in mm
    wxPoint topLeft = pageSetupData.GetMarginTopLeft();
    wxPoint bottomRight = pageSetupData.GetMarginBottomRight();

    // calculate margins in device units
    wxRect pageMarginsRect(
        paperRect.x      + wxRound(mmToDeviceX * topLeft.x),
        paperRect.y      + wxRound(mmToDeviceY * topLeft.y),
        paperRect.width  - wxRound(mmToDeviceX * (topLeft.x + bottomRight.x)),
        paperRect.height - wxRound(mmToDeviceY * (topLeft.y + bottomRight.y)));

    wxCoord w, h;
    m_printoutDC->GetSize(&w, &h);
    if (w == pw && h == ph)
    {
        // this DC matches the printed page, so no scaling
        return wxRect(
            m_printoutDC->DeviceToLogicalX(pageMarginsRect.x),
            m_printoutDC->DeviceToLogicalY(pageMarginsRect.y),
            m_printoutDC->DeviceToLogicalXRel(pageMarginsRect.width),
            m_printoutDC->DeviceToLogicalYRel(pageMarginsRect.height));
    }

    // This DC doesn't match the printed page, so we have to scale.
    float scaleX = float(w) / pw;
    float scaleY = float(h) / ph;
    return wxRect(m_printoutDC->DeviceToLogicalX(wxRound(pageMarginsRect.x * scaleX)),
        m_printoutDC->DeviceToLogicalY(wxRound(pageMarginsRect.y * scaleY)),
        m_printoutDC->DeviceToLogicalXRel(wxRound(pageMarginsRect.width * scaleX)),
        m_printoutDC->DeviceToLogicalYRel(wxRound(pageMarginsRect.height * scaleY)));
}

void wxPrintout::SetLogicalOrigin(wxCoord x, wxCoord y)
{
    // Set the device origin by specifying a point in logical coordinates.
    m_printoutDC->SetDeviceOrigin(
        m_printoutDC->LogicalToDeviceX(x),
        m_printoutDC->LogicalToDeviceY(y) );
}

void wxPrintout::OffsetLogicalOrigin(wxCoord xoff, wxCoord yoff)
{
    // Offset the device origin by a specified distance in device coordinates.
    wxPoint dev_org = m_printoutDC->GetDeviceOrigin();
    m_printoutDC->SetDeviceOrigin(
        dev_org.x + m_printoutDC->LogicalToDeviceXRel(xoff),
        dev_org.y + m_printoutDC->LogicalToDeviceYRel(yoff) );
}


//----------------------------------------------------------------------------
// wxPreviewCanvas
//----------------------------------------------------------------------------

IMPLEMENT_CLASS(wxPreviewCanvas, wxWindow)

BEGIN_EVENT_TABLE(wxPreviewCanvas, wxScrolledWindow)
    EVT_PAINT(wxPreviewCanvas::OnPaint)
    EVT_CHAR(wxPreviewCanvas::OnChar)
    EVT_IDLE(wxPreviewCanvas::OnIdle)
    EVT_SYS_COLOUR_CHANGED(wxPreviewCanvas::OnSysColourChanged)
#if wxUSE_MOUSEWHEEL
    EVT_MOUSEWHEEL(wxPreviewCanvas::OnMouseWheel)
#endif
END_EVENT_TABLE()

// VZ: the current code doesn't refresh properly without
//     wxFULL_REPAINT_ON_RESIZE, this must be fixed as otherwise we have
//     really horrible flicker when resizing the preview frame, but without
//     this style it simply doesn't work correctly at all...
wxPreviewCanvas::wxPreviewCanvas(wxPrintPreviewBase *preview, wxWindow *parent,
                                 const wxPoint& pos, const wxSize& size, long style, const wxString& name):
wxScrolledWindow(parent, wxID_ANY, pos, size, style | wxFULL_REPAINT_ON_RESIZE, name)
{
    m_printPreview = preview;
#ifdef __WXMAC__
    // The app workspace colour is always white, but we should have
    // a contrast with the page.
    wxSystemColour colourIndex = wxSYS_COLOUR_3DDKSHADOW;
#elif defined(__WXGTK__)
    wxSystemColour colourIndex = wxSYS_COLOUR_BTNFACE;
#else
    wxSystemColour colourIndex = wxSYS_COLOUR_APPWORKSPACE;
#endif
    SetBackgroundColour(wxSystemSettings::GetColour(colourIndex));

    SetScrollbars(10, 10, 100, 100);
}

wxPreviewCanvas::~wxPreviewCanvas()
{
}

void wxPreviewCanvas::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);
    PrepareDC( dc );

/*
#ifdef __WXGTK__
    if (!GetUpdateRegion().IsEmpty())
        dc.SetClippingRegion( GetUpdateRegion() );
#endif
*/

    if (m_printPreview)
    {
        m_printPreview->PaintPage(this, dc);
    }
}

void wxPreviewCanvas::OnIdle(wxIdleEvent& event)
{
    event.Skip();

    // prevent UpdatePageRendering() from being called recursively:
    static bool s_inIdle = false;
    if ( s_inIdle )
        return;
    s_inIdle = true;

    if ( m_printPreview )
    {
        if ( m_printPreview->UpdatePageRendering() )
            Refresh();
    }

    s_inIdle = false;
}

// Responds to colour changes, and passes event on to children.
void wxPreviewCanvas::OnSysColourChanged(wxSysColourChangedEvent& event)
{
#ifdef __WXMAC__
    // The app workspace colour is always white, but we should have
    // a contrast with the page.
    wxSystemColour colourIndex = wxSYS_COLOUR_3DDKSHADOW;
#elif defined(__WXGTK__)
    wxSystemColour colourIndex = wxSYS_COLOUR_BTNFACE;
#else
    wxSystemColour colourIndex = wxSYS_COLOUR_APPWORKSPACE;
#endif
    SetBackgroundColour(wxSystemSettings::GetColour(colourIndex));
    Refresh();

    // Propagate the event to the non-top-level children
    wxWindow::OnSysColourChanged(event);
}

void wxPreviewCanvas::OnChar(wxKeyEvent &event)
{
    wxPreviewControlBar* controlBar = ((wxPreviewFrame*) GetParent())->GetControlBar();
    switch (event.GetKeyCode())
    {
        case WXK_RETURN:
            controlBar->OnPrint();
            return;
        case (int)'+':
        case WXK_NUMPAD_ADD:
        case WXK_ADD:
            controlBar->DoZoomIn();
            return;
        case (int)'-':
        case WXK_NUMPAD_SUBTRACT:
        case WXK_SUBTRACT:
            controlBar->DoZoomOut();
            return;
    }

    if (!event.ControlDown())
    {
        event.Skip();
        return;
    }

    switch(event.GetKeyCode())
    {
        case WXK_PAGEDOWN:
            controlBar->OnNext(); break;
        case WXK_PAGEUP:
            controlBar->OnPrevious(); break;
        case WXK_HOME:
            controlBar->OnFirst(); break;
        case WXK_END:
            controlBar->OnLast(); break;
        default:
            event.Skip();
    }
}

#if wxUSE_MOUSEWHEEL

void wxPreviewCanvas::OnMouseWheel(wxMouseEvent& event)
{
    wxPreviewControlBar *
        controlBar = wxStaticCast(GetParent(), wxPreviewFrame)->GetControlBar();

    if ( controlBar )
    {
        if ( event.ControlDown() && event.GetWheelRotation() != 0 )
        {
            int currentZoom = controlBar->GetZoomControl();

            int delta;
            if ( currentZoom < 100 )
                delta = 5;
            else if ( currentZoom <= 120 )
                delta = 10;
            else
                delta = 50;

            if ( event.GetWheelRotation() > 0 )
                delta = -delta;

            int newZoom = currentZoom + delta;
            if ( newZoom < 10 )
                newZoom = 10;
            if ( newZoom > 200 )
                newZoom = 200;
            if ( newZoom != currentZoom )
            {
                controlBar->SetZoomControl(newZoom);
                m_printPreview->SetZoom(newZoom);
                Refresh();
            }
            return;
        }
    }

    event.Skip();
}

#endif // wxUSE_MOUSEWHEEL

namespace
{

// This is by the controls in the print preview as the maximal (and hence
// longest) page number we may have to display.
enum { MAX_PAGE_NUMBER = 99999 };

} // anonymous namespace

// ----------------------------------------------------------------------------
// wxPrintPageMaxCtrl
// ----------------------------------------------------------------------------

// A simple static control showing the maximal number of pages.
class wxPrintPageMaxCtrl : public wxStaticText
{
public:
    wxPrintPageMaxCtrl(wxWindow *parent)
        : wxStaticText(
                        parent,
                        wxID_ANY,
                        wxString(),
                        wxDefaultPosition,
                        wxSize
                        (
                         parent->GetTextExtent(MaxAsString(MAX_PAGE_NUMBER)).x,
                         wxDefaultCoord
                        ),
                        wxST_NO_AUTORESIZE | wxALIGN_CENTRE
                      )
    {
    }

    // Set the maximal page to display once we really know what it is.
    void SetMaxPage(int maxPage)
    {
        SetLabel(MaxAsString(maxPage));
    }

private:
    static wxString MaxAsString(int maxPage)
    {
        return wxString::Format("/ %d", maxPage);
    }

    wxDECLARE_NO_COPY_CLASS(wxPrintPageMaxCtrl);
};

// ----------------------------------------------------------------------------
// wxPrintPageTextCtrl
// ----------------------------------------------------------------------------

// This text control contains the page number in the specified interval.
//
// Invalid pages are not accepted and the control contents is validated when it
// loses focus. Conversely, if the user changes the page to another valid one
// or presses Enter, OnGotoPage() method of the preview object will be called.
class wxPrintPageTextCtrl : public wxTextCtrl
{
public:
    wxPrintPageTextCtrl(wxPreviewControlBar *preview)
        : wxTextCtrl(preview,
                     wxID_PREVIEW_GOTO,
                     wxString(),
                     wxDefaultPosition,
                     // We use hardcoded maximal page number for the width
                     // instead of fitting it to the values we can show because
                     // the control looks uncomfortably narrow if the real page
                     // number is just one or two digits.
                     wxSize
                     (
                      preview->GetTextExtent(PageAsString(MAX_PAGE_NUMBER)).x,
                      wxDefaultCoord
                     ),
                     wxTE_PROCESS_ENTER
#if wxUSE_VALIDATORS
                     , wxTextValidator(wxFILTER_DIGITS)
#endif // wxUSE_VALIDATORS
                    ),
          m_preview(preview)
    {
        m_minPage =
        m_maxPage =
        m_page = 1;

        Connect(wxEVT_KILL_FOCUS,
                wxFocusEventHandler(wxPrintPageTextCtrl::OnKillFocus));
        Connect(wxEVT_TEXT_ENTER,
                wxCommandEventHandler(wxPrintPageTextCtrl::OnTextEnter));
    }

    // Update the pages range, must be called after OnPreparePrinting() as
    // these values are not known before.
    void SetPageInfo(int minPage, int maxPage)
    {
        m_minPage = minPage;
        m_maxPage = maxPage;

        // Show the first page by default.
        SetPageNumber(minPage);
    }

    // Helpers to conveniently set or get the current page number. Return value
    // is 0 if the current controls contents is invalid.
    void SetPageNumber(int page)
    {
        wxASSERT( IsValidPage(page) );

        SetValue(PageAsString(page));
    }

    int GetPageNumber() const
    {
        long value;
        if ( !GetValue().ToLong(&value) || !IsValidPage(value) )
            return 0;

        // Cast is safe because the value is less than (int) m_maxPage.
        return static_cast<int>(value);
    }

private:
    static wxString PageAsString(int page)
    {
        return wxString::Format("%d", page);
    }

    bool IsValidPage(int page) const
    {
        return page >= m_minPage && page <= m_maxPage;
    }

    bool DoChangePage()
    {
        const int page = GetPageNumber();

        if ( !page )
            return false;

        if ( page != m_page )
        {
            // We have a valid page, remember it.
            m_page = page;

            // And notify the owner about the change.
            m_preview->OnGotoPage();
        }
        //else: Nothing really changed.

        return true;
    }

    void OnKillFocus(wxFocusEvent& event)
    {
        if ( !DoChangePage() )
        {
            // The current contents is invalid so reset it back to the last
            // known good page index.
            SetPageNumber(m_page);
        }

        event.Skip();
    }

    void OnTextEnter(wxCommandEvent& WXUNUSED(event))
    {
        DoChangePage();
    }


    wxPreviewControlBar * const m_preview;

    int m_minPage,
        m_maxPage;

    // This is the last valid page value that we had, we revert to it if an
    // invalid page is entered.
    int m_page;

    wxDECLARE_NO_COPY_CLASS(wxPrintPageTextCtrl);
};

//----------------------------------------------------------------------------
// wxPreviewControlBar
//----------------------------------------------------------------------------

IMPLEMENT_CLASS(wxPreviewControlBar, wxWindow)

BEGIN_EVENT_TABLE(wxPreviewControlBar, wxPanel)
    EVT_BUTTON(wxID_PREVIEW_CLOSE,    wxPreviewControlBar::OnWindowClose)
    EVT_BUTTON(wxID_PREVIEW_PRINT,    wxPreviewControlBar::OnPrintButton)
    EVT_BUTTON(wxID_PREVIEW_PREVIOUS, wxPreviewControlBar::OnPreviousButton)
    EVT_BUTTON(wxID_PREVIEW_NEXT,     wxPreviewControlBar::OnNextButton)
    EVT_BUTTON(wxID_PREVIEW_FIRST,    wxPreviewControlBar::OnFirstButton)
    EVT_BUTTON(wxID_PREVIEW_LAST,     wxPreviewControlBar::OnLastButton)
    EVT_BUTTON(wxID_PREVIEW_ZOOM_IN,  wxPreviewControlBar::OnZoomInButton)
    EVT_BUTTON(wxID_PREVIEW_ZOOM_OUT, wxPreviewControlBar::OnZoomOutButton)

    EVT_UPDATE_UI(wxID_PREVIEW_PREVIOUS, wxPreviewControlBar::OnUpdatePreviousButton)
    EVT_UPDATE_UI(wxID_PREVIEW_NEXT,     wxPreviewControlBar::OnUpdateNextButton)
    EVT_UPDATE_UI(wxID_PREVIEW_FIRST,    wxPreviewControlBar::OnUpdateFirstButton)
    EVT_UPDATE_UI(wxID_PREVIEW_LAST,     wxPreviewControlBar::OnUpdateLastButton)
    EVT_UPDATE_UI(wxID_PREVIEW_ZOOM_IN,  wxPreviewControlBar::OnUpdateZoomInButton)
    EVT_UPDATE_UI(wxID_PREVIEW_ZOOM_OUT, wxPreviewControlBar::OnUpdateZoomOutButton)

    EVT_CHOICE(wxID_PREVIEW_ZOOM,     wxPreviewControlBar::OnZoomChoice)
    EVT_PAINT(wxPreviewControlBar::OnPaint)

END_EVENT_TABLE()

wxPreviewControlBar::wxPreviewControlBar(wxPrintPreviewBase *preview, long buttons,
                                         wxWindow *parent, const wxPoint& pos, const wxSize& size,
                                         long style, const wxString& name):
wxPanel(parent, wxID_ANY, pos, size, style, name)
{
    m_printPreview = preview;
    m_closeButton = NULL;
    m_zoomControl = NULL;
    m_currentPageText = NULL;
    m_maxPageText = NULL;
    m_buttonFlags = buttons;
}

wxPreviewControlBar::~wxPreviewControlBar()
{
}

void wxPreviewControlBar::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);

    int w, h;
    GetSize(&w, &h);
    dc.SetPen(*wxBLACK_PEN);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawLine( 0, h-1, w, h-1 );
}

void wxPreviewControlBar::OnWindowClose(wxCommandEvent& WXUNUSED(event))
{
    wxPreviewFrame *frame = (wxPreviewFrame *)GetParent();
    frame->Close(true);
}

void wxPreviewControlBar::OnPrint(void)
{
    wxPrintPreviewBase *preview = GetPrintPreview();
    preview->Print(true);
}

void wxPreviewControlBar::OnNext()
{
    if ( IsNextEnabled() )
        DoGotoPage(GetPrintPreview()->GetCurrentPage() + 1);
}

void wxPreviewControlBar::OnPrevious()
{
    if ( IsPreviousEnabled() )
        DoGotoPage(GetPrintPreview()->GetCurrentPage() - 1);
}

void wxPreviewControlBar::OnFirst()
{
    if ( IsFirstEnabled() )
        DoGotoPage(GetPrintPreview()->GetMinPage());
}

void wxPreviewControlBar::OnLast()
{
    if ( IsLastEnabled() )
        DoGotoPage(GetPrintPreview()->GetMaxPage());
}

bool wxPreviewControlBar::IsNextEnabled() const
{
    wxPrintPreviewBase *preview = GetPrintPreview();
    if ( !preview )
        return false;

    const int currentPage = preview->GetCurrentPage();
    return currentPage < preview->GetMaxPage() &&
                preview->GetPrintout()->HasPage(currentPage + 1);
}

bool wxPreviewControlBar::IsPreviousEnabled() const
{
    wxPrintPreviewBase *preview = GetPrintPreview();
    if ( !preview )
        return false;

    const int currentPage = preview->GetCurrentPage();
    return currentPage > preview->GetMinPage() &&
                preview->GetPrintout()->HasPage(currentPage - 1);
}

bool wxPreviewControlBar::IsFirstEnabled() const
{
    wxPrintPreviewBase *preview = GetPrintPreview();
    if (!preview)
        return false;

    return preview->GetPrintout()->HasPage(preview->GetMinPage());
}

bool wxPreviewControlBar::IsLastEnabled() const
{
    wxPrintPreviewBase *preview = GetPrintPreview();
    if (!preview)
        return false;

    return preview->GetPrintout()->HasPage(preview->GetMaxPage());
}

void wxPreviewControlBar::DoGotoPage(int page)
{
    wxPrintPreviewBase *preview = GetPrintPreview();
    wxCHECK_RET( preview, "Shouldn't be called if there is no preview." );

    preview->SetCurrentPage(page);

    if ( m_currentPageText )
        m_currentPageText->SetPageNumber(page);
}

void wxPreviewControlBar::OnGotoPage()
{
    wxPrintPreviewBase *preview = GetPrintPreview();
    if (preview)
    {
        if (preview->GetMinPage() > 0)
        {
            long currentPage = m_currentPageText->GetPageNumber();
            if ( currentPage )
            {
                if (preview->GetPrintout()->HasPage(currentPage))
                {
                    preview->SetCurrentPage(currentPage);
                }
            }
        }
    }
}

void wxPreviewControlBar::DoZoom()
{
    int zoom = GetZoomControl();
    if (GetPrintPreview())
        GetPrintPreview()->SetZoom(zoom);
}

bool wxPreviewControlBar::IsZoomInEnabled() const
{
    if ( !m_zoomControl )
        return false;

    const unsigned sel = m_zoomControl->GetSelection();
    return sel < m_zoomControl->GetCount() - 1;
}

bool wxPreviewControlBar::IsZoomOutEnabled() const
{
    return m_zoomControl && m_zoomControl->GetSelection() > 0;
}

void wxPreviewControlBar::DoZoomIn()
{
    if (IsZoomInEnabled())
    {
        m_zoomControl->SetSelection(m_zoomControl->GetSelection() + 1);
        DoZoom();
    }
}

void wxPreviewControlBar::DoZoomOut()
{
    if (IsZoomOutEnabled())
    {
        m_zoomControl->SetSelection(m_zoomControl->GetSelection() - 1);
        DoZoom();
    }
}

namespace
{

// Helper class used by wxPreviewControlBar::CreateButtons() to add buttons
// sequentially to it in the simplest way possible.
class SizerWithButtons
{
public:
    // Constructor creates the sizer that will hold the buttons and stores the
    // parent that will be used for their creation.
    SizerWithButtons(wxWindow *parent)
        : m_sizer(new wxBoxSizer(wxHORIZONTAL)),
          m_parent(parent)
    {
        m_hasContents =
        m_needsSeparator = false;
    }

    // Destructor associates the sizer with the parent window.
    ~SizerWithButtons()
    {
        m_parent->SetSizer(m_sizer);
        m_sizer->Fit(m_parent);
    }


    // Add an arbitrary window to the sizer.
    void Add(wxWindow *win)
    {
        if ( m_needsSeparator )
        {
            m_needsSeparator = false;

            m_sizer->AddSpacer(2*wxSizerFlags::GetDefaultBorder());
        }

        m_hasContents = true;

        m_sizer->Add(win,
                     wxSizerFlags().Border(wxLEFT | wxTOP | wxBOTTOM).Center());
    }

    // Add a button with the specified id, bitmap and tooltip.
    void AddButton(wxWindowID btnId,
                   const wxArtID& artId,
                   const wxString& tooltip)
    {
        // We don't use (smaller) images inside a button with a text label but
        // rather toolbar-like bitmap buttons hence use wxART_TOOLBAR and not
        // wxART_BUTTON here.
        wxBitmap bmp = wxArtProvider::GetBitmap(artId, wxART_TOOLBAR);
        wxBitmapButton * const btn = new wxBitmapButton(m_parent, btnId, bmp);
        btn->SetToolTip(tooltip);

        Add(btn);
    }

    // Add a control at the right end of the window. This should be called last
    // as everything else added after it will be added on the right side too.
    void AddAtEnd(wxWindow *win)
    {
        m_sizer->AddStretchSpacer();
        m_sizer->Add(win,
                     wxSizerFlags().Border(wxTOP | wxBOTTOM | wxRIGHT).Center());
    }

    // Indicates the end of a group of buttons, a separator will be added after
    // it.
    void EndOfGroup()
    {
        if ( m_hasContents )
        {
            m_needsSeparator = true;
            m_hasContents = false;
        }
    }

private:
    wxSizer * const m_sizer;
    wxWindow * const m_parent;

    // If true, we have some controls since the last group beginning. This is
    // used to avoid inserting two consecutive separators if EndOfGroup() is
    // called twice.
    bool m_hasContents;

    // If true, a separator should be inserted before adding the next button.
    bool m_needsSeparator;

    wxDECLARE_NO_COPY_CLASS(SizerWithButtons);
};

} // anonymous namespace

void wxPreviewControlBar::CreateButtons()
{
    SizerWithButtons sizer(this);

    // Print button group (a single button).
    if (m_buttonFlags & wxPREVIEW_PRINT)
    {
        sizer.AddButton(wxID_PREVIEW_PRINT, wxART_PRINT, _("Print"));
        sizer.EndOfGroup();
    }

    // Page selection buttons group.
    if (m_buttonFlags & wxPREVIEW_FIRST)
    {
        sizer.AddButton(wxID_PREVIEW_FIRST, wxART_GOTO_FIRST, _("First page"));
    }

    if (m_buttonFlags & wxPREVIEW_PREVIOUS)
    {
        sizer.AddButton(wxID_PREVIEW_PREVIOUS, wxART_GO_BACK, _("Previous page"));
    }

    if (m_buttonFlags & wxPREVIEW_GOTO)
    {
        m_currentPageText = new wxPrintPageTextCtrl(this);
        sizer.Add(m_currentPageText);

        m_maxPageText = new wxPrintPageMaxCtrl(this);
        sizer.Add(m_maxPageText);
    }

    if (m_buttonFlags & wxPREVIEW_NEXT)
    {
        sizer.AddButton(wxID_PREVIEW_NEXT, wxART_GO_FORWARD, _("Next page"));
    }

    if (m_buttonFlags & wxPREVIEW_LAST)
    {
        sizer.AddButton(wxID_PREVIEW_LAST, wxART_GOTO_LAST, _("Last page"));
    }

    sizer.EndOfGroup();

    // Zoom controls group.
    if (m_buttonFlags & wxPREVIEW_ZOOM)
    {
        sizer.AddButton(wxID_PREVIEW_ZOOM_OUT, wxART_MINUS, _("Zoom Out"));

        wxString choices[] =
        {
            wxT("10%"), wxT("15%"), wxT("20%"), wxT("25%"), wxT("30%"), wxT("35%"), wxT("40%"), wxT("45%"), wxT("50%"), wxT("55%"),
                wxT("60%"), wxT("65%"), wxT("70%"), wxT("75%"), wxT("80%"), wxT("85%"), wxT("90%"), wxT("95%"), wxT("100%"), wxT("110%"),
                wxT("120%"), wxT("150%"), wxT("200%")
        };
        int n = WXSIZEOF(choices);

        m_zoomControl = new wxChoice( this, wxID_PREVIEW_ZOOM, wxDefaultPosition, wxSize(70,wxDefaultCoord), n, choices, 0 );
        sizer.Add(m_zoomControl);
        SetZoomControl(m_printPreview->GetZoom());

        sizer.AddButton(wxID_PREVIEW_ZOOM_IN, wxART_PLUS, _("Zoom In"));

        sizer.EndOfGroup();
    }

    // Close button group (single button again).
    m_closeButton = new wxButton(this, wxID_PREVIEW_CLOSE, _("&Close"));
    sizer.AddAtEnd(m_closeButton);
}

void wxPreviewControlBar::SetPageInfo(int minPage, int maxPage)
{
    if ( m_currentPageText )
        m_currentPageText->SetPageInfo(minPage, maxPage);

    if ( m_maxPageText )
        m_maxPageText->SetMaxPage(maxPage);
}

void wxPreviewControlBar::SetZoomControl(int zoom)
{
    if (m_zoomControl)
    {
        int n, count = m_zoomControl->GetCount();
        long val;
        for (n=0; n<count; n++)
        {
            if (m_zoomControl->GetString(n).BeforeFirst(wxT('%')).ToLong(&val) &&
                (val >= long(zoom)))
            {
                m_zoomControl->SetSelection(n);
                return;
            }
        }

        m_zoomControl->SetSelection(count-1);
    }
}

int wxPreviewControlBar::GetZoomControl()
{
    if (m_zoomControl && (m_zoomControl->GetStringSelection() != wxEmptyString))
    {
        long val;
        if (m_zoomControl->GetStringSelection().BeforeFirst(wxT('%')).ToLong(&val))
            return int(val);
    }

    return 0;
}


/*
* Preview frame
*/

IMPLEMENT_CLASS(wxPreviewFrame, wxFrame)

BEGIN_EVENT_TABLE(wxPreviewFrame, wxFrame)
    EVT_CHAR_HOOK(wxPreviewFrame::OnChar)
    EVT_CLOSE(wxPreviewFrame::OnCloseWindow)
END_EVENT_TABLE()

void wxPreviewFrame::OnChar(wxKeyEvent &event)
{
    if ( event.GetKeyCode() == WXK_ESCAPE )
    {
        Close(true);
    }
    else
    {
        event.Skip();
    }
}

wxPreviewFrame::wxPreviewFrame(wxPrintPreviewBase *preview, wxWindow *parent, const wxString& title,
                               const wxPoint& pos, const wxSize& size, long style, const wxString& name):
wxFrame(parent, wxID_ANY, title, pos, size, style, name)
{
    m_printPreview = preview;
    m_controlBar = NULL;
    m_previewCanvas = NULL;
    m_windowDisabler = NULL;
    m_modalityKind = wxPreviewFrame_NonModal;

    // Give the application icon
#ifdef __WXMSW__
    wxFrame* topFrame = wxDynamicCast(wxTheApp->GetTopWindow(), wxFrame);
    if (topFrame)
        SetIcons(topFrame->GetIcons());
#endif
}

wxPreviewFrame::~wxPreviewFrame()
{
    wxPrintout *printout = m_printPreview->GetPrintout();
    if (printout)
    {
        delete printout;
        m_printPreview->SetPrintout(NULL);
        m_printPreview->SetCanvas(NULL);
        m_printPreview->SetFrame(NULL);
    }

    m_previewCanvas->SetPreview(NULL);
    delete m_printPreview;
}

void wxPreviewFrame::OnCloseWindow(wxCloseEvent& WXUNUSED(event))
{
    // Reenable any windows we disabled by undoing whatever we did in our
    // Initialize().
    switch ( m_modalityKind )
    {
        case wxPreviewFrame_AppModal:
            delete m_windowDisabler;
            m_windowDisabler = NULL;
            break;

        case wxPreviewFrame_WindowModal:
            if ( GetParent() )
                GetParent()->Enable();
            break;

        case wxPreviewFrame_NonModal:
            break;
    }

    Destroy();
}

void wxPreviewFrame::InitializeWithModality(wxPreviewFrameModalityKind kind)
{
#if wxUSE_STATUSBAR
    CreateStatusBar();
#endif
    CreateCanvas();
    CreateControlBar();

    m_printPreview->SetCanvas(m_previewCanvas);
    m_printPreview->SetFrame(this);

    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    item0->Add( m_controlBar, 0, wxGROW|wxALIGN_CENTER_VERTICAL, 5 );
    item0->Add( m_previewCanvas, 1, wxGROW|wxALIGN_CENTER_VERTICAL, 5 );

    SetAutoLayout( true );
    SetSizer( item0 );

    m_modalityKind = kind;
    switch ( m_modalityKind )
    {
        case wxPreviewFrame_AppModal:
            // Disable everything.
            m_windowDisabler = new wxWindowDisabler( this );
            break;

        case wxPreviewFrame_WindowModal:
            // Disable our parent if we have one.
            if ( GetParent() )
                GetParent()->Disable();
            break;

        case wxPreviewFrame_NonModal:
            // Nothing to do, we don't need to disable any windows.
            break;
    }

    if ( m_modalityKind != wxPreviewFrame_NonModal )
    {
        // Behave like modal dialogs, don't show in taskbar. This implies
        // removing the minimize box, because minimizing windows without
        // taskbar entry is confusing.
        SetWindowStyle((GetWindowStyle() & ~wxMINIMIZE_BOX) | wxFRAME_NO_TASKBAR);
    }

    Layout();

    m_printPreview->AdjustScrollbars(m_previewCanvas);
    m_previewCanvas->SetFocus();
    m_controlBar->SetFocus();
}

void wxPreviewFrame::CreateCanvas()
{
    m_previewCanvas = new wxPreviewCanvas(m_printPreview, this);
}

void wxPreviewFrame::CreateControlBar()
{
    long buttons = wxPREVIEW_DEFAULT;
    if (m_printPreview->GetPrintoutForPrinting())
        buttons |= wxPREVIEW_PRINT;

    m_controlBar = new wxPreviewControlBar(m_printPreview, buttons, this);
    m_controlBar->CreateButtons();
}

/*
* Print preview
*/

IMPLEMENT_CLASS(wxPrintPreviewBase, wxObject)

wxPrintPreviewBase::wxPrintPreviewBase(wxPrintout *printout,
                                       wxPrintout *printoutForPrinting,
                                       wxPrintData *data)
{
    if (data)
        m_printDialogData = (*data);

    Init(printout, printoutForPrinting);
}

wxPrintPreviewBase::wxPrintPreviewBase(wxPrintout *printout,
                                       wxPrintout *printoutForPrinting,
                                       wxPrintDialogData *data)
{
    if (data)
        m_printDialogData = (*data);

    Init(printout, printoutForPrinting);
}

void wxPrintPreviewBase::Init(wxPrintout *printout,
                              wxPrintout *printoutForPrinting)
{
    m_isOk = true;
    m_previewPrintout = printout;
    if (m_previewPrintout)
        m_previewPrintout->SetPreview(static_cast<wxPrintPreview *>(this));

    m_printPrintout = printoutForPrinting;

    m_previewCanvas = NULL;
    m_previewFrame = NULL;
    m_previewBitmap = NULL;
    m_previewFailed = false;
    m_currentPage = 1;
    m_currentZoom = 70;
    m_topMargin =
    m_leftMargin = 2*wxSizerFlags::GetDefaultBorder();
    m_pageWidth = 0;
    m_pageHeight = 0;
    m_printingPrepared = false;
    m_minPage = 1;
    m_maxPage = 1;
}

wxPrintPreviewBase::~wxPrintPreviewBase()
{
    if (m_previewPrintout)
        delete m_previewPrintout;
    if (m_previewBitmap)
        delete m_previewBitmap;
    if (m_printPrintout)
        delete m_printPrintout;
}

bool wxPrintPreviewBase::SetCurrentPage(int pageNum)
{
    if (m_currentPage == pageNum)
        return true;

    m_currentPage = pageNum;

    InvalidatePreviewBitmap();

    if (m_previewCanvas)
    {
        AdjustScrollbars(m_previewCanvas);

        m_previewCanvas->Refresh();
        m_previewCanvas->SetFocus();
    }
    return true;
}

int wxPrintPreviewBase::GetCurrentPage() const
    { return m_currentPage; }
void wxPrintPreviewBase::SetPrintout(wxPrintout *printout)
    { m_previewPrintout = printout; }
wxPrintout *wxPrintPreviewBase::GetPrintout() const
    { return m_previewPrintout; }
wxPrintout *wxPrintPreviewBase::GetPrintoutForPrinting() const
    { return m_printPrintout; }
void wxPrintPreviewBase::SetFrame(wxFrame *frame)
    { m_previewFrame = frame; }
void wxPrintPreviewBase::SetCanvas(wxPreviewCanvas *canvas)
    { m_previewCanvas = canvas; }
wxFrame *wxPrintPreviewBase::GetFrame() const
    { return m_previewFrame; }
wxPreviewCanvas *wxPrintPreviewBase::GetCanvas() const
    { return m_previewCanvas; }

void wxPrintPreviewBase::CalcRects(wxPreviewCanvas *canvas, wxRect& pageRect, wxRect& paperRect)
{
    // Calculate the rectangles for the printable area of the page and the
    // entire paper as they appear on the canvas on-screen.
    int canvasWidth, canvasHeight;
    canvas->GetSize(&canvasWidth, &canvasHeight);

    float zoomScale = float(m_currentZoom) / 100;
    float screenPrintableWidth = zoomScale * m_pageWidth * m_previewScaleX;
    float screenPrintableHeight = zoomScale * m_pageHeight * m_previewScaleY;

    wxRect devicePaperRect = m_previewPrintout->GetPaperRectPixels();
    wxCoord devicePrintableWidth, devicePrintableHeight;
    m_previewPrintout->GetPageSizePixels(&devicePrintableWidth, &devicePrintableHeight);
    float scaleX = screenPrintableWidth / devicePrintableWidth;
    float scaleY = screenPrintableHeight / devicePrintableHeight;
    paperRect.width = wxCoord(scaleX * devicePaperRect.width);
    paperRect.height = wxCoord(scaleY * devicePaperRect.height);

    paperRect.x = wxCoord((canvasWidth - paperRect.width)/ 2.0);
    if (paperRect.x < m_leftMargin)
        paperRect.x = m_leftMargin;
    paperRect.y = wxCoord((canvasHeight - paperRect.height)/ 2.0);
    if (paperRect.y < m_topMargin)
        paperRect.y = m_topMargin;

    pageRect.x = paperRect.x - wxCoord(scaleX * devicePaperRect.x);
    pageRect.y = paperRect.y - wxCoord(scaleY * devicePaperRect.y);
    pageRect.width = wxCoord(screenPrintableWidth);
    pageRect.height = wxCoord(screenPrintableHeight);
}


void wxPrintPreviewBase::InvalidatePreviewBitmap()
{
    wxDELETE(m_previewBitmap);
    // if there was a problem with rendering the preview, try again now
    // that it changed in some way (less memory may be needed, for example):
    m_previewFailed = false;
}

bool wxPrintPreviewBase::UpdatePageRendering()
{
    if ( m_previewBitmap )
        return false;

    if ( m_previewFailed )
        return false;

    if ( !RenderPage(m_currentPage) )
    {
        m_previewFailed = true; // don't waste time failing again
        return false;
    }

    return true;
}

bool wxPrintPreviewBase::PaintPage(wxPreviewCanvas *canvas, wxDC& dc)
{
    DrawBlankPage(canvas, dc);

    if (!m_previewBitmap)
        return false;
    if (!canvas)
        return false;

    wxRect pageRect, paperRect;
    CalcRects(canvas, pageRect, paperRect);
    wxMemoryDC temp_dc;
    temp_dc.SelectObject(*m_previewBitmap);

    dc.Blit(pageRect.x, pageRect.y,
        m_previewBitmap->GetWidth(), m_previewBitmap->GetHeight(), &temp_dc, 0, 0);

    temp_dc.SelectObject(wxNullBitmap);
    return true;
}

// Adjusts the scrollbars for the current scale
void wxPrintPreviewBase::AdjustScrollbars(wxPreviewCanvas *canvas)
{
    if (!canvas)
        return ;

    wxRect pageRect, paperRect;
    CalcRects(canvas, pageRect, paperRect);
     int totalWidth = paperRect.width + 2 * m_leftMargin;
    int totalHeight = paperRect.height + 2 * m_topMargin;
    int scrollUnitsX = totalWidth / 10;
    int scrollUnitsY = totalHeight / 10;
    wxSize virtualSize = canvas->GetVirtualSize();
    if (virtualSize.GetWidth() != totalWidth || virtualSize.GetHeight() != totalHeight)
        canvas->SetScrollbars(10, 10, scrollUnitsX, scrollUnitsY, 0, 0, true);
}

bool wxPrintPreviewBase::RenderPageIntoDC(wxDC& dc, int pageNum)
{
    m_previewPrintout->SetDC(&dc);
    m_previewPrintout->SetPageSizePixels(m_pageWidth, m_pageHeight);

    // Need to delay OnPreparePrinting() until here, so we have enough
    // information and a wxDC.
    if (!m_printingPrepared)
    {
        m_printingPrepared = true;

        m_previewPrintout->OnPreparePrinting();
        int selFrom, selTo;
        m_previewPrintout->GetPageInfo(&m_minPage, &m_maxPage, &selFrom, &selTo);

        // Update the wxPreviewControlBar page range display.
        if ( m_previewFrame )
        {
            wxPreviewControlBar * const
                controlBar = ((wxPreviewFrame*)m_previewFrame)->GetControlBar();
            if ( controlBar )
                controlBar->SetPageInfo(m_minPage, m_maxPage);
        }
    }

    m_previewPrintout->OnBeginPrinting();

    if (!m_previewPrintout->OnBeginDocument(m_printDialogData.GetFromPage(), m_printDialogData.GetToPage()))
    {
        wxMessageBox(_("Could not start document preview."), _("Print Preview Failure"), wxOK);
        return false;
    }

    m_previewPrintout->OnPrintPage(pageNum);
    m_previewPrintout->OnEndDocument();
    m_previewPrintout->OnEndPrinting();

    m_previewPrintout->SetDC(NULL);

    return true;
}

bool wxPrintPreviewBase::RenderPageIntoBitmap(wxBitmap& bmp, int pageNum)
{
    wxMemoryDC memoryDC;
    memoryDC.SelectObject(bmp);
    memoryDC.Clear();

    return RenderPageIntoDC(memoryDC, pageNum);
}

bool wxPrintPreviewBase::RenderPage(int pageNum)
{
    wxBusyCursor busy;

    if (!m_previewCanvas)
    {
        wxFAIL_MSG(wxT("wxPrintPreviewBase::RenderPage: must use wxPrintPreviewBase::SetCanvas to let me know about the canvas!"));
        return false;
    }

    wxRect pageRect, paperRect;
    CalcRects(m_previewCanvas, pageRect, paperRect);

    if (!m_previewBitmap)
    {
        m_previewBitmap = new wxBitmap(pageRect.width, pageRect.height);

        if (!m_previewBitmap || !m_previewBitmap->IsOk())
        {
            InvalidatePreviewBitmap();
            wxMessageBox(_("Sorry, not enough memory to create a preview."), _("Print Preview Failure"), wxOK);
            return false;
        }
    }

    if ( !RenderPageIntoBitmap(*m_previewBitmap, pageNum) )
    {
        InvalidatePreviewBitmap();
        wxMessageBox(_("Sorry, not enough memory to create a preview."), _("Print Preview Failure"), wxOK);
        return false;
    }

#if wxUSE_STATUSBAR
    wxString status;
    if (m_maxPage != 0)
        status = wxString::Format(_("Page %d of %d"), pageNum, m_maxPage);
    else
        status = wxString::Format(_("Page %d"), pageNum);

    if (m_previewFrame)
        m_previewFrame->SetStatusText(status);
#endif

    return true;
}

bool wxPrintPreviewBase::DrawBlankPage(wxPreviewCanvas *canvas, wxDC& dc)
{
    wxRect pageRect, paperRect;

    CalcRects(canvas, pageRect, paperRect);

    // Draw shadow, allowing for 1-pixel border AROUND the actual paper
    wxCoord shadowOffset = 4;

    dc.SetPen(*wxBLACK_PEN);
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.DrawRectangle(paperRect.x + shadowOffset, paperRect.y + paperRect.height + 1,
        paperRect.width, shadowOffset);

    dc.DrawRectangle(paperRect.x + paperRect.width, paperRect.y + shadowOffset,
        shadowOffset, paperRect.height);

    // Draw blank page allowing for 1-pixel border AROUND the actual paper
    dc.SetPen(*wxBLACK_PEN);
    dc.SetBrush(*wxWHITE_BRUSH);
    dc.DrawRectangle(paperRect.x - 2, paperRect.y - 1,
        paperRect.width + 3, paperRect.height + 2);

    return true;
}

void wxPrintPreviewBase::SetZoom(int percent)
{
    if (m_currentZoom == percent)
        return;

    m_currentZoom = percent;

    InvalidatePreviewBitmap();

    if (m_previewCanvas)
    {
        AdjustScrollbars(m_previewCanvas);
        ((wxScrolledWindow *) m_previewCanvas)->Scroll(0, 0);
        m_previewCanvas->ClearBackground();
        m_previewCanvas->Refresh();
        m_previewCanvas->SetFocus();
    }
}

wxPrintDialogData& wxPrintPreviewBase::GetPrintDialogData()
{
    return m_printDialogData;
}

int wxPrintPreviewBase::GetZoom() const
{ return m_currentZoom; }
int wxPrintPreviewBase::GetMaxPage() const
{ return m_maxPage; }
int wxPrintPreviewBase::GetMinPage() const
{ return m_minPage; }
bool wxPrintPreviewBase::IsOk() const
{ return m_isOk; }
void wxPrintPreviewBase::SetOk(bool ok)
{ m_isOk = ok; }

//----------------------------------------------------------------------------
// wxPrintPreview
//----------------------------------------------------------------------------

IMPLEMENT_CLASS(wxPrintPreview, wxPrintPreviewBase)

wxPrintPreview::wxPrintPreview(wxPrintout *printout,
                   wxPrintout *printoutForPrinting,
                   wxPrintDialogData *data) :
    wxPrintPreviewBase( printout, printoutForPrinting, data )
{
    m_pimpl = wxPrintFactory::GetFactory()->
        CreatePrintPreview( printout, printoutForPrinting, data );
}

wxPrintPreview::wxPrintPreview(wxPrintout *printout,
                   wxPrintout *printoutForPrinting,
                   wxPrintData *data ) :
    wxPrintPreviewBase( printout, printoutForPrinting, data )
{
    m_pimpl = wxPrintFactory::GetFactory()->
        CreatePrintPreview( printout, printoutForPrinting, data );
}

wxPrintPreview::~wxPrintPreview()
{
    delete m_pimpl;

    // don't delete twice
    m_printPrintout = NULL;
    m_previewPrintout = NULL;
    m_previewBitmap = NULL;
}

bool wxPrintPreview::SetCurrentPage(int pageNum)
{
    return m_pimpl->SetCurrentPage( pageNum );
}

int wxPrintPreview::GetCurrentPage() const
{
    return m_pimpl->GetCurrentPage();
}

void wxPrintPreview::SetPrintout(wxPrintout *printout)
{
    m_pimpl->SetPrintout( printout );
}

wxPrintout *wxPrintPreview::GetPrintout() const
{
    return m_pimpl->GetPrintout();
}

wxPrintout *wxPrintPreview::GetPrintoutForPrinting() const
{
    return m_pimpl->GetPrintoutForPrinting();
}

void wxPrintPreview::SetFrame(wxFrame *frame)
{
    m_pimpl->SetFrame( frame );
}

void wxPrintPreview::SetCanvas(wxPreviewCanvas *canvas)
{
    m_pimpl->SetCanvas( canvas );
}

wxFrame *wxPrintPreview::GetFrame() const
{
    return m_pimpl->GetFrame();
}

wxPreviewCanvas *wxPrintPreview::GetCanvas() const
{
    return m_pimpl->GetCanvas();
}

bool wxPrintPreview::PaintPage(wxPreviewCanvas *canvas, wxDC& dc)
{
    return m_pimpl->PaintPage( canvas, dc );
}

bool wxPrintPreview::UpdatePageRendering()
{
    return m_pimpl->UpdatePageRendering();
}

bool wxPrintPreview::DrawBlankPage(wxPreviewCanvas *canvas, wxDC& dc)
{
    return m_pimpl->DrawBlankPage( canvas, dc );
}

void wxPrintPreview::AdjustScrollbars(wxPreviewCanvas *canvas)
{
    m_pimpl->AdjustScrollbars( canvas );
}

bool wxPrintPreview::RenderPage(int pageNum)
{
    return m_pimpl->RenderPage( pageNum );
}

void wxPrintPreview::SetZoom(int percent)
{
    m_pimpl->SetZoom( percent );
}

int wxPrintPreview::GetZoom() const
{
    return m_pimpl->GetZoom();
}

wxPrintDialogData& wxPrintPreview::GetPrintDialogData()
{
    return m_pimpl->GetPrintDialogData();
}

int wxPrintPreview::GetMaxPage() const
{
    return m_pimpl->GetMaxPage();
}

int wxPrintPreview::GetMinPage() const
{
    return m_pimpl->GetMinPage();
}

bool wxPrintPreview::IsOk() const
{
    return m_pimpl->IsOk();
}

void wxPrintPreview::SetOk(bool ok)
{
    m_pimpl->SetOk( ok );
}

bool wxPrintPreview::Print(bool interactive)
{
    return m_pimpl->Print( interactive );
}

void wxPrintPreview::DetermineScaling()
{
    m_pimpl->DetermineScaling();
}

#endif // wxUSE_PRINTING_ARCHITECTURE
