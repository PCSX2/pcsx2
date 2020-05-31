///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/textcmn.cpp
// Purpose:     implementation of platform-independent functions of wxTextCtrl
// Author:      Julian Smart
// Modified by:
// Created:     13.07.99
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/event.h"
#endif // WX_PRECOMP

#if wxUSE_TEXTCTRL

#include "wx/textctrl.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
#endif // WX_PRECOMP

#include "wx/ffile.h"

extern WXDLLEXPORT_DATA(const char) wxTextCtrlNameStr[] = "text";

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

// we don't have any objects of type wxTextCtrlBase in the program, only
// wxTextCtrl, so this cast is safe
#define TEXTCTRL(ptr)   ((wxTextCtrl *)(ptr))

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// XTI
// ----------------------------------------------------------------------------

wxDEFINE_FLAGS( wxTextCtrlStyle )
wxBEGIN_FLAGS( wxTextCtrlStyle )
// new style border flags, we put them first to
// use them for streaming out
wxFLAGS_MEMBER(wxBORDER_SIMPLE)
wxFLAGS_MEMBER(wxBORDER_SUNKEN)
wxFLAGS_MEMBER(wxBORDER_DOUBLE)
wxFLAGS_MEMBER(wxBORDER_RAISED)
wxFLAGS_MEMBER(wxBORDER_STATIC)
wxFLAGS_MEMBER(wxBORDER_NONE)

// old style border flags
wxFLAGS_MEMBER(wxSIMPLE_BORDER)
wxFLAGS_MEMBER(wxSUNKEN_BORDER)
wxFLAGS_MEMBER(wxDOUBLE_BORDER)
wxFLAGS_MEMBER(wxRAISED_BORDER)
wxFLAGS_MEMBER(wxSTATIC_BORDER)
wxFLAGS_MEMBER(wxBORDER)

// standard window styles
wxFLAGS_MEMBER(wxTAB_TRAVERSAL)
wxFLAGS_MEMBER(wxCLIP_CHILDREN)
wxFLAGS_MEMBER(wxTRANSPARENT_WINDOW)
wxFLAGS_MEMBER(wxWANTS_CHARS)
wxFLAGS_MEMBER(wxFULL_REPAINT_ON_RESIZE)
wxFLAGS_MEMBER(wxALWAYS_SHOW_SB )
wxFLAGS_MEMBER(wxVSCROLL)
wxFLAGS_MEMBER(wxHSCROLL)

wxFLAGS_MEMBER(wxTE_PROCESS_ENTER)
wxFLAGS_MEMBER(wxTE_PROCESS_TAB)
wxFLAGS_MEMBER(wxTE_MULTILINE)
wxFLAGS_MEMBER(wxTE_PASSWORD)
wxFLAGS_MEMBER(wxTE_READONLY)
wxFLAGS_MEMBER(wxHSCROLL)
wxFLAGS_MEMBER(wxTE_RICH)
wxFLAGS_MEMBER(wxTE_RICH2)
wxFLAGS_MEMBER(wxTE_AUTO_URL)
wxFLAGS_MEMBER(wxTE_NOHIDESEL)
wxFLAGS_MEMBER(wxTE_LEFT)
wxFLAGS_MEMBER(wxTE_CENTRE)
wxFLAGS_MEMBER(wxTE_RIGHT)
wxFLAGS_MEMBER(wxTE_DONTWRAP)
wxFLAGS_MEMBER(wxTE_CHARWRAP)
wxFLAGS_MEMBER(wxTE_WORDWRAP)
wxEND_FLAGS( wxTextCtrlStyle )

wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxTextCtrl, wxControl, "wx/textctrl.h")

wxBEGIN_PROPERTIES_TABLE(wxTextCtrl)
wxEVENT_PROPERTY( TextUpdated, wxEVT_TEXT, wxCommandEvent )
wxEVENT_PROPERTY( TextEnter, wxEVT_TEXT_ENTER, wxCommandEvent )

wxPROPERTY( Font, wxFont, SetFont, GetFont , wxEMPTY_PARAMETER_VALUE, \
           0 /*flags*/, wxT("Helpstring"), wxT("group") )
wxPROPERTY( Value, wxString, SetValue, GetValue, wxString(), \
           0 /*flags*/, wxT("Helpstring"), wxT("group"))

wxPROPERTY_FLAGS( WindowStyle, wxTextCtrlStyle, long, SetWindowStyleFlag, \
                 GetWindowStyleFlag, wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, \
                 wxT("Helpstring"), wxT("group")) // style
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxTextCtrl)

wxCONSTRUCTOR_6( wxTextCtrl, wxWindow*, Parent, wxWindowID, Id, \
                wxString, Value, wxPoint, Position, wxSize, Size, \
                long, WindowStyle)


IMPLEMENT_DYNAMIC_CLASS(wxTextUrlEvent, wxCommandEvent)

wxDEFINE_EVENT( wxEVT_TEXT, wxCommandEvent );
wxDEFINE_EVENT( wxEVT_TEXT_ENTER, wxCommandEvent );
wxDEFINE_EVENT( wxEVT_TEXT_URL, wxTextUrlEvent );
wxDEFINE_EVENT( wxEVT_TEXT_MAXLEN, wxCommandEvent );

IMPLEMENT_ABSTRACT_CLASS(wxTextCtrlBase, wxControl)

// ============================================================================
// wxTextAttr implementation
// ============================================================================

wxTextAttr::wxTextAttr(const wxColour& colText,
               const wxColour& colBack,
               const wxFont& font,
               wxTextAttrAlignment alignment): m_textAlignment(alignment), m_colText(colText), m_colBack(colBack)
{
    Init();

    if (m_colText.IsOk()) m_flags |= wxTEXT_ATTR_TEXT_COLOUR;
    if (m_colBack.IsOk()) m_flags |= wxTEXT_ATTR_BACKGROUND_COLOUR;
    if (alignment != wxTEXT_ALIGNMENT_DEFAULT)
        m_flags |= wxTEXT_ATTR_ALIGNMENT;

    GetFontAttributes(font);
}

