/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/colrdlgg.h
// Purpose:     wxGenericColourDialog
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COLORDLGG_H_
#define _WX_COLORDLGG_H_

#include "wx/gdicmn.h"
#include "wx/dialog.h"

#define wxID_ADD_CUSTOM     3000

#if wxUSE_SLIDER

    #define wxID_RED_SLIDER     3001
    #define wxID_GREEN_SLIDER   3002
    #define wxID_BLUE_SLIDER    3003

    class WXDLLIMPEXP_FWD_CORE wxSlider;

#endif // wxUSE_SLIDER

class WXDLLIMPEXP_CORE wxGenericColourDialog : public wxDialog
{
public:
    wxGenericColourDialog();
    wxGenericColourDialog(wxWindow *parent,
                          wxColourData *data = NULL);
    virtual ~wxGenericColourDialog();

    bool Create(wxWindow *parent, wxColourData *data = NULL);

    wxColourData &GetColourData() { return m_colourData; }

    virtual int ShowModal();

    // Internal functions
    void OnMouseEvent(wxMouseEvent& event);
    void OnPaint(wxPaintEvent& event);

    virtual void CalculateMeasurements();
    virtual void CreateWidgets();
    virtual void InitializeColours();

    virtual void PaintBasicColours(wxDC& dc);
    virtual void PaintCustomColours(wxDC& dc);
    virtual void PaintCustomColour(wxDC& dc);
    virtual void PaintHighlight(wxDC& dc, bool draw);

    virtual void OnBasicColourClick(int which);
    virtual void OnCustomColourClick(int which);

    void OnAddCustom(wxCommandEvent& event);

#if wxUSE_SLIDER
    void OnRedSlider(wxCommandEvent& event);
    void OnGreenSlider(wxCommandEvent& event);
    void OnBlueSlider(wxCommandEvent& event);
#endif // wxUSE_SLIDER

    void OnCloseWindow(wxCloseEvent& event);

protected:
    wxColourData m_colourData;

    // Area reserved for grids of colours
    wxRect m_standardColoursRect;
    wxRect m_customColoursRect;
    wxRect m_singleCustomColourRect;

    // Size of each colour rectangle
    wxPoint m_smallRectangleSize;

    // For single customizable colour
    wxPoint m_customRectangleSize;

    // Grid spacing (between rectangles)
    int m_gridSpacing;

    // Section spacing (between left and right halves of dialog box)
    int m_sectionSpacing;

    // 48 'standard' colours
    wxColour m_standardColours[48];

    // 16 'custom' colours
    wxColour m_customColours[16];

    // Which colour is selected? An index into one of the two areas.
    int m_colourSelection;
    int m_whichKind; // 1 for standard colours, 2 for custom colours,

#if wxUSE_SLIDER
    wxSlider *m_redSlider;
    wxSlider *m_greenSlider;
    wxSlider *m_blueSlider;
#endif // wxUSE_SLIDER

    int m_buttonY;

    int m_okButtonX;
    int m_customButtonX;

    //  static bool colourDialogCancelled;

    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxGenericColourDialog)
};

#endif // _WX_COLORDLGG_H_
