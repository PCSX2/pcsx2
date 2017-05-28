/////////////////////////////////////////////////////////////////////////////
// Name:        wx/prntbase.h
// Purpose:     Base classes for printing framework
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRNTBASEH__
#define _WX_PRNTBASEH__

#include "wx/defs.h"

#if wxUSE_PRINTING_ARCHITECTURE

#include "wx/event.h"
#include "wx/cmndata.h"
#include "wx/panel.h"
#include "wx/scrolwin.h"
#include "wx/dialog.h"
#include "wx/frame.h"
#include "wx/dc.h"

class WXDLLIMPEXP_FWD_CORE wxDC;
class WXDLLIMPEXP_FWD_CORE wxButton;
class WXDLLIMPEXP_FWD_CORE wxChoice;
class WXDLLIMPEXP_FWD_CORE wxPrintout;
class WXDLLIMPEXP_FWD_CORE wxPrinterBase;
class WXDLLIMPEXP_FWD_CORE wxPrintDialogBase;
class WXDLLIMPEXP_FWD_CORE wxPrintDialog;
class WXDLLIMPEXP_FWD_CORE wxPageSetupDialogBase;
class WXDLLIMPEXP_FWD_CORE wxPageSetupDialog;
class WXDLLIMPEXP_FWD_CORE wxPrintPreviewBase;
class WXDLLIMPEXP_FWD_CORE wxPreviewCanvas;
class WXDLLIMPEXP_FWD_CORE wxPreviewControlBar;
class WXDLLIMPEXP_FWD_CORE wxPreviewFrame;
class WXDLLIMPEXP_FWD_CORE wxPrintFactory;
class WXDLLIMPEXP_FWD_CORE wxPrintNativeDataBase;
class WXDLLIMPEXP_FWD_CORE wxPrintPreview;
class WXDLLIMPEXP_FWD_CORE wxPrintAbortDialog;
class WXDLLIMPEXP_FWD_CORE wxStaticText;
class wxPrintPageMaxCtrl;
class wxPrintPageTextCtrl;

//----------------------------------------------------------------------------
// error consts
//----------------------------------------------------------------------------

enum wxPrinterError
{
    wxPRINTER_NO_ERROR = 0,
    wxPRINTER_CANCELLED,
    wxPRINTER_ERROR
};

// Preview frame modality kind used with wxPreviewFrame::Initialize()
enum wxPreviewFrameModalityKind
{
    // Disable all the other top level windows while the preview is shown.
    wxPreviewFrame_AppModal,

    // Disable only the parent window while the preview is shown.
    wxPreviewFrame_WindowModal,

    // Don't disable any windows.
    wxPreviewFrame_NonModal
};

//----------------------------------------------------------------------------
// wxPrintFactory
//----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPrintFactory
{
public:
    wxPrintFactory() {}
    virtual ~wxPrintFactory() {}

    virtual wxPrinterBase *CreatePrinter( wxPrintDialogData* data ) = 0;

    virtual wxPrintPreviewBase *CreatePrintPreview( wxPrintout *preview,
                                                    wxPrintout *printout = NULL,
                                                    wxPrintDialogData *data = NULL ) = 0;
    virtual wxPrintPreviewBase *CreatePrintPreview( wxPrintout *preview,
                                                    wxPrintout *printout,
                                                    wxPrintData *data ) = 0;

    virtual wxPrintDialogBase *CreatePrintDialog( wxWindow *parent,
                                                  wxPrintDialogData *data = NULL ) = 0;
    virtual wxPrintDialogBase *CreatePrintDialog( wxWindow *parent,
                                                  wxPrintData *data ) = 0;

    virtual wxPageSetupDialogBase *CreatePageSetupDialog( wxWindow *parent,
                                                          wxPageSetupDialogData * data = NULL ) = 0;

    virtual wxDCImpl* CreatePrinterDCImpl( wxPrinterDC *owner, const wxPrintData& data ) = 0;

    // What to do and what to show in the wxPrintDialog
    // a) Use the generic print setup dialog or a native one?
    virtual bool HasPrintSetupDialog() = 0;
    virtual wxDialog *CreatePrintSetupDialog( wxWindow *parent, wxPrintData *data ) = 0;
    // b) Provide the "print to file" option ourselves or via print setup?
    virtual bool HasOwnPrintToFile() = 0;
    // c) Show current printer
    virtual bool HasPrinterLine() = 0;
    virtual wxString CreatePrinterLine() = 0;
    // d) Show Status line for current printer?
    virtual bool HasStatusLine() = 0;
    virtual wxString CreateStatusLine() = 0;


    virtual wxPrintNativeDataBase *CreatePrintNativeData() = 0;

    static void SetPrintFactory( wxPrintFactory *factory );
    static wxPrintFactory *GetFactory();
private:
    static wxPrintFactory *m_factory;
};