// Initialisation
void wxTextAttr::Init()
{
    m_textAlignment = wxTEXT_ALIGNMENT_DEFAULT;
    m_flags = 0;
    m_leftIndent = 0;
    m_leftSubIndent = 0;
    m_rightIndent = 0;

    m_fontSize = 12;
    m_fontStyle = wxFONTSTYLE_NORMAL;
    m_fontWeight = wxFONTWEIGHT_NORMAL;
    m_fontUnderlined = false;
    m_fontStrikethrough = false;
    m_fontEncoding = wxFONTENCODING_DEFAULT;
    m_fontFamily = wxFONTFAMILY_DEFAULT;

    m_paragraphSpacingAfter = 0;
    m_paragraphSpacingBefore = 0;
    m_lineSpacing = 0;
    m_bulletStyle = wxTEXT_ATTR_BULLET_STYLE_NONE;
    m_textEffects = wxTEXT_ATTR_EFFECT_NONE;
    m_textEffectFlags = wxTEXT_ATTR_EFFECT_NONE;
    m_outlineLevel = 0;
    m_bulletNumber = 0;
}

// Copy
void wxTextAttr::Copy(const wxTextAttr& attr)
{
    m_colText = attr.m_colText;
    m_colBack = attr.m_colBack;
    m_textAlignment = attr.m_textAlignment;
    m_leftIndent = attr.m_leftIndent;
    m_leftSubIndent = attr.m_leftSubIndent;
    m_rightIndent = attr.m_rightIndent;
    m_tabs = attr.m_tabs;
    m_flags = attr.m_flags;

    m_fontSize = attr.m_fontSize;
    m_fontStyle = attr.m_fontStyle;
    m_fontWeight = attr.m_fontWeight;
    m_fontUnderlined = attr.m_fontUnderlined;
    m_fontStrikethrough = attr.m_fontStrikethrough;
    m_fontFaceName = attr.m_fontFaceName;
    m_fontEncoding = attr.m_fontEncoding;
    m_fontFamily = attr.m_fontFamily;
    m_textEffects = attr.m_textEffects;
    m_textEffectFlags = attr.m_textEffectFlags;

    m_paragraphSpacingAfter = attr.m_paragraphSpacingAfter;
    m_paragraphSpacingBefore = attr.m_paragraphSpacingBefore;
    m_lineSpacing = attr.m_lineSpacing;
    m_characterStyleName = attr.m_characterStyleName;
    m_paragraphStyleName = attr.m_paragraphStyleName;
    m_listStyleName = attr.m_listStyleName;
    m_bulletStyle = attr.m_bulletStyle;
    m_bulletNumber = attr.m_bulletNumber;
    m_bulletText = attr.m_bulletText;
    m_bulletFont = attr.m_bulletFont;
    m_bulletName = attr.m_bulletName;
    m_outlineLevel = attr.m_outlineLevel;

    m_urlTarget = attr.m_urlTarget;
}

// operators
void wxTextAttr::operator= (const wxTextAttr& attr)
{
    Copy(attr);
}

// Equality test
bool wxTextAttr::operator== (const wxTextAttr& attr) const
{
    return  GetFlags() == attr.GetFlags() &&

            (!HasTextColour() || (GetTextColour() == attr.GetTextColour())) &&
            (!HasBackgroundColour() || (GetBackgroundColour() == attr.GetBackgroundColour())) &&

            (!HasAlignment() || (GetAlignment() == attr.GetAlignment())) &&
            (!HasLeftIndent() || (GetLeftIndent() == attr.GetLeftIndent() &&
                                  GetLeftSubIndent() == attr.GetLeftSubIndent())) &&
            (!HasRightIndent() || (GetRightIndent() == attr.GetRightIndent())) &&
            (!HasTabs() || (TabsEq(GetTabs(), attr.GetTabs()))) &&

            (!HasParagraphSpacingAfter() || (GetParagraphSpacingAfter() == attr.GetParagraphSpacingAfter())) &&
            (!HasParagraphSpacingBefore() || (GetParagraphSpacingBefore() == attr.GetParagraphSpacingBefore())) &&
            (!HasLineSpacing() || (GetLineSpacing() == attr.GetLineSpacing())) &&
            (!HasCharacterStyleName() || (GetCharacterStyleName() == attr.GetCharacterStyleName())) &&
            (!HasParagraphStyleName() || (GetParagraphStyleName() == attr.GetParagraphStyleName())) &&
            (!HasListStyleName() || (GetListStyleName() == attr.GetListStyleName())) &&

            (!HasBulletStyle() || (GetBulletStyle() == attr.GetBulletStyle())) &&
            (!HasBulletText() || (GetBulletText() == attr.GetBulletText())) &&
            (!HasBulletNumber() || (GetBulletNumber() == attr.GetBulletNumber())) &&
            (GetBulletFont() == attr.GetBulletFont()) &&
            (!HasBulletName() || (GetBulletName() == attr.GetBulletName())) &&

            (!HasTextEffects() || (GetTextEffects() == attr.GetTextEffects() &&
                                   GetTextEffectFlags() == attr.GetTextEffectFlags())) &&

            (!HasOutlineLevel() || (GetOutlineLevel() == attr.GetOutlineLevel())) &&

            (!HasFontSize() || (GetFontSize() == attr.GetFontSize())) &&
            (!HasFontItalic() || (GetFontStyle() == attr.GetFontStyle())) &&
            (!HasFontWeight() || (GetFontWeight() == attr.GetFontWeight())) &&
            (!HasFontUnderlined() || (GetFontUnderlined() == attr.GetFontUnderlined())) &&
            (!HasFontStrikethrough() || (GetFontStrikethrough() == attr.GetFontStrikethrough())) &&
            (!HasFontFaceName() || (GetFontFaceName() == attr.GetFontFaceName())) &&
            (!HasFontEncoding() || (GetFontEncoding() == attr.GetFontEncoding())) &&
            (!HasFontFamily() || (GetFontFamily() == attr.GetFontFamily())) &&

            (!HasURL() || (GetURL() == attr.GetURL()));
}

