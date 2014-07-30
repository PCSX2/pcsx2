///////////////////////////////////////////////////////////////////////////////
// Name:        wx/textctrl.h
// Purpose:     wxTextAttr and wxTextCtrlBase class - the interface of wxTextCtrl
// Author:      Vadim Zeitlin
// Modified by:
// Created:     13.07.99
// Copyright:   (c) Vadim Zeitlin
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TEXTCTRL_H_BASE_
#define _WX_TEXTCTRL_H_BASE_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/defs.h"

#if wxUSE_TEXTCTRL

#include "wx/control.h"         // the base class
#include "wx/textentry.h"       // single-line text entry interface
#include "wx/dynarray.h"        // wxArrayInt
#include "wx/gdicmn.h"          // wxPoint

// some compilers don't have standard compliant rdbuf() (and MSVC has it only
// in its new iostream library, not in the old one used with iostream.h)
#if defined(__WATCOMC__) || \
    ((defined(__VISUALC5__) || defined(__VISUALC6__)) && wxUSE_IOSTREAMH)
    #define wxHAS_TEXT_WINDOW_STREAM 0
#elif wxUSE_STD_IOSTREAM
    #include "wx/ioswrap.h"
    #define wxHAS_TEXT_WINDOW_STREAM 1
#else
    #define wxHAS_TEXT_WINDOW_STREAM 0
#endif

class WXDLLIMPEXP_FWD_CORE wxTextCtrl;
class WXDLLIMPEXP_FWD_CORE wxTextCtrlBase;

// ----------------------------------------------------------------------------
// wxTextCtrl types
// ----------------------------------------------------------------------------

// wxTextCoord is the line or row number (which should have been unsigned but
// is long for backwards compatibility)
typedef long wxTextCoord;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

extern WXDLLIMPEXP_DATA_CORE(const char) wxTextCtrlNameStr[];

// this is intentionally not enum to avoid warning fixes with
// typecasting from enum type to wxTextCoord
const wxTextCoord wxOutOfRangeTextCoord = -1;
const wxTextCoord wxInvalidTextCoord    = -2;

// ----------------------------------------------------------------------------
// wxTextCtrl style flags
// ----------------------------------------------------------------------------

#define wxTE_NO_VSCROLL     0x0002

#define wxTE_READONLY       0x0010
#define wxTE_MULTILINE      0x0020
#define wxTE_PROCESS_TAB    0x0040

// alignment flags
#define wxTE_LEFT           0x0000                    // 0x0000
#define wxTE_CENTER         wxALIGN_CENTER_HORIZONTAL // 0x0100
#define wxTE_RIGHT          wxALIGN_RIGHT             // 0x0200
#define wxTE_CENTRE         wxTE_CENTER

// this style means to use RICHEDIT control and does something only under wxMSW
// and Win32 and is silently ignored under all other platforms
#define wxTE_RICH           0x0080

#define wxTE_PROCESS_ENTER  0x0400
#define wxTE_PASSWORD       0x0800

// automatically detect the URLs and generate the events when mouse is
// moved/clicked over an URL
//
// this is for Win32 richedit and wxGTK2 multiline controls only so far
#define wxTE_AUTO_URL       0x1000

// by default, the Windows text control doesn't show the selection when it
// doesn't have focus - use this style to force it to always show it
#define wxTE_NOHIDESEL      0x2000

// use wxHSCROLL to not wrap text at all, wxTE_CHARWRAP to wrap it at any
// position and wxTE_WORDWRAP to wrap at words boundary
//
// if no wrapping style is given at all, the control wraps at word boundary
#define wxTE_DONTWRAP       wxHSCROLL
#define wxTE_CHARWRAP       0x4000  // wrap at any position
#define wxTE_WORDWRAP       0x0001  // wrap only at words boundaries
#define wxTE_BESTWRAP       0x0000  // this is the default

#if WXWIN_COMPATIBILITY_2_6
    // obsolete synonym
    #define wxTE_LINEWRAP       wxTE_CHARWRAP
#endif // WXWIN_COMPATIBILITY_2_6

#if WXWIN_COMPATIBILITY_2_8
    // this style is (or at least should be) on by default now, don't use it
    #define wxTE_AUTO_SCROLL    0
#endif // WXWIN_COMPATIBILITY_2_8

// force using RichEdit version 2.0 or 3.0 instead of 1.0 (default) for
// wxTE_RICH controls - can be used together with or instead of wxTE_RICH
#define wxTE_RICH2          0x8000

// reuse wxTE_RICH2's value for CAPEDIT control on Windows CE
#if defined(__SMARTPHONE__) || defined(__POCKETPC__)
#define wxTE_CAPITALIZE     wxTE_RICH2
#else
#define wxTE_CAPITALIZE     0
#endif

// ----------------------------------------------------------------------------
// wxTextCtrl file types
// ----------------------------------------------------------------------------

#define wxTEXT_TYPE_ANY     0

// ----------------------------------------------------------------------------
// wxTextCtrl::HitTest return values
// ----------------------------------------------------------------------------

// the point asked is ...
enum wxTextCtrlHitTestResult
{
    wxTE_HT_UNKNOWN = -2,   // this means HitTest() is simply not implemented
    wxTE_HT_BEFORE,         // either to the left or upper
    wxTE_HT_ON_TEXT,        // directly on
    wxTE_HT_BELOW,          // below [the last line]
    wxTE_HT_BEYOND          // after [the end of line]
};
// ... the character returned

// ----------------------------------------------------------------------------
// Types for wxTextAttr
// ----------------------------------------------------------------------------

