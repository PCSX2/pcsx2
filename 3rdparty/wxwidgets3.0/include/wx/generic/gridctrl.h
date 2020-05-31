///////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/gridctrl.h
// Purpose:     wxGrid controls
// Author:      Paul Gammans, Roger Gammans
// Modified by:
// Created:     11/04/2001
// Copyright:   (c) The Computer Surgery (paul@compsurg.co.uk)
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_GRIDCTRL_H_
#define _WX_GENERIC_GRIDCTRL_H_

#include "wx/grid.h"

#if wxUSE_GRID

#define wxGRID_VALUE_CHOICEINT    wxT("choiceint")
#define wxGRID_VALUE_DATETIME     wxT("datetime")


// the default renderer for the cells containing string data
class WXDLLIMPEXP_ADV wxGridCellStringRenderer : public wxGridCellRenderer
{
public:
    // draw the string
    virtual void Draw(wxGrid& grid,
                      wxGridCellAttr& attr,
                      wxDC& dc,
                      const wxRect& rect,
                      int row, int col,
                      bool isSelected);

    // return the string extent
    virtual wxSize GetBestSize(wxGrid& grid,
                               wxGridCellAttr& attr,
                               wxDC& dc,
                               int row, int col);

    virtual wxGridCellRenderer *Clone() const
        { return new wxGridCellStringRenderer; }

protected:
    // set the text colours before drawing
    void SetTextColoursAndFont(const wxGrid& grid,
                               const wxGridCellAttr& attr,
                               wxDC& dc,
                               bool isSelected);

    // calc the string extent for given string/font
    wxSize DoGetBestSize(const wxGridCellAttr& attr,
                         wxDC& dc,
                         const wxString& text);
};

// the default renderer for the cells containing numeric (long) data
class WXDLLIMPEXP_ADV wxGridCellNumberRenderer : public wxGridCellStringRenderer
{
public:
    // draw the string right aligned
    virtual void Draw(wxGrid& grid,
                      wxGridCellAttr& attr,
                      wxDC& dc,
                      const wxRect& rect,
                      int row, int col,
                      bool isSelected);

    virtual wxSize GetBestSize(wxGrid& grid,
                               wxGridCellAttr& attr,
                               wxDC& dc,
                               int row, int col);

    virtual wxGridCellRenderer *Clone() const
        { return new wxGridCellNumberRenderer; }

protected:
    wxString GetString(const wxGrid& grid, int row, int col);
};

class WXDLLIMPEXP_ADV wxGridCellFloatRenderer : public wxGridCellStringRenderer
{
public:
    wxGridCellFloatRenderer(int width = -1,
                            int precision = -1,
                            int format = wxGRID_FLOAT_FORMAT_DEFAULT);

    // get/change formatting parameters
    int GetWidth() const { return m_width; }
    void SetWidth(int width) { m_width = width; m_format.clear(); }
    int GetPrecision() const { return m_precision; }
    void SetPrecision(int precision) { m_precision = precision; m_format.clear(); }
    int GetFormat() const { return m_style; }
    void SetFormat(int format) { m_style = format; m_format.clear(); }

    // draw the string right aligned with given width/precision
    virtual void Draw(wxGrid& grid,
                      wxGridCellAttr& attr,
                      wxDC& dc,
                      const wxRect& rect,
                      int row, int col,
                      bool isSelected);

    virtual wxSize GetBestSize(wxGrid& grid,
                               wxGridCellAttr& attr,
                               wxDC& dc,
                               int row, int col);

    // parameters string format is "width[,precision[,format]]"
    // with format being one of f|e|g|E|F|G
    virtual void SetParameters(const wxString& params);

    virtual wxGridCellRenderer *Clone() const;

protected:
    wxString GetString(const wxGrid& grid, int row, int col);

private:
    // formatting parameters
    int m_width,
        m_precision;

    int m_style;
    wxString m_format;
};

// renderer for boolean fields
class WXDLLIMPEXP_ADV wxGridCellBoolRenderer : public wxGridCellRenderer
{
public:
    // draw a check mark or nothing
    virtual void Draw(wxGrid& grid,
                      wxGridCellAttr& attr,
                      wxDC& dc,
                      const wxRect& rect,
                      int row, int col,
                      bool isSelected);

