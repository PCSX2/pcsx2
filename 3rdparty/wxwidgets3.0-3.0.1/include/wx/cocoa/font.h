/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/font.h
// Purpose:     wxFont class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FONT_H_
#define _WX_FONT_H_

// ----------------------------------------------------------------------------
// wxFont
// ----------------------------------------------------------------------------

DECLARE_WXCOCOA_OBJC_CLASS(NSFont);

// Internal class that bridges us with code like wxSystemSettings
class wxCocoaFontFactory;
// We have c-tors/methods taking pointers of these
class wxFontRefData;

/*! @discussion
    wxCocoa's implementation of wxFont is very incomplete.  In particular,
    a lot of work needs to be done on wxNativeFontInfo which is currently
    using the totally generic implementation.

    See the documentation in src/cocoa/font.mm for more implementatoin details.
 */
class WXDLLIMPEXP_CORE wxFont : public wxFontBase
{
    friend class wxCocoaFontFactory;
public:
    /*! @abstract   Default construction of invalid font for 2-step construct then Create process.
     */
    wxFont() { }

    wxFont(const wxFontInfo& info)
    {
        Create(info.GetPointSize(),
               info.GetFamily(),
               info.GetStyle(),
               info.GetWeight(),
               info.IsUnderlined(),
               info.GetFaceName(),
               info.GetEncoding());

        if ( info.IsUsingSizeInPixels() )
            SetPixelSize(info.GetPixelSize());
    }

    /*! @abstract   Platform-independent construction with individual properties
     */
#if FUTURE_WXWIN_COMPATIBILITY_3_0
    wxFont(int size,
           int family,
           int style,
           int weight,
           bool underlined = FALSE,
           const wxString& face = wxEmptyString,
           wxFontEncoding encoding = wxFONTENCODING_DEFAULT)
    {
        (void)Create(size, (wxFontFamily)family, (wxFontStyle)style, (wxFontWeight)weight, underlined, face, encoding);
    }
#endif
    wxFont(int size,
           wxFontFamily family,
           wxFontStyle style,
           wxFontWeight weight,
           bool underlined = FALSE,
           const wxString& face = wxEmptyString,
           wxFontEncoding encoding = wxFONTENCODING_DEFAULT)
    {
        (void)Create(size, family, style, weight, underlined, face, encoding);
    }

    wxFont(const wxSize& pixelSize,
           wxFontFamily family,
           wxFontStyle style,
           wxFontWeight weight,
           bool underlined = false,
           const wxString& face = wxEmptyString,
           wxFontEncoding encoding = wxFONTENCODING_DEFAULT)
    {
        Create(10, family, style, weight, underlined, face, encoding);
        SetPixelSize(pixelSize);
    }

    /*! @abstract   Construction with opaque wxNativeFontInfo
     */
    wxFont(const wxNativeFontInfo& info)
    {
        (void)Create(info);
    }

    /*! @abstract   Construction with platform-dependent font descriptor string.
        @param  fontDesc        Usually the result of wxNativeFontInfo::ToUserString()
     */
    wxFont(const wxString& fontDesc);

    // NOTE: Copy c-tor and assignment from wxObject is fine

    bool Create(int size,
                wxFontFamily family,
                wxFontStyle style,
                wxFontWeight weight,
                bool underlined = FALSE,
                const wxString& face = wxEmptyString,
                wxFontEncoding encoding = wxFONTENCODING_DEFAULT);

    bool Create(const wxNativeFontInfo& info);

    virtual ~wxFont();

    // implement base class pure virtuals
    virtual int GetPointSize() const;
    virtual wxFontStyle GetStyle() const;
    virtual wxFontWeight GetWeight() const;
    virtual bool GetUnderlined() const;
    virtual wxString GetFaceName() const;
    virtual wxFontEncoding GetEncoding() const;
    virtual const wxNativeFontInfo *GetNativeFontInfo() const;

    virtual void SetPointSize(int pointSize);
    virtual void SetFamily(wxFontFamily family);
    virtual void SetStyle(wxFontStyle style);
    virtual void SetWeight(wxFontWeight weight);
    virtual bool SetFaceName(const wxString& faceName);
    virtual void SetUnderlined(bool underlined);
    virtual void SetEncoding(wxFontEncoding encoding);

    wxDECLARE_COMMON_FONT_METHODS();

    // implementation only from now on
    // -------------------------------

    /*! @abstract   Defined on some ports (not including this one) in wxGDIObject
        @discussion
        The intention here I suppose is to allow one to create a wxFont without yet
        creating the underlying native object.  There's no point not to create the
        NSFont immediately in wxCocoa so this is useless.
        This method came from the stub code copied in the early days of wxCocoa.
        FIXME(1): Remove this in trunk.  FIXME(2): Is it really a good idea for this to
        be part of the public API for wxGDIObject?
     */
    virtual bool RealizeResource();

protected:
    /*! @abstract   Internal constructor with ref data
        @discussion
        Takes ownership of @a refData.  That is, it is assumed that refData has either just been
        created using new (which initializes its m_refCount to 1) or if you are sharing a ref that
        you have called IncRef on it before passing it to this method.
     */
    explicit wxFont(wxFontRefData *refData)
    {   Create(refData); }
    bool Create(wxFontRefData *refData);

    virtual wxGDIRefData *CreateGDIRefData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

    virtual wxFontFamily DoGetFamily() const;

private:
    DECLARE_DYNAMIC_CLASS(wxFont)
};

#endif
    // _WX_FONT_H_