// Partial equality test. Only returns false if an attribute doesn't match.
bool wxTextAttr::EqPartial(const wxTextAttr& attr, bool weakTest) const
{
    int flags = attr.GetFlags();

    if (!weakTest &&
        ((!HasTextColour() && attr.HasTextColour()) ||
         (!HasBackgroundColour() && attr.HasBackgroundColour()) ||
         (!HasFontFaceName() && attr.HasFontFaceName()) ||
         (!HasFontSize() && attr.HasFontSize()) ||
         (!HasFontWeight() && attr.HasFontWeight()) ||
         (!HasFontItalic() && attr.HasFontItalic()) ||
         (!HasFontUnderlined() && attr.HasFontUnderlined()) ||
         (!HasFontStrikethrough() && attr.HasFontStrikethrough()) ||
         (!HasFontEncoding() && attr.HasFontEncoding()) ||
         (!HasFontFamily() && attr.HasFontFamily()) ||
         (!HasURL() && attr.HasURL()) ||
         (!HasAlignment() && attr.HasAlignment()) ||
         (!HasLeftIndent() && attr.HasLeftIndent()) ||
         (!HasParagraphSpacingAfter() && attr.HasParagraphSpacingAfter()) ||
         (!HasParagraphSpacingBefore() && attr.HasParagraphSpacingBefore()) ||
         (!HasLineSpacing() && attr.HasLineSpacing()) ||
         (!HasCharacterStyleName() && attr.HasCharacterStyleName()) ||
         (!HasParagraphStyleName() && attr.HasParagraphStyleName()) ||
         (!HasListStyleName() && attr.HasListStyleName()) ||
         (!HasBulletStyle() && attr.HasBulletStyle()) ||
         (!HasBulletNumber() && attr.HasBulletNumber()) ||
         (!HasBulletText() && attr.HasBulletText()) ||
         (!HasBulletName() && attr.HasBulletName()) ||
         (!HasTabs() && attr.HasTabs()) ||
         (!HasTextEffects() && attr.HasTextEffects()) ||
         (!HasOutlineLevel() && attr.HasOutlineLevel())))
    {
        return false;
    }

    if (HasTextColour() && attr.HasTextColour() && GetTextColour() != attr.GetTextColour())
        return false;

    if (HasBackgroundColour() && attr.HasBackgroundColour() && GetBackgroundColour() != attr.GetBackgroundColour())
        return false;

    if (HasFontFaceName() && attr.HasFontFaceName() && GetFontFaceName() != attr.GetFontFaceName())
        return false;

    // This checks whether the two objects have the same font size dimension (px versus pt)
    if (HasFontSize() && attr.HasFontSize() && (flags & wxTEXT_ATTR_FONT) != (GetFlags() & wxTEXT_ATTR_FONT))
        return false;

    if (HasFontPointSize() && attr.HasFontPointSize() && GetFontSize() != attr.GetFontSize())
        return false;

    if (HasFontPixelSize() && attr.HasFontPixelSize() && GetFontSize() != attr.GetFontSize())
        return false;

    if (HasFontWeight() && attr.HasFontWeight() && GetFontWeight() != attr.GetFontWeight())
        return false;

    if (HasFontItalic() && attr.HasFontItalic() && GetFontStyle() != attr.GetFontStyle())
        return false;

    if (HasFontUnderlined() && attr.HasFontUnderlined() && GetFontUnderlined() != attr.GetFontUnderlined())
        return false;

    if (HasFontStrikethrough() && attr.HasFontStrikethrough() && GetFontStrikethrough() != attr.GetFontStrikethrough())
        return false;

    if (HasFontEncoding() && attr.HasFontEncoding() && GetFontEncoding() != attr.GetFontEncoding())
        return false;

    if (HasFontFamily() && attr.HasFontFamily() && GetFontFamily() != attr.GetFontFamily())
        return false;

    if (HasURL() && attr.HasURL() && GetURL() != attr.GetURL())
        return false;

    if (HasAlignment() && attr.HasAlignment() && GetAlignment() != attr.GetAlignment())
        return false;

    if (HasLeftIndent() && attr.HasLeftIndent() &&
        ((GetLeftIndent() != attr.GetLeftIndent()) || (GetLeftSubIndent() != attr.GetLeftSubIndent())))
        return false;

    if (HasRightIndent() && attr.HasRightIndent() && (GetRightIndent() != attr.GetRightIndent()))
        return false;

    if (HasParagraphSpacingAfter() && attr.HasParagraphSpacingAfter() &&
        (GetParagraphSpacingAfter() != attr.GetParagraphSpacingAfter()))
        return false;

    if (HasParagraphSpacingBefore() && attr.HasParagraphSpacingBefore() &&
        (GetParagraphSpacingBefore() != attr.GetParagraphSpacingBefore()))
        return false;

    if (HasLineSpacing() && attr.HasLineSpacing() && (GetLineSpacing() != attr.GetLineSpacing()))
        return false;

    if (HasCharacterStyleName() && attr.HasCharacterStyleName() && (GetCharacterStyleName() != attr.GetCharacterStyleName()))
        return false;

    if (HasParagraphStyleName() && attr.HasParagraphStyleName() && (GetParagraphStyleName() != attr.GetParagraphStyleName()))
        return false;

    if (HasListStyleName() && attr.HasListStyleName() && (GetListStyleName() != attr.GetListStyleName()))
        return false;

    if (HasBulletStyle() && attr.HasBulletStyle() && (GetBulletStyle() != attr.GetBulletStyle()))
         return false;

    if (HasBulletNumber() && attr.HasBulletNumber() && (GetBulletNumber() != attr.GetBulletNumber()))
         return false;

    if (HasBulletText() && attr.HasBulletText() &&
        (GetBulletText() != attr.GetBulletText()) &&
        (GetBulletFont() != attr.GetBulletFont()))
         return false;

    if (HasBulletName() && attr.HasBulletName() && (GetBulletName() != attr.GetBulletName()))
         return false;

    if (HasTabs() && attr.HasTabs() && !TabsEq(GetTabs(), attr.GetTabs()))
        return false;

    if ((HasPageBreak() != attr.HasPageBreak()))
         return false;

    if (HasTextEffects() && attr.HasTextEffects())
    {
        if (!BitlistsEqPartial(GetTextEffects(), attr.GetTextEffects(), GetTextEffectFlags()))
            return false;
    }

    if (HasOutlineLevel() && attr.HasOutlineLevel() && (GetOutlineLevel() != attr.GetOutlineLevel()))
         return false;

    return true;
}