class WXDLLIMPEXP_CORE wxNativePrintFactory: public wxPrintFactory
{
public:
    virtual wxPrinterBase *CreatePrinter( wxPrintDialogData *data ) wxOVERRIDE;

    virtual wxPrintPreviewBase *CreatePrintPreview( wxPrintout *preview,
                                                    wxPrintout *printout = NULL,
                                                    wxPrintDialogData *data = NULL ) wxOVERRIDE;
    virtual wxPrintPreviewBase *CreatePrintPreview( wxPrintout *preview,
                                                    wxPrintout *printout,
                                                    wxPrintData *data ) wxOVERRIDE;

    virtual wxPrintDialogBase *CreatePrintDialog( wxWindow *parent,
                                                  wxPrintDialogData *data = NULL ) wxOVERRIDE;
    virtual wxPrintDialogBase *CreatePrintDialog( wxWindow *parent,
                                                  wxPrintData *data ) wxOVERRIDE;

    virtual wxPageSetupDialogBase *CreatePageSetupDialog( wxWindow *parent,
                                                          wxPageSetupDialogData * data = NULL ) wxOVERRIDE;

    virtual wxDCImpl* CreatePrinterDCImpl( wxPrinterDC *owner, const wxPrintData& data ) wxOVERRIDE;

    virtual bool HasPrintSetupDialog() wxOVERRIDE;
    virtual wxDialog *CreatePrintSetupDialog( wxWindow *parent, wxPrintData *data ) wxOVERRIDE;
    virtual bool HasOwnPrintToFile() wxOVERRIDE;
    virtual bool HasPrinterLine() wxOVERRIDE;
    virtual wxString CreatePrinterLine() wxOVERRIDE;
    virtual bool HasStatusLine() wxOVERRIDE;
    virtual wxString CreateStatusLine() wxOVERRIDE;

    virtual wxPrintNativeDataBase *CreatePrintNativeData() wxOVERRIDE;
};

//----------------------------------------------------------------------------
// wxPrintNativeDataBase
//----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPrintNativeDataBase: public wxObject
{
public:
    wxPrintNativeDataBase();
    virtual ~wxPrintNativeDataBase() {}

    virtual bool TransferTo( wxPrintData &data ) = 0;
    virtual bool TransferFrom( const wxPrintData &data ) = 0;
#ifdef __WXOSX__
    // in order to expose functionality already to the result type of the ..PrintData->GetNativeData()
    virtual void TransferFrom( const wxPageSetupDialogData * ) = 0;
    virtual void TransferTo( wxPageSetupDialogData * ) = 0;
#endif
    virtual bool Ok() const { return IsOk(); }
    virtual bool IsOk() const = 0;

    int  m_ref;

private:
    wxDECLARE_CLASS(wxPrintNativeDataBase);
    wxDECLARE_NO_COPY_CLASS(wxPrintNativeDataBase);
};

//----------------------------------------------------------------------------
// wxPrinterBase
//----------------------------------------------------------------------------

/*
 * Represents the printer: manages printing a wxPrintout object
 */

class WXDLLIMPEXP_CORE wxPrinterBase: public wxObject
{
public:
    wxPrinterBase(wxPrintDialogData *data = NULL);
    virtual ~wxPrinterBase();

    virtual wxPrintAbortDialog *CreateAbortWindow(wxWindow *parent, wxPrintout *printout);
    virtual void ReportError(wxWindow *parent, wxPrintout *printout, const wxString& message);

    virtual wxPrintDialogData& GetPrintDialogData() const;
    bool GetAbort() const { return sm_abortIt; }

    static wxPrinterError GetLastError() { return sm_lastError; }

    ///////////////////////////////////////////////////////////////////////////
    // OVERRIDES