// Alignment

enum wxTextAttrAlignment
{
    wxTEXT_ALIGNMENT_DEFAULT,
    wxTEXT_ALIGNMENT_LEFT,
    wxTEXT_ALIGNMENT_CENTRE,
    wxTEXT_ALIGNMENT_CENTER = wxTEXT_ALIGNMENT_CENTRE,
    wxTEXT_ALIGNMENT_RIGHT,
    wxTEXT_ALIGNMENT_JUSTIFIED
};

// Flags to indicate which attributes are being applied
enum wxTextAttrFlags
{
    wxTEXT_ATTR_TEXT_COLOUR          = 0x00000001,
    wxTEXT_ATTR_BACKGROUND_COLOUR    = 0x00000002,

    wxTEXT_ATTR_FONT_FACE            = 0x00000004,
    wxTEXT_ATTR_FONT_POINT_SIZE      = 0x00000008,
    wxTEXT_ATTR_FONT_PIXEL_SIZE      = 0x10000000,
    wxTEXT_ATTR_FONT_WEIGHT          = 0x00000010,
    wxTEXT_ATTR_FONT_ITALIC          = 0x00000020,
    wxTEXT_ATTR_FONT_UNDERLINE       = 0x00000040,
    wxTEXT_ATTR_FONT_STRIKETHROUGH   = 0x08000000,
    wxTEXT_ATTR_FONT_ENCODING        = 0x02000000,
    wxTEXT_ATTR_FONT_FAMILY          = 0x04000000,
    wxTEXT_ATTR_FONT_SIZE = \
        ( wxTEXT_ATTR_FONT_POINT_SIZE | wxTEXT_ATTR_FONT_PIXEL_SIZE ),
    wxTEXT_ATTR_FONT = \
        ( wxTEXT_ATTR_FONT_FACE | wxTEXT_ATTR_FONT_SIZE | wxTEXT_ATTR_FONT_WEIGHT | \
            wxTEXT_ATTR_FONT_ITALIC | wxTEXT_ATTR_FONT_UNDERLINE | wxTEXT_ATTR_FONT_STRIKETHROUGH | wxTEXT_ATTR_FONT_ENCODING | wxTEXT_ATTR_FONT_FAMILY ),

    wxTEXT_ATTR_ALIGNMENT            = 0x00000080,
    wxTEXT_ATTR_LEFT_INDENT          = 0x00000100,
    wxTEXT_ATTR_RIGHT_INDENT         = 0x00000200,
    wxTEXT_ATTR_TABS                 = 0x00000400,
    wxTEXT_ATTR_PARA_SPACING_AFTER   = 0x00000800,
    wxTEXT_ATTR_PARA_SPACING_BEFORE  = 0x00001000,
    wxTEXT_ATTR_LINE_SPACING         = 0x00002000,
    wxTEXT_ATTR_CHARACTER_STYLE_NAME = 0x00004000,
    wxTEXT_ATTR_PARAGRAPH_STYLE_NAME = 0x00008000,
    wxTEXT_ATTR_LIST_STYLE_NAME      = 0x00010000,

    wxTEXT_ATTR_BULLET_STYLE         = 0x00020000,
    wxTEXT_ATTR_BULLET_NUMBER        = 0x00040000,
    wxTEXT_ATTR_BULLET_TEXT          = 0x00080000,
    wxTEXT_ATTR_BULLET_NAME          = 0x00100000,

    wxTEXT_ATTR_BULLET = \
        ( wxTEXT_ATTR_BULLET_STYLE | wxTEXT_ATTR_BULLET_NUMBER | wxTEXT_ATTR_BULLET_TEXT | \
          wxTEXT_ATTR_BULLET_NAME ),


    wxTEXT_ATTR_URL                  = 0x00200000,
    wxTEXT_ATTR_PAGE_BREAK           = 0x00400000,
    wxTEXT_ATTR_EFFECTS              = 0x00800000,
    wxTEXT_ATTR_OUTLINE_LEVEL        = 0x01000000,

    /*!
    * Character and paragraph combined styles
    */

    wxTEXT_ATTR_CHARACTER = \
        (wxTEXT_ATTR_FONT|wxTEXT_ATTR_EFFECTS| \
            wxTEXT_ATTR_BACKGROUND_COLOUR|wxTEXT_ATTR_TEXT_COLOUR|wxTEXT_ATTR_CHARACTER_STYLE_NAME|wxTEXT_ATTR_URL),

    wxTEXT_ATTR_PARAGRAPH = \
        (wxTEXT_ATTR_ALIGNMENT|wxTEXT_ATTR_LEFT_INDENT|wxTEXT_ATTR_RIGHT_INDENT|wxTEXT_ATTR_TABS|\
            wxTEXT_ATTR_PARA_SPACING_BEFORE|wxTEXT_ATTR_PARA_SPACING_AFTER|wxTEXT_ATTR_LINE_SPACING|\
            wxTEXT_ATTR_BULLET|wxTEXT_ATTR_PARAGRAPH_STYLE_NAME|wxTEXT_ATTR_LIST_STYLE_NAME|wxTEXT_ATTR_OUTLINE_LEVEL|wxTEXT_ATTR_PAGE_BREAK),

    wxTEXT_ATTR_ALL = (wxTEXT_ATTR_CHARACTER|wxTEXT_ATTR_PARAGRAPH)
};

/*!
 * Styles for wxTextAttr::SetBulletStyle
 */