// Create font from font attributes.
wxFont wxTextAttr::GetFont() const
{
    if ( !HasFont() )
        return wxNullFont;

    int fontSize = 10;
    if (HasFontSize())
        fontSize = GetFontSize();

    wxFontStyle fontStyle = wxFONTSTYLE_NORMAL;
    if (HasFontItalic())
        fontStyle = GetFontStyle();

    wxFontWeight fontWeight = wxFONTWEIGHT_NORMAL;
    if (HasFontWeight())
        fontWeight = GetFontWeight();

    bool underlined = false;
    if (HasFontUnderlined())
        underlined = GetFontUnderlined();

    bool strikethrough = false;
    if (HasFontStrikethrough())
        strikethrough = GetFontStrikethrough();

    wxString fontFaceName;
    if (HasFontFaceName())
        fontFaceName = GetFontFaceName();

    wxFontEncoding encoding = wxFONTENCODING_DEFAULT;
    if (HasFontEncoding())
        encoding = GetFontEncoding();

    wxFontFamily fontFamily = wxFONTFAMILY_DEFAULT;
    if (HasFontFamily())
        fontFamily = GetFontFamily();

    if (HasFontPixelSize())
    {
        wxFont font(wxSize(0, fontSize), fontFamily, fontStyle, fontWeight, underlined, fontFaceName, encoding);
        if (strikethrough)
            font.SetStrikethrough(true);
        return font;
    }
    else
    {
        wxFont font(fontSize, fontFamily, fontStyle, fontWeight, underlined, fontFaceName, encoding);
        if (strikethrough)
            font.SetStrikethrough(true);
        return font;
    }
}

// Get attributes from font.
bool wxTextAttr::GetFontAttributes(const wxFont& font, int flags)
{
    if (!font.IsOk())
        return false;

    // If we pass both pixel and point size attributes, this is an indication
    // to choose the most appropriate units.
    if ((flags & wxTEXT_ATTR_FONT) == wxTEXT_ATTR_FONT)
    {
        if (font.IsUsingSizeInPixels())
        {
            m_fontSize = font.GetPixelSize().y;
            flags &= ~wxTEXT_ATTR_FONT_POINT_SIZE;
        }
        else
        {
            m_fontSize = font.GetPointSize();
            flags &= ~wxTEXT_ATTR_FONT_PIXEL_SIZE;
        }
    }
    else if (flags & wxTEXT_ATTR_FONT_POINT_SIZE)
    {
        m_fontSize = font.GetPointSize();
        flags &= ~wxTEXT_ATTR_FONT_PIXEL_SIZE;
    }
    else if (flags & wxTEXT_ATTR_FONT_PIXEL_SIZE)
    {
        m_fontSize = font.GetPixelSize().y;
    }

    if (flags & wxTEXT_ATTR_FONT_ITALIC)
        m_fontStyle = font.GetStyle();

    if (flags & wxTEXT_ATTR_FONT_WEIGHT)
        m_fontWeight = font.GetWeight();

    if (flags & wxTEXT_ATTR_FONT_UNDERLINE)
        m_fontUnderlined = font.GetUnderlined();

    if (flags & wxTEXT_ATTR_FONT_STRIKETHROUGH)
        m_fontStrikethrough = font.GetStrikethrough();

    if (flags & wxTEXT_ATTR_FONT_FACE)
        m_fontFaceName = font.GetFaceName();

    if (flags & wxTEXT_ATTR_FONT_ENCODING)
        m_fontEncoding = font.GetEncoding();

    if (flags & wxTEXT_ATTR_FONT_FAMILY)
    {
        // wxFont might not know its family, avoid setting m_fontFamily to an
        // invalid value and rather pretend that we don't have any font family
        // information at all in this case
        const wxFontFamily fontFamily = font.GetFamily();
        if ( fontFamily == wxFONTFAMILY_UNKNOWN )
            flags &= ~wxTEXT_ATTR_FONT_FAMILY;
        else
            m_fontFamily = fontFamily;
    }

    m_flags |= flags;

    return true;
}

// Resets bits in destination so new attributes aren't merged with mutually exclusive ones
static bool wxResetIncompatibleBits(const int mask, const int srcFlags, int& destFlags, int& destBits)
{
    if ((srcFlags & mask) && (destFlags & mask))
    {
        destBits &= ~mask;
        destFlags &= ~mask;
    }

    return true;
}

