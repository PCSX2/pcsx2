/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/print.h
// Author:      Anthony Bretaudeau
// Purpose:     GTK printing support
// Created:     2007-08-25
// Copyright:   (c) Anthony Bretaudeau
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_PRINT_H_
#define _WX_GTK_PRINT_H_

#include "wx/defs.h"

#if wxUSE_GTKPRINT

#include "wx/print.h"
#include "wx/printdlg.h"
#include "wx/prntbase.h"
#include "wx/dc.h"

typedef struct _GtkPrintOperation GtkPrintOperation;
typedef struct _GtkPrintContext GtkPrintContext;
typedef struct _GtkPrintSettings GtkPrintSettings;
typedef struct _GtkPageSetup GtkPageSetup;

typedef struct _cairo cairo_t;

//----------------------------------------------------------------------------
// wxGtkPrintFactory
//----------------------------------------------------------------------------

class wxGtkPrintFactory: public wxPrintFactory
{
public:
    virtual wxPrinterBase *CreatePrinter( wxPrintDialogData *data );

    virtual wxPrintPreviewBase *CreatePrintPreview( wxPrintout *preview,
                                                    wxPrintout *printout = NULL,
                                                    wxPrintDialogData *data = NULL );
    virtual wxPrintPreviewBase *CreatePrintPreview( wxPrintout *preview,
                                                    wxPrintout *printout,
                                                    wxPrintData *data );

    virtual wxPrintDialogBase *CreatePrintDialog( wxWindow *parent,
                                                  wxPrintDialogData *data = NULL );
    virtual wxPrintDialogBase *CreatePrintDialog( wxWindow *parent,
                                                  wxPrintData *data );

    virtual wxPageSetupDialogBase *CreatePageSetupDialog( wxWindow *parent,
                                                          wxPageSetupDialogData * data = NULL );

    virtual wxDCImpl* CreatePrinterDCImpl( wxPrinterDC *owner, const wxPrintData& data );

    virtual bool HasPrintSetupDialog();
    virtual wxDialog *CreatePrintSetupDialog( wxWindow *parent, wxPrintData *data );
    virtual bool HasOwnPrintToFile();
    virtual bool HasPrinterLine();
    virtual wxString CreatePrinterLine();
    virtual bool HasStatusLine();
    virtual wxString CreateStatusLine();

    virtual wxPrintNativeDataBase *CreatePrintNativeData();
};

//----------------------------------------------------------------------------
// wxGtkPrintDialog
//----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGtkPrintDialog: public wxPrintDialogBase
{
public:
    wxGtkPrintDialog( wxWindow *parent,
                         wxPrintDialogData* data = NULL );
    wxGtkPrintDialog( wxWindow *parent, wxPrintData* data);
    virtual ~wxGtkPrintDialog();

    wxPrintData& GetPrintData()
        { return m_printDialogData.GetPrintData(); }
    wxPrintDialogData& GetPrintDialogData()
        { return m_printDialogData; }

    wxDC *GetPrintDC() { return m_dc; }
    void SetPrintDC(wxDC * printDC) { m_dc = printDC; }

    virtual int ShowModal();

    virtual bool Validate() { return true; }
    virtual bool TransferDataToWindow() { return true; }
    virtual bool TransferDataFromWindow() { return true; }

    void SetShowDialog(bool show) { m_showDialog = show; }
    bool GetShowDialog() { return m_showDialog; }

protected:
    // Implement some base class methods to do nothing to avoid asserts and
    // GTK warnings, since this is not a real wxDialog.
    virtual void DoSetSize(int WXUNUSED(x), int WXUNUSED(y),
                           int WXUNUSED(width), int WXUNUSED(height),
                           int WXUNUSED(sizeFlags) = wxSIZE_AUTO) {}
    virtual void DoMoveWindow(int WXUNUSED(x), int WXUNUSED(y),
                              int WXUNUSED(width), int WXUNUSED(height)) {}

private:
    wxPrintDialogData    m_printDialogData;
    wxWindow            *m_parent;
    bool                 m_showDialog;
    wxDC                *m_dc;

    DECLARE_DYNAMIC_CLASS(wxGtkPrintDialog)
};

//----------------------------------------------------------------------------
// wxGtkPageSetupDialog
//----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGtkPageSetupDialog: public wxPageSetupDialogBase
{
public:
    wxGtkPageSetupDialog( wxWindow *parent,
                            wxPageSetupDialogData* data = NULL );
    virtual ~wxGtkPageSetupDialog();

    virtual wxPageSetupDialogData& GetPageSetupDialogData() { return m_pageDialogData; }

    virtual int ShowModal();

    virtual bool Validate() { return true; }
    virtual bool TransferDataToWindow() { return true; }
    virtual bool TransferDataFromWindow() { return true; }

protected:
    // Implement some base class methods to do nothing to avoid asserts and
    // GTK warnings, since this is not a real wxDialog.
    virtual void DoSetSize(int WXUNUSED(x), int WXUNUSED(y),
                           int WXUNUSED(width), int WXUNUSED(height),
                           int WXUNUSED(sizeFlags) = wxSIZE_AUTO) {}
    virtual void DoMoveWindow(int WXUNUSED(x), int WXUNUSED(y),
                              int WXUNUSED(width), int WXUNUSED(height)) {}

private:
    wxPageSetupDialogData    m_pageDialogData;
    wxWindow                *m_parent;

    DECLARE_DYNAMIC_CLASS(wxGtkPageSetupDialog)
};