enum wxTextAttrBulletStyle
{
    wxTEXT_ATTR_BULLET_STYLE_NONE            = 0x00000000,
    wxTEXT_ATTR_BULLET_STYLE_ARABIC          = 0x00000001,
    wxTEXT_ATTR_BULLET_STYLE_LETTERS_UPPER   = 0x00000002,
    wxTEXT_ATTR_BULLET_STYLE_LETTERS_LOWER   = 0x00000004,
    wxTEXT_ATTR_BULLET_STYLE_ROMAN_UPPER     = 0x00000008,
    wxTEXT_ATTR_BULLET_STYLE_ROMAN_LOWER     = 0x00000010,
    wxTEXT_ATTR_BULLET_STYLE_SYMBOL          = 0x00000020,
    wxTEXT_ATTR_BULLET_STYLE_BITMAP          = 0x00000040,
    wxTEXT_ATTR_BULLET_STYLE_PARENTHESES     = 0x00000080,
    wxTEXT_ATTR_BULLET_STYLE_PERIOD          = 0x00000100,
    wxTEXT_ATTR_BULLET_STYLE_STANDARD        = 0x00000200,
    wxTEXT_ATTR_BULLET_STYLE_RIGHT_PARENTHESIS = 0x00000400,
    wxTEXT_ATTR_BULLET_STYLE_OUTLINE         = 0x00000800,

    wxTEXT_ATTR_BULLET_STYLE_ALIGN_LEFT      = 0x00000000,
    wxTEXT_ATTR_BULLET_STYLE_ALIGN_RIGHT     = 0x00001000,
    wxTEXT_ATTR_BULLET_STYLE_ALIGN_CENTRE    = 0x00002000,

    wxTEXT_ATTR_BULLET_STYLE_CONTINUATION    = 0x00004000
};

/*!
 * Styles for wxTextAttr::SetTextEffects
 */
enum wxTextAttrEffects
{
    wxTEXT_ATTR_EFFECT_NONE                  = 0x00000000,
    wxTEXT_ATTR_EFFECT_CAPITALS              = 0x00000001,
    wxTEXT_ATTR_EFFECT_SMALL_CAPITALS        = 0x00000002,
    wxTEXT_ATTR_EFFECT_STRIKETHROUGH         = 0x00000004,
    wxTEXT_ATTR_EFFECT_DOUBLE_STRIKETHROUGH  = 0x00000008,
    wxTEXT_ATTR_EFFECT_SHADOW                = 0x00000010,
    wxTEXT_ATTR_EFFECT_EMBOSS                = 0x00000020,
    wxTEXT_ATTR_EFFECT_OUTLINE               = 0x00000040,
    wxTEXT_ATTR_EFFECT_ENGRAVE               = 0x00000080,
    wxTEXT_ATTR_EFFECT_SUPERSCRIPT           = 0x00000100,
    wxTEXT_ATTR_EFFECT_SUBSCRIPT             = 0x00000200
};

/*!
 * Line spacing values
 */
enum wxTextAttrLineSpacing
{
    wxTEXT_ATTR_LINE_SPACING_NORMAL         = 10,
    wxTEXT_ATTR_LINE_SPACING_HALF           = 15,
    wxTEXT_ATTR_LINE_SPACING_TWICE          = 20
};