bool wxTextAttr::Apply(const wxTextAttr& style, const wxTextAttr* compareWith)
{
    wxTextAttr& destStyle = (*this);

    if (style.HasFontWeight())
    {
        if (!(compareWith && compareWith->HasFontWeight() && compareWith->GetFontWeight() == style.GetFontWeight()))
            destStyle.SetFontWeight(style.GetFontWeight());
    }

    if (style.HasFontPointSize())
    {
        if (!(compareWith && compareWith->HasFontPointSize() && compareWith->GetFontSize() == style.GetFontSize()))
            destStyle.SetFontPointSize(style.GetFontSize());
    }
    else if (style.HasFontPixelSize())
    {
        if (!(compareWith && compareWith->HasFontPixelSize() && compareWith->GetFontSize() == style.GetFontSize()))
            destStyle.SetFontPixelSize(style.GetFontSize());
    }

    if (style.HasFontItalic())
    {
        if (!(compareWith && compareWith->HasFontItalic() && compareWith->GetFontStyle() == style.GetFontStyle()))
            destStyle.SetFontStyle(style.GetFontStyle());
    }

    if (style.HasFontUnderlined())
    {
        if (!(compareWith && compareWith->HasFontUnderlined() && compareWith->GetFontUnderlined() == style.GetFontUnderlined()))
            destStyle.SetFontUnderlined(style.GetFontUnderlined());
    }

    if (style.HasFontStrikethrough())
    {
        if (!(compareWith && compareWith->HasFontStrikethrough() && compareWith->GetFontStrikethrough() == style.GetFontStrikethrough()))
            destStyle.SetFontStrikethrough(style.GetFontStrikethrough());
    }

    if (style.HasFontFaceName())
    {
        if (!(compareWith && compareWith->HasFontFaceName() && compareWith->GetFontFaceName() == style.GetFontFaceName()))
            destStyle.SetFontFaceName(style.GetFontFaceName());
    }

    if (style.HasFontEncoding())
    {
        if (!(compareWith && compareWith->HasFontEncoding() && compareWith->GetFontEncoding() == style.GetFontEncoding()))
            destStyle.SetFontEncoding(style.GetFontEncoding());
    }

    if (style.HasFontFamily())
    {
        if (!(compareWith && compareWith->HasFontFamily() && compareWith->GetFontFamily() == style.GetFontFamily()))
            destStyle.SetFontFamily(style.GetFontFamily());
    }

    if (style.GetTextColour().IsOk() && style.HasTextColour())
    {
        if (!(compareWith && compareWith->HasTextColour() && compareWith->GetTextColour() == style.GetTextColour()))
            destStyle.SetTextColour(style.GetTextColour());
    }

    if (style.GetBackgroundColour().IsOk() && style.HasBackgroundColour())
    {
        if (!(compareWith && compareWith->HasBackgroundColour() && compareWith->GetBackgroundColour() == style.GetBackgroundColour()))
            destStyle.SetBackgroundColour(style.GetBackgroundColour());
    }

    if (style.HasAlignment())
    {
        if (!(compareWith && compareWith->HasAlignment() && compareWith->GetAlignment() == style.GetAlignment()))
            destStyle.SetAlignment(style.GetAlignment());
    }

    if (style.HasTabs())
    {
        if (!(compareWith && compareWith->HasTabs() && TabsEq(compareWith->GetTabs(), style.GetTabs())))
            destStyle.SetTabs(style.GetTabs());
    }

    if (style.HasLeftIndent())
    {
        if (!(compareWith && compareWith->HasLeftIndent() && compareWith->GetLeftIndent() == style.GetLeftIndent()
                          && compareWith->GetLeftSubIndent() == style.GetLeftSubIndent()))
            destStyle.SetLeftIndent(style.GetLeftIndent(), style.GetLeftSubIndent());
    }

    if (style.HasRightIndent())
    {
        if (!(compareWith && compareWith->HasRightIndent() && compareWith->GetRightIndent() == style.GetRightIndent()))
            destStyle.SetRightIndent(style.GetRightIndent());
    }

    if (style.HasParagraphSpacingAfter())
    {
        if (!(compareWith && compareWith->HasParagraphSpacingAfter() && compareWith->GetParagraphSpacingAfter() == style.GetParagraphSpacingAfter()))
            destStyle.SetParagraphSpacingAfter(style.GetParagraphSpacingAfter());
    }

    if (style.HasParagraphSpacingBefore())
    {
        if (!(compareWith && compareWith->HasParagraphSpacingBefore() && compareWith->GetParagraphSpacingBefore() == style.GetParagraphSpacingBefore()))
            destStyle.SetParagraphSpacingBefore(style.GetParagraphSpacingBefore());
    }

    if (style.HasLineSpacing())
    {
        if (!(compareWith && compareWith->HasLineSpacing() && compareWith->GetLineSpacing() == style.GetLineSpacing()))
            destStyle.SetLineSpacing(style.GetLineSpacing());
    }

    if (style.HasCharacterStyleName())
    {
        if (!(compareWith && compareWith->HasCharacterStyleName() && compareWith->GetCharacterStyleName() == style.GetCharacterStyleName()))
            destStyle.SetCharacterStyleName(style.GetCharacterStyleName());
    }

    if (style.HasParagraphStyleName())
    {
        if (!(compareWith && compareWith->HasParagraphStyleName() && compareWith->GetParagraphStyleName() == style.GetParagraphStyleName()))
            destStyle.SetParagraphStyleName(style.GetParagraphStyleName());
    }

    if (style.HasListStyleName())
    {
        if (!(compareWith && compareWith->HasListStyleName() && compareWith->GetListStyleName() == style.GetListStyleName()))
            destStyle.SetListStyleName(style.GetListStyleName());
    }

    if (style.HasBulletStyle())
    {
        if (!(compareWith && compareWith->HasBulletStyle() && compareWith->GetBulletStyle() == style.GetBulletStyle()))
            destStyle.SetBulletStyle(style.GetBulletStyle());
    }

    if (style.HasBulletText())
    {
        if (!(compareWith && compareWith->HasBulletText() && compareWith->GetBulletText() == style.GetBulletText()))
        {
            destStyle.SetBulletText(style.GetBulletText());
            destStyle.SetBulletFont(style.GetBulletFont());
        }
    }

    if (style.HasBulletNumber())
    {
        if (!(compareWith && compareWith->HasBulletNumber() && compareWith->GetBulletNumber() == style.GetBulletNumber()))
            destStyle.SetBulletNumber(style.GetBulletNumber());
    }

    if (style.HasBulletName())
    {
        if (!(compareWith && compareWith->HasBulletName() && compareWith->GetBulletName() == style.GetBulletName()))
            destStyle.SetBulletName(style.GetBulletName());
    }

    if (style.HasURL())
    {
        if (!(compareWith && compareWith->HasURL() && compareWith->GetURL() == style.GetURL()))
            destStyle.SetURL(style.GetURL());
    }

    if (style.HasPageBreak())
    {
        if (!(compareWith && compareWith->HasPageBreak()))
            destStyle.SetPageBreak();
    }

    if (style.HasTextEffects())
    {
        if (!(compareWith && compareWith->HasTextEffects() && compareWith->GetTextEffects() == style.GetTextEffects()))
        {
            int destBits = destStyle.GetTextEffects();
            int destFlags = destStyle.GetTextEffectFlags();

            int srcBits = style.GetTextEffects();
            int srcFlags = style.GetTextEffectFlags();

            // Reset incompatible bits in the destination
            wxResetIncompatibleBits((wxTEXT_ATTR_EFFECT_SUPERSCRIPT|wxTEXT_ATTR_EFFECT_SUBSCRIPT), srcFlags, destFlags, destBits);
            wxResetIncompatibleBits((wxTEXT_ATTR_EFFECT_CAPITALS|wxTEXT_ATTR_EFFECT_SMALL_CAPITALS), srcFlags, destFlags, destBits);
            wxResetIncompatibleBits((wxTEXT_ATTR_EFFECT_STRIKETHROUGH|wxTEXT_ATTR_EFFECT_DOUBLE_STRIKETHROUGH), srcFlags, destFlags, destBits);

            CombineBitlists(destBits, srcBits, destFlags, srcFlags);

            destStyle.SetTextEffects(destBits);
            destStyle.SetTextEffectFlags(destFlags);
        }
    }

    if (style.HasOutlineLevel())
    {
        if (!(compareWith && compareWith->HasOutlineLevel() && compareWith->GetOutlineLevel() == style.GetOutlineLevel()))
            destStyle.SetOutlineLevel(style.GetOutlineLevel());
    }

    return true;
}

