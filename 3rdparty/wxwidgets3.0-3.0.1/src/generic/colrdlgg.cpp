/////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/colrdlgg.cpp
// Purpose:     Choice dialogs
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

#if wxUSE_COLOURDLG

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/intl.h"
    #include "wx/dialog.h"
    #include "wx/listbox.h"
    #include "wx/button.h"
    #include "wx/stattext.h"
    #include "wx/layout.h"
    #include "wx/dcclient.h"
    #include "wx/sizer.h"
    #include "wx/slider.h"
#endif

#if wxUSE_STATLINE
    #include "wx/statline.h"
#endif

#include "wx/colourdata.h"
#include "wx/generic/colrdlgg.h"

IMPLEMENT_DYNAMIC_CLASS(wxGenericColourDialog, wxDialog)

BEGIN_EVENT_TABLE(wxGenericColourDialog, wxDialog)
    EVT_BUTTON(wxID_ADD_CUSTOM, wxGenericColourDialog::OnAddCustom)
#if wxUSE_SLIDER
    EVT_SLIDER(wxID_RED_SLIDER, wxGenericColourDialog::OnRedSlider)
    EVT_SLIDER(wxID_GREEN_SLIDER, wxGenericColourDialog::OnGreenSlider)
    EVT_SLIDER(wxID_BLUE_SLIDER, wxGenericColourDialog::OnBlueSlider)
#endif
    EVT_PAINT(wxGenericColourDialog::OnPaint)
    EVT_MOUSE_EVENTS(wxGenericColourDialog::OnMouseEvent)
    EVT_CLOSE(wxGenericColourDialog::OnCloseWindow)
END_EVENT_TABLE()


/*
 * Generic wxColourDialog
 */

// don't change the number of elements (48) in this array, the code below is
// hardcoded to use it
static const wxChar *const wxColourDialogNames[] =
{
    wxT("ORANGE"),
    wxT("GOLDENROD"),
    wxT("WHEAT"),
    wxT("SPRING GREEN"),
    wxT("SKY BLUE"),
    wxT("SLATE BLUE"),
    wxT("MEDIUM VIOLET RED"),
    wxT("PURPLE"),

    wxT("RED"),
    wxT("YELLOW"),
    wxT("MEDIUM SPRING GREEN"),
    wxT("PALE GREEN"),
    wxT("CYAN"),
    wxT("LIGHT STEEL BLUE"),
    wxT("ORCHID"),
    wxT("LIGHT MAGENTA"),

    wxT("BROWN"),
    wxT("YELLOW"),
    wxT("GREEN"),
    wxT("CADET BLUE"),
    wxT("MEDIUM BLUE"),
    wxT("MAGENTA"),
    wxT("MAROON"),
    wxT("ORANGE RED"),

    wxT("FIREBRICK"),
    wxT("CORAL"),
    wxT("FOREST GREEN"),
    wxT("AQUAMARINE"),
    wxT("BLUE"),
    wxT("NAVY"),
    wxT("THISTLE"),
    wxT("MEDIUM VIOLET RED"),

    wxT("INDIAN RED"),
    wxT("GOLD"),
    wxT("MEDIUM SEA GREEN"),
    wxT("MEDIUM BLUE"),
    wxT("MIDNIGHT BLUE"),
    wxT("GREY"),
    wxT("PURPLE"),
    wxT("KHAKI"),

    wxT("BLACK"),
    wxT("MEDIUM FOREST GREEN"),
    wxT("KHAKI"),
    wxT("DARK GREY"),
    wxT("SEA GREEN"),
    wxT("LIGHT GREY"),
    wxT("MEDIUM SLATE BLUE"),
    wxT("WHITE")
};

wxGenericColourDialog::wxGenericColourDialog()
{
    m_whichKind = 1;
    m_colourSelection = -1;
}

wxGenericColourDialog::wxGenericColourDialog(wxWindow *parent,
                                             wxColourData *data)
{
    m_whichKind = 1;
    m_colourSelection = -1;
    Create(parent, data);
}