// ----------------------------------------------------------------------------
// wxTextAttr: a structure containing the visual attributes of a text
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxTextAttr
{
public:
    // ctors
    wxTextAttr() { Init(); }
    wxTextAttr(const wxTextAttr& attr) { Init(); Copy(attr); }
    wxTextAttr(const wxColour& colText,
               const wxColour& colBack = wxNullColour,
               const wxFont& font = wxNullFont,
               wxTextAttrAlignment alignment = wxTEXT_ALIGNMENT_DEFAULT);

    // Initialise this object.
    void Init();

    // Copy
    void Copy(const wxTextAttr& attr);

    // Assignment
    void operator= (const wxTextAttr& attr);

    // Equality test
    bool operator== (const wxTextAttr& attr) const;

    // Partial equality test.  If @a weakTest is @true, attributes of this object do not
    // have to be present if those attributes of @a attr are present. If @a weakTest is
    // @false, the function will fail if an attribute is present in @a attr but not
    // in this object.
    bool EqPartial(const wxTextAttr& attr, bool weakTest = true) const;

    // Get attributes from font.
    bool GetFontAttributes(const wxFont& font, int flags = wxTEXT_ATTR_FONT);

    // setters
    void SetTextColour(const wxColour& colText) { m_colText = colText; m_flags |= wxTEXT_ATTR_TEXT_COLOUR; }
    void SetBackgroundColour(const wxColour& colBack) { m_colBack = colBack; m_flags |= wxTEXT_ATTR_BACKGROUND_COLOUR; }
    void SetAlignment(wxTextAttrAlignment alignment) { m_textAlignment = alignment; m_flags |= wxTEXT_ATTR_ALIGNMENT; }
    void SetTabs(const wxArrayInt& tabs) { m_tabs = tabs; m_flags |= wxTEXT_ATTR_TABS; }
    void SetLeftIndent(int indent, int subIndent = 0) { m_leftIndent = indent; m_leftSubIndent = subIndent; m_flags |= wxTEXT_ATTR_LEFT_INDENT; }
    void SetRightIndent(int indent) { m_rightIndent = indent; m_flags |= wxTEXT_ATTR_RIGHT_INDENT; }

    void SetFontSize(int pointSize) { m_fontSize = pointSize; m_flags &= ~wxTEXT_ATTR_FONT_SIZE; m_flags |= wxTEXT_ATTR_FONT_POINT_SIZE; }
    void SetFontPointSize(int pointSize) { m_fontSize = pointSize; m_flags &= ~wxTEXT_ATTR_FONT_SIZE; m_flags |= wxTEXT_ATTR_FONT_POINT_SIZE; }
    void SetFontPixelSize(int pixelSize) { m_fontSize = pixelSize; m_flags &= ~wxTEXT_ATTR_FONT_SIZE; m_flags |= wxTEXT_ATTR_FONT_PIXEL_SIZE; }
    void SetFontStyle(wxFontStyle fontStyle) { m_fontStyle = fontStyle; m_flags |= wxTEXT_ATTR_FONT_ITALIC; }
    void SetFontWeight(wxFontWeight fontWeight) { m_fontWeight = fontWeight; m_flags |= wxTEXT_ATTR_FONT_WEIGHT; }
    void SetFontFaceName(const wxString& faceName) { m_fontFaceName = faceName; m_flags |= wxTEXT_ATTR_FONT_FACE; }
    void SetFontUnderlined(bool underlined) { m_fontUnderlined = underlined; m_flags |= wxTEXT_ATTR_FONT_UNDERLINE; }
    void SetFontStrikethrough(bool strikethrough) { m_fontStrikethrough = strikethrough; m_flags |= wxTEXT_ATTR_FONT_STRIKETHROUGH; }
    void SetFontEncoding(wxFontEncoding encoding) { m_fontEncoding = encoding; m_flags |= wxTEXT_ATTR_FONT_ENCODING; }
    void SetFontFamily(wxFontFamily family) { m_fontFamily = family; m_flags |= wxTEXT_ATTR_FONT_FAMILY; }

    // Set font
    void SetFont(const wxFont& font, int flags = (wxTEXT_ATTR_FONT & ~wxTEXT_ATTR_FONT_PIXEL_SIZE)) { GetFontAttributes(font, flags); }

    void SetFlags(long flags) { m_flags = flags; }

    void SetCharacterStyleName(const wxString& name) { m_characterStyleName = name; m_flags |= wxTEXT_ATTR_CHARACTER_STYLE_NAME; }
    void SetParagraphStyleName(const wxString& name) { m_paragraphStyleName = name; m_flags |= wxTEXT_ATTR_PARAGRAPH_STYLE_NAME; }
    void SetListStyleName(const wxString& name) { m_listStyleName = name; SetFlags(GetFlags() | wxTEXT_ATTR_LIST_STYLE_NAME); }
    void SetParagraphSpacingAfter(int spacing) { m_paragraphSpacingAfter = spacing; m_flags |= wxTEXT_ATTR_PARA_SPACING_AFTER; }
    void SetParagraphSpacingBefore(int spacing) { m_paragraphSpacingBefore = spacing; m_flags |= wxTEXT_ATTR_PARA_SPACING_BEFORE; }
    void SetLineSpacing(int spacing) { m_lineSpacing = spacing; m_flags |= wxTEXT_ATTR_LINE_SPACING; }
    void SetBulletStyle(int style) { m_bulletStyle = style; m_flags |= wxTEXT_ATTR_BULLET_STYLE; }
    void SetBulletNumber(int n) { m_bulletNumber = n; m_flags |= wxTEXT_ATTR_BULLET_NUMBER; }
    void SetBulletText(const wxString& text) { m_bulletText = text; m_flags |= wxTEXT_ATTR_BULLET_TEXT; }
    void SetBulletFont(const wxString& bulletFont) { m_bulletFont = bulletFont; }
    void SetBulletName(const wxString& name) { m_bulletName = name; m_flags |= wxTEXT_ATTR_BULLET_NAME; }
    void SetURL(const wxString& url) { m_urlTarget = url; m_flags |= wxTEXT_ATTR_URL; }
    void SetPageBreak(bool pageBreak = true) { SetFlags(pageBreak ? (GetFlags() | wxTEXT_ATTR_PAGE_BREAK) : (GetFlags() & ~wxTEXT_ATTR_PAGE_BREAK)); }
    void SetTextEffects(int effects) { m_textEffects = effects; SetFlags(GetFlags() | wxTEXT_ATTR_EFFECTS); }
    void SetTextEffectFlags(int effects) { m_textEffectFlags = effects; }
    void SetOutlineLevel(int level) { m_outlineLevel = level; SetFlags(GetFlags() | wxTEXT_ATTR_OUTLINE_LEVEL); }

    const wxColour& GetTextColour() const { return m_colText; }
    const wxColour& GetBackgroundColour() const { return m_colBack; }
    wxTextAttrAlignment GetAlignment() const { return m_textAlignment; }
    const wxArrayInt& GetTabs() const { return m_tabs; }
    long GetLeftIndent() const { return m_leftIndent; }
    long GetLeftSubIndent() const { return m_leftSubIndent; }
    long GetRightIndent() const { return m_rightIndent; }
    long GetFlags() const { return m_flags; }

    int GetFontSize() const { return m_fontSize; }
    wxFontStyle GetFontStyle() const { return m_fontStyle; }
    wxFontWeight GetFontWeight() const { return m_fontWeight; }
    bool GetFontUnderlined() const { return m_fontUnderlined; }
    bool GetFontStrikethrough() const { return m_fontStrikethrough; }
    const wxString& GetFontFaceName() const { return m_fontFaceName; }
    wxFontEncoding GetFontEncoding() const { return m_fontEncoding; }
    wxFontFamily GetFontFamily() const { return m_fontFamily; }

    wxFont GetFont() const;

    const wxString& GetCharacterStyleName() const { return m_characterStyleName; }
    const wxString& GetParagraphStyleName() const { return m_paragraphStyleName; }
    const wxString& GetListStyleName() const { return m_listStyleName; }
    int GetParagraphSpacingAfter() const { return m_paragraphSpacingAfter; }
    int GetParagraphSpacingBefore() const { return m_paragraphSpacingBefore; }

    int GetLineSpacing() const { return m_lineSpacing; }
    int GetBulletStyle() const { return m_bulletStyle; }
    int GetBulletNumber() const { return m_bulletNumber; }
    const wxString& GetBulletText() const { return m_bulletText; }
    const wxString& GetBulletFont() const { return m_bulletFont; }
    const wxString& GetBulletName() const { return m_bulletName; }
    const wxString& GetURL() const { return m_urlTarget; }
    int GetTextEffects() const { return m_textEffects; }
    int GetTextEffectFlags() const { return m_textEffectFlags; }
    int GetOutlineLevel() const { return m_outlineLevel; }

    // accessors
    bool HasTextColour() const { return m_colText.IsOk() && HasFlag(wxTEXT_ATTR_TEXT_COLOUR) ; }
    bool HasBackgroundColour() const { return m_colBack.IsOk() && HasFlag(wxTEXT_ATTR_BACKGROUND_COLOUR) ; }
    bool HasAlignment() const { return (m_textAlignment != wxTEXT_ALIGNMENT_DEFAULT) && HasFlag(wxTEXT_ATTR_ALIGNMENT) ; }
    bool HasTabs() const { return HasFlag(wxTEXT_ATTR_TABS) ; }
    bool HasLeftIndent() const { return HasFlag(wxTEXT_ATTR_LEFT_INDENT); }
    bool HasRightIndent() const { return HasFlag(wxTEXT_ATTR_RIGHT_INDENT); }
    bool HasFontWeight() const { return HasFlag(wxTEXT_ATTR_FONT_WEIGHT); }
    bool HasFontSize() const { return HasFlag(wxTEXT_ATTR_FONT_SIZE); }
    bool HasFontPointSize() const { return HasFlag(wxTEXT_ATTR_FONT_POINT_SIZE); }
    bool HasFontPixelSize() const { return HasFlag(wxTEXT_ATTR_FONT_PIXEL_SIZE); }
    bool HasFontItalic() const { return HasFlag(wxTEXT_ATTR_FONT_ITALIC); }
    bool HasFontUnderlined() const { return HasFlag(wxTEXT_ATTR_FONT_UNDERLINE); }
    bool HasFontStrikethrough() const { return HasFlag(wxTEXT_ATTR_FONT_STRIKETHROUGH); }
    bool HasFontFaceName() const { return HasFlag(wxTEXT_ATTR_FONT_FACE); }
    bool HasFontEncoding() const { return HasFlag(wxTEXT_ATTR_FONT_ENCODING); }
    bool HasFontFamily() const { return HasFlag(wxTEXT_ATTR_FONT_FAMILY); }
    bool HasFont() const { return HasFlag(wxTEXT_ATTR_FONT); }

    bool HasParagraphSpacingAfter() const { return HasFlag(wxTEXT_ATTR_PARA_SPACING_AFTER); }
    bool HasParagraphSpacingBefore() const { return HasFlag(wxTEXT_ATTR_PARA_SPACING_BEFORE); }
    bool HasLineSpacing() const { return HasFlag(wxTEXT_ATTR_LINE_SPACING); }
    bool HasCharacterStyleName() const { return HasFlag(wxTEXT_ATTR_CHARACTER_STYLE_NAME) && !m_characterStyleName.IsEmpty(); }
    bool HasParagraphStyleName() const { return HasFlag(wxTEXT_ATTR_PARAGRAPH_STYLE_NAME) && !m_paragraphStyleName.IsEmpty(); }
    bool HasListStyleName() const { return HasFlag(wxTEXT_ATTR_LIST_STYLE_NAME) || !m_listStyleName.IsEmpty(); }
    bool HasBulletStyle() const { return HasFlag(wxTEXT_ATTR_BULLET_STYLE); }
    bool HasBulletNumber() const { return HasFlag(wxTEXT_ATTR_BULLET_NUMBER); }
    bool HasBulletText() const { return HasFlag(wxTEXT_ATTR_BULLET_TEXT); }
    bool HasBulletName() const { return HasFlag(wxTEXT_ATTR_BULLET_NAME); }
    bool HasURL() const { return HasFlag(wxTEXT_ATTR_URL); }
    bool HasPageBreak() const { return HasFlag(wxTEXT_ATTR_PAGE_BREAK); }
    bool HasTextEffects() const { return HasFlag(wxTEXT_ATTR_EFFECTS); }
    bool HasTextEffect(int effect) const { return HasFlag(wxTEXT_ATTR_EFFECTS) && ((GetTextEffectFlags() & effect) != 0); }
    bool HasOutlineLevel() const { return HasFlag(wxTEXT_ATTR_OUTLINE_LEVEL); }

    bool HasFlag(long flag) const { return (m_flags & flag) != 0; }
    void RemoveFlag(long flag) { m_flags &= ~flag; }
    void AddFlag(long flag) { m_flags |= flag; }

    // Is this a character style?
    bool IsCharacterStyle() const { return HasFlag(wxTEXT_ATTR_CHARACTER); }
    bool IsParagraphStyle() const { return HasFlag(wxTEXT_ATTR_PARAGRAPH); }

    // returns false if we have any attributes set, true otherwise
    bool IsDefault() const
    {
        return GetFlags() == 0;
    }

    // Merges the given attributes. If compareWith
    // is non-NULL, then it will be used to mask out those attributes that are the same in style
    // and compareWith, for situations where we don't want to explicitly set inherited attributes.
    bool Apply(const wxTextAttr& style, const wxTextAttr* compareWith = NULL);

    // merges the attributes of the base and the overlay objects and returns
    // the result; the parameter attributes take precedence
    //
    // WARNING: the order of arguments is the opposite of Combine()
    static wxTextAttr Merge(const wxTextAttr& base, const wxTextAttr& overlay)
    {
        return Combine(overlay, base, NULL);
    }

    // merges the attributes of this object and overlay
    void Merge(const wxTextAttr& overlay)
    {
        *this = Merge(*this, overlay);
    }

    // return the attribute having the valid font and colours: it uses the
    // attributes set in attr and falls back first to attrDefault and then to
    // the text control font/colours for those attributes which are not set
    static wxTextAttr Combine(const wxTextAttr& attr,
                              const wxTextAttr& attrDef,
                              const wxTextCtrlBase *text);

    // Compare tabs
    static bool TabsEq(const wxArrayInt& tabs1, const wxArrayInt& tabs2);

    // Remove attributes
    static bool RemoveStyle(wxTextAttr& destStyle, const wxTextAttr& style);

    // Combine two bitlists, specifying the bits of interest with separate flags.
    static bool CombineBitlists(int& valueA, int valueB, int& flagsA, int flagsB);

    // Compare two bitlists
    static bool BitlistsEqPartial(int valueA, int valueB, int flags);

    // Split into paragraph and character styles
    static bool SplitParaCharStyles(const wxTextAttr& style, wxTextAttr& parStyle, wxTextAttr& charStyle);

private:
    long                m_flags;

    // Paragraph styles
    wxArrayInt          m_tabs; // array of int: tab stops in 1/10 mm
    int                 m_leftIndent; // left indent in 1/10 mm
    int                 m_leftSubIndent; // left indent for all but the first
                                         // line in a paragraph relative to the
                                         // first line, in 1/10 mm
    int                 m_rightIndent; // right indent in 1/10 mm
    wxTextAttrAlignment m_textAlignment;

    int                 m_paragraphSpacingAfter;
    int                 m_paragraphSpacingBefore;
    int                 m_lineSpacing;
    int                 m_bulletStyle;
    int                 m_bulletNumber;
    int                 m_textEffects;
    int                 m_textEffectFlags;
    int                 m_outlineLevel;
    wxString            m_bulletText;
    wxString            m_bulletFont;
    wxString            m_bulletName;
    wxString            m_urlTarget;
    wxFontEncoding      m_fontEncoding;

    // Character styles
    wxColour            m_colText,
                        m_colBack;
    int                 m_fontSize;
    wxFontStyle         m_fontStyle;
    wxFontWeight        m_fontWeight;
    wxFontFamily        m_fontFamily;
    bool                m_fontUnderlined;
    bool                m_fontStrikethrough;
    wxString            m_fontFaceName;

    // Character style
    wxString            m_characterStyleName;

    // Paragraph style
    wxString            m_paragraphStyleName;

    // List style
    wxString            m_listStyleName;
};

