///////////////////////////////////////////////////////////////////////////////
// Name:        wx/univ/stdrend.h
// Purpose:     wxStdRenderer class declaration
// Author:      Vadim Zeitlin
// Created:     2006-09-18
// Copyright:   (c) 2006 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIV_STDREND_H_
#define _WX_UNIV_STDREND_H_

#include "wx/univ/renderer.h"
#include "wx/pen.h"

class WXDLLIMPEXP_FWD_CORE wxColourScheme;

// ----------------------------------------------------------------------------
// wxStdRenderer: implements as much of wxRenderer API as possible generically
// ----------------------------------------------------------------------------

class wxStdRenderer : public wxRenderer
{
public:
    // the renderer will use the given scheme, whose lifetime must be at least
    // as long as of this object itself, to choose the colours for drawing
    wxStdRenderer(const wxColourScheme *scheme);

    virtual void DrawBackground(wxDC& dc,
                                const wxColour& col,
                                const wxRect& rect,
                                int flags = 0,
                                wxWindow *window = NULL) wxOVERRIDE;
    virtual void DrawButtonSurface(wxDC& dc,
                                   const wxColour& col,
                                   const wxRect& rect,
                                   int flags) wxOVERRIDE;


    virtual void DrawFocusRect(wxWindow* win, wxDC& dc, const wxRect& rect, int flags = 0) wxOVERRIDE;

    virtual void DrawLabel(wxDC& dc,
                           const wxString& label,
                           const wxRect& rect,
                           int flags = 0,
                           int alignment = wxALIGN_LEFT | wxALIGN_TOP,
                           int indexAccel = -1,
                           wxRect *rectBounds = NULL) wxOVERRIDE;
    virtual void DrawButtonLabel(wxDC& dc,
                                 const wxString& label,
                                 const wxBitmap& image,
                                 const wxRect& rect,
                                 int flags = 0,
                                 int alignment = wxALIGN_LEFT | wxALIGN_TOP,
                                 int indexAccel = -1,
                                 wxRect *rectBounds = NULL) wxOVERRIDE;


    virtual void DrawBorder(wxDC& dc,
                            wxBorder border,
                            const wxRect& rect,
                            int flags = 0,
                            wxRect *rectIn = NULL) wxOVERRIDE;
    virtual void DrawTextBorder(wxDC& dc,
                                wxBorder border,
                                const wxRect& rect,
                                int flags = 0,
                                wxRect *rectIn = NULL) wxOVERRIDE;

    virtual void DrawHorizontalLine(wxDC& dc,
                                    wxCoord y, wxCoord x1, wxCoord x2) wxOVERRIDE;
    virtual void DrawVerticalLine(wxDC& dc,
                                  wxCoord x, wxCoord y1, wxCoord y2) wxOVERRIDE;
    virtual void DrawFrame(wxDC& dc,
                           const wxString& label,
                           const wxRect& rect,
                           int flags = 0,
                           int alignment = wxALIGN_LEFT,
                           int indexAccel = -1) wxOVERRIDE;


    virtual void DrawItem(wxDC& dc,
                          const wxString& label,
                          const wxRect& rect,
                          int flags = 0) wxOVERRIDE;
    virtual void DrawCheckItem(wxDC& dc,
                               const wxString& label,
                               const wxBitmap& bitmap,
                               const wxRect& rect,
                               int flags = 0) wxOVERRIDE;

    virtual void DrawCheckButton(wxDC& dc,
                                 const wxString& label,
                                 const wxBitmap& bitmap,
                                 const wxRect& rect,
                                 int flags = 0,
                                 wxAlignment align = wxALIGN_LEFT,
                                 int indexAccel = -1) wxOVERRIDE;
    virtual void DrawRadioButton(wxDC& dc,
                                 const wxString& label,
                                 const wxBitmap& bitmap,
                                 const wxRect& rect,
                                 int flags = 0,
                                 wxAlignment align = wxALIGN_LEFT,
                                 int indexAccel = -1) wxOVERRIDE;

    virtual void DrawScrollbarArrow(wxDC& dc,
                                    wxDirection dir,
                                    const wxRect& rect,
                                    int flags = 0) wxOVERRIDE;
    virtual void DrawScrollCorner(wxDC& dc,
                                  const wxRect& rect) wxOVERRIDE;

#if wxUSE_TEXTCTRL
    virtual void DrawTextLine(wxDC& dc,
                              const wxString& text,
                              const wxRect& rect,
                              int selStart = -1,
                              int selEnd = -1,
                              int flags = 0) wxOVERRIDE;