/* static */
wxTextAttr wxTextAttr::Combine(const wxTextAttr& attr,
                               const wxTextAttr& attrDef,
                               const wxTextCtrlBase *text)
{
    wxFont font;
    if (attr.HasFont())
        font = attr.GetFont();

    if ( !font.IsOk() )
    {
        if (attrDef.HasFont())
            font = attrDef.GetFont();

        if ( text && !font.IsOk() )
            font = text->GetFont();
    }

    wxColour colFg = attr.GetTextColour();
    if ( !colFg.IsOk() )
    {
        colFg = attrDef.GetTextColour();

        if ( text && !colFg.IsOk() )
            colFg = text->GetForegroundColour();
    }

    wxColour colBg = attr.GetBackgroundColour();
    if ( !colBg.IsOk() )
    {
        colBg = attrDef.GetBackgroundColour();

        if ( text && !colBg.IsOk() )
            colBg = text->GetBackgroundColour();
    }

    wxTextAttr newAttr(colFg, colBg, font);

    if (attr.HasAlignment())
        newAttr.SetAlignment(attr.GetAlignment());
    else if (attrDef.HasAlignment())
        newAttr.SetAlignment(attrDef.GetAlignment());

    if (attr.HasTabs())
        newAttr.SetTabs(attr.GetTabs());
    else if (attrDef.HasTabs())
        newAttr.SetTabs(attrDef.GetTabs());

    if (attr.HasLeftIndent())
        newAttr.SetLeftIndent(attr.GetLeftIndent(), attr.GetLeftSubIndent());
    else if (attrDef.HasLeftIndent())
        newAttr.SetLeftIndent(attrDef.GetLeftIndent(), attr.GetLeftSubIndent());

    if (attr.HasRightIndent())
        newAttr.SetRightIndent(attr.GetRightIndent());
    else if (attrDef.HasRightIndent())
        newAttr.SetRightIndent(attrDef.GetRightIndent());

    return newAttr;
}

/// Compare tabs
bool wxTextAttr::TabsEq(const wxArrayInt& tabs1, const wxArrayInt& tabs2)
{
    if (tabs1.GetCount() != tabs2.GetCount())
        return false;

    size_t i;
    for (i = 0; i < tabs1.GetCount(); i++)
    {
        if (tabs1[i] != tabs2[i])
            return false;
    }
    return true;
}

// Remove attributes
bool wxTextAttr::RemoveStyle(wxTextAttr& destStyle, const wxTextAttr& style)
{
    int flags = style.GetFlags();
    int destFlags = destStyle.GetFlags();

    // We must treat text effects specially, since we must remove only some.
    if (style.HasTextEffects() && (style.GetTextEffectFlags() != 0))
    {
        int newTextEffectFlags = destStyle.GetTextEffectFlags() & ~style.GetTextEffectFlags();
        int newTextEffects = destStyle.GetTextEffects() & ~style.GetTextEffectFlags();
        destStyle.SetTextEffects(newTextEffects);
        destStyle.SetTextEffectFlags(newTextEffectFlags);

        // Don't remove wxTEXT_ATTR_EFFECTS unless the resulting flags are zero
        if (newTextEffectFlags != 0)
            flags &= ~wxTEXT_ATTR_EFFECTS;
    }

    destStyle.SetFlags(destFlags & ~flags);

    return true;
}