wxGenericColourDialog::~wxGenericColourDialog()
{
}

void wxGenericColourDialog::OnCloseWindow(wxCloseEvent& WXUNUSED(event))
{
    EndModal(wxID_CANCEL);
}

bool wxGenericColourDialog::Create(wxWindow *parent, wxColourData *data)
{
    if ( !wxDialog::Create(GetParentForModalDialog(parent, 0), wxID_ANY,
                           _("Choose colour"),
                           wxPoint(0, 0), wxSize(900, 900)) )
        return false;

    if (data)
        m_colourData = *data;

    InitializeColours();
    CalculateMeasurements();
    CreateWidgets();

    return true;
}

int wxGenericColourDialog::ShowModal()
{
    return wxDialog::ShowModal();
}


// Internal functions
void wxGenericColourDialog::OnMouseEvent(wxMouseEvent& event)
{
  if (event.ButtonDown(1))
  {
    int x = (int)event.GetX();
    int y = (int)event.GetY();

#ifdef __WXPM__
    // Handle OS/2's reverse coordinate system and account for the dialog title
    int                             nClientHeight;

    GetClientSize(NULL, &nClientHeight);
    y = (nClientHeight - y) + 20;
#endif
    if ((x >= m_standardColoursRect.x && x <= (m_standardColoursRect.x + m_standardColoursRect.width)) &&
        (y >= m_standardColoursRect.y && y <= (m_standardColoursRect.y + m_standardColoursRect.height)))
    {
      int selX = (int)(x - m_standardColoursRect.x)/(m_smallRectangleSize.x + m_gridSpacing);
      int selY = (int)(y - m_standardColoursRect.y)/(m_smallRectangleSize.y + m_gridSpacing);
      int ptr = (int)(selX + selY*8);
      OnBasicColourClick(ptr);
    }
    else if ((x >= m_customColoursRect.x && x <= (m_customColoursRect.x + m_customColoursRect.width)) &&
        (y >= m_customColoursRect.y && y <= (m_customColoursRect.y + m_customColoursRect.height)))
    {
      int selX = (int)(x - m_customColoursRect.x)/(m_smallRectangleSize.x + m_gridSpacing);
      int selY = (int)(y - m_customColoursRect.y)/(m_smallRectangleSize.y + m_gridSpacing);
      int ptr = (int)(selX + selY*8);
      OnCustomColourClick(ptr);
    }
    else
        event.Skip();
  }
  else
      event.Skip();
}

void wxGenericColourDialog::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);

    PaintBasicColours(dc);
    PaintCustomColours(dc);
    PaintCustomColour(dc);
    PaintHighlight(dc, true);
}

void wxGenericColourDialog::CalculateMeasurements()
{
    m_smallRectangleSize.x = 18;
    m_smallRectangleSize.y = 14;
    m_customRectangleSize.x = 40;
    m_customRectangleSize.y = 40;

    m_gridSpacing = 6;
    m_sectionSpacing = 15;

    m_standardColoursRect.x = 10;
#ifdef __WXPM__
    m_standardColoursRect.y = 15 + 20; /* OS/2 needs to account for dialog titlebar */
#else
    m_standardColoursRect.y = 15;
#endif
    m_standardColoursRect.width = (8*m_smallRectangleSize.x) + (7*m_gridSpacing);
    m_standardColoursRect.height = (6*m_smallRectangleSize.y) + (5*m_gridSpacing);

    m_customColoursRect.x = m_standardColoursRect.x;
    m_customColoursRect.y = m_standardColoursRect.y + m_standardColoursRect.height  + 20;
    m_customColoursRect.width = (8*m_smallRectangleSize.x) + (7*m_gridSpacing);
    m_customColoursRect.height = (2*m_smallRectangleSize.y) + (1*m_gridSpacing);

    m_singleCustomColourRect.x = m_customColoursRect.width + m_customColoursRect.x + m_sectionSpacing;
    m_singleCustomColourRect.y = 80;
    m_singleCustomColourRect.width = m_customRectangleSize.x;
    m_singleCustomColourRect.height = m_customRectangleSize.y;

    m_okButtonX = 10;
    m_customButtonX = m_singleCustomColourRect.x ;
    m_buttonY = m_customColoursRect.y + m_customColoursRect.height + 10;
}

