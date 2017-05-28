///////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/private/textmeasure.h
// Purpose:     Generic wxTextMeasure declaration.
// Author:      Vadim Zeitlin
// Created:     2012-10-17
// Copyright:   (c) 1997-2012 wxWidgets team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_PRIVATE_TEXTMEASURE_H_
#define _WX_GENERIC_PRIVATE_TEXTMEASURE_H_

// ----------------------------------------------------------------------------
// wxTextMeasure for the platforms without native support.
// ----------------------------------------------------------------------------

class wxTextMeasure : public wxTextMeasureBase
{
public:
    wxEXPLICIT wxTextMeasure(const wxDC *dc, const wxFont *font = NULL)
        : wxTextMeasureBase(dc, font) {}
    wxEXPLICIT wxTextMeasure(const wxWindow *win, const wxFont *font = NULL)
        : wxTextMeasureBase(win, font) {}

protected:
    virtual void DoGetTextExtent(const wxString& string,
                               wxCoord *width,
                               wxCoord *height,
                               wxCoord *descent = NULL,
                               wxCoord *externalLeading = NULL);

    virtual bool DoGetPartialTextExtents(const wxString& text,
                                         wxArrayInt& widths,
                                         double scaleX);

    wxDECLARE_NO_COPY_CLASS(wxTextMeasure);
};

#endif // _WX_GENERIC_PRIVATE_TEXTMEASURE_H_