    // return the checkmark size
    virtual wxSize GetBestSize(wxGrid& grid,
                               wxGridCellAttr& attr,
                               wxDC& dc,
                               int row, int col);

    virtual wxGridCellRenderer *Clone() const
        { return new wxGridCellBoolRenderer; }

private:
    static wxSize ms_sizeCheckMark;
};


#if wxUSE_DATETIME

#include "wx/datetime.h"

// the default renderer for the cells containing times and dates
class WXDLLIMPEXP_ADV wxGridCellDateTimeRenderer : public wxGridCellStringRenderer
{
public:
    wxGridCellDateTimeRenderer(const wxString& outformat = wxDefaultDateTimeFormat,
                               const wxString& informat = wxDefaultDateTimeFormat);

    // draw the string right aligned
    virtual void Draw(wxGrid& grid,
                      wxGridCellAttr& attr,
                      wxDC& dc,
                      const wxRect& rect,
                      int row, int col,
                      bool isSelected);

    virtual wxSize GetBestSize(wxGrid& grid,
                               wxGridCellAttr& attr,
                               wxDC& dc,
                               int row, int col);

    virtual wxGridCellRenderer *Clone() const;

    // output strptime()-like format string
    virtual void SetParameters(const wxString& params);

protected:
    wxString GetString(const wxGrid& grid, int row, int col);

    wxString m_iformat;
    wxString m_oformat;
    wxDateTime m_dateDef;
    wxDateTime::TimeZone m_tz;
};

#endif // wxUSE_DATETIME

// renders a number using the corresponding text string
class WXDLLIMPEXP_ADV wxGridCellEnumRenderer : public wxGridCellStringRenderer
{
public:
    wxGridCellEnumRenderer( const wxString& choices = wxEmptyString );

    // draw the string right aligned
    virtual void Draw(wxGrid& grid,
                      wxGridCellAttr& attr,
                      wxDC& dc,
                      const wxRect& rect,
                      int row, int col,
                      bool isSelected);

    virtual wxSize GetBestSize(wxGrid& grid,
                               wxGridCellAttr& attr,
                               wxDC& dc,
                               int row, int col);

    virtual wxGridCellRenderer *Clone() const;

    // parameters string format is "item1[,item2[...,itemN]]" where itemN will
    // be used if the cell value is N-1
    virtual void SetParameters(const wxString& params);

protected:
    wxString GetString(const wxGrid& grid, int row, int col);

    wxArrayString m_choices;
};


class WXDLLIMPEXP_ADV wxGridCellAutoWrapStringRenderer : public wxGridCellStringRenderer
{
public:
    wxGridCellAutoWrapStringRenderer() : wxGridCellStringRenderer() { }

    virtual void Draw(wxGrid& grid,
                      wxGridCellAttr& attr,
                      wxDC& dc,
                      const wxRect& rect,
                      int row, int col,
                      bool isSelected);

    virtual wxSize GetBestSize(wxGrid& grid,
                               wxGridCellAttr& attr,
                               wxDC& dc,
                               int row, int col);

    virtual wxGridCellRenderer *Clone() const
        { return new wxGridCellAutoWrapStringRenderer; }

private:
    wxArrayString GetTextLines( wxGrid& grid,
                                wxDC& dc,
                                const wxGridCellAttr& attr,
                                const wxRect& rect,
                                int row, int col);

    // Helper methods of GetTextLines()

    // Break a single logical line of text into several physical lines, all of
    // which are added to the lines array. The lines are broken at maxWidth and
    // the dc is used for measuring text extent only.
    void BreakLine(wxDC& dc,
                   const wxString& logicalLine,
                   wxCoord maxWidth,
                   wxArrayString& lines);

    // Break a word, which is supposed to be wider than maxWidth, into several
    // lines, which are added to lines array and the last, incomplete, of which
    // is returned in line output parameter.
    //
    // Returns the width of the last line.
    wxCoord BreakWord(wxDC& dc,
                      const wxString& word,
                      wxCoord maxWidth,
                      wxArrayString& lines,
                      wxString& line);


};

#endif  // wxUSE_GRID
#endif // _WX_GENERIC_GRIDCTRL_H_
