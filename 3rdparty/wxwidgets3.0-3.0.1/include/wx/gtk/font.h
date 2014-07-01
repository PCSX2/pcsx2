/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/font.h
// Purpose:
// Author:      Robert Roebling
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_FONT_H_
#define _WX_GTK_FONT_H_

// ----------------------------------------------------------------------------
// wxFont
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFont : public wxFontBase
{
public:
    wxFont() { }

    wxFont(const wxFontInfo& info);

    wxFont(const wxString& nativeFontInfoString)
    {
        Create(nativeFontInfoString);
    }

    wxFont(const wxNativeFontInfo& info);

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

    // wxGTK-specific
    bool Create(const wxString& fontname);

    virtual ~wxFont();

    // implement base class pure virtuals
    virtual int GetPointSize() const;
    virtual wxFontStyle GetStyle() const;
    virtual wxFontWeight GetWeight() const;
    virtual wxString GetFaceName() const;
    virtual bool GetUnderlined() const;
    virtual bool GetStrikethrough() const;
    virtual wxFontEncoding GetEncoding() const;
    virtual const wxNativeFontInfo *GetNativeFontInfo() const;
    virtual bool IsFixedWidth() const;

    virtual void SetPointSize( int pointSize );
    virtual void SetFamily(wxFontFamily family);
    virtual void SetStyle(wxFontStyle style);
    virtual void SetWeight(wxFontWeight weight);
    virtual bool SetFaceName( const wxString& faceName );
    virtual void SetUnderlined( bool underlined );
    virtual void SetStrikethrough(bool strikethrough);
    virtual void SetEncoding(wxFontEncoding encoding);

    wxDECLARE_COMMON_FONT_METHODS();

    // Set Pango attributes in the specified layout. Currently only
    // underlined and strike-through attributes are handled by this function.
    //
    // If neither of them is specified, returns false, otherwise sets up the
    // attributes and returns true.
    bool GTKSetPangoAttrs(PangoLayout* layout) const;

    // implementation from now on
    void Unshare();

    // no data :-)

protected:
    virtual void DoSetNativeFontInfo( const wxNativeFontInfo& info );

    virtual wxGDIRefData* CreateGDIRefData() const;
    virtual wxGDIRefData* CloneGDIRefData(const wxGDIRefData* data) const;

    virtual wxFontFamily DoGetFamily() const;

private:
    void Init();

    DECLARE_DYNAMIC_CLASS(wxFont)
};

#endif // _WX_GTK_FONT_H_