// ----------------------------------------------------------------------------
// wxTextAreaBase: multiline text control specific methods
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxTextAreaBase
{
public:
    wxTextAreaBase() { }
    virtual ~wxTextAreaBase() { }

    // lines access
    // ------------

    virtual int GetLineLength(long lineNo) const = 0;
    virtual wxString GetLineText(long lineNo) const = 0;
    virtual int GetNumberOfLines() const = 0;


    // file IO
    // -------

    bool LoadFile(const wxString& file, int fileType = wxTEXT_TYPE_ANY)
        { return DoLoadFile(file, fileType); }
    bool SaveFile(const wxString& file = wxEmptyString,
                  int fileType = wxTEXT_TYPE_ANY);

    // dirty flag handling
    // -------------------

    virtual bool IsModified() const = 0;
    virtual void MarkDirty() = 0;
    virtual void DiscardEdits() = 0;
    void SetModified(bool modified)
    {
        if ( modified )
            MarkDirty();
        else
            DiscardEdits();
    }


    // styles handling
    // ---------------

    // text control under some platforms supports the text styles: these
    // methods allow to apply the given text style to the given selection or to
    // set/get the style which will be used for all appended text
    virtual bool SetStyle(long start, long end, const wxTextAttr& style) = 0;
    virtual bool GetStyle(long position, wxTextAttr& style) = 0;
    virtual bool SetDefaultStyle(const wxTextAttr& style) = 0;
    virtual const wxTextAttr& GetDefaultStyle() const { return m_defaultStyle; }


    // coordinates translation
    // -----------------------

    // translate between the position (which is just an index in the text ctrl
    // considering all its contents as a single strings) and (x, y) coordinates
    // which represent column and line.
    virtual long XYToPosition(long x, long y) const = 0;
    virtual bool PositionToXY(long pos, long *x, long *y) const = 0;

    // translate the given position (which is just an index in the text control)
    // to client coordinates
    wxPoint PositionToCoords(long pos) const;


    virtual void ShowPosition(long pos) = 0;

    // find the character at position given in pixels
    //
    // NB: pt is in device coords (not adjusted for the client area origin nor
    //     scrolling)
    virtual wxTextCtrlHitTestResult HitTest(const wxPoint& pt, long *pos) const;
    virtual wxTextCtrlHitTestResult HitTest(const wxPoint& pt,
                                            wxTextCoord *col,
                                            wxTextCoord *row) const;
    virtual wxString GetValue() const = 0;
    virtual void SetValue(const wxString& value) = 0;

protected:
    // implementation of loading/saving
    virtual bool DoLoadFile(const wxString& file, int fileType);
    virtual bool DoSaveFile(const wxString& file, int fileType);

    // Return true if the given position is valid, i.e. positive and less than
    // the last position.
    virtual bool IsValidPosition(long pos) const = 0;

    // Default stub implementation of PositionToCoords() always returns
    // wxDefaultPosition.
    virtual wxPoint DoPositionToCoords(long pos) const;

    // the name of the last file loaded with LoadFile() which will be used by
    // SaveFile() by default
    wxString m_filename;

    // the text style which will be used for any new text added to the control
    wxTextAttr m_defaultStyle;


    wxDECLARE_NO_COPY_CLASS(wxTextAreaBase);
};