void wxGenericColourDialog::CreateWidgets()
{
    wxBeginBusyCursor();

    wxBoxSizer *topSizer = new wxBoxSizer( wxVERTICAL );

    const int sliderHeight = 160;

    // first sliders
#if wxUSE_SLIDER
    const int sliderX = m_singleCustomColourRect.x + m_singleCustomColourRect.width + m_sectionSpacing;

    m_redSlider = new wxSlider(this, wxID_RED_SLIDER, m_colourData.m_dataColour.Red(), 0, 255,
        wxDefaultPosition, wxSize(wxDefaultCoord, sliderHeight), wxSL_VERTICAL|wxSL_LABELS|wxSL_INVERSE);
    m_greenSlider = new wxSlider(this, wxID_GREEN_SLIDER, m_colourData.m_dataColour.Green(), 0, 255,
        wxDefaultPosition, wxSize(wxDefaultCoord, sliderHeight), wxSL_VERTICAL|wxSL_LABELS|wxSL_INVERSE);
    m_blueSlider = new wxSlider(this, wxID_BLUE_SLIDER, m_colourData.m_dataColour.Blue(), 0, 255,
        wxDefaultPosition, wxSize(wxDefaultCoord, sliderHeight), wxSL_VERTICAL|wxSL_LABELS|wxSL_INVERSE);

    wxBoxSizer *sliderSizer = new wxBoxSizer( wxHORIZONTAL );

    sliderSizer->Add(sliderX, sliderHeight );

    wxSizerFlags flagsRight;
    flagsRight.Align(wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL).DoubleBorder();

    sliderSizer->Add(m_redSlider, flagsRight);
    sliderSizer->Add(m_greenSlider,flagsRight);
    sliderSizer->Add(m_blueSlider,flagsRight);

    topSizer->Add(sliderSizer, wxSizerFlags().Centre().DoubleBorder());
#else
    topSizer->Add(1, sliderHeight, wxSizerFlags(1).Centre().TripleBorder());
#endif // wxUSE_SLIDER

    // then the custom button
    topSizer->Add(new wxButton(this, wxID_ADD_CUSTOM,
                                  _("Add to custom colours") ),
                     wxSizerFlags().DoubleHorzBorder());

    // then the standard buttons
    wxSizer *buttonsizer = CreateSeparatedButtonSizer(wxOK | wxCANCEL);
    if ( buttonsizer )
    {
        topSizer->Add(buttonsizer, wxSizerFlags().Expand().DoubleBorder());
    }

    SetAutoLayout( true );
    SetSizer( topSizer );

    topSizer->SetSizeHints( this );
    topSizer->Fit( this );

    Centre( wxBOTH );

    wxEndBusyCursor();
}

void wxGenericColourDialog::InitializeColours(void)
{
    size_t i;

    for (i = 0; i < WXSIZEOF(wxColourDialogNames); i++)
    {
        wxColour col = wxTheColourDatabase->Find(wxColourDialogNames[i]);
        if (col.IsOk())
            m_standardColours[i].Set(col.Red(), col.Green(), col.Blue());
        else
            m_standardColours[i].Set(0, 0, 0);
    }

    for (i = 0; i < WXSIZEOF(m_customColours); i++)
    {
        wxColour c = m_colourData.GetCustomColour(i);
        if (c.IsOk())
            m_customColours[i] = m_colourData.GetCustomColour(i);
        else
            m_customColours[i] = wxColour(255, 255, 255);
    }

    wxColour curr = m_colourData.GetColour();
    if ( curr.IsOk() )
    {
        bool m_initColourFound = false;

        for (i = 0; i < WXSIZEOF(wxColourDialogNames); i++)
        {
            if ( m_standardColours[i] == curr && !m_initColourFound )
            {
                m_whichKind = 1;
                m_colourSelection = i;
                m_initColourFound = true;
                break;
            }
        }
        if ( !m_initColourFound )
        {
            for ( i = 0; i < WXSIZEOF(m_customColours); i++ )
            {
                if ( m_customColours[i] == curr )
                {
                    m_whichKind = 2;
                    m_colourSelection = i;
                    break;
                }
            }
        }
        m_colourData.m_dataColour.Set( curr.Red(), curr.Green(), curr.Blue() );
    }
    else
    {
        m_whichKind = 1;
        m_colourSelection = 0;
        m_colourData.m_dataColour.Set( 0, 0, 0 );
    }
}