/// Combine two bitlists, specifying the bits of interest with separate flags.
bool wxTextAttr::CombineBitlists(int& valueA, int valueB, int& flagsA, int flagsB)
{
    // We want to apply B's bits to A, taking into account each's flags which indicate which bits
    // are to be taken into account. A zero in B's bits should reset that bit in A but only if B's flags
    // indicate it.

    // First, reset the 0 bits from B. We make a mask so we're only dealing with B's zero
    // bits at this point, ignoring any 1 bits in B or 0 bits in B that are not relevant.
    int valueA2 = ~(~valueB & flagsB) & valueA;

    // Now combine the 1 bits.
    int valueA3 = (valueB & flagsB) | valueA2;

    valueA = valueA3;
    flagsA = (flagsA | flagsB);

    return true;
}

/// Compare two bitlists
bool wxTextAttr::BitlistsEqPartial(int valueA, int valueB, int flags)
{
    int relevantBitsA = valueA & flags;
    int relevantBitsB = valueB & flags;
    return relevantBitsA == relevantBitsB;
}

/// Split into paragraph and character styles
bool wxTextAttr::SplitParaCharStyles(const wxTextAttr& style, wxTextAttr& parStyle, wxTextAttr& charStyle)
{
    wxTextAttr defaultCharStyle1(style);
    wxTextAttr defaultParaStyle1(style);
    defaultCharStyle1.SetFlags(defaultCharStyle1.GetFlags()&wxTEXT_ATTR_CHARACTER);
    defaultParaStyle1.SetFlags(defaultParaStyle1.GetFlags()&wxTEXT_ATTR_PARAGRAPH);

    charStyle.Apply(defaultCharStyle1);
    parStyle.Apply(defaultParaStyle1);

    return true;
}

// apply styling to text range
bool wxTextCtrlBase::SetStyle(long WXUNUSED(start), long WXUNUSED(end),
                              const wxTextAttr& WXUNUSED(style))
{
    // to be implemented in derived classes
    return false;
}

// get the styling at the given position
bool wxTextCtrlBase::GetStyle(long WXUNUSED(position), wxTextAttr& WXUNUSED(style))
{
    // to be implemented in derived classes
    return false;
}

// change default text attributes
bool wxTextCtrlBase::SetDefaultStyle(const wxTextAttr& style)
{
    // keep the old attributes if the new style doesn't specify them unless the
    // new style is empty - then reset m_defaultStyle (as there is no other way
    // to do it)
    if ( style.IsDefault() )
        m_defaultStyle = style;
    else
        m_defaultStyle = wxTextAttr::Combine(style, m_defaultStyle, this);

    return true;
}

// ----------------------------------------------------------------------------
// file IO functions
// ----------------------------------------------------------------------------

bool wxTextAreaBase::DoLoadFile(const wxString& filename, int WXUNUSED(fileType))
{
#if wxUSE_FFILE
    wxFFile file(filename);
    if ( file.IsOpened() )
    {
        wxString text;
        if ( file.ReadAll(&text) )
        {
            SetValue(text);

            DiscardEdits();
            m_filename = filename;

            return true;
        }
    }
#else
    (void)filename;   // avoid compiler warning about unreferenced parameter
#endif // wxUSE_FFILE

    wxLogError(_("File couldn't be loaded."));

    return false;
}

bool wxTextAreaBase::DoSaveFile(const wxString& filename, int WXUNUSED(fileType))
{
#if wxUSE_FFILE
    wxFFile file(filename, wxT("w"));
    if ( file.IsOpened() && file.Write(GetValue()) )
    {
        // if it worked, save for future calls
        m_filename = filename;

        // it's not modified any longer
        DiscardEdits();

        return true;
    }
#else
    (void)filename;   // avoid compiler warning about unreferenced parameter
#endif // wxUSE_FFILE

    return false;
}

bool wxTextAreaBase::SaveFile(const wxString& filename, int fileType)
{
    wxString filenameToUse = filename.empty() ? m_filename : filename;
    if ( filenameToUse.empty() )
    {
        // what kind of message to give? is it an error or a program bug?
        wxLogDebug(wxT("Can't save textctrl to file without filename."));

        return false;
    }

    return DoSaveFile(filenameToUse, fileType);
}

// ----------------------------------------------------------------------------
// stream-like insertion operator
// ----------------------------------------------------------------------------

wxTextCtrl& wxTextCtrlBase::operator<<(const wxString& s)
{
    AppendText(s);
    return *TEXTCTRL(this);
}

wxTextCtrl& wxTextCtrlBase::operator<<(double d)
{
    return *this << wxString::Format("%.2f", d);
}

wxTextCtrl& wxTextCtrlBase::operator<<(int i)
{
    return *this << wxString::Format("%d", i);
}

wxTextCtrl& wxTextCtrlBase::operator<<(long l)
{
    return *this << wxString::Format("%ld", l);
}

// ----------------------------------------------------------------------------
// streambuf methods implementation
// ----------------------------------------------------------------------------

#if wxHAS_TEXT_WINDOW_STREAM

int wxTextCtrlBase::overflow(int c)
{
    AppendText((wxChar)c);

    // return something different from EOF
    return 0;
}

#endif // wxHAS_TEXT_WINDOW_STREAM

// ----------------------------------------------------------------------------
// emulating key presses
// ----------------------------------------------------------------------------

