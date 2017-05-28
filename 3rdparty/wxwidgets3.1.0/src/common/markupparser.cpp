///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/markupparser.cpp
// Purpose:     Implementation of wxMarkupParser.
// Author:      Vadim Zeitlin
// Created:     2011-02-16
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_MARKUP

#ifndef WX_PRECOMP
    #include "wx/log.h"
#endif

#include "wx/private/markupparser.h"

#include "wx/stack.h"

namespace
{

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// Array containing the predefined XML 1.0 entities.
const struct XMLEntity
{
    const char *name;
    int len;            // == strlen(name)
    char value;
} xmlEntities[] =
{
    { "lt",     2,  '<' },
    { "gt",     2,  '>' },
    { "amp",    3,  '&' },
    { "apos",   4,  '\''},
    { "quot",   4,  '"' },
};

// ----------------------------------------------------------------------------
// helper functions
// ----------------------------------------------------------------------------

wxString
ExtractUntil(char ch, wxString::const_iterator& it, wxString::const_iterator end)
{
    wxString str;
    for ( ; it != end; ++it )
    {
        if ( *it == ch )
            return str;

        str += *it;
    }

    // Return empty string to indicate that we didn't find ch at all.
    return wxString();
}

} // anonymous namespace

// ============================================================================
// wxMarkupParser implementation
// ============================================================================

wxString
wxMarkupParser::ParseAttrs(wxString attrs, TagAndAttrs& tagAndAttrs)
{
    if ( tagAndAttrs.name.CmpNoCase("span") != 0 && !attrs.empty() )
    {
        return wxString::Format("tag \"%s\" can't have attributes",
                                tagAndAttrs.name);
    }

    // TODO: Parse more attributes described at
    //       http://library.gnome.org/devel/pango/stable/PangoMarkupFormat.html
    //       and at least ignore them gracefully instead of giving errors (but
    //       quite a few of them could be supported as well, notable font_desc).

    wxMarkupSpanAttributes& spanAttrs = tagAndAttrs.attrs;

    while ( !attrs.empty() )
    {
        wxString rest;
        const wxString attr = attrs.BeforeFirst(' ', &rest);
        attrs = rest;

        // The "original" versions are used for error messages only.
        wxString valueOrig;
        const wxString nameOrig = attr.BeforeFirst('=', &valueOrig);

        const wxString name = nameOrig.Lower();
        wxString value = valueOrig.Lower();

        // All attributes values must be quoted.
        if ( value.length() < 2 ||
                (value[0] != value.Last()) ||
                    (value[0] != '"' && value[0] != '\'') )
        {
            return wxString::Format("bad quoting for value of \"%s\"",
                                    nameOrig);
        }

        value.assign(value, 1, value.length() - 2);

        if ( name == "foreground" || name == "fgcolor" || name == "color" )
        {
            spanAttrs.m_fgCol = value;
        }
        else if ( name == "background" || name == "bgcolor" )
        {
            spanAttrs.m_bgCol = value;
        }
        else if ( name == "font_family" || name == "face" )
        {
            spanAttrs.m_fontFace = value;
        }
        else if ( name == "font_weight" || name == "weight" )
        {
            unsigned long weight;

            if ( value == "ultralight" || value == "light" || value == "normal" )
                spanAttrs.m_isBold = wxMarkupSpanAttributes::No;
            else if ( value == "bold" || value == "ultrabold" || value == "heavy" )
                spanAttrs.m_isBold = wxMarkupSpanAttributes::Yes;
            else if ( value.ToULong(&weight) )
                spanAttrs.m_isBold = weight >= 600 ? wxMarkupSpanAttributes::Yes
                                                   : wxMarkupSpanAttributes::No;
            else
                return wxString::Format("invalid font weight \"%s\"", valueOrig);
        }
        else if ( name == "font_style" || name == "style" )
        {
            if ( value == "normal" )
                spanAttrs.m_isItalic = wxMarkupSpanAttributes::No;
            else if ( value == "oblique" || value == "italic" )
                spanAttrs.m_isItalic = wxMarkupSpanAttributes::Yes;
            else
                return wxString::Format("invalid font style \"%s\"", valueOrig);
        }
        else if ( name == "size" )
        {
            unsigned long size;
            if ( value.ToULong(&size) )
            {
                spanAttrs.m_sizeKind = wxMarkupSpanAttributes::Size_PointParts;
                spanAttrs.m_fontSize = size;
            }
            else if ( value == "smaller" || value == "larger" )
            {
                spanAttrs.m_sizeKind = wxMarkupSpanAttributes::Size_Relative;
                spanAttrs.m_fontSize = value == "smaller" ? -1 : +1;
            }
            else // Must be a CSS-like size specification
            {
                int cssSize = 1;
                if ( value.StartsWith("xx-", &rest) )
                    cssSize = 3;
                else if ( value.StartsWith("x-", &rest) )
                    cssSize = 2;
                else if ( value == "medium" )
                    cssSize = 0;
                else
                    rest = value;

                if ( cssSize != 0 )
                {
                    if ( rest == "small" )
                        cssSize = -cssSize;
                    else if ( rest != "large" )
                        return wxString::Format("invalid font size \"%s\"",
                                                valueOrig);
                }

                spanAttrs.m_sizeKind = wxMarkupSpanAttributes::Size_Symbolic;
                spanAttrs.m_fontSize = cssSize;
            }
        }
    }

    return wxString();
}