//----------------------------------------------------------------------------
// wxGtkPrinter
//----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGtkPrinter : public wxPrinterBase
{
public:
    wxGtkPrinter(wxPrintDialogData *data = NULL);
    virtual ~wxGtkPrinter();

    virtual bool Print(wxWindow *parent,
                       wxPrintout *printout,
                       bool prompt = true);
    virtual wxDC* PrintDialog(wxWindow *parent);
    virtual bool Setup(wxWindow *parent);

    GtkPrintContext *GetPrintContext() { return m_gpc; }
    void SetPrintContext(GtkPrintContext *context) {m_gpc = context;}
    void BeginPrint(wxPrintout *printout, GtkPrintOperation *operation, GtkPrintContext *context);
    void DrawPage(wxPrintout *printout, GtkPrintOperation *operation, GtkPrintContext *context, int page_nr);

private:
    GtkPrintContext *m_gpc;
    wxDC            *m_dc;

    DECLARE_DYNAMIC_CLASS(wxGtkPrinter)
    wxDECLARE_NO_COPY_CLASS(wxGtkPrinter);
};

//----------------------------------------------------------------------------
// wxGtkPrintNativeData
//----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGtkPrintNativeData : public wxPrintNativeDataBase
{
public:
    wxGtkPrintNativeData();
    virtual ~wxGtkPrintNativeData();

    virtual bool TransferTo( wxPrintData &data );
    virtual bool TransferFrom( const wxPrintData &data );

    virtual bool Ok() const { return IsOk(); }
    virtual bool IsOk() const { return true; }

    GtkPrintSettings* GetPrintConfig() { return m_config; }
    void SetPrintConfig( GtkPrintSettings * config );

    GtkPrintOperation* GetPrintJob() { return m_job; }
    void SetPrintJob(GtkPrintOperation *job) { m_job = job; }

    GtkPrintContext *GetPrintContext() { return m_context; }
    void SetPrintContext(GtkPrintContext *context) {m_context = context; }


    GtkPageSetup* GetPageSetupFromSettings(GtkPrintSettings* settings);
    void SetPageSetupToSettings(GtkPrintSettings* settings, GtkPageSetup* page_setup);

private:
    // NB: m_config is created and owned by us, but the other objects are not
    //     and their accessors don't change their ref count.
    GtkPrintSettings    *m_config;
    GtkPrintOperation   *m_job;
    GtkPrintContext     *m_context;

    DECLARE_DYNAMIC_CLASS(wxGtkPrintNativeData)
};

//-----------------------------------------------------------------------------
// wxGtkPrinterDC
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGtkPrinterDCImpl : public wxDCImpl
{
public:
    wxGtkPrinterDCImpl( wxPrinterDC *owner, const wxPrintData& data );
    virtual ~wxGtkPrinterDCImpl();

    bool Ok() const { return IsOk(); }
    bool IsOk() const;

    virtual void* GetCairoContext() const;
    virtual void* GetHandle() const;
    
    bool CanDrawBitmap() const { return true; }
    void Clear();
    void SetFont( const wxFont& font );
    void SetPen( const wxPen& pen );
    void SetBrush( const wxBrush& brush );
    void SetLogicalFunction( wxRasterOperationMode function );
    void SetBackground( const wxBrush& brush );
    void DestroyClippingRegion();
    bool StartDoc(const wxString& message);
    void EndDoc();
    void StartPage();
    void EndPage();
    wxCoord GetCharHeight() const;
    wxCoord GetCharWidth() const;
    bool CanGetTextExtent() const { return true; }
    wxSize GetPPI() const;
    virtual int GetDepth() const { return 24; }
    void SetBackgroundMode(int mode);
    void SetPalette(const wxPalette& WXUNUSED(palette)) { }
    void SetResolution(int ppi);

    // overridden for wxPrinterDC Impl
    virtual int GetResolution() const;
    virtual wxRect GetPaperRect() const;

protected:
    bool DoFloodFill(wxCoord x1, wxCoord y1, const wxColour &col,
                     wxFloodFillStyle style=wxFLOOD_SURFACE );
    void DoGradientFillConcentric(const wxRect& rect, const wxColour& initialColour, const wxColour& destColour, const wxPoint& circleCenter);
    void DoGradientFillLinear(const wxRect& rect, const wxColour& initialColour, const wxColour& destColour, wxDirection nDirection = wxEAST);
    bool DoGetPixel(wxCoord x1, wxCoord y1, wxColour *col) const;
    void DoDrawLine(wxCoord x1, wxCoord y1, wxCoord x2, wxCoord y2);
    void DoCrossHair(wxCoord x, wxCoord y);
    void DoDrawArc(wxCoord x1,wxCoord y1,wxCoord x2,wxCoord y2,wxCoord xc,wxCoord yc);
    void DoDrawEllipticArc(wxCoord x,wxCoord y,wxCoord w,wxCoord h,double sa,double ea);
    void DoDrawPoint(wxCoord x, wxCoord y);
    void DoDrawLines(int n, const wxPoint points[], wxCoord xoffset = 0, wxCoord yoffset = 0);
    void DoDrawPolygon(int n, const wxPoint points[], wxCoord xoffset = 0, wxCoord yoffset = 0, wxPolygonFillMode fillStyle=wxODDEVEN_RULE);
    void DoDrawPolyPolygon(int n, const int count[], const wxPoint points[], wxCoord xoffset = 0, wxCoord yoffset = 0, wxPolygonFillMode fillStyle=wxODDEVEN_RULE);
    void DoDrawRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height);
    void DoDrawRoundedRectangle(wxCoord x, wxCoord y, wxCoord width, wxCoord height, double radius = 20.0);
    void DoDrawEllipse(wxCoord x, wxCoord y, wxCoord width, wxCoord height);