bool wxTextCtrlBase::EmulateKeyPress(const wxKeyEvent& event)
{
    bool handled = false;
    // we have a native implementation for Win32 and so don't need this one
#ifndef __WIN32__
    wxChar ch = 0;
    int keycode = event.GetKeyCode();

    long from, to;
    GetSelection(&from,&to);
    long insert = GetInsertionPoint();
    long last = GetLastPosition();

    // catch arrow left and right

    switch ( keycode )
    {
        case WXK_LEFT:
            if ( event.ShiftDown() )
                SetSelection( (from > 0 ? from - 1 : 0) , to );
            else
            {
                if ( from != to )
                    insert = from;
                else if ( insert > 0 )
                    insert -= 1;
                SetInsertionPoint( insert );
            }
            handled = true;
            break;
        case WXK_RIGHT:
            if ( event.ShiftDown() )
                SetSelection( from, (to < last ? to + 1 : last) );
            else
            {
                if ( from != to )
                    insert = to;
                else if ( insert < last )
                    insert += 1;
                SetInsertionPoint( insert );
            }
            handled = true;
            break;
        case WXK_NUMPAD0:
        case WXK_NUMPAD1:
        case WXK_NUMPAD2:
        case WXK_NUMPAD3:
        case WXK_NUMPAD4:
        case WXK_NUMPAD5:
        case WXK_NUMPAD6:
        case WXK_NUMPAD7:
        case WXK_NUMPAD8:
        case WXK_NUMPAD9:
            ch = (wxChar)(wxT('0') + keycode - WXK_NUMPAD0);
            break;

        case WXK_MULTIPLY:
        case WXK_NUMPAD_MULTIPLY:
            ch = wxT('*');
            break;

        case WXK_ADD:
        case WXK_NUMPAD_ADD:
            ch = wxT('+');
            break;

        case WXK_SUBTRACT:
        case WXK_NUMPAD_SUBTRACT:
            ch = wxT('-');
            break;

        case WXK_DECIMAL:
        case WXK_NUMPAD_DECIMAL:
            ch = wxT('.');
            break;

        case WXK_DIVIDE:
        case WXK_NUMPAD_DIVIDE:
            ch = wxT('/');
            break;

        case WXK_DELETE:
        case WXK_NUMPAD_DELETE:
            // delete the character at cursor
            {
                const long pos = GetInsertionPoint();
                if ( pos < GetLastPosition() )
                    Remove(pos, pos + 1);
                handled = true;
            }
            break;

        case WXK_BACK:
            // delete the character before the cursor
            {
                const long pos = GetInsertionPoint();
                if ( pos > 0 )
                    Remove(pos - 1, pos);
                handled = true;
            }
            break;

        default:
#if wxUSE_UNICODE
            if ( event.GetUnicodeKey() )
            {
                ch = event.GetUnicodeKey();
            }
            else
#endif
            if ( keycode < 256 && keycode >= 0 && wxIsprint(keycode) )
            {
                // FIXME this is not going to work for non letters...
                if ( !event.ShiftDown() )
                {
                    keycode = wxTolower(keycode);
                }

                ch = (wxChar)keycode;
            }
            else
            {
                ch = wxT('\0');
            }
    }

    if ( ch )
    {
        WriteText(ch);

        handled = true;
    }
#else // __WIN32__
    wxUnusedVar(event);
#endif // !__WIN32__/__WIN32__

    return handled;
}

// ----------------------------------------------------------------------------
// Other miscellaneous stuff
// ----------------------------------------------------------------------------

bool wxTextCtrlBase::SetHint(const wxString& hint)
{
    wxCHECK_MSG( IsSingleLine(), false,
                 wxS("Hints can only be set for single line text controls") );

    return wxTextEntry::SetHint(hint);
}

// do the window-specific processing after processing the update event
void wxTextCtrlBase::DoUpdateWindowUI(wxUpdateUIEvent& event)
{
    // call inherited, but skip the wxControl's version, and call directly the
    // wxWindow's one instead, because the only reason why we are overriding this
    // function is that we want to use SetValue() instead of wxControl::SetLabel()
    wxWindowBase::DoUpdateWindowUI(event);

    // update text
    if ( event.GetSetText() )
    {
        if ( event.GetText() != GetValue() )
            SetValue(event.GetText());
    }
}

// ----------------------------------------------------------------------------
// hit testing
// ----------------------------------------------------------------------------

wxTextCtrlHitTestResult
wxTextAreaBase::HitTest(const wxPoint& pt, wxTextCoord *x, wxTextCoord *y) const
{
    // implement in terms of the other overload as the native ports typically
    // can get the position and not (x, y) pair directly (although wxUniv
    // directly gets x and y -- and so overrides this method as well)
    long pos;
    wxTextCtrlHitTestResult rc = HitTest(pt, &pos);

    if ( rc != wxTE_HT_UNKNOWN )
    {
        PositionToXY(pos, x, y);
    }

    return rc;
}

wxTextCtrlHitTestResult
wxTextAreaBase::HitTest(const wxPoint& WXUNUSED(pt), long * WXUNUSED(pos)) const
{
    // not implemented
    return wxTE_HT_UNKNOWN;
}

wxPoint wxTextAreaBase::PositionToCoords(long pos) const
{
    wxCHECK_MSG( IsValidPosition(pos), wxDefaultPosition,
                 wxS("Position argument out of range.") );

    return DoPositionToCoords(pos);
}

wxPoint wxTextAreaBase::DoPositionToCoords(long WXUNUSED(pos)) const
{
    return wxDefaultPosition;
}

#else // !wxUSE_TEXTCTRL

// define this one even if !wxUSE_TEXTCTRL because it is also used by other
// controls (wxComboBox and wxSpinCtrl)

wxDEFINE_EVENT( wxEVT_TEXT, wxCommandEvent );

#endif // wxUSE_TEXTCTRL/!wxUSE_TEXTCTRL