    virtual bool Setup(wxWindow *parent) = 0;
    virtual bool Print(wxWindow *parent, wxPrintout *printout, bool prompt = true) = 0;
    virtual wxDC* PrintDialog(wxWindow *parent) = 0;

protected:
    wxPrintDialogData     m_printDialogData;
    wxPrintout*           m_currentPrintout;

    static wxPrinterError sm_lastError;

public:
    static wxWindow*      sm_abortWindow;
    static bool           sm_abortIt;

private:
    wxDECLARE_CLASS(wxPrinterBase);
    wxDECLARE_NO_COPY_CLASS(wxPrinterBase);
};

//----------------------------------------------------------------------------
// wxPrinter
//----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPrinter: public wxPrinterBase
{
public:
    wxPrinter(wxPrintDialogData *data = NULL);
    virtual ~wxPrinter();

    virtual wxPrintAbortDialog *CreateAbortWindow(wxWindow *parent, wxPrintout *printout) wxOVERRIDE;
    virtual void ReportError(wxWindow *parent, wxPrintout *printout, const wxString& message) wxOVERRIDE;

    virtual bool Setup(wxWindow *parent) wxOVERRIDE;
    virtual bool Print(wxWindow *parent, wxPrintout *printout, bool prompt = true) wxOVERRIDE;
    virtual wxDC* PrintDialog(wxWindow *parent) wxOVERRIDE;

    virtual wxPrintDialogData& GetPrintDialogData() const wxOVERRIDE;

protected:
    wxPrinterBase    *m_pimpl;

private:
    wxDECLARE_CLASS(wxPrinter);
    wxDECLARE_NO_COPY_CLASS(wxPrinter);
};

//----------------------------------------------------------------------------
// wxPrintout
//----------------------------------------------------------------------------

/*
 * Represents an object via which a document may be printed.
 * The programmer derives from this, overrides (at least) OnPrintPage,
 * and passes it to a wxPrinter object for printing, or a wxPrintPreview
 * object for previewing.
 */

class WXDLLIMPEXP_CORE wxPrintout: public wxObject
{
public:
    wxPrintout(const wxString& title = wxGetTranslation("Printout"));
    virtual ~wxPrintout();

    virtual bool OnBeginDocument(int startPage, int endPage);
    virtual void OnEndDocument();
    virtual void OnBeginPrinting();
    virtual void OnEndPrinting();

    // Guaranteed to be before any other functions are called
    virtual void OnPreparePrinting() { }

    virtual bool HasPage(int page);
    virtual bool OnPrintPage(int page) = 0;
    virtual void GetPageInfo(int *minPage, int *maxPage, int *pageFrom, int *pageTo);

    virtual wxString GetTitle() const { return m_printoutTitle; }

    wxDC *GetDC() const { return m_printoutDC; }
    void SetDC(wxDC *dc) { m_printoutDC = dc; }

    void FitThisSizeToPaper(const wxSize& imageSize);
    void FitThisSizeToPage(const wxSize& imageSize);
    void FitThisSizeToPageMargins(const wxSize& imageSize, const wxPageSetupDialogData& pageSetupData);
    void MapScreenSizeToPaper();
    void MapScreenSizeToPage();
    void MapScreenSizeToPageMargins(const wxPageSetupDialogData& pageSetupData);
    void MapScreenSizeToDevice();

    wxRect GetLogicalPaperRect() const;
    wxRect GetLogicalPageRect() const;
    wxRect GetLogicalPageMarginsRect(const wxPageSetupDialogData& pageSetupData) const;

    void SetLogicalOrigin(wxCoord x, wxCoord y);
    void OffsetLogicalOrigin(wxCoord xoff, wxCoord yoff);

    void SetPageSizePixels(int w, int  h) { m_pageWidthPixels = w; m_pageHeightPixels = h; }
    void GetPageSizePixels(int *w, int  *h) const { *w = m_pageWidthPixels; *h = m_pageHeightPixels; }
    void SetPageSizeMM(int w, int  h) { m_pageWidthMM = w; m_pageHeightMM = h; }
    void GetPageSizeMM(int *w, int  *h) const { *w = m_pageWidthMM; *h = m_pageHeightMM; }