    virtual void DrawLineWrapMark(wxDC& dc, const wxRect& rect) wxOVERRIDE;

    virtual wxRect GetTextTotalArea(const wxTextCtrl *text,
                                    const wxRect& rect) const wxOVERRIDE;
    virtual wxRect GetTextClientArea(const wxTextCtrl *text,
                                     const wxRect& rect,
                                     wxCoord *extraSpaceBeyond) const wxOVERRIDE;
#endif // wxUSE_TEXTCTRL

    virtual wxRect GetBorderDimensions(wxBorder border) const wxOVERRIDE;

    virtual bool AreScrollbarsInsideBorder() const wxOVERRIDE;

    virtual void AdjustSize(wxSize *size, const wxWindow *window) wxOVERRIDE;

    virtual wxCoord GetListboxItemHeight(wxCoord fontHeight) wxOVERRIDE;

#if wxUSE_STATUSBAR
    virtual void DrawStatusField(wxDC& dc,
                                 const wxRect& rect,
                                 const wxString& label,
                                 int flags = 0, int style = 0) wxOVERRIDE;

    virtual wxSize GetStatusBarBorders() const wxOVERRIDE;

    virtual wxCoord GetStatusBarBorderBetweenFields() const wxOVERRIDE;

    virtual wxSize GetStatusBarFieldMargins() const wxOVERRIDE;
#endif // wxUSE_STATUSBAR

    virtual wxCoord GetCheckItemMargin() const wxOVERRIDE { return 0; }


    virtual void DrawFrameTitleBar(wxDC& dc,
                                   const wxRect& rect,
                                   const wxString& title,
                                   const wxIcon& icon,
                                   int flags,
                                   int specialButton = 0,
                                   int specialButtonFlag = 0) wxOVERRIDE;
    virtual void DrawFrameBorder(wxDC& dc,
                                 const wxRect& rect,
                                 int flags) wxOVERRIDE;
    virtual void DrawFrameBackground(wxDC& dc,
                                     const wxRect& rect,
                                     int flags) wxOVERRIDE;
    virtual void DrawFrameTitle(wxDC& dc,
                                const wxRect& rect,
                                const wxString& title,
                                int flags) wxOVERRIDE;
    virtual void DrawFrameIcon(wxDC& dc,
                               const wxRect& rect,
                               const wxIcon& icon,
                               int flags) wxOVERRIDE;
    virtual void DrawFrameButton(wxDC& dc,
                                 wxCoord x, wxCoord y,
                                 int button,
                                 int flags = 0) wxOVERRIDE;

    virtual wxRect GetFrameClientArea(const wxRect& rect, int flags) const wxOVERRIDE;

    virtual wxSize GetFrameTotalSize(const wxSize& clientSize, int flags) const wxOVERRIDE;

    virtual wxSize GetFrameMinSize(int flags) const wxOVERRIDE;

    virtual wxSize GetFrameIconSize() const wxOVERRIDE;

    virtual int HitTestFrame(const wxRect& rect,
                             const wxPoint& pt,
                             int flags = 0) const wxOVERRIDE;
protected:
    // various constants
    enum ArrowDirection
    {
        Arrow_Left,
        Arrow_Right,
        Arrow_Up,
        Arrow_Down,
        Arrow_Max
    };

    enum ArrowStyle
    {
        Arrow_Normal,
        Arrow_Disabled,
        Arrow_Pressed,
        Arrow_Inverted,
        Arrow_InvertedDisabled,
        Arrow_StateMax
    };

    enum FrameButtonType
    {
        FrameButton_Close,
        FrameButton_Minimize,
        FrameButton_Maximize,
        FrameButton_Restore,
        FrameButton_Help,
        FrameButton_Max
    };

    enum IndicatorType
    {
        IndicatorType_Check,
        IndicatorType_Radio,
        IndicatorType_MaxCtrl,
        IndicatorType_Menu = IndicatorType_MaxCtrl,
        IndicatorType_Max
    };