// this class defines wxTextCtrl interface, wxTextCtrlBase actually implements
// too much things because it derives from wxTextEntry and not wxTextEntryBase
// and so any classes which "look like" wxTextCtrl (such as wxRichTextCtrl)
// but don't need the (native) implementation bits from wxTextEntry should
// actually derive from this one and not wxTextCtrlBase
class WXDLLIMPEXP_CORE wxTextCtrlIface : public wxTextAreaBase,
                                         public wxTextEntryBase
{
public:
    wxTextCtrlIface() { }

    // wxTextAreaBase overrides
    virtual wxString GetValue() const
    {
       return wxTextEntryBase::GetValue();
    }
    virtual void SetValue(const wxString& value)
    {
       wxTextEntryBase::SetValue(value);
    }

protected:
    virtual bool IsValidPosition(long pos) const
    {
        return pos >= 0 && pos <= GetLastPosition();
    }

private:
    wxDECLARE_NO_COPY_CLASS(wxTextCtrlIface);
};

// ----------------------------------------------------------------------------
// wxTextCtrl: a single or multiple line text zone where user can edit text
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxTextCtrlBase : public wxControl,
#if wxHAS_TEXT_WINDOW_STREAM
                                   public wxSTD streambuf,
#endif
                                   public wxTextAreaBase,
                                   public wxTextEntry
{
public:
    // creation
    // --------

    wxTextCtrlBase() { }
    virtual ~wxTextCtrlBase() { }


    // more readable flag testing methods
    bool IsSingleLine() const { return !HasFlag(wxTE_MULTILINE); }
    bool IsMultiLine() const { return !IsSingleLine(); }

    // stream-like insertion operators: these are always available, whether we
    // were, or not, compiled with streambuf support
    wxTextCtrl& operator<<(const wxString& s);
    wxTextCtrl& operator<<(int i);
    wxTextCtrl& operator<<(long i);
    wxTextCtrl& operator<<(float f) { return *this << double(f); }
    wxTextCtrl& operator<<(double d);
    wxTextCtrl& operator<<(char c) { return *this << wxString(c); }
    wxTextCtrl& operator<<(wchar_t c) { return *this << wxString(c); }

    // insert the character which would have resulted from this key event,
    // return true if anything has been inserted
    virtual bool EmulateKeyPress(const wxKeyEvent& event);


    // do the window-specific processing after processing the update event
    virtual void DoUpdateWindowUI(wxUpdateUIEvent& event);

    virtual bool ShouldInheritColours() const { return false; }

    // work around the problem with having HitTest() both in wxControl and
    // wxTextAreaBase base classes
    virtual wxTextCtrlHitTestResult HitTest(const wxPoint& pt, long *pos) const
    {
        return wxTextAreaBase::HitTest(pt, pos);
    }

    virtual wxTextCtrlHitTestResult HitTest(const wxPoint& pt,
                                            wxTextCoord *col,
                                            wxTextCoord *row) const
    {
        return wxTextAreaBase::HitTest(pt, col, row);
    }

    // we provide stubs for these functions as not all platforms have styles
    // support, but we really should leave them pure virtual here
    virtual bool SetStyle(long start, long end, const wxTextAttr& style);
    virtual bool GetStyle(long position, wxTextAttr& style);
    virtual bool SetDefaultStyle(const wxTextAttr& style);

    // wxTextAreaBase overrides
    virtual wxString GetValue() const
    {
       return wxTextEntry::GetValue();
    }
    virtual void SetValue(const wxString& value)
    {
       wxTextEntry::SetValue(value);
    }

    // wxTextEntry overrides
    virtual bool SetHint(const wxString& hint);

    // wxWindow overrides
    virtual wxVisualAttributes GetDefaultAttributes() const
    {
        return GetClassDefaultAttributes(GetWindowVariant());
    }

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL)
    {
        return GetCompositeControlsDefaultAttributes(variant);
    }