    void SetPPIScreen(int x, int y) { m_PPIScreenX = x; m_PPIScreenY = y; }
    void SetPPIScreen(const wxSize& ppi) { SetPPIScreen(ppi.x, ppi.y); }
    void GetPPIScreen(int *x, int *y) const { *x = m_PPIScreenX; *y = m_PPIScreenY; }
    void SetPPIPrinter(int x, int y) { m_PPIPrinterX = x; m_PPIPrinterY = y; }
    void SetPPIPrinter(const wxSize& ppi) { SetPPIPrinter(ppi.x, ppi.y); }
    void GetPPIPrinter(int *x, int *y) const { *x = m_PPIPrinterX; *y = m_PPIPrinterY; }

    void SetPaperRectPixels(const wxRect& paperRectPixels) { m_paperRectPixels = paperRectPixels; }
    wxRect GetPaperRectPixels() const { return m_paperRectPixels; }

    // This must be called by wxPrintPreview to associate itself with the
    // printout it uses.
    virtual void SetPreview(wxPrintPreview *preview) { m_preview = preview; }

    wxPrintPreview *GetPreview() const { return m_preview; }
    virtual bool IsPreview() const { return GetPreview() != NULL; }

private:
    wxString         m_printoutTitle;
    wxDC*            m_printoutDC;
    wxPrintPreview  *m_preview;

    int              m_pageWidthPixels;
    int              m_pageHeightPixels;

    int              m_pageWidthMM;
    int              m_pageHeightMM;

    int              m_PPIScreenX;
    int              m_PPIScreenY;
    int              m_PPIPrinterX;
    int              m_PPIPrinterY;

    wxRect           m_paperRectPixels;

private:
    wxDECLARE_ABSTRACT_CLASS(wxPrintout);
    wxDECLARE_NO_COPY_CLASS(wxPrintout);
};

//----------------------------------------------------------------------------
// wxPreviewCanvas
//----------------------------------------------------------------------------

/*
 * Canvas upon which a preview is drawn.
 */

class WXDLLIMPEXP_CORE wxPreviewCanvas: public wxScrolledWindow
{
public:
    wxPreviewCanvas(wxPrintPreviewBase *preview,
                    wxWindow *parent,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxDefaultSize,
                    long style = 0,
                    const wxString& name = wxT("canvas"));
    virtual ~wxPreviewCanvas();

    void SetPreview(wxPrintPreviewBase *preview) { m_printPreview = preview; }

    void OnPaint(wxPaintEvent& event);
    void OnChar(wxKeyEvent &event);
    // Responds to colour changes
    void OnSysColourChanged(wxSysColourChangedEvent& event);

private:
#if wxUSE_MOUSEWHEEL
    void OnMouseWheel(wxMouseEvent& event);
#endif // wxUSE_MOUSEWHEEL
    void OnIdle(wxIdleEvent& event);

    wxPrintPreviewBase* m_printPreview;

    wxDECLARE_CLASS(wxPreviewCanvas);
    wxDECLARE_EVENT_TABLE();
    wxDECLARE_NO_COPY_CLASS(wxPreviewCanvas);
};

//----------------------------------------------------------------------------
// wxPreviewFrame
//----------------------------------------------------------------------------

/*
 * Default frame for showing preview.
 */

class WXDLLIMPEXP_CORE wxPreviewFrame: public wxFrame
{
public:
    wxPreviewFrame(wxPrintPreviewBase *preview,
                   wxWindow *parent,
                   const wxString& title = wxGetTranslation("Print Preview"),
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   long style = wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT,
                   const wxString& name = wxFrameNameStr);
    virtual ~wxPreviewFrame();

    // Either Initialize() or InitializeWithModality() must be called before
    // showing the preview frame, the former being just a particular case of
    // the latter initializing the frame for being showing app-modally.

    // Notice that we must keep Initialize() with its existing signature to
    // avoid breaking the old code that overrides it and we can't reuse the
    // same name for the other functions to avoid virtual function hiding
    // problem and the associated warnings given by some compilers (e.g. from
    // g++ with -Woverloaded-virtual).
    virtual void Initialize()
    {
        InitializeWithModality(wxPreviewFrame_AppModal);
    }

    // Also note that this method is not virtual as it doesn't need to be
    // overridden: it's never called by wxWidgets (of course, the same is true
    // for Initialize() but, again, it must remain virtual for compatibility).
    void InitializeWithModality(wxPreviewFrameModalityKind kind);

    void OnCloseWindow(wxCloseEvent& event);
    virtual void CreateCanvas();
    virtual void CreateControlBar();

