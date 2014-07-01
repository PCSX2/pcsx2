///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/private/textmeasure.h
// Purpose:     wxGTK-specific declaration of wxTextMeasure class
// Author:      Manuel Martin
// Created:     2012-10-05
// Copyright:   (c) 1997-2012 wxWidgets team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_PRIVATE_TEXTMEASURE_H_
#define _WX_GTK_PRIVATE_TEXTMEASURE_H_

// ----------------------------------------------------------------------------
// wxTextMeasure
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxWindowDCImpl;

class wxTextMeasure : public wxTextMeasureBase
{
public:
    wxEXPLICIT wxTextMeasure(const wxDC *dc, const wxFont *font = NULL)
        : wxTextMeasureBase(dc, font)
    {
        Init();
    }

    wxEXPLICIT wxTextMeasure(const wxWindow *win, const wxFont *font = NULL)
        : wxTextMeasureBase(win, font)
    {
        Init();
    }

protected:
    // Common part of both ctors.
    void Init();

    virtual void BeginMeasuring();
    virtual void EndMeasuring();

    virtual void DoGetTextExtent(const wxString& string,
                                 wxCoord *width,
                                 wxCoord *height,
                                 wxCoord *descent = NULL,
                                 wxCoord *externalLeading = NULL);

    virtual bool DoGetPartialTextExtents(const wxString& text,
                                         wxArrayInt& widths,
                                         double scaleX);

    // This class is only used for DC text measuring with GTK+ 2 as GTK+ 3 uses
    // Cairo and not Pango for this. However it's still used even with GTK+ 3
    // for window text measuring, so the context and the layout are still
    // needed.
#ifndef __WXGTK3__
    wxWindowDCImpl *m_wdc;
#endif // GTK+ < 3
    PangoContext *m_context;
    PangoLayout *m_layout;

    wxDECLARE_NO_COPY_CLASS(wxTextMeasure);
};

#endif // _WX_GTK_PRIVATE_TEXTMEASURE_H_