    enum IndicatorState
    {
        IndicatorState_Normal,
        IndicatorState_Pressed, // this one is for check/radioboxes
        IndicatorState_Disabled,
        IndicatorState_MaxCtrl,

        // the rest of the states are valid for menu items only
        IndicatorState_Selected = IndicatorState_Pressed,
        IndicatorState_SelectedDisabled = IndicatorState_MaxCtrl,
        IndicatorState_MaxMenu
    };

    enum IndicatorStatus
    {
        IndicatorStatus_Checked,
        IndicatorStatus_Unchecked,
        IndicatorStatus_Undetermined,
        IndicatorStatus_Max
    };

    // translate the appropriate bits in flags to the above enum elements
    static void GetIndicatorsFromFlags(int flags,
                                       IndicatorState& state,
                                       IndicatorStatus& status);

    // translate wxDirection to ArrowDirection
    static ArrowDirection GetArrowDirection(wxDirection dir);


    // fill the rectangle with a brush of given colour (must be valid)
    void DrawSolidRect(wxDC& dc, const wxColour& col, const wxRect& rect);


    // all the functions in this section adjust the rect parameter to
    // correspond to the interiour of the drawn area

        // draw complete rectangle
    void DrawRect(wxDC& dc, wxRect *rect, const wxPen& pen);

        // draw the rectange using the first pen for the left and top sides
        // and the second one for the bottom and right ones
    void DrawShadedRect(wxDC& dc, wxRect *rect,
                        const wxPen& pen1, const wxPen& pen2);

        // border drawing routines, may be overridden in the derived class
    virtual void DrawRaisedBorder(wxDC& dc, wxRect *rect);
    virtual void DrawSunkenBorder(wxDC& dc, wxRect *rect);
    virtual void DrawAntiSunkenBorder(wxDC& dc, wxRect *rect);
    virtual void DrawBoxBorder(wxDC& dc, wxRect *rect);
    virtual void DrawStaticBorder(wxDC& dc, wxRect *rect);
    virtual void DrawExtraBorder(wxDC& dc, wxRect *rect);


    // draw the frame with non-empty label inside the given rectText
    virtual void DrawFrameWithLabel(wxDC& dc,
                                    const wxString& label,
                                    const wxRect& rectFrame,
                                    const wxRect& rectText,
                                    int flags,
                                    int alignment,
                                    int indexAccel);

    // draw the (static box) frame without the part corresponding to rectLabel
    void DrawFrameWithoutLabel(wxDC& dc,
                               const wxRect& rectFrame,
                               const wxRect& rectLabel);


    // draw the bitmap for a check item (which is by default the same as check
    // box one but may be different)
    virtual void DrawCheckItemBitmap(wxDC& dc,
                                     const wxBitmap& bitmap,
                                     const wxRect& rect,
                                     int flags);

    // common routine for drawing check and radio buttons
    void DrawCheckOrRadioButton(wxDC& dc,
                                const wxString& label,
                                const wxBitmap& bitmap,
                                const wxRect& rect,
                                int flags,
                                wxAlignment align,
                                int indexAccel);

    // return the check/radio bitmap for the given flags
    virtual wxBitmap GetRadioBitmap(int flags) = 0;
    virtual wxBitmap GetCheckBitmap(int flags) = 0;

    // return the frame icon bitmap
    virtual wxBitmap GetFrameButtonBitmap(FrameButtonType type) = 0;

    // get the width of either normal or resizable frame border depending on
    // whether flags contains wxTOPLEVEL_RESIZEABLE bit
    //
    // notice that these methods only make sense with standard border drawing
    // code which uses the borders of the same width on all sides, this is why
    // they are only present here and not in wxRenderer itself
    virtual int GetFrameBorderWidth(int flags) const;

#if wxUSE_TEXTCTRL
    // return the width of the border around the text area in the text control
    virtual int GetTextBorderWidth(const wxTextCtrl *text) const;
#endif // wxUSE_TEXTCTRL

    // GDI objects we often use
    wxPen m_penBlack,
          m_penDarkGrey,
          m_penLightGrey,
          m_penHighlight;

    wxFont m_titlebarFont;

    // the colours we use, they never change currently so we don't have to ever
    // update m_penXXX objects above
    const wxColourScheme * const m_scheme;

    wxDECLARE_NO_COPY_CLASS(wxStdRenderer);
};

#endif // _WX_UNIV_STDREND_H_