    inline wxPreviewControlBar* GetControlBar() const { return m_controlBar; }

protected:
    wxPreviewCanvas*      m_previewCanvas;
    wxPreviewControlBar*  m_controlBar;
    wxPrintPreviewBase*   m_printPreview;
    wxWindowDisabler*     m_windowDisabler;

    wxPreviewFrameModalityKind m_modalityKind;


private:
    void OnChar(wxKeyEvent& event);

    wxDECLARE_EVENT_TABLE();
    wxDECLARE_CLASS(wxPreviewFrame);
    wxDECLARE_NO_COPY_CLASS(wxPreviewFrame);
};

//----------------------------------------------------------------------------
// wxPreviewControlBar
//----------------------------------------------------------------------------

/*
 * A panel with buttons for controlling a print preview.
 * The programmer may wish to use other means for controlling
 * the print preview.
 */

#define wxPREVIEW_PRINT        1
#define wxPREVIEW_PREVIOUS     2
#define wxPREVIEW_NEXT         4
#define wxPREVIEW_ZOOM         8
#define wxPREVIEW_FIRST       16
#define wxPREVIEW_LAST        32
#define wxPREVIEW_GOTO        64

#define wxPREVIEW_DEFAULT  (wxPREVIEW_PREVIOUS|wxPREVIEW_NEXT|wxPREVIEW_ZOOM\
                            |wxPREVIEW_FIRST|wxPREVIEW_GOTO|wxPREVIEW_LAST)

// Ids for controls
#define wxID_PREVIEW_CLOSE      1
#define wxID_PREVIEW_NEXT       2
#define wxID_PREVIEW_PREVIOUS   3
#define wxID_PREVIEW_PRINT      4
#define wxID_PREVIEW_ZOOM       5
#define wxID_PREVIEW_FIRST      6
#define wxID_PREVIEW_LAST       7
#define wxID_PREVIEW_GOTO       8
#define wxID_PREVIEW_ZOOM_IN    9
#define wxID_PREVIEW_ZOOM_OUT   10

class WXDLLIMPEXP_CORE wxPreviewControlBar: public wxPanel
{
    wxDECLARE_CLASS(wxPreviewControlBar);

public:
    wxPreviewControlBar(wxPrintPreviewBase *preview,
                        long buttons,
                        wxWindow *parent,
                        const wxPoint& pos = wxDefaultPosition,
                        const wxSize& size = wxDefaultSize,
                        long style = wxTAB_TRAVERSAL,
                        const wxString& name = wxT("panel"));
    virtual ~wxPreviewControlBar();

    virtual void CreateButtons();
    virtual void SetPageInfo(int minPage, int maxPage);
    virtual void SetZoomControl(int zoom);
    virtual int GetZoomControl();
    virtual wxPrintPreviewBase *GetPrintPreview() const
        { return m_printPreview; }


    // Implementation only from now on.
    void OnWindowClose(wxCommandEvent& event);
    void OnNext();
    void OnPrevious();
    void OnFirst();
    void OnLast();
    void OnGotoPage();
    void OnPrint();

    void OnPrintButton(wxCommandEvent& WXUNUSED(event)) { OnPrint(); }
    void OnNextButton(wxCommandEvent & WXUNUSED(event)) { OnNext(); }
    void OnPreviousButton(wxCommandEvent & WXUNUSED(event)) { OnPrevious(); }
    void OnFirstButton(wxCommandEvent & WXUNUSED(event)) { OnFirst(); }
    void OnLastButton(wxCommandEvent & WXUNUSED(event)) { OnLast(); }
    void OnPaint(wxPaintEvent& event);

    void OnUpdateNextButton(wxUpdateUIEvent& event)
        { event.Enable(IsNextEnabled()); }
    void OnUpdatePreviousButton(wxUpdateUIEvent& event)
        { event.Enable(IsPreviousEnabled()); }
    void OnUpdateFirstButton(wxUpdateUIEvent& event)
        { event.Enable(IsFirstEnabled()); }
    void OnUpdateLastButton(wxUpdateUIEvent& event)
        { event.Enable(IsLastEnabled()); }
    void OnUpdateZoomInButton(wxUpdateUIEvent& event)
        { event.Enable(IsZoomInEnabled()); }
    void OnUpdateZoomOutButton(wxUpdateUIEvent& event)
        { event.Enable(IsZoomOutEnabled()); }

