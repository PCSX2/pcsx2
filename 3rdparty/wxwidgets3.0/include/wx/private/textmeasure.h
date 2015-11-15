///////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/textmeasure.h
// Purpose:     declaration of wxTextMeasure class
// Author:      Manuel Martin
// Created:     2012-10-05
// Copyright:   (c) 1997-2012 wxWidgets team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_TEXTMEASURE_H_
#define _WX_PRIVATE_TEXTMEASURE_H_

class WXDLLIMPEXP_FWD_CORE wxDC;
class WXDLLIMPEXP_FWD_CORE wxFont;
class WXDLLIMPEXP_FWD_CORE wxWindow;

// ----------------------------------------------------------------------------
// wxTextMeasure: class used to measure text extent.
// ----------------------------------------------------------------------------

class wxTextMeasureBase
{
public:
    // The first ctor argument must be non-NULL, i.e. each object of this class
    // is associated with either a valid wxDC or a valid wxWindow. The font can
    // be NULL to use the current DC/window font or can be specified explicitly.
    wxTextMeasureBase(const wxDC *dc, const wxFont *theFont);
    wxTextMeasureBase(const wxWindow *win, const wxFont *theFont);

    // Even though this class is not supposed to be used polymorphically, give
    // it a virtual dtor to avoid compiler warnings.
    virtual ~wxTextMeasureBase() { }


    // Return the extent of a single line string.
    void GetTextExtent(const wxString& string,
                       wxCoord *width,
                       wxCoord *height,
                       wxCoord *descent = NULL,
                       wxCoord *externalLeading = NULL);

    // The same for a multiline (with '\n') string.
    void GetMultiLineTextExtent(const wxString& text,
                                wxCoord *width,
                                wxCoord *height,
                                wxCoord *heightOneLine = NULL);

    // Find the dimensions of the largest string.
    wxSize GetLargestStringExtent(size_t n, const wxString* strings);
    wxSize GetLargestStringExtent(const wxArrayString& strings)
    {
        return GetLargestStringExtent(strings.size(), &strings[0]);
    }

    // Fill the array with the widths for each "0..N" substrings for N from 1
    // to text.length().
    //
    // The scaleX argument is the horizontal scale used by wxDC and is only
    // used in the generic implementation.
    bool GetPartialTextExtents(const wxString& text,
                               wxArrayInt& widths,
                               double scaleX);


    // These functions are called by our public methods before and after each
    // call to DoGetTextExtent(). Derived classes may override them to prepare
    // for -- possibly several -- subsequent calls to DoGetTextExtent().
    //
    // As these calls must be always paired, they're never called directly but
    // only by our friend MeasuringGuard class.
    //
    // NB: They're public only to allow VC6 to compile this code, there doesn't
    //     seem to be any way to give MeasuringGuard access to them (FIXME-VC6)
    virtual void BeginMeasuring() { }
    virtual void EndMeasuring() { }

    // This is another method which is only used by MeasuringGuard.
    bool IsUsingDCImpl() const { return m_useDCImpl; }

protected:
    // RAII wrapper for the two methods above.
    class MeasuringGuard
    {
    public:
        MeasuringGuard(wxTextMeasureBase& tm) : m_tm(tm)
        {
            // BeginMeasuring() should only be called if we have a native DC,
            // so don't call it if we delegate to a DC of unknown type.
            if ( !m_tm.IsUsingDCImpl() )
                m_tm.BeginMeasuring();
        }

        ~MeasuringGuard()
        {
            if ( !m_tm.IsUsingDCImpl() )
                m_tm.EndMeasuring();
        }

    private:
        wxTextMeasureBase& m_tm;
    };


    // The main function of this class, to be implemented in platform-specific
    // way used by all our public methods.
    //
    // The width and height pointers here are never NULL and the input string
    // is not empty.
    virtual void DoGetTextExtent(const wxString& string,
                                 wxCoord *width,
                                 wxCoord *height,
                                 wxCoord *descent = NULL,
                                 wxCoord *externalLeading = NULL) = 0;

    // The real implementation of GetPartialTextExtents().
    //
    // On input, widths array contains text.length() zero elements and the text
    // is guaranteed to be non-empty.
    virtual bool DoGetPartialTextExtents(const wxString& text,
                                         wxArrayInt& widths,
                                         double scaleX) = 0;

    // Call either DoGetTextExtent() or wxDC::GetTextExtent() depending on the
    // value of m_useDCImpl.
    //
    // This must be always used instead of calling DoGetTextExtent() directly!
    void CallGetTextExtent(const wxString& string,
                           wxCoord *width,
                           wxCoord *height,
                           wxCoord *descent = NULL,
                           wxCoord *externalLeading = NULL);

    // Return a valid font: if one was given to us in the ctor, use this one,
    // otherwise use the current font of the associated wxDC or wxWindow.
    wxFont GetFont() const;


    // Exactly one of m_dc and m_win is non-NULL for any given object of this
    // class.
    const wxDC* const m_dc;
    const wxWindow* const m_win;

    // If this is true, simply forward to wxDC::GetTextExtent() from our
    // CallGetTextExtent() instead of calling our own DoGetTextExtent().
    //
    // We need this because our DoGetTextExtent() typically only works with
    // native DCs, i.e. those having an HDC under Windows or using Pango under
    // GTK+. However wxTextMeasure object can be constructed for any wxDC, not
    // necessarily a native one and in this case we must call back into the DC
    // implementation of text measuring itself.
    bool m_useDCImpl;

    // This one can be NULL or not.
    const wxFont* const m_font;

    wxDECLARE_NO_COPY_CLASS(wxTextMeasureBase);
};

// Include the platform dependent class declaration, if any.
#if defined(__WXGTK20__)
    #include "wx/gtk/private/textmeasure.h"
#elif defined(__WXMSW__)
    #include "wx/msw/private/textmeasure.h"
#else // no platform-specific implementation of wxTextMeasure yet
    #include "wx/generic/private/textmeasure.h"

    #define wxUSE_GENERIC_TEXTMEASURE 1
#endif

#ifndef wxUSE_GENERIC_TEXTMEASURE
    #define wxUSE_GENERIC_TEXTMEASURE 0
#endif

#endif // _WX_PRIVATE_TEXTMEASURE_H_
