///////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/markupparser.h
// Purpose:     Classes for parsing simple markup.
// Author:      Vadim Zeitlin
// Created:     2011-02-16
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_MARKUPPARSER_H_
#define _WX_PRIVATE_MARKUPPARSER_H_

#include "wx/string.h"

// ----------------------------------------------------------------------------
// wxMarkupSpanAttributes: information about attributes for a markup span.
// ----------------------------------------------------------------------------

struct wxMarkupSpanAttributes
{
    enum OptionalBool
    {
        Unspecified = -1,
        No,
        Yes
    };

    wxMarkupSpanAttributes()
    {
        m_sizeKind = Size_Unspecified;

        m_isBold =
        m_isItalic =
        m_isUnderlined =
        m_isStrikethrough = Unspecified;
    }

    // If a string is empty, it means that the corresponding attribute is not
    // set.
    wxString m_fgCol,
             m_bgCol,
             m_fontFace;

    // There are many ways of specifying the size. First of all, the size may
    // be relative in which case m_fontSize is either -1 or +1 meaning that
    // it's one step smaller or larger than the current font. Second, it may be
    // absolute in which case m_fontSize contains either the size in 1024th of
    // a point (Pango convention) or its values are in [-3, 3] interval and map
    // to [xx-small, xx-large] CSS-like font size specification. And finally it
    // may be not specified at all, of course, in which case the value of
    // m_fontSize doesn't matter and it shouldn't be used.
    enum
    {
        Size_Unspecified,
        Size_Relative,
        Size_Symbolic,
        Size_PointParts
    } m_sizeKind;
    int m_fontSize;

    // If the value is Unspecified, the attribute wasn't given.
    OptionalBool m_isBold,
                 m_isItalic,
                 m_isUnderlined,
                 m_isStrikethrough;
};

// ----------------------------------------------------------------------------
// wxMarkupParserOutput: gathers the results of parsing markup.
// ----------------------------------------------------------------------------

// A class deriving directly from this one needs to implement all the pure
// virtual functions below but as the handling of all simple tags (bold, italic
// &c) is often very similar, it is usually more convenient to inherit from
// wxMarkupParserFontOutput defined in wx/private/markupparserfont.h instead.
class wxMarkupParserOutput
{
public:
    wxMarkupParserOutput() { }
    virtual ~wxMarkupParserOutput() { }

    // Virtual functions called by wxMarkupParser while parsing the markup.

    // Called for a run of normal text.
    virtual void OnText(const wxString& text) = 0;

    // These functions correspond to the simple tags without parameters.
    virtual void OnBoldStart() = 0;
    virtual void OnBoldEnd() = 0;

    virtual void OnItalicStart() = 0;
    virtual void OnItalicEnd() = 0;

    virtual void OnUnderlinedStart() = 0;
    virtual void OnUnderlinedEnd() = 0;

    virtual void OnStrikethroughStart() = 0;
    virtual void OnStrikethroughEnd() = 0;

    virtual void OnBigStart() = 0;
    virtual void OnBigEnd() = 0;

    virtual void OnSmallStart() = 0;
    virtual void OnSmallEnd() = 0;

    virtual void OnTeletypeStart() = 0;
    virtual void OnTeletypeEnd() = 0;

    // The generic span start and end functions.
    virtual void OnSpanStart(const wxMarkupSpanAttributes& attrs) = 0;
    virtual void OnSpanEnd(const wxMarkupSpanAttributes& attrs) = 0;

private:
    wxDECLARE_NO_COPY_CLASS(wxMarkupParserOutput);
};

// ----------------------------------------------------------------------------
// wxMarkupParser: parses the given markup text into wxMarkupParserOutput.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxMarkupParser
{
public:
    // Initialize the parser with the object that will receive parsing results.
    // This object lifetime must be greater than ours.
    explicit wxMarkupParser(wxMarkupParserOutput& output)
        : m_output(output)
    {
    }

    // Parse the entire string and call wxMarkupParserOutput methods.
    //
    // Return true if the string was successfully parsed or false if it failed
    // (presumably because of syntax errors in the markup).
    bool Parse(const wxString& text);

    // Quote a normal string, not meant to be interpreted as markup, so that it
    // produces the same string when parsed as markup. This means, for example,
    // replacing '<' in the input string with "&lt;" to prevent them from being
    // interpreted as tag opening characters.
    static wxString Quote(const wxString& text);

    // Strip markup from a string, i.e. simply remove all tags and replace
    // XML entities with their values (or with "&&" in case of "&amp;" to
    // prevent it from being interpreted as mnemonic marker).
    static wxString Strip(const wxString& text);

private:
    // Simple struct combining the name of a tag and its attributes.
    struct TagAndAttrs
    {
        TagAndAttrs(const wxString& name_) : name(name_) { }

        wxString name;
        wxMarkupSpanAttributes attrs;
    };

    // Call the wxMarkupParserOutput method corresponding to the given tag.
    //
    // Return false if the tag doesn't match any of the known ones.
    bool OutputTag(const TagAndAttrs& tagAndAttrs, bool start);

    // Parse the attributes and fill the provided TagAndAttrs object with the
    // information about them. Does nothing if attrs string is empty.
    //
    // Returns empty string on success of a [fragment of an] error message if
    // we failed to parse the attributes.
    wxString ParseAttrs(wxString attrs, TagAndAttrs& tagAndAttrs);


    wxMarkupParserOutput& m_output;

    wxDECLARE_NO_COPY_CLASS(wxMarkupParser);
};

#endif // _WX_PRIVATE_MARKUPPARSER_H_