    // These methods are not private because they are called by wxPreviewCanvas.
    void DoZoomIn();
    void DoZoomOut();

protected:
    wxPrintPreviewBase*   m_printPreview;
    wxButton*             m_closeButton;
    wxChoice*             m_zoomControl;
    wxPrintPageTextCtrl*  m_currentPageText;
    wxPrintPageMaxCtrl*   m_maxPageText;

    long                  m_buttonFlags;

private:
    void DoGotoPage(int page);

    void DoZoom();

    bool IsNextEnabled() const;
    bool IsPreviousEnabled() const;
    bool IsFirstEnabled() const;
    bool IsLastEnabled() const;
    bool IsZoomInEnabled() const;
    bool IsZoomOutEnabled() const;

    void OnZoomInButton(wxCommandEvent & WXUNUSED(event)) { DoZoomIn(); }
    void OnZoomOutButton(wxCommandEvent & WXUNUSED(event)) { DoZoomOut(); }
    void OnZoomChoice(wxCommandEvent& WXUNUSED(event)) { DoZoom(); }

    wxDECLARE_EVENT_TABLE();
    wxDECLARE_NO_COPY_CLASS(wxPreviewControlBar);
};

//----------------------------------------------------------------------------
// wxPrintPreviewBase
//----------------------------------------------------------------------------

/*
 * Programmer creates an object of this class to preview a wxPrintout.
 */

class WXDLLIMPEXP_CORE wxPrintPreviewBase: public wxObject
{
public:
    wxPrintPreviewBase(wxPrintout *printout,
                       wxPrintout *printoutForPrinting = NULL,
                       wxPrintDialogData *data = NULL);
    wxPrintPreviewBase(wxPrintout *printout,
                       wxPrintout *printoutForPrinting,
                       wxPrintData *data);
    virtual ~wxPrintPreviewBase();

    virtual bool SetCurrentPage(int pageNum);
    virtual int GetCurrentPage() const;

    virtual void SetPrintout(wxPrintout *printout);
    virtual wxPrintout *GetPrintout() const;
    virtual wxPrintout *GetPrintoutForPrinting() const;

    virtual void SetFrame(wxFrame *frame);
    virtual void SetCanvas(wxPreviewCanvas *canvas);

    virtual wxFrame *GetFrame() const;
    virtual wxPreviewCanvas *GetCanvas() const;

    // This is a helper routine, used by the next 4 routines.

    virtual void CalcRects(wxPreviewCanvas *canvas, wxRect& printableAreaRect, wxRect& paperRect);

    // The preview canvas should call this from OnPaint
    virtual bool PaintPage(wxPreviewCanvas *canvas, wxDC& dc);

    // Updates rendered page by calling RenderPage() if needed, returns true
    // if there was some change. Preview canvas should call it at idle time
    virtual bool UpdatePageRendering();

    // This draws a blank page onto the preview canvas
    virtual bool DrawBlankPage(wxPreviewCanvas *canvas, wxDC& dc);

    // Adjusts the scrollbars for the current scale
    virtual void AdjustScrollbars(wxPreviewCanvas *canvas);

    // This is called by wxPrintPreview to render a page into a wxMemoryDC.
    virtual bool RenderPage(int pageNum);


    virtual void SetZoom(int percent);
    virtual int GetZoom() const;

    virtual wxPrintDialogData& GetPrintDialogData();

    virtual int GetMaxPage() const;
    virtual int GetMinPage() const;

    virtual bool Ok() const { return IsOk(); }
    virtual bool IsOk() const;
    virtual void SetOk(bool ok);

    ///////////////////////////////////////////////////////////////////////////
    // OVERRIDES

    // If we own a wxPrintout that can be used for printing, this
    // will invoke the actual printing procedure. Called
    // by the wxPreviewControlBar.
    virtual bool Print(bool interactive) = 0;

    // Calculate scaling that needs to be done to get roughly
    // the right scaling for the screen pretending to be
    // the currently selected printer.
    virtual void DetermineScaling() = 0;

protected:
    // helpers for RenderPage():
    virtual bool RenderPageIntoDC(wxDC& dc, int pageNum);
    // renders preview into m_previewBitmap
    virtual bool RenderPageIntoBitmap(wxBitmap& bmp, int pageNum);