bool wxMarkupParser::OutputTag(const TagAndAttrs& tagAndAttrs, bool start)
{
    if ( tagAndAttrs.name.CmpNoCase("span") == 0 )
    {
        if ( start )
            m_output.OnSpanStart(tagAndAttrs.attrs);
        else
            m_output.OnSpanEnd(tagAndAttrs.attrs);

        return true;
    }
    else // non-span tag
    {
        static const struct TagHandler
        {
            const char *name;
            void (wxMarkupParserOutput::*startFunc)();
            void (wxMarkupParserOutput::*endFunc)();
        } tagHandlers[] =
        {
            { "b", &wxMarkupParserOutput::OnBoldStart,
                   &wxMarkupParserOutput::OnBoldEnd },
            { "i", &wxMarkupParserOutput::OnItalicStart,
                   &wxMarkupParserOutput::OnItalicEnd },
            { "u", &wxMarkupParserOutput::OnUnderlinedStart,
                   &wxMarkupParserOutput::OnUnderlinedEnd },
            { "s", &wxMarkupParserOutput::OnStrikethroughStart,
                   &wxMarkupParserOutput::OnStrikethroughEnd },
            { "big", &wxMarkupParserOutput::OnBigStart,
                     &wxMarkupParserOutput::OnBigEnd },
            { "small", &wxMarkupParserOutput::OnSmallStart,
                       &wxMarkupParserOutput::OnSmallEnd },
            { "tt", &wxMarkupParserOutput::OnTeletypeStart,
                    &wxMarkupParserOutput::OnTeletypeEnd },
        };

        for ( unsigned n = 0; n < WXSIZEOF(tagHandlers); n++ )
        {
            const TagHandler& h = tagHandlers[n];

            if ( tagAndAttrs.name.CmpNoCase(h.name) == 0 )
            {
                if ( start )
                    (m_output.*(h.startFunc))();
                else
                    (m_output.*(h.endFunc))();

                return true;
            }
        }
    }

    // Unknown tag name.
    return false;
}

