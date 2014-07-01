///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/cocoa/private/markuptoattr.h
// Purpose:     Class to convert markup to Cocoa attributed strings.
// Author:      Vadim Zeitlin
// Created:     2011-02-22
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_COCOA_PRIVATE_MARKUPTOATTR_H_
#define _WX_OSX_COCOA_PRIVATE_MARKUPTOATTR_H_

#include "wx/private/markupparserattr.h"

// ----------------------------------------------------------------------------
// wxMarkupToAttrString: create NSAttributedString from markup.
// ----------------------------------------------------------------------------

class wxMarkupToAttrString : public wxMarkupParserAttrOutput
{
public:
    // We don't care about the original colours because we never use them but
    // we do need the correct initial font as we apply modifiers (e.g. create a
    // font larger than it) to it and so it must be valid.
    wxMarkupToAttrString(wxWindow *win, const wxString& markup)
        : wxMarkupParserAttrOutput(win->GetFont(), wxColour(), wxColour())
    {
        const wxCFStringRef
            label(wxControl::RemoveMnemonics(wxMarkupParser::Strip(markup)));
        m_attrString = [[NSMutableAttributedString alloc]
                        initWithString: label.AsNSString()];

        m_pos = 0;

        [m_attrString beginEditing];

        // First thing we do is change the default string font: as mentioned in
        // Apple documentation, attributed strings use "Helvetica 12" font by
        // default which is different from the system "Lucida Grande" font. So
        // we need to explicitly change the font for the entire string.
        [m_attrString addAttribute:NSFontAttributeName
                      value:win->GetFont().OSXGetNSFont()
                      range:NSMakeRange(0, [m_attrString length])];

        // Now translate the markup tags to corresponding attributes.
        wxMarkupParser parser(*this);
        parser.Parse(markup);

        [m_attrString endEditing];
    }

    ~wxMarkupToAttrString()
    {
        [m_attrString release];
    }

    // Accessor for the users of this class.
    //
    // We keep ownership of the returned string.
    NSMutableAttributedString *GetNSAttributedString() const
    {
        return m_attrString;
    }


    // Implement base class pure virtual methods to process markup tags.
    virtual void OnText(const wxString& text)
    {
        m_pos += wxControl::RemoveMnemonics(text).length();
    }

    virtual void OnAttrStart(const Attr& WXUNUSED(attr))
    {
        // Just remember the starting position of the range, we can't really
        // set the attribute until we find the end of it.
        m_rangeStarts.push(m_pos);
    }

    virtual void OnAttrEnd(const Attr& attr)
    {
        unsigned start = m_rangeStarts.top();
        m_rangeStarts.pop();

        const NSRange range = NSMakeRange(start, m_pos - start);

        [m_attrString addAttribute:NSFontAttributeName
                      value:attr.font.OSXGetNSFont()
                      range:range];

        if ( attr.foreground.IsOk() )
        {
            [m_attrString addAttribute:NSForegroundColorAttributeName
                          value:attr.foreground.OSXGetNSColor()
                          range:range];
        }

        if ( attr.background.IsOk() )
        {
            [m_attrString addAttribute:NSBackgroundColorAttributeName
                          value:attr.background.OSXGetNSColor()
                          range:range];
        }
    }

private:
    // The attributed string we're building.
    NSMutableAttributedString *m_attrString;

    // The current position in the output string.
    unsigned m_pos;

    // The positions of starting ranges.
    wxStack<unsigned> m_rangeStarts;


    wxDECLARE_NO_COPY_CLASS(wxMarkupToAttrString);
};

#endif // _WX_OSX_COCOA_PRIVATE_MARKUPTOATTR_H_