    void InvalidatePreviewBitmap();

protected:
    wxPrintDialogData m_printDialogData;
    wxPreviewCanvas*  m_previewCanvas;
    wxFrame*          m_previewFrame;
    wxBitmap*         m_previewBitmap;
    bool              m_previewFailed;
    wxPrintout*       m_previewPrintout;
    wxPrintout*       m_printPrintout;
    int               m_currentPage;
    int               m_currentZoom;
    float             m_previewScaleX;
    float             m_previewScaleY;
    int               m_topMargin;
    int               m_leftMargin;
    int               m_pageWidth;
    int               m_pageHeight;
    int               m_minPage;
    int               m_maxPage;

    bool              m_isOk;
    bool              m_printingPrepared; // Called OnPreparePrinting?

private:
    void Init(wxPrintout *printout, wxPrintout *printoutForPrinting);

    wxDECLARE_NO_COPY_CLASS(wxPrintPreviewBase);
    wxDECLARE_CLASS(wxPrintPreviewBase);
};

//----------------------------------------------------------------------------
// wxPrintPreview
//----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPrintPreview: public wxPrintPreviewBase
{
public:
    wxPrintPreview(wxPrintout *printout,
                   wxPrintout *printoutForPrinting = NULL,
                   wxPrintDialogData *data = NULL);
    wxPrintPreview(wxPrintout *printout,
                   wxPrintout *printoutForPrinting,
                   wxPrintData *data);
    virtual ~wxPrintPreview();

    virtual bool SetCurrentPage(int pageNum) wxOVERRIDE;
    virtual int GetCurrentPage() const wxOVERRIDE;
    virtual void SetPrintout(wxPrintout *printout) wxOVERRIDE;
    virtual wxPrintout *GetPrintout() const wxOVERRIDE;
    virtual wxPrintout *GetPrintoutForPrinting() const wxOVERRIDE;
    virtual void SetFrame(wxFrame *frame) wxOVERRIDE;
    virtual void SetCanvas(wxPreviewCanvas *canvas) wxOVERRIDE;

    virtual wxFrame *GetFrame() const wxOVERRIDE;
    virtual wxPreviewCanvas *GetCanvas() const wxOVERRIDE;
    virtual bool PaintPage(wxPreviewCanvas *canvas, wxDC& dc) wxOVERRIDE;
    virtual bool UpdatePageRendering() wxOVERRIDE;
    virtual bool DrawBlankPage(wxPreviewCanvas *canvas, wxDC& dc) wxOVERRIDE;
    virtual void AdjustScrollbars(wxPreviewCanvas *canvas) wxOVERRIDE;
    virtual bool RenderPage(int pageNum) wxOVERRIDE;
    virtual void SetZoom(int percent) wxOVERRIDE;
    virtual int GetZoom() const wxOVERRIDE;

    virtual bool Print(bool interactive) wxOVERRIDE;
    virtual void DetermineScaling() wxOVERRIDE;

    virtual wxPrintDialogData& GetPrintDialogData() wxOVERRIDE;

    virtual int GetMaxPage() const wxOVERRIDE;
    virtual int GetMinPage() const wxOVERRIDE;

    virtual bool Ok() const wxOVERRIDE { return IsOk(); }
    virtual bool IsOk() const wxOVERRIDE;
    virtual void SetOk(bool ok) wxOVERRIDE;

private:
    wxPrintPreviewBase *m_pimpl;

private:
    wxDECLARE_CLASS(wxPrintPreview);
    wxDECLARE_NO_COPY_CLASS(wxPrintPreview);
};

//----------------------------------------------------------------------------
// wxPrintAbortDialog
//----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxPrintAbortDialog: public wxDialog
{
public:
    wxPrintAbortDialog(wxWindow *parent,
                       const wxString& documentTitle,
                       const wxPoint& pos = wxDefaultPosition,
                       const wxSize& size = wxDefaultSize,
                       long style = wxDEFAULT_DIALOG_STYLE,
                       const wxString& name = wxT("dialog"));

    void SetProgress(int currentPage, int totalPages,
                     int currentCopy, int totalCopies);

    void OnCancel(wxCommandEvent& event);

private:
    wxStaticText *m_progress;

    wxDECLARE_EVENT_TABLE();
    wxDECLARE_NO_COPY_CLASS(wxPrintAbortDialog);
};

#endif // wxUSE_PRINTING_ARCHITECTURE

#endif
    // _WX_PRNTBASEH__
