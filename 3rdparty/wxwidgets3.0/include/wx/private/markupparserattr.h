///////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/markupparserattr.h
// Purpose:     Classes mapping markup attributes to wxFont/wxColour.
// Author:      Vadim Zeitlin
// Created:     2011-02-18
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_MARKUPPARSERATTR_H_
#define _WX_PRIVATE_MARKUPPARSERATTR_H_

#include "wx/private/markupparser.h"

#include "wx/stack.h"

#include "wx/colour.h"
#include "wx/font.h"

// ----------------------------------------------------------------------------
// wxMarkupParserAttrOutput: simplified wxFont-using version of the above.
// ----------------------------------------------------------------------------

// This class assumes that wxFont and wxColour are used to perform all the
// markup tags and implements the base class virtual functions in terms of
// OnAttr{Start,End}() only.
//
// Notice that you still must implement OnText() inherited from the base class
// when deriving from this one.
class wxMarkupParserAttrOutput : public wxMarkupParserOutput
{
public:
    // A simple container of font and colours.
    struct Attr
    {
        Attr(const wxFont& font_,
             const wxColour& foreground_ = wxColour(),
             const wxColour& background_ = wxColour())
            : font(font_), foreground(foreground_), background(background_)
        {
        }

        wxFont font;
        wxColour foreground,
                 background;
    };


    // This object must be initialized with the font and colours to use
    // initially, i.e. the ones used before any tags in the string.
    wxMarkupParserAttrOutput(const wxFont& font,
                             const wxColour& foreground,
                             const wxColour& background)
    {
        m_attrs.push(Attr(font, foreground, background));
    }

    // Indicates the change of the font and/or colours used. Any of the
    // fields of the argument may be invalid indicating that the corresponding
    // attribute didn't actually change.
    virtual void OnAttrStart(const Attr& attr) = 0;

    // Indicates the end of the region affected by the given attributes
    // (the same ones that were passed to the matching OnAttrStart(), use
    // GetAttr() to get the ones that will be used from now on).
    virtual void OnAttrEnd(const Attr& attr) = 0;


    // Implement all pure virtual methods inherited from the base class in
    // terms of our own ones.
    virtual void OnBoldStart() { DoChangeFont(&wxFont::Bold); }
    virtual void OnBoldEnd() { DoEndAttr(); }

    virtual void OnItalicStart() { DoChangeFont(&wxFont::Italic); }
    virtual void OnItalicEnd() { DoEndAttr(); }

    virtual void OnUnderlinedStart() { DoChangeFont(&wxFont::Underlined); }
    virtual void OnUnderlinedEnd() { DoEndAttr(); }

    virtual void OnStrikethroughStart() { DoChangeFont(&wxFont::Strikethrough); }
    virtual void OnStrikethroughEnd() { DoEndAttr(); }

    virtual void OnBigStart() { DoChangeFont(&wxFont::Larger); }
    virtual void OnBigEnd() { DoEndAttr(); }

    virtual void OnSmallStart() { DoChangeFont(&wxFont::Smaller); }
    virtual void OnSmallEnd() { DoEndAttr(); }

    virtual void OnTeletypeStart()
    {
        wxFont font(GetFont());
        font.SetFamily(wxFONTFAMILY_TELETYPE);
        DoSetFont(font);
    }
    virtual void OnTeletypeEnd() { DoEndAttr(); }

    virtual void OnSpanStart(const wxMarkupSpanAttributes& spanAttr)
    {
        wxFont font(GetFont());
        if ( !spanAttr.m_fontFace.empty() )
            font.SetFaceName(spanAttr.m_fontFace);

        FontModifier<wxFontWeight>()(spanAttr.m_isBold,
                                     font, &wxFont::SetWeight,
                                     wxFONTWEIGHT_NORMAL, wxFONTWEIGHT_BOLD);

        FontModifier<wxFontStyle>()(spanAttr.m_isItalic,
                                    font, &wxFont::SetStyle,
                                    wxFONTSTYLE_NORMAL, wxFONTSTYLE_ITALIC);

        FontModifier<bool>()(spanAttr.m_isUnderlined,
                             font, &wxFont::SetUnderlined,
                             false, true);

        // TODO: No support for strike-through yet.

        switch ( spanAttr.m_sizeKind )
        {
            case wxMarkupSpanAttributes::Size_Unspecified:
                break;

            case wxMarkupSpanAttributes::Size_Relative:
                if ( spanAttr.m_fontSize > 0 )
                    font.MakeLarger();
                else
                    font.MakeSmaller();
                break;

            case wxMarkupSpanAttributes::Size_Symbolic:
                // The values of font size intentionally coincide with the
                // values of wxFontSymbolicSize enum elements so simply cast
                // one to the other.
                font.SetSymbolicSize(
                    static_cast<wxFontSymbolicSize>(spanAttr.m_fontSize)
                );
                break;

            case wxMarkupSpanAttributes::Size_PointParts:
                font.SetPointSize((spanAttr.m_fontSize + 1023)/1024);
                break;
        }


        const Attr attr(font, spanAttr.m_fgCol, spanAttr.m_bgCol);
        OnAttrStart(attr);

        m_attrs.push(attr);
    }

    virtual void OnSpanEnd(const wxMarkupSpanAttributes& WXUNUSED(spanAttr))
    {
        DoEndAttr();
    }

protected:
    // Get the current attributes, i.e. the ones that should be used for
    // rendering (or measuring or whatever) the text at the current position in
    // the string.
    //
    // It may be called from OnAttrStart() to get the old attributes used
    // before and from OnAttrEnd() to get the new attributes that will be used
    // from now on but is mostly meant to be used from overridden OnText()
    // implementations.
    const Attr& GetAttr() const { return m_attrs.top(); }

    // A shortcut for accessing the font of the current attribute.
    const wxFont& GetFont() const { return GetAttr().font; }

private:
    // Change only the font to the given one. Call OnAttrStart() to notify
    // about the change and update the attributes stack.
    void DoSetFont(const wxFont& font)
    {
        const Attr attr(font);

        OnAttrStart(attr);

        m_attrs.push(attr);
    }

    // Apply the given function to the font currently on top of the font stack,
    // push the new font on the stack and call OnAttrStart() with it.
    void DoChangeFont(wxFont (wxFont::*func)() const)
    {
        DoSetFont((GetFont().*func)());
    }

    void DoEndAttr()
    {
        const Attr attr(m_attrs.top());
        m_attrs.pop();

        OnAttrEnd(attr);
    }

    // A helper class used to apply the given function to a wxFont object
    // depending on the value of an OptionalBool.
    template <typename T>
    struct FontModifier
    {
        FontModifier() { }

        void operator()(wxMarkupSpanAttributes::OptionalBool isIt,
                        wxFont& font,
                        void (wxFont::*func)(T),
                        T noValue,
                        T yesValue)
        {
            switch ( isIt )
            {
                case wxMarkupSpanAttributes::Unspecified:
                    break;

                case wxMarkupSpanAttributes::No:
                    (font.*func)(noValue);
                    break;

                case wxMarkupSpanAttributes::Yes:
                    (font.*func)(yesValue);
                    break;
            }
        }
    };


    wxStack<Attr> m_attrs;

    wxDECLARE_NO_COPY_CLASS(wxMarkupParserAttrOutput);
};

#endif // _WX_PRIVATE_MARKUPPARSERATTR_H_