protected:
    // override streambuf method
#if wxHAS_TEXT_WINDOW_STREAM
    int overflow(int i);
#endif // wxHAS_TEXT_WINDOW_STREAM

    // Another wxTextAreaBase override.
    virtual bool IsValidPosition(long pos) const
    {
        return pos >= 0 && pos <= GetLastPosition();
    }

    // implement the wxTextEntry pure virtual method
    virtual wxWindow *GetEditableWindow() { return this; }

    wxDECLARE_NO_COPY_CLASS(wxTextCtrlBase);
    DECLARE_ABSTRACT_CLASS(wxTextCtrlBase)
};

// ----------------------------------------------------------------------------
// include the platform-dependent class definition
// ----------------------------------------------------------------------------

#if defined(__WXX11__)
    #include "wx/x11/textctrl.h"
#elif defined(__WXUNIVERSAL__)
    #include "wx/univ/textctrl.h"
#elif defined(__SMARTPHONE__) && defined(__WXWINCE__)
    #include "wx/msw/wince/textctrlce.h"
#elif defined(__WXMSW__)
    #include "wx/msw/textctrl.h"
#elif defined(__WXMOTIF__)
    #include "wx/motif/textctrl.h"
#elif defined(__WXGTK20__)
    #include "wx/gtk/textctrl.h"
