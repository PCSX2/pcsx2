/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/font.h
// Purpose:     wxFont class
// Author:      David Webster
// Modified by:
// Created:     10/06/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FONT_H_
#define _WX_FONT_H_

#include "wx/gdiobj.h"
#include "wx/os2/private.h"

WXDLLIMPEXP_DATA_CORE(extern const wxChar*) wxEmptyString;

// ----------------------------------------------------------------------------
// wxFont
// ----------------------------------------------------------------------------

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

    wxFont( const wxNativeFontInfo& rInfo
           ,WXHFONT                 hFont = 0
          )

    {
        (void)Create( rInfo
                     ,hFont
                    );
    }

    wxFont(const wxString& rsFontDesc);

    bool Create( const wxNativeFontInfo& rInfo
                ,WXHFONT                 hFont = 0
               );

    virtual ~wxFont();

    //
    // Implement base class pure virtuals
    //
    virtual int               GetPointSize(void) const;
    virtual wxFontStyle GetStyle() const;
    virtual wxFontWeight GetWeight() const;
    virtual bool              GetUnderlined(void) const;
    virtual wxString          GetFaceName(void) const;
    virtual wxFontEncoding    GetEncoding(void) const;
    virtual const wxNativeFontInfo* GetNativeFontInfo() const;

    virtual void SetPointSize(int nPointSize);
    virtual void SetFamily(wxFontFamily family);
    virtual void SetStyle(wxFontStyle style);
    virtual void SetWeight(wxFontWeight weight);
    virtual bool SetFaceName(const wxString& rsFaceName);
    virtual void SetUnderlined(bool bUnderlined);
    virtual void SetEncoding(wxFontEncoding vEncoding);

    wxDECLARE_COMMON_FONT_METHODS();

    //
    // For internal use only!
    //
    void SetPS(HPS hPS);
    void SetFM( PFONTMETRICS pFM
               ,int          nNumFonts
              );
    //
    // Implementation only from now on
    // -------------------------------
    //
    virtual bool     IsFree(void) const;
    virtual bool     RealizeResource(void);
    virtual WXHANDLE GetResourceHandle(void) const;
    virtual bool     FreeResource(bool bForce = false);

    WXHFONT GetHFONT(void) const;

protected:
    virtual void DoSetNativeFontInfo(const wxNativeFontInfo& rInfo);
    virtual wxFontFamily DoGetFamily() const;

    // implement wxObject virtuals which are used by AllocExclusive()
    virtual wxGDIRefData *CreateGDIRefData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

private:
    DECLARE_DYNAMIC_CLASS(wxFont)
}; // end of wxFont

#endif // _WX_FONT_H_
