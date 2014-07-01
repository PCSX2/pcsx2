/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/font.h
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

// font styles
enum wxOSXSystemFont
{
    wxOSX_SYSTEM_FONT_NONE = 0,
    wxOSX_SYSTEM_FONT_NORMAL,
    wxOSX_SYSTEM_FONT_BOLD,
    wxOSX_SYSTEM_FONT_SMALL,
    wxOSX_SYSTEM_FONT_SMALL_BOLD,
    wxOSX_SYSTEM_FONT_MINI,
    wxOSX_SYSTEM_FONT_MINI_BOLD,
    wxOSX_SYSTEM_FONT_LABELS,
    wxOSX_SYSTEM_FONT_VIEWS
};


class WXDLLIMPEXP_CORE wxFont : public wxFontBase
{
public:
    // ctors and such
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

    wxFont( wxOSXSystemFont systemFont );

#if wxOSX_USE_COCOA
    wxFont(WX_NSFont nsfont);
#endif

#if FUTURE_WXWIN_COMPATIBILITY_3_0
    wxFont(int size,
           int family,
           int style,
           int weight,
           bool underlined = false,
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
           bool underlined = false,
           const wxString& face = wxEmptyString,
           wxFontEncoding encoding = wxFONTENCODING_DEFAULT)
    {
        Create(size, family, style, weight, underlined, face, encoding);
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

    bool Create(int size,
                wxFontFamily family,
                wxFontStyle style,
                wxFontWeight weight,
                bool underlined = false,
                const wxString& face = wxEmptyString,
                wxFontEncoding encoding = wxFONTENCODING_DEFAULT);

    wxFont(const wxNativeFontInfo& info)
    {
        (void)Create(info);
    }

    wxFont(const wxString& fontDesc);

    bool Create(const wxNativeFontInfo& info);

    virtual ~wxFont();

    // implement base class pure virtuals
    virtual int GetPointSize() const;
    virtual wxSize GetPixelSize() const;
    virtual wxFontStyle GetStyle() const;
    virtual wxFontWeight GetWeight() const;
    virtual bool GetUnderlined() const;
    virtual wxString GetFaceName() const;
    virtual wxFontEncoding GetEncoding() const;
    virtual const wxNativeFontInfo *GetNativeFontInfo() const;

    virtual bool IsFixedWidth() const;

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

    virtual bool RealizeResource();

    // Mac-specific, risks to change, don't use in portable code

#if wxOSX_USE_CARBON && wxOSX_USE_ATSU_TEXT
    wxUint16 MacGetThemeFontID() const ;

    // 'old' Quickdraw accessors
    short MacGetFontNum() const;
    wxByte  MacGetFontStyle() const;
#endif

#if wxOSX_USE_COCOA_OR_CARBON
    CGFontRef OSXGetCGFont() const;
#endif

    CTFontRef OSXGetCTFont() const;

#if wxOSX_USE_ATSU_TEXT
    // Returns an ATSUStyle not ATSUStyle*
    void* MacGetATSUStyle() const ;
    void* OSXGetATSUStyle() const { return MacGetATSUStyle() ; }

    wxDEPRECATED( wxUint32 MacGetATSUFontID() const );
    wxDEPRECATED( wxUint32 MacGetATSUAdditionalQDStyles() const );
#endif

#if wxOSX_USE_COCOA
    WX_NSFont OSXGetNSFont() const;
    static WX_NSFont OSXCreateNSFont(wxOSXSystemFont font, wxNativeFontInfo* info);
    static WX_NSFont OSXCreateNSFont(const wxNativeFontInfo* info);
    static void SetNativeInfoFromNSFont(WX_NSFont nsfont, wxNativeFontInfo* info);
#endif

#if wxOSX_USE_IPHONE
    WX_UIFont OSXGetUIFont() const;
    static WX_UIFont OSXCreateUIFont(wxOSXSystemFont font, wxNativeFontInfo* info);
    static WX_UIFont OSXCreateUIFont(const wxNativeFontInfo* info);
#endif

protected:
    virtual void DoSetNativeFontInfo(const wxNativeFontInfo& info);
    virtual wxFontFamily DoGetFamily() const;

    virtual wxGDIRefData *CreateGDIRefData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

private:

    DECLARE_DYNAMIC_CLASS(wxFont)
};

#endif // _WX_FONT_H_