#elif defined(__WXGTK__)
    #include "wx/gtk1/textctrl.h"
#elif defined(__WXMAC__)
    #include "wx/osx/textctrl.h"
#elif defined(__WXCOCOA__)
    #include "wx/cocoa/textctrl.h"
#elif defined(__WXPM__)
    #include "wx/os2/textctrl.h"
#endif

// ----------------------------------------------------------------------------
// wxTextCtrl events
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_CORE wxTextUrlEvent;

wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_TEXT, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_TEXT_ENTER, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_TEXT_URL, wxTextUrlEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_TEXT_MAXLEN, wxCommandEvent);

class WXDLLIMPEXP_CORE wxTextUrlEvent : public wxCommandEvent
{
public:
    wxTextUrlEvent(int winid, const wxMouseEvent& evtMouse,
                   long start, long end)
        : wxCommandEvent(wxEVT_TEXT_URL, winid),
          m_evtMouse(evtMouse), m_start(start), m_end(end)
        { }
    wxTextUrlEvent(const wxTextUrlEvent& event)
        : wxCommandEvent(event),
          m_evtMouse(event.m_evtMouse),
          m_start(event.m_start),
          m_end(event.m_end) { }

    // get the mouse event which happened over the URL
    const wxMouseEvent& GetMouseEvent() const { return m_evtMouse; }

    // get the start of the URL
    long GetURLStart() const { return m_start; }

    // get the end of the URL
    long GetURLEnd() const { return m_end; }

    virtual wxEvent *Clone() const { return new wxTextUrlEvent(*this); }

protected:
    // the corresponding mouse event
    wxMouseEvent m_evtMouse;

    // the start and end indices of the URL in the text control
    long m_start,
         m_end;

private:
    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxTextUrlEvent)

public:
    // for wxWin RTTI only, don't use
    wxTextUrlEvent() : m_evtMouse(), m_start(0), m_end(0) { }
};

typedef void (wxEvtHandler::*wxTextUrlEventFunction)(wxTextUrlEvent&);

#define wxTextEventHandler(func) wxCommandEventHandler(func)
#define wxTextUrlEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxTextUrlEventFunction, func)

#define wx__DECLARE_TEXTEVT(evt, id, fn) \
    wx__DECLARE_EVT1(wxEVT_TEXT_ ## evt, id, wxTextEventHandler(fn))

#define wx__DECLARE_TEXTURLEVT(evt, id, fn) \
    wx__DECLARE_EVT1(wxEVT_TEXT_ ## evt, id, wxTextUrlEventHandler(fn))

#define EVT_TEXT(id, fn) wx__DECLARE_EVT1(wxEVT_TEXT, id, wxTextEventHandler(fn))
#define EVT_TEXT_ENTER(id, fn) wx__DECLARE_TEXTEVT(ENTER, id, fn)
#define EVT_TEXT_URL(id, fn) wx__DECLARE_TEXTURLEVT(URL, id, fn)
#define EVT_TEXT_MAXLEN(id, fn) wx__DECLARE_TEXTEVT(MAXLEN, id, fn)

#if wxHAS_TEXT_WINDOW_STREAM

// ----------------------------------------------------------------------------
// wxStreamToTextRedirector: this class redirects all data sent to the given
// C++ stream to the wxTextCtrl given to its ctor during its lifetime.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxStreamToTextRedirector
{
private:
    void Init(wxTextCtrl *text)
    {
        m_sbufOld = m_ostr.rdbuf();
        m_ostr.rdbuf(text);
    }

public:
    wxStreamToTextRedirector(wxTextCtrl *text)
        : m_ostr(wxSTD cout)
    {
        Init(text);
    }

    wxStreamToTextRedirector(wxTextCtrl *text, wxSTD ostream *ostr)
        : m_ostr(*ostr)
    {
        Init(text);
    }

    ~wxStreamToTextRedirector()
    {
        m_ostr.rdbuf(m_sbufOld);
    }

private:
    // the stream we're redirecting
    wxSTD ostream&   m_ostr;

    // the old streambuf (before we changed it)
    wxSTD streambuf *m_sbufOld;
};

#endif // wxHAS_TEXT_WINDOW_STREAM

// old wxEVT_COMMAND_* constants
#define wxEVT_COMMAND_TEXT_UPDATED   wxEVT_TEXT
#define wxEVT_COMMAND_TEXT_ENTER     wxEVT_TEXT_ENTER
#define wxEVT_COMMAND_TEXT_URL       wxEVT_TEXT_URL
#define wxEVT_COMMAND_TEXT_MAXLEN    wxEVT_TEXT_MAXLEN

#endif // wxUSE_TEXTCTRL

#endif
    // _WX_TEXTCTRL_H_BASE_