bool wxMarkupParser::Parse(const wxString& text)
{
    // The stack containing the names and corresponding attributes (which are
    // actually only used for <span> tags) of all of the currently opened tag
    // or none if we're not inside any tag.
    wxStack<TagAndAttrs> tags;

    // Current run of text.
    wxString current;

    const wxString::const_iterator end = text.end();
    for ( wxString::const_iterator it = text.begin(); it != end; ++it )
    {
        switch ( (*it).GetValue() )
        {
            case '<':
                {
                    // Flush the text preceding the tag, if any.
                    if ( !current.empty() )
                    {
                        m_output.OnText(current);
                        current.clear();
                    }

                    // This variable is used only in the debugging messages
                    // and doesn't need to be defined if they're not compiled
                    // at all (it actually would result in unused variable
                    // messages in this case).
#if wxUSE_LOG_DEBUG || !defined(HAVE_VARIADIC_MACROS)
                    // Remember the tag starting position for the error
                    // messages.
                    const size_t pos = it - text.begin();
#endif
                    bool start = true;
                    if ( ++it != end && *it == '/' )
                    {
                        start = false;
                        ++it;
                    }

                    const wxString tag = ExtractUntil('>', it, end);
                    if ( tag.empty() )
                    {
                        wxLogDebug("%s at %lu.",
                                   it == end ? "Unclosed tag starting"
                                             : "Empty tag",
                                   pos);
                        return false;
                    }

                    if ( start )
                    {
                        wxString attrs;
                        const wxString name = tag.BeforeFirst(' ', &attrs);

                        TagAndAttrs tagAndAttrs(name);
                        const wxString err = ParseAttrs(attrs, tagAndAttrs);
                        if ( !err.empty() )
                        {
                            wxLogDebug("Bad attributes for \"%s\" "
                                       "at %lu: %s.",
                                       name, pos, err);
                            return false;
                        }

                        tags.push(tagAndAttrs);
                    }
                    else // end tag
                    {
                        if ( tags.empty() || tags.top().name != tag )
                        {
                            wxLogDebug("Unmatched closing tag \"%s\" at %lu.",
                                       tag, pos);
                            return false;
                        }
                    }

                    if ( !OutputTag(tags.top(), start) )
                    {
                        wxLogDebug("Unknown tag at %lu.", pos);
                        return false;
                    }

                    if ( !start )
                        tags.pop();
                }
                break;

            case '>':
                wxLogDebug("'>' should be escaped as \"&gt\"; at %lu.",
                           it - text.begin());
                break;

            case '&':
                // Processing is somewhat complicated: we need to recognize at
                // least the "&lt;" entity to allow escaping left square
                // brackets in the markup and, in fact, we recognize all of the
                // standard XML entities for consistency with Pango markup
                // parsing.
                //
                // However we also allow '&' to appear unescaped, i.e. directly
                // and not as "&amp;" when it is used to introduce the mnemonic
                // for the label. In this case we simply leave it alone.
                //
                // Notice that this logic makes it impossible to have a label
                // with "lt;" inside it and using "l" as mnemonic but hopefully
                // this shouldn't be a problem in practice.
                {
                    const size_t pos = it - text.begin() + 1;

                    unsigned n;
                    for ( n = 0; n < WXSIZEOF(xmlEntities); n++ )
                    {
                        const XMLEntity& xmlEnt = xmlEntities[n];
                        if ( text.compare(pos, xmlEnt.len, xmlEnt.name) == 0
                                && text[pos + xmlEnt.len] == ';' )
                        {
                            // Escape the ampersands if needed to protect them
                            // from being interpreted as mnemonics indicators.
                            if ( xmlEnt.value == '&' )
                                current += "&&";
                            else
                                current += xmlEnt.value;

                            it += xmlEnt.len + 1; // +1 for '&' itself

                            break;
                        }
                    }

                    if ( n < WXSIZEOF(xmlEntities) )
                        break;
                    wxFALLTHROUGH;//else: fall through, '&' is not special
                }

            default:
                current += *it;
        }
    }

    if ( !tags.empty() )
    {
        wxLogDebug("Missing closing tag for \"%s\"", tags.top().name);
        return false;
    }

    if ( !current.empty() )
        m_output.OnText(current);

    return true;
}

/* static */
wxString wxMarkupParser::Quote(const wxString& text)
{
    wxString quoted;
    quoted.reserve(text.length());

    for ( wxString::const_iterator it = text.begin(); it != text.end(); ++it )
    {
        unsigned n;
        for ( n = 0; n < WXSIZEOF(xmlEntities); n++ )
        {
            const XMLEntity& xmlEnt = xmlEntities[n];
            if ( *it == xmlEnt.value )
            {
                quoted << '&' << xmlEnt.name << ';';
                break;
            }
        }

        if ( n == WXSIZEOF(xmlEntities) )
            quoted += *it;
    }

    return quoted;
}

/* static */
wxString wxMarkupParser::Strip(const wxString& text)
{
    class StripOutput : public wxMarkupParserOutput
    {
    public:
        StripOutput() { }

        const wxString& GetText() const { return m_text; }

        virtual void OnText(const wxString& text) wxOVERRIDE { m_text += text; }

        virtual void OnBoldStart() wxOVERRIDE { }
        virtual void OnBoldEnd() wxOVERRIDE { }

        virtual void OnItalicStart() wxOVERRIDE { }
        virtual void OnItalicEnd() wxOVERRIDE { }

        virtual void OnUnderlinedStart() wxOVERRIDE { }
        virtual void OnUnderlinedEnd() wxOVERRIDE { }

        virtual void OnStrikethroughStart() wxOVERRIDE { }
        virtual void OnStrikethroughEnd() wxOVERRIDE { }

        virtual void OnBigStart() wxOVERRIDE { }
        virtual void OnBigEnd() wxOVERRIDE { }

        virtual void OnSmallStart() wxOVERRIDE { }
        virtual void OnSmallEnd() wxOVERRIDE { }

        virtual void OnTeletypeStart() wxOVERRIDE { }
        virtual void OnTeletypeEnd() wxOVERRIDE { }

        virtual void OnSpanStart(const wxMarkupSpanAttributes& WXUNUSED(a)) wxOVERRIDE { }
        virtual void OnSpanEnd(const wxMarkupSpanAttributes& WXUNUSED(a)) wxOVERRIDE { }

    private:
        wxString m_text;
    };

    StripOutput output;
    wxMarkupParser parser(output);
    if ( !parser.Parse(text) )
        return wxString();

    return output.GetText();
}

#endif // wxUSE_MARKUP