void wxGenericColourDialog::PaintBasicColours(wxDC& dc)
{
    int i;
    for (i = 0; i < 6; i++)
    {
        int j;
        for (j = 0; j < 8; j++)
        {
            int ptr = i*8 + j;

            int x = (j*(m_smallRectangleSize.x+m_gridSpacing) + m_standardColoursRect.x);
            int y = (i*(m_smallRectangleSize.y+m_gridSpacing) + m_standardColoursRect.y);

            dc.SetPen(*wxBLACK_PEN);
            wxBrush brush(m_standardColours[ptr]);
            dc.SetBrush(brush);

            dc.DrawRectangle( x, y, m_smallRectangleSize.x, m_smallRectangleSize.y);
        }
    }
}

void wxGenericColourDialog::PaintCustomColours(wxDC& dc)
{
  int i;
  for (i = 0; i < 2; i++)
  {
    int j;
    for (j = 0; j < 8; j++)
    {
      int ptr = i*8 + j;

      int x = (j*(m_smallRectangleSize.x+m_gridSpacing)) + m_customColoursRect.x;
      int y = (i*(m_smallRectangleSize.y+m_gridSpacing)) + m_customColoursRect.y;

      dc.SetPen(*wxBLACK_PEN);

      wxBrush brush(m_customColours[ptr]);
      dc.SetBrush(brush);

      dc.DrawRectangle( x, y, m_smallRectangleSize.x, m_smallRectangleSize.y);
    }
  }
}

void wxGenericColourDialog::PaintHighlight(wxDC& dc, bool draw)
{
  if ( m_colourSelection < 0 )
      return;

  // Number of pixels bigger than the standard rectangle size
  // for drawing a highlight
  int deltaX = 2;
  int deltaY = 2;

  if (m_whichKind == 1)
  {
    // Standard colours
    int y = (int)(m_colourSelection / 8);
    int x = (int)(m_colourSelection - (y*8));

    x = (x*(m_smallRectangleSize.x + m_gridSpacing) + m_standardColoursRect.x) - deltaX;
    y = (y*(m_smallRectangleSize.y + m_gridSpacing) + m_standardColoursRect.y) - deltaY;

    if (draw)
      dc.SetPen(*wxBLACK_PEN);
    else
      dc.SetPen(*wxLIGHT_GREY_PEN);

    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle( x, y, (m_smallRectangleSize.x + (2*deltaX)), (m_smallRectangleSize.y + (2*deltaY)));
  }
  else
  {
    // User-defined colours
    int y = (int)(m_colourSelection / 8);
    int x = (int)(m_colourSelection - (y*8));

    x = (x*(m_smallRectangleSize.x + m_gridSpacing) + m_customColoursRect.x) - deltaX;
    y = (y*(m_smallRectangleSize.y + m_gridSpacing) + m_customColoursRect.y) - deltaY;

    if (draw)
      dc.SetPen(*wxBLACK_PEN);
    else
      dc.SetPen(*wxLIGHT_GREY_PEN);

    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle( x, y, (m_smallRectangleSize.x + (2*deltaX)), (m_smallRectangleSize.y + (2*deltaY)));
  }
}

void wxGenericColourDialog::PaintCustomColour(wxDC& dc)
{
    dc.SetPen(*wxBLACK_PEN);

    wxBrush *brush = new wxBrush(m_colourData.m_dataColour);
    dc.SetBrush(*brush);

    dc.DrawRectangle( m_singleCustomColourRect.x, m_singleCustomColourRect.y,
                      m_customRectangleSize.x, m_customRectangleSize.y);

    dc.SetBrush(wxNullBrush);
    delete brush;
}