#if wxUSE_SPLINES
    void DoDrawSpline(const wxPointList *points);
#endif
    bool DoBlit(wxCoord xdest, wxCoord ydest, wxCoord width, wxCoord height,
            wxDC *source, wxCoord xsrc, wxCoord ysrc,
            wxRasterOperationMode rop = wxCOPY, bool useMask = false,
            wxCoord xsrcMask = wxDefaultCoord, wxCoord ysrcMask = wxDefaultCoord);
    void DoDrawIcon( const wxIcon& icon, wxCoord x, wxCoord y );
    void DoDrawBitmap( const wxBitmap& bitmap, wxCoord x, wxCoord y, bool useMask = false  );
    void DoDrawText(const wxString& text, wxCoord x, wxCoord y );
    void DoDrawRotatedText(const wxString& text, wxCoord x, wxCoord y, double angle);
    void DoSetClippingRegion(wxCoord x, wxCoord y, wxCoord width, wxCoord height);
    void DoSetDeviceClippingRegion( const wxRegion &WXUNUSED(clip) )
    {
        wxFAIL_MSG( "not implemented" );
    }
    void DoGetTextExtent(const wxString& string, wxCoord *x, wxCoord *y,
                     wxCoord *descent = NULL,
                     wxCoord *externalLeading = NULL,
                     const wxFont *theFont = NULL ) const;
    void DoGetSize(int* width, int* height) const;
    void DoGetSizeMM(int *width, int *height) const;

    wxPrintData& GetPrintData() { return m_printData; }
    void SetPrintData(const wxPrintData& data);

private:
    wxPrintData             m_printData;
    PangoContext           *m_context;
    PangoLayout            *m_layout;
    PangoFontDescription   *m_fontdesc;
    cairo_t                *m_cairo;

    unsigned char           m_currentRed;
    unsigned char           m_currentGreen;
    unsigned char           m_currentBlue;
    unsigned char           m_currentAlpha;

    GtkPrintContext        *m_gpc;
    int                     m_resolution;
    double                  m_PS2DEV;
    double                  m_DEV2PS;

    DECLARE_DYNAMIC_CLASS(wxGtkPrinterDCImpl)
    wxDECLARE_NO_COPY_CLASS(wxGtkPrinterDCImpl);
};

// ----------------------------------------------------------------------------
// wxGtkPrintPreview: programmer creates an object of this class to preview a
// wxPrintout.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGtkPrintPreview : public wxPrintPreviewBase
{
public:
    wxGtkPrintPreview(wxPrintout *printout,
                             wxPrintout *printoutForPrinting = NULL,
                             wxPrintDialogData *data = NULL);
    wxGtkPrintPreview(wxPrintout *printout,
                             wxPrintout *printoutForPrinting,
                             wxPrintData *data);

    virtual ~wxGtkPrintPreview();

    virtual bool Print(bool interactive);
    virtual void DetermineScaling();

private:
    void Init(wxPrintout *printout,
              wxPrintout *printoutForPrinting,
              wxPrintData *data);

    // resolution to use in DPI
    int m_resolution;

    DECLARE_CLASS(wxGtkPrintPreview)
};

#endif // wxUSE_GTKPRINT

#endif // _WX_GTK_PRINT_H_