void wxGenericColourDialog::OnBasicColourClick(int which)
{
    wxClientDC dc(this);

    PaintHighlight(dc, false);
    m_whichKind = 1;
    m_colourSelection = which;

#if wxUSE_SLIDER
    m_redSlider->SetValue( m_standardColours[m_colourSelection].Red() );
    m_greenSlider->SetValue( m_standardColours[m_colourSelection].Green() );
    m_blueSlider->SetValue( m_standardColours[m_colourSelection].Blue() );
#endif // wxUSE_SLIDER

    m_colourData.m_dataColour.Set(m_standardColours[m_colourSelection].Red(),
                                m_standardColours[m_colourSelection].Green(),
                                m_standardColours[m_colourSelection].Blue());

    PaintCustomColour(dc);
    PaintHighlight(dc, true);
}

void wxGenericColourDialog::OnCustomColourClick(int which)
{
    wxClientDC dc(this);
    PaintHighlight(dc, false);
    m_whichKind = 2;
    m_colourSelection = which;

#if wxUSE_SLIDER
    m_redSlider->SetValue( m_customColours[m_colourSelection].Red() );
    m_greenSlider->SetValue( m_customColours[m_colourSelection].Green() );
    m_blueSlider->SetValue( m_customColours[m_colourSelection].Blue() );
#endif // wxUSE_SLIDER

    m_colourData.m_dataColour.Set(m_customColours[m_colourSelection].Red(),
                                m_customColours[m_colourSelection].Green(),
                                m_customColours[m_colourSelection].Blue());

    PaintCustomColour(dc);
    PaintHighlight(dc, true);
}

/*
void wxGenericColourDialog::OnOk(void)
{
  Show(false);
}

void wxGenericColourDialog::OnCancel(void)
{
  colourDialogCancelled = true;
  Show(false);
}
*/

void wxGenericColourDialog::OnAddCustom(wxCommandEvent& WXUNUSED(event))
{
  wxClientDC dc(this);
  if (m_whichKind != 2)
  {
    PaintHighlight(dc, false);
    m_whichKind = 2;
    m_colourSelection = 0;
    PaintHighlight(dc, true);
  }

  m_customColours[m_colourSelection].Set(m_colourData.m_dataColour.Red(),
                                     m_colourData.m_dataColour.Green(),
                                     m_colourData.m_dataColour.Blue());

  m_colourData.SetCustomColour(m_colourSelection, m_customColours[m_colourSelection]);

  PaintCustomColours(dc);
}

#if wxUSE_SLIDER

void wxGenericColourDialog::OnRedSlider(wxCommandEvent& WXUNUSED(event))
{
  if (!m_redSlider)
    return;

  wxClientDC dc(this);
  m_colourData.m_dataColour.Set((unsigned char)m_redSlider->GetValue(), m_colourData.m_dataColour.Green(), m_colourData.m_dataColour.Blue());
  PaintCustomColour(dc);
}

void wxGenericColourDialog::OnGreenSlider(wxCommandEvent& WXUNUSED(event))
{
  if (!m_greenSlider)
    return;

  wxClientDC dc(this);
  m_colourData.m_dataColour.Set(m_colourData.m_dataColour.Red(), (unsigned char)m_greenSlider->GetValue(), m_colourData.m_dataColour.Blue());
  PaintCustomColour(dc);
}

void wxGenericColourDialog::OnBlueSlider(wxCommandEvent& WXUNUSED(event))
{
  if (!m_blueSlider)
    return;

  wxClientDC dc(this);
  m_colourData.m_dataColour.Set(m_colourData.m_dataColour.Red(), m_colourData.m_dataColour.Green(), (unsigned char)m_blueSlider->GetValue());
  PaintCustomColour(dc);
}

#endif // wxUSE_SLIDER

#endif // wxUSE_COLOURDLG
