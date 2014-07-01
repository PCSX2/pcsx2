/////////////////////////////////////////////////////////////////////////////
// Name:        wx/richtext/richtextbuffer.h
// Purpose:     Buffer for wxRichTextCtrl
// Author:      Julian Smart
// Modified by:
// Created:     2005-09-30
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_RICHTEXTBUFFER_H_
#define _WX_RICHTEXTBUFFER_H_

/*

  Data structures
  ===============

  Data is represented by a hierarchy of objects, all derived from
  wxRichTextObject.

  The top of the hierarchy is the buffer, a kind of wxRichTextParagraphLayoutBox.
  These boxes will allow flexible placement of text boxes on a page, but
  for now there is a single box representing the document, and this box is
  a wxRichTextParagraphLayoutBox which contains further wxRichTextParagraph
  objects, each of which can include text and images.

  Each object maintains a range (start and end position) measured
  from the start of the main parent box.
  A paragraph object knows its range, and a text fragment knows its range
  too. So, a character or image in a page has a position relative to the
  start of the document, and a character in an embedded text box has
  a position relative to that text box. For now, we will not be dealing with
  embedded objects but it's something to bear in mind for later.

  Note that internally, a range (5,5) represents a range of one character.
  In the public wx[Rich]TextCtrl API, this would be passed to e.g. SetSelection
  as (5,6). A paragraph with one character might have an internal range of (0, 1)
  since the end of the paragraph takes up one position.

  Layout
  ======

  When Layout is called on an object, it is given a size which the object
  must limit itself to, or one or more flexible directions (vertical
  or horizontal). So for example a centered paragraph is given the page
  width to play with (minus any margins), but can extend indefinitely
  in the vertical direction. The implementation of Layout can then
  cache the calculated size and position within the parent.

 */

/*!
 * Includes
 */

#include "wx/defs.h"

#if wxUSE_RICHTEXT

#include "wx/list.h"
#include "wx/textctrl.h"
#include "wx/bitmap.h"
#include "wx/image.h"
#include "wx/cmdproc.h"
#include "wx/txtstrm.h"
#include "wx/variant.h"
#include "wx/position.h"

#if wxUSE_DATAOBJ
#include "wx/dataobj.h"
#endif

// Compatibility
//#define wxRichTextAttr wxTextAttr
#define wxTextAttrEx wxTextAttr

// Setting wxRICHTEXT_USE_OWN_CARET to 1 implements a
// caret reliably without using wxClientDC in case there
// are platform-specific problems with the generic caret.
#if defined(__WXGTK__) || defined(__WXMAC__)
#define wxRICHTEXT_USE_OWN_CARET 1
#else
#define wxRICHTEXT_USE_OWN_CARET 0
#endif

// Switch off for binary compatibility, on for faster drawing
// Note: this seems to be buggy (overzealous use of extents) so
// don't use for now
#define wxRICHTEXT_USE_OPTIMIZED_LINE_DRAWING 0

// The following two symbols determine whether an output implementation
// is present. To switch the relevant one on, set wxRICHTEXT_USE_XMLDOCUMENT_OUTPUT in
// richtextxml.cpp. By default, the faster direct output implementation is used.

// Include the wxXmlDocument implementation for output
#define wxRICHTEXT_HAVE_XMLDOCUMENT_OUTPUT 1

// Include the faster, direct implementation for output
#define wxRICHTEXT_HAVE_DIRECT_OUTPUT 1

/**
    The line break character that can be embedded in content.
 */

extern WXDLLIMPEXP_RICHTEXT const wxChar wxRichTextLineBreakChar;

/**
    File types in wxRichText context.
 */
enum wxRichTextFileType
{
    wxRICHTEXT_TYPE_ANY = 0,
    wxRICHTEXT_TYPE_TEXT,
    wxRICHTEXT_TYPE_XML,
    wxRICHTEXT_TYPE_HTML,
    wxRICHTEXT_TYPE_RTF,
    wxRICHTEXT_TYPE_PDF
};

/*
 * Forward declarations
 */

class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextCtrl;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextObject;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextImage;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextPlainText;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextCacheObject;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextObjectList;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextLine;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextParagraph;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextFileHandler;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextDrawingHandler;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextField;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextFieldType;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextStyleSheet;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextListStyleDefinition;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextEvent;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextRenderer;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextBuffer;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextXMLHandler;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextParagraphLayoutBox;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextImageBlock;
class WXDLLIMPEXP_FWD_XML      wxXmlNode;
class                          wxRichTextFloatCollector;
class WXDLLIMPEXP_FWD_BASE wxDataInputStream;
class WXDLLIMPEXP_FWD_BASE wxDataOutputStream;

/**
    Flags determining the available space, passed to Layout.
 */

#define wxRICHTEXT_FIXED_WIDTH      0x01
#define wxRICHTEXT_FIXED_HEIGHT     0x02
#define wxRICHTEXT_VARIABLE_WIDTH   0x04
#define wxRICHTEXT_VARIABLE_HEIGHT  0x08

// Only lay out the part of the buffer that lies within
// the rect passed to Layout.
#define wxRICHTEXT_LAYOUT_SPECIFIED_RECT 0x10

/**
    Flags to pass to Draw
 */

// Ignore paragraph cache optimization, e.g. for printing purposes
// where one line may be drawn higher (on the next page) compared
// with the previous line
#define wxRICHTEXT_DRAW_IGNORE_CACHE    0x01
#define wxRICHTEXT_DRAW_SELECTED        0x02
#define wxRICHTEXT_DRAW_PRINT           0x04
#define wxRICHTEXT_DRAW_GUIDELINES      0x08

/**
    Flags returned from hit-testing, or passed to hit-test function.
 */
enum wxRichTextHitTestFlags
{
    // The point was not on this object
    wxRICHTEXT_HITTEST_NONE =    0x01,

    // The point was before the position returned from HitTest
    wxRICHTEXT_HITTEST_BEFORE =  0x02,

    // The point was after the position returned from HitTest
    wxRICHTEXT_HITTEST_AFTER =   0x04,

    // The point was on the position returned from HitTest
    wxRICHTEXT_HITTEST_ON =      0x08,

    // The point was on space outside content
    wxRICHTEXT_HITTEST_OUTSIDE = 0x10,

    // Only do hit-testing at the current level (don't traverse into top-level objects)
    wxRICHTEXT_HITTEST_NO_NESTED_OBJECTS = 0x20,

    // Ignore floating objects
    wxRICHTEXT_HITTEST_NO_FLOATING_OBJECTS = 0x40,

    // Don't recurse into objects marked as atomic
    wxRICHTEXT_HITTEST_HONOUR_ATOMIC = 0x80
};

/**
    Flags for GetRangeSize.
 */

#define wxRICHTEXT_FORMATTED        0x01
#define wxRICHTEXT_UNFORMATTED      0x02
#define wxRICHTEXT_CACHE_SIZE       0x04
#define wxRICHTEXT_HEIGHT_ONLY      0x08

/**
    Flags for SetStyle/SetListStyle.
 */

#define wxRICHTEXT_SETSTYLE_NONE            0x00

// Specifies that this operation should be undoable
#define wxRICHTEXT_SETSTYLE_WITH_UNDO       0x01

// Specifies that the style should not be applied if the
// combined style at this point is already the style in question.
#define wxRICHTEXT_SETSTYLE_OPTIMIZE        0x02

// Specifies that the style should only be applied to paragraphs,
// and not the content. This allows content styling to be
// preserved independently from that of e.g. a named paragraph style.
#define wxRICHTEXT_SETSTYLE_PARAGRAPHS_ONLY 0x04

// Specifies that the style should only be applied to characters,
// and not the paragraph. This allows content styling to be
// preserved independently from that of e.g. a named paragraph style.
#define wxRICHTEXT_SETSTYLE_CHARACTERS_ONLY 0x08

// For SetListStyle only: specifies starting from the given number, otherwise
// deduces number from existing attributes
#define wxRICHTEXT_SETSTYLE_RENUMBER        0x10

// For SetListStyle only: specifies the list level for all paragraphs, otherwise
// the current indentation will be used
#define wxRICHTEXT_SETSTYLE_SPECIFY_LEVEL   0x20

// Resets the existing style before applying the new style
#define wxRICHTEXT_SETSTYLE_RESET           0x40

// Removes the given style instead of applying it
#define wxRICHTEXT_SETSTYLE_REMOVE          0x80

/**
    Flags for SetProperties.
 */

#define wxRICHTEXT_SETPROPERTIES_NONE            0x00

// Specifies that this operation should be undoable
#define wxRICHTEXT_SETPROPERTIES_WITH_UNDO       0x01

// Specifies that the properties should only be applied to paragraphs,
// and not the content.
#define wxRICHTEXT_SETPROPERTIES_PARAGRAPHS_ONLY 0x02

// Specifies that the properties should only be applied to characters,
// and not the paragraph.
#define wxRICHTEXT_SETPROPERTIES_CHARACTERS_ONLY 0x04

// Resets the existing properties before applying the new properties.
#define wxRICHTEXT_SETPROPERTIES_RESET           0x08

// Removes the given properties instead of applying them.
#define wxRICHTEXT_SETPROPERTIES_REMOVE          0x10

/**
    Flags for object insertion.
 */

#define wxRICHTEXT_INSERT_NONE                              0x00
#define wxRICHTEXT_INSERT_WITH_PREVIOUS_PARAGRAPH_STYLE     0x01
#define wxRICHTEXT_INSERT_INTERACTIVE                       0x02

// A special flag telling the buffer to keep the first paragraph style
// as-is, when deleting a paragraph marker. In future we might pass a
// flag to InsertFragment and DeleteRange to indicate the appropriate mode.
#define wxTEXT_ATTR_KEEP_FIRST_PARA_STYLE   0x20000000

/**
    Default superscript/subscript font multiplication factor.
 */

#define wxSCRIPT_MUL_FACTOR             1.5

/**
    The type for wxTextAttrDimension flags.
 */
typedef unsigned short wxTextAttrDimensionFlags;

/**
    Miscellaneous text box flags
 */
enum wxTextBoxAttrFlags
{
    wxTEXT_BOX_ATTR_FLOAT                   = 0x00000001,
    wxTEXT_BOX_ATTR_CLEAR                   = 0x00000002,
    wxTEXT_BOX_ATTR_COLLAPSE_BORDERS        = 0x00000004,
    wxTEXT_BOX_ATTR_VERTICAL_ALIGNMENT      = 0x00000008,
    wxTEXT_BOX_ATTR_BOX_STYLE_NAME          = 0x00000010
};

/**
    Whether a value is present, used in dimension flags.
 */
enum wxTextAttrValueFlags
{
    wxTEXT_ATTR_VALUE_VALID               = 0x1000,
    wxTEXT_ATTR_VALUE_VALID_MASK          = 0x1000
};

/**
    Units, included in the dimension value.
 */
enum wxTextAttrUnits
{
    wxTEXT_ATTR_UNITS_TENTHS_MM             = 0x0001,
    wxTEXT_ATTR_UNITS_PIXELS                = 0x0002,
    wxTEXT_ATTR_UNITS_PERCENTAGE            = 0x0004,
    wxTEXT_ATTR_UNITS_POINTS                = 0x0008,
    wxTEXT_ATTR_UNITS_HUNDREDTHS_POINT      = 0x0100,

    wxTEXT_ATTR_UNITS_MASK                  = 0x010F
};

/**
    Position alternatives, included in the dimension flags.
 */
enum wxTextBoxAttrPosition
{
    wxTEXT_BOX_ATTR_POSITION_STATIC         = 0x0000, // Default is static, i.e. as per normal layout
    wxTEXT_BOX_ATTR_POSITION_RELATIVE       = 0x0010, // Relative to the relevant edge
    wxTEXT_BOX_ATTR_POSITION_ABSOLUTE       = 0x0020, // Relative to the parent
    wxTEXT_BOX_ATTR_POSITION_FIXED          = 0x0040, // Relative to the top-level window

    wxTEXT_BOX_ATTR_POSITION_MASK           = 0x00F0
};

/**
    @class wxTextAttrDimension

    A class representing a rich text dimension, including units and position.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextAttr, wxRichTextCtrl,  wxTextAttrDimensions
*/

class WXDLLIMPEXP_RICHTEXT wxTextAttrDimension
{
public:
    /**
        Default constructor.
    */
    wxTextAttrDimension() { Reset(); }
    /**
        Constructor taking value and units flag.
    */
    wxTextAttrDimension(int value, wxTextAttrUnits units = wxTEXT_ATTR_UNITS_TENTHS_MM) { m_value = value; m_flags = units|wxTEXT_ATTR_VALUE_VALID; }

    /**
        Resets the dimension value and flags.
    */
    void Reset() { m_value = 0; m_flags = 0; }

    /**
        Partial equality test. If @a weakTest is @true, attributes of this object do not
        have to be present if those attributes of @a dim are present. If @a weakTest is
        @false, the function will fail if an attribute is present in @a dim but not
        in this object.
    */
    bool EqPartial(const wxTextAttrDimension& dim, bool weakTest = true) const;

    /** Apply the dimension, but not those identical to @a compareWith if present.
    */
    bool Apply(const wxTextAttrDimension& dim, const wxTextAttrDimension* compareWith = NULL);

    /** Collects the attributes that are common to a range of content, building up a note of
        which attributes are absent in some objects and which clash in some objects.
    */
    void CollectCommonAttributes(const wxTextAttrDimension& attr, wxTextAttrDimension& clashingAttr, wxTextAttrDimension& absentAttr);

    /**
        Equality operator.
    */
    bool operator==(const wxTextAttrDimension& dim) const { return m_value == dim.m_value && m_flags == dim.m_flags; }

    /**
        Returns the integer value of the dimension.
    */
    int GetValue() const { return m_value; }

    /**
        Returns the floating-pointing value of the dimension in mm.

    */
    float GetValueMM() const { return m_value / 10.0f; }

    /**
        Sets the value of the dimension in mm.
    */
    void SetValueMM(float value) { m_value = (int) ((value * 10.0f) + 0.5f); m_flags |= wxTEXT_ATTR_VALUE_VALID; }

    /**
        Sets the integer value of the dimension.
    */
    void SetValue(int value) { m_value = value; m_flags |= wxTEXT_ATTR_VALUE_VALID; }

    /**
        Sets the integer value of the dimension, passing dimension flags.
    */
    void SetValue(int value, wxTextAttrDimensionFlags flags) { SetValue(value); m_flags = flags; }

    /**
        Sets the integer value and units.
    */
    void SetValue(int value, wxTextAttrUnits units) { m_value = value; m_flags = units | wxTEXT_ATTR_VALUE_VALID; }

    /**
        Sets the dimension.
    */
    void SetValue(const wxTextAttrDimension& dim) { (*this) = dim; }

    /**
        Gets the units of the dimension.
    */
    wxTextAttrUnits GetUnits() const { return (wxTextAttrUnits) (m_flags & wxTEXT_ATTR_UNITS_MASK); }

    /**
        Sets the units of the dimension.
    */
    void SetUnits(wxTextAttrUnits units) { m_flags &= ~wxTEXT_ATTR_UNITS_MASK; m_flags |= units; }

    /**
        Gets the position flags.
    */
    wxTextBoxAttrPosition GetPosition() const { return (wxTextBoxAttrPosition) (m_flags & wxTEXT_BOX_ATTR_POSITION_MASK); }

    /**
        Sets the position flags.
    */
    void SetPosition(wxTextBoxAttrPosition pos) { m_flags &= ~wxTEXT_BOX_ATTR_POSITION_MASK; m_flags |= pos; }

    /**
        Returns @true if the dimension is valid.
    */
    bool IsValid() const { return (m_flags & wxTEXT_ATTR_VALUE_VALID) != 0; }

    /**
        Sets the valid flag.
    */
    void SetValid(bool b) { m_flags &= ~wxTEXT_ATTR_VALUE_VALID_MASK; m_flags |= (b ? wxTEXT_ATTR_VALUE_VALID : 0); }

    /**
        Gets the dimension flags.
    */
    wxTextAttrDimensionFlags GetFlags() const { return m_flags; }

    /**
        Sets the dimension flags.
    */
    void SetFlags(wxTextAttrDimensionFlags flags) { m_flags = flags; }

    int                         m_value;
    wxTextAttrDimensionFlags    m_flags;
};

/**
    @class wxTextAttrDimensions
    A class for left, right, top and bottom dimensions.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextAttr, wxRichTextCtrl, wxTextAttrDimension
*/

class WXDLLIMPEXP_RICHTEXT wxTextAttrDimensions
{
public:
    /**
        Default constructor.
    */
    wxTextAttrDimensions() {}

    /**
        Resets the value and flags for all dimensions.
    */
    void Reset() { m_left.Reset(); m_top.Reset(); m_right.Reset(); m_bottom.Reset(); }

    /**
        Equality operator.
    */
    bool operator==(const wxTextAttrDimensions& dims) const { return m_left == dims.m_left && m_top == dims.m_top && m_right == dims.m_right && m_bottom == dims.m_bottom; }

    /**
        Partial equality test. If @a weakTest is @true, attributes of this object do not
        have to be present if those attributes of @a dim sare present. If @a weakTest is
        @false, the function will fail if an attribute is present in @a dims but not
        in this object.

    */
    bool EqPartial(const wxTextAttrDimensions& dims, bool weakTest = true) const;

    /**
        Apply to 'this', but not if the same as @a compareWith.

    */
    bool Apply(const wxTextAttrDimensions& dims, const wxTextAttrDimensions* compareWith = NULL);

    /**
        Collects the attributes that are common to a range of content, building up a note of
        which attributes are absent in some objects and which clash in some objects.

    */
    void CollectCommonAttributes(const wxTextAttrDimensions& attr, wxTextAttrDimensions& clashingAttr, wxTextAttrDimensions& absentAttr);

    /**
        Remove specified attributes from this object.
    */
    bool RemoveStyle(const wxTextAttrDimensions& attr);

    /**
        Gets the left dimension.
    */
    const wxTextAttrDimension& GetLeft() const { return m_left; }
    wxTextAttrDimension& GetLeft() { return m_left; }

    /**
        Gets the right dimension.

    */
    const wxTextAttrDimension& GetRight() const { return m_right; }
    wxTextAttrDimension& GetRight() { return m_right; }

    /**
        Gets the top dimension.

    */
    const wxTextAttrDimension& GetTop() const { return m_top; }
    wxTextAttrDimension& GetTop() { return m_top; }

    /**
        Gets the bottom dimension.

    */
    const wxTextAttrDimension& GetBottom() const { return m_bottom; }
    wxTextAttrDimension& GetBottom() { return m_bottom; }

    /**
        Are all dimensions valid?

    */
    bool IsValid() const { return m_left.IsValid() && m_top.IsValid() && m_right.IsValid() && m_bottom.IsValid(); }

    wxTextAttrDimension         m_left;
    wxTextAttrDimension         m_top;
    wxTextAttrDimension         m_right;
    wxTextAttrDimension         m_bottom;
};

/**
    @class wxTextAttrSize
    A class for representing width and height.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextAttr, wxRichTextCtrl, wxTextAttrDimension
*/

class WXDLLIMPEXP_RICHTEXT wxTextAttrSize
{
public:
    /**
        Default constructor.
    */
    wxTextAttrSize() {}

    /**
        Resets the width and height dimensions.
    */
    void Reset() { m_width.Reset(); m_height.Reset(); }

    /**
        Equality operator.
    */
    bool operator==(const wxTextAttrSize& size) const { return m_width == size.m_width && m_height == size.m_height ; }

    /**
        Partial equality test. If @a weakTest is @true, attributes of this object do not
        have to be present if those attributes of @a size are present. If @a weakTest is
        @false, the function will fail if an attribute is present in @a size but not
        in this object.
    */
    bool EqPartial(const wxTextAttrSize& size, bool weakTest = true) const;

    /**
        Apply to this object, but not if the same as @a compareWith.
    */
    bool Apply(const wxTextAttrSize& dims, const wxTextAttrSize* compareWith = NULL);

    /**
        Collects the attributes that are common to a range of content, building up a note of
        which attributes are absent in some objects and which clash in some objects.
    */
    void CollectCommonAttributes(const wxTextAttrSize& attr, wxTextAttrSize& clashingAttr, wxTextAttrSize& absentAttr);

    /**
        Removes the specified attributes from this object.
    */
    bool RemoveStyle(const wxTextAttrSize& attr);

    /**
        Returns the width.
    */
    wxTextAttrDimension& GetWidth() { return m_width; }
    const wxTextAttrDimension& GetWidth() const { return m_width; }

    /**
        Sets the width.
    */
    void SetWidth(int value, wxTextAttrDimensionFlags flags) { m_width.SetValue(value, flags); }

    /**
        Sets the width.
    */
    void SetWidth(int value, wxTextAttrUnits units) { m_width.SetValue(value, units); }

    /**
        Sets the width.
    */
    void SetWidth(const wxTextAttrDimension& dim) { m_width.SetValue(dim); }

    /**
        Gets the height.
    */
    wxTextAttrDimension& GetHeight() { return m_height; }
    const wxTextAttrDimension& GetHeight() const { return m_height; }

    /**
        Sets the height.
    */
    void SetHeight(int value, wxTextAttrDimensionFlags flags) { m_height.SetValue(value, flags); }

    /**
        Sets the height.
    */
    void SetHeight(int value, wxTextAttrUnits units) { m_height.SetValue(value, units); }

    /**
        Sets the height.
    */
    void SetHeight(const wxTextAttrDimension& dim) { m_height.SetValue(dim); }

    /**
        Is the size valid?
    */
    bool IsValid() const { return m_width.IsValid() && m_height.IsValid(); }

    wxTextAttrDimension         m_width;
    wxTextAttrDimension         m_height;
};

/**
    @class wxTextAttrDimensionConverter
    A class to make it easier to convert dimensions.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextAttr, wxRichTextCtrl, wxTextAttrDimension
*/

class WXDLLIMPEXP_RICHTEXT wxTextAttrDimensionConverter
{
public:
    /**
        Constructor.
    */
    wxTextAttrDimensionConverter(wxDC& dc, double scale = 1.0, const wxSize& parentSize = wxDefaultSize);
    /**
        Constructor.
    */
    wxTextAttrDimensionConverter(int ppi, double scale = 1.0, const wxSize& parentSize = wxDefaultSize);

    /**
        Gets the pixel size for the given dimension.
    */
    int GetPixels(const wxTextAttrDimension& dim, int direction = wxHORIZONTAL) const;
    /**
        Gets the mm size for the given dimension.
    */
    int GetTenthsMM(const wxTextAttrDimension& dim) const;

    /**
        Converts tenths of a mm to pixels.
    */
    int ConvertTenthsMMToPixels(int units) const;
    /**
        Converts pixels to tenths of a mm.
    */
    int ConvertPixelsToTenthsMM(int pixels) const;

    /**
        Sets the scale factor.
    */
    void SetScale(double scale) { m_scale = scale; }
    /**
        Returns the scale factor.
    */
    double GetScale() const { return m_scale; }

    /**
        Sets the ppi.
    */
    void SetPPI(int ppi) { m_ppi = ppi; }
    /**
        Returns the ppi.
    */
    int GetPPI() const { return m_ppi; }

    /**
        Sets the parent size.
    */
    void SetParentSize(const wxSize& parentSize) { m_parentSize = parentSize; }
    /**
        Returns the parent size.
    */
    const wxSize& GetParentSize() const { return m_parentSize; }

    int     m_ppi;
    double  m_scale;
    wxSize  m_parentSize;
};

/**
    Border styles, used with wxTextAttrBorder.
 */
enum wxTextAttrBorderStyle
{
    wxTEXT_BOX_ATTR_BORDER_NONE             = 0,
    wxTEXT_BOX_ATTR_BORDER_SOLID            = 1,
    wxTEXT_BOX_ATTR_BORDER_DOTTED           = 2,
    wxTEXT_BOX_ATTR_BORDER_DASHED           = 3,
    wxTEXT_BOX_ATTR_BORDER_DOUBLE           = 4,
    wxTEXT_BOX_ATTR_BORDER_GROOVE           = 5,
    wxTEXT_BOX_ATTR_BORDER_RIDGE            = 6,
    wxTEXT_BOX_ATTR_BORDER_INSET            = 7,
    wxTEXT_BOX_ATTR_BORDER_OUTSET           = 8
};

/**
    Border style presence flags, used with wxTextAttrBorder.
 */
enum wxTextAttrBorderFlags
{
    wxTEXT_BOX_ATTR_BORDER_STYLE            = 0x0001,
    wxTEXT_BOX_ATTR_BORDER_COLOUR           = 0x0002
};

/**
    Border width symbols for qualitative widths, used with wxTextAttrBorder.
 */
enum wxTextAttrBorderWidth
{
    wxTEXT_BOX_ATTR_BORDER_THIN             = -1,
    wxTEXT_BOX_ATTR_BORDER_MEDIUM           = -2,
    wxTEXT_BOX_ATTR_BORDER_THICK            = -3
};

/**
    Float styles.
 */
enum wxTextBoxAttrFloatStyle
{
    wxTEXT_BOX_ATTR_FLOAT_NONE              = 0,
    wxTEXT_BOX_ATTR_FLOAT_LEFT              = 1,
    wxTEXT_BOX_ATTR_FLOAT_RIGHT             = 2
};

/**
    Clear styles.
 */
enum wxTextBoxAttrClearStyle
{
    wxTEXT_BOX_ATTR_CLEAR_NONE              = 0,
    wxTEXT_BOX_ATTR_CLEAR_LEFT              = 1,
    wxTEXT_BOX_ATTR_CLEAR_RIGHT             = 2,
    wxTEXT_BOX_ATTR_CLEAR_BOTH              = 3
};

/**
    Collapse mode styles. TODO: can they be switched on per side?
 */
enum wxTextBoxAttrCollapseMode
{
    wxTEXT_BOX_ATTR_COLLAPSE_NONE           = 0,
    wxTEXT_BOX_ATTR_COLLAPSE_FULL           = 1
};

/**
    Vertical alignment values.
 */
enum wxTextBoxAttrVerticalAlignment
{
    wxTEXT_BOX_ATTR_VERTICAL_ALIGNMENT_NONE =       0,
    wxTEXT_BOX_ATTR_VERTICAL_ALIGNMENT_TOP  =       1,
    wxTEXT_BOX_ATTR_VERTICAL_ALIGNMENT_CENTRE =     2,
    wxTEXT_BOX_ATTR_VERTICAL_ALIGNMENT_BOTTOM  =    3
};

/**
    @class wxTextAttrBorder
    A class representing a rich text object border.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextAttr, wxRichTextCtrl, wxRichTextAttrBorders
*/

class WXDLLIMPEXP_RICHTEXT wxTextAttrBorder
{
public:
    /**
        Default constructor.
    */
    wxTextAttrBorder() { Reset(); }

    /**
        Equality operator.
    */
    bool operator==(const wxTextAttrBorder& border) const
    {
        return m_flags == border.m_flags && m_borderStyle == border.m_borderStyle &&
               m_borderColour == border.m_borderColour && m_borderWidth == border.m_borderWidth;
    }

    /**
        Resets the border style, colour, width and flags.
    */
    void Reset() { m_borderStyle = 0; m_borderColour = 0; m_flags = 0; m_borderWidth.Reset(); }

    /**
        Partial equality test. If @a weakTest is @true, attributes of this object do not
        have to be present if those attributes of @a border are present. If @a weakTest is
        @false, the function will fail if an attribute is present in @a border but not
        in this object.
    */
    bool EqPartial(const wxTextAttrBorder& border, bool weakTest = true) const;

    /**
        Applies the border to this object, but not if the same as @a compareWith.

    */
    bool Apply(const wxTextAttrBorder& border, const wxTextAttrBorder* compareWith = NULL);

    /**
        Removes the specified attributes from this object.
    */
    bool RemoveStyle(const wxTextAttrBorder& attr);

    /**
        Collects the attributes that are common to a range of content, building up a note of
        which attributes are absent in some objects and which clash in some objects.
    */
    void CollectCommonAttributes(const wxTextAttrBorder& attr, wxTextAttrBorder& clashingAttr, wxTextAttrBorder& absentAttr);

    /**
        Sets the border style.
    */
    void SetStyle(int style) { m_borderStyle = style; m_flags |= wxTEXT_BOX_ATTR_BORDER_STYLE; }

    /**
        Gets the border style.

    */
    int GetStyle() const { return m_borderStyle; }

    /**
        Sets the border colour.
    */
    void SetColour(unsigned long colour) { m_borderColour = colour; m_flags |= wxTEXT_BOX_ATTR_BORDER_COLOUR; }

    /**
        Sets the border colour.
    */
    void SetColour(const wxColour& colour) { m_borderColour = colour.GetRGB(); m_flags |= wxTEXT_BOX_ATTR_BORDER_COLOUR; }

    /**
        Gets the colour as a long.
    */
    unsigned long GetColourLong() const { return m_borderColour; }

    /**
        Gets the colour.
    */
    wxColour GetColour() const { return wxColour(m_borderColour); }

    /**
        Gets the border width.
    */
    wxTextAttrDimension& GetWidth() { return m_borderWidth; }
    const wxTextAttrDimension& GetWidth() const { return m_borderWidth; }

    /**
        Sets the border width.
    */
    void SetWidth(const wxTextAttrDimension& width) { m_borderWidth = width; }
    /**
        Sets the border width.
    */
    void SetWidth(int value, wxTextAttrUnits units = wxTEXT_ATTR_UNITS_TENTHS_MM) { SetWidth(wxTextAttrDimension(value, units)); }

    /**
        True if the border has a valid style.
    */
    bool HasStyle() const { return (m_flags & wxTEXT_BOX_ATTR_BORDER_STYLE) != 0; }

    /**
        True if the border has a valid colour.
    */
    bool HasColour() const { return (m_flags & wxTEXT_BOX_ATTR_BORDER_COLOUR) != 0; }

    /**
        True if the border has a valid width.
    */
    bool HasWidth() const { return m_borderWidth.IsValid(); }

    /**
        True if the border is valid.
    */
    bool IsValid() const { return HasWidth(); }

    /**
        Set the valid flag for this border.
    */
    void MakeValid() { m_borderWidth.SetValid(true); }

    /**
        True if the border has no attributes set.
    */
    bool IsDefault() const { return (m_flags == 0); }

    /**
        Returns the border flags.
    */
    int GetFlags() const { return m_flags; }

    /**
        Sets the border flags.
    */
    void SetFlags(int flags) { m_flags = flags; }

    /**
        Adds a border flag.
    */
    void AddFlag(int flag) { m_flags |= flag; }

    /**
        Removes a border flag.
    */
    void RemoveFlag(int flag) { m_flags &= ~flag; }

    int                         m_borderStyle;
    unsigned long               m_borderColour;
    wxTextAttrDimension         m_borderWidth;
    int                         m_flags;
};

/**
    @class wxTextAttrBorders
    A class representing a rich text object's borders.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextAttr, wxRichTextCtrl, wxRichTextAttrBorder
*/

class WXDLLIMPEXP_RICHTEXT wxTextAttrBorders
{
public:
    /**
        Default constructor.
    */
    wxTextAttrBorders() { }

    /**
        Equality operator.
    */
    bool operator==(const wxTextAttrBorders& borders) const
    {
        return m_left == borders.m_left && m_right == borders.m_right &&
               m_top == borders.m_top && m_bottom == borders.m_bottom;
    }

    /**
        Sets the style of all borders.
    */
    void SetStyle(int style);

    /**
        Sets colour of all borders.
    */
    void SetColour(unsigned long colour);

    /**
        Sets the colour for all borders.
    */
    void SetColour(const wxColour& colour);

    /**
        Sets the width of all borders.
    */
    void SetWidth(const wxTextAttrDimension& width);

    /**
        Sets the width of all borders.
    */
    void SetWidth(int value, wxTextAttrUnits units = wxTEXT_ATTR_UNITS_TENTHS_MM) { SetWidth(wxTextAttrDimension(value, units)); }

    /**
        Resets all borders.
    */
    void Reset() { m_left.Reset(); m_right.Reset(); m_top.Reset(); m_bottom.Reset(); }

    /**
        Partial equality test. If @a weakTest is @true, attributes of this object do not
        have to be present if those attributes of @a borders are present. If @a weakTest is
        @false, the function will fail if an attribute is present in @a borders but not
        in this object.
    */
    bool EqPartial(const wxTextAttrBorders& borders, bool weakTest = true) const;

    /**
        Applies border to this object, but not if the same as @a compareWith.
    */
    bool Apply(const wxTextAttrBorders& borders, const wxTextAttrBorders* compareWith = NULL);

    /**
        Removes the specified attributes from this object.
    */
    bool RemoveStyle(const wxTextAttrBorders& attr);

    /**
        Collects the attributes that are common to a range of content, building up a note of
        which attributes are absent in some objects and which clash in some objects.
    */
    void CollectCommonAttributes(const wxTextAttrBorders& attr, wxTextAttrBorders& clashingAttr, wxTextAttrBorders& absentAttr);

    /**
        Returns @true if at least one border is valid.
    */
    bool IsValid() const { return m_left.IsValid() || m_right.IsValid() || m_top.IsValid() || m_bottom.IsValid(); }

    /**
        Returns @true if no border attributes were set.
    */
    bool IsDefault() const { return m_left.IsDefault() && m_right.IsDefault() && m_top.IsDefault() && m_bottom.IsDefault(); }

    /**
        Returns the left border.
    */
    const wxTextAttrBorder& GetLeft() const { return m_left; }
    wxTextAttrBorder& GetLeft() { return m_left; }

    /**
        Returns the right border.
    */
    const wxTextAttrBorder& GetRight() const { return m_right; }
    wxTextAttrBorder& GetRight() { return m_right; }

    /**
        Returns the top border.
    */
    const wxTextAttrBorder& GetTop() const { return m_top; }
    wxTextAttrBorder& GetTop() { return m_top; }

    /**
        Returns the bottom border.
    */
    const wxTextAttrBorder& GetBottom() const { return m_bottom; }
    wxTextAttrBorder& GetBottom() { return m_bottom; }

    wxTextAttrBorder m_left, m_right, m_top, m_bottom;

};

/**
    @class wxTextBoxAttr
    A class representing the box attributes of a rich text object.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextAttr, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxTextBoxAttr
{
public:
    /**
        Default constructor.
    */
    wxTextBoxAttr() { Init(); }

    /**
        Copy constructor.
    */
    wxTextBoxAttr(const wxTextBoxAttr& attr) { Init(); (*this) = attr; }

    /**
        Initialises this object.
    */
    void Init() { Reset(); }

    /**
        Resets this object.
    */
    void Reset();

    // Copy. Unnecessary since we let it do a binary copy
    //void Copy(const wxTextBoxAttr& attr);

    // Assignment
    //void operator= (const wxTextBoxAttr& attr);

    /**
        Equality test.
    */
    bool operator== (const wxTextBoxAttr& attr) const;

    /**
        Partial equality test, ignoring unset attributes. If @a weakTest is @true, attributes of this object do not
        have to be present if those attributes of @a attr are present. If @a weakTest is
        @false, the function will fail if an attribute is present in @a attr but not
        in this object.

    */
    bool EqPartial(const wxTextBoxAttr& attr, bool weakTest = true) const;

    /**
        Merges the given attributes. If @a compareWith is non-NULL, then it will be used
        to mask out those attributes that are the same in style and @a compareWith, for
        situations where we don't want to explicitly set inherited attributes.
    */
    bool Apply(const wxTextBoxAttr& style, const wxTextBoxAttr* compareWith = NULL);

    /**
        Collects the attributes that are common to a range of content, building up a note of
        which attributes are absent in some objects and which clash in some objects.
    */
    void CollectCommonAttributes(const wxTextBoxAttr& attr, wxTextBoxAttr& clashingAttr, wxTextBoxAttr& absentAttr);

    /**
        Removes the specified attributes from this object.
    */
    bool RemoveStyle(const wxTextBoxAttr& attr);

    /**
        Sets the flags.
    */
    void SetFlags(int flags) { m_flags = flags; }

    /**
        Returns the flags.
    */
    int GetFlags() const { return m_flags; }

    /**
        Is this flag present?
    */
    bool HasFlag(wxTextBoxAttrFlags flag) const { return (m_flags & flag) != 0; }

    /**
        Removes this flag.
    */
    void RemoveFlag(wxTextBoxAttrFlags flag) { m_flags &= ~flag; }

    /**
        Adds this flag.
    */
    void AddFlag(wxTextBoxAttrFlags flag) { m_flags |= flag; }

    /**
        Returns @true if no attributes are set.
    */
    bool IsDefault() const;

    /**
        Returns the float mode.
    */
    wxTextBoxAttrFloatStyle GetFloatMode() const { return m_floatMode; }

    /**
        Sets the float mode.
    */
    void SetFloatMode(wxTextBoxAttrFloatStyle mode) { m_floatMode = mode; m_flags |= wxTEXT_BOX_ATTR_FLOAT; }

    /**
        Returns @true if float mode is active.
    */
    bool HasFloatMode() const { return HasFlag(wxTEXT_BOX_ATTR_FLOAT); }

    /**
        Returns @true if this object is floating.
    */
    bool IsFloating() const { return HasFloatMode() && GetFloatMode() != wxTEXT_BOX_ATTR_FLOAT_NONE; }

    /**
        Returns the clear mode - whether to wrap text after object. Currently unimplemented.
    */
    wxTextBoxAttrClearStyle GetClearMode() const { return m_clearMode; }

    /**
        Set the clear mode. Currently unimplemented.
    */
    void SetClearMode(wxTextBoxAttrClearStyle mode) { m_clearMode = mode; m_flags |= wxTEXT_BOX_ATTR_CLEAR; }

    /**
        Returns @true if we have a clear flag.
    */
    bool HasClearMode() const { return HasFlag(wxTEXT_BOX_ATTR_CLEAR); }

    /**
        Returns the collapse mode - whether to collapse borders.
    */
    wxTextBoxAttrCollapseMode GetCollapseBorders() const { return m_collapseMode; }

    /**
        Sets the collapse mode - whether to collapse borders.
    */
    void SetCollapseBorders(wxTextBoxAttrCollapseMode collapse) { m_collapseMode = collapse; m_flags |= wxTEXT_BOX_ATTR_COLLAPSE_BORDERS; }

    /**
        Returns @true if the collapse borders flag is present.
    */
    bool HasCollapseBorders() const { return HasFlag(wxTEXT_BOX_ATTR_COLLAPSE_BORDERS); }

    /**
        Returns the vertical alignment.
    */
    wxTextBoxAttrVerticalAlignment GetVerticalAlignment() const { return m_verticalAlignment; }

    /**
        Sets the vertical alignment.
    */
    void SetVerticalAlignment(wxTextBoxAttrVerticalAlignment verticalAlignment) { m_verticalAlignment = verticalAlignment; m_flags |= wxTEXT_BOX_ATTR_VERTICAL_ALIGNMENT; }

    /**
        Returns @true if a vertical alignment flag is present.
    */
    bool HasVerticalAlignment() const { return HasFlag(wxTEXT_BOX_ATTR_VERTICAL_ALIGNMENT); }

    /**
        Returns the margin values.
    */
    wxTextAttrDimensions& GetMargins() { return m_margins; }
    const wxTextAttrDimensions& GetMargins() const { return m_margins; }

    /**
        Returns the left margin.
    */
    wxTextAttrDimension& GetLeftMargin() { return m_margins.m_left; }
    const wxTextAttrDimension& GetLeftMargin() const { return m_margins.m_left; }

    /**
        Returns the right margin.
    */
    wxTextAttrDimension& GetRightMargin() { return m_margins.m_right; }
    const wxTextAttrDimension& GetRightMargin() const { return m_margins.m_right; }

    /**
        Returns the top margin.
    */
    wxTextAttrDimension& GetTopMargin() { return m_margins.m_top; }
    const wxTextAttrDimension& GetTopMargin() const { return m_margins.m_top; }

    /**
        Returns the bottom margin.
    */
    wxTextAttrDimension& GetBottomMargin() { return m_margins.m_bottom; }
    const wxTextAttrDimension& GetBottomMargin() const { return m_margins.m_bottom; }

    /**
        Returns the position.
    */
    wxTextAttrDimensions& GetPosition() { return m_position; }
    const wxTextAttrDimensions& GetPosition() const { return m_position; }

    /**
        Returns the left position.
    */
    wxTextAttrDimension& GetLeft() { return m_position.m_left; }
    const wxTextAttrDimension& GetLeft() const { return m_position.m_left; }

    /**
        Returns the right position.
    */
    wxTextAttrDimension& GetRight() { return m_position.m_right; }
    const wxTextAttrDimension& GetRight() const { return m_position.m_right; }

    /**
        Returns the top position.
    */
    wxTextAttrDimension& GetTop() { return m_position.m_top; }
    const wxTextAttrDimension& GetTop() const { return m_position.m_top; }

    /**
        Returns the bottom position.
    */
    wxTextAttrDimension& GetBottom() { return m_position.m_bottom; }
    const wxTextAttrDimension& GetBottom() const { return m_position.m_bottom; }

    /**
        Returns the padding values.
    */
    wxTextAttrDimensions& GetPadding() { return m_padding; }
    const wxTextAttrDimensions& GetPadding() const { return m_padding; }

    /**
        Returns the left padding value.
    */
    wxTextAttrDimension& GetLeftPadding() { return m_padding.m_left; }
    const wxTextAttrDimension& GetLeftPadding() const { return m_padding.m_left; }

    /**
        Returns the right padding value.
    */
    wxTextAttrDimension& GetRightPadding() { return m_padding.m_right; }
    const wxTextAttrDimension& GetRightPadding() const { return m_padding.m_right; }

    /**
        Returns the top padding value.
    */
    wxTextAttrDimension& GetTopPadding() { return m_padding.m_top; }
    const wxTextAttrDimension& GetTopPadding() const { return m_padding.m_top; }

    /**
        Returns the bottom padding value.
    */
    wxTextAttrDimension& GetBottomPadding() { return m_padding.m_bottom; }
    const wxTextAttrDimension& GetBottomPadding() const { return m_padding.m_bottom; }

    /**
        Returns the borders.
    */
    wxTextAttrBorders& GetBorder() { return m_border; }
    const wxTextAttrBorders& GetBorder() const { return m_border; }

    /**
        Returns the left border.
    */
    wxTextAttrBorder& GetLeftBorder() { return m_border.m_left; }
    const wxTextAttrBorder& GetLeftBorder() const { return m_border.m_left; }

    /**
        Returns the top border.
    */
    wxTextAttrBorder& GetTopBorder() { return m_border.m_top; }
    const wxTextAttrBorder& GetTopBorder() const { return m_border.m_top; }

    /**
        Returns the right border.
    */
    wxTextAttrBorder& GetRightBorder() { return m_border.m_right; }
    const wxTextAttrBorder& GetRightBorder() const { return m_border.m_right; }

    /**
        Returns the bottom border.
    */
    wxTextAttrBorder& GetBottomBorder() { return m_border.m_bottom; }
    const wxTextAttrBorder& GetBottomBorder() const { return m_border.m_bottom; }

    /**
        Returns the outline.
    */
    wxTextAttrBorders& GetOutline() { return m_outline; }
    const wxTextAttrBorders& GetOutline() const { return m_outline; }

    /**
        Returns the left outline.
    */
    wxTextAttrBorder& GetLeftOutline() { return m_outline.m_left; }
    const wxTextAttrBorder& GetLeftOutline() const { return m_outline.m_left; }

    /**
        Returns the top outline.
    */
    wxTextAttrBorder& GetTopOutline() { return m_outline.m_top; }
    const wxTextAttrBorder& GetTopOutline() const { return m_outline.m_top; }

    /**
        Returns the right outline.
    */
    wxTextAttrBorder& GetRightOutline() { return m_outline.m_right; }
    const wxTextAttrBorder& GetRightOutline() const { return m_outline.m_right; }

    /**
        Returns the bottom outline.
    */
    wxTextAttrBorder& GetBottomOutline() { return m_outline.m_bottom; }
    const wxTextAttrBorder& GetBottomOutline() const { return m_outline.m_bottom; }

    /**
        Returns the object size.
    */
    wxTextAttrSize& GetSize() { return m_size; }
    const wxTextAttrSize& GetSize() const { return m_size; }

    /**
        Returns the object minimum size.
    */

    wxTextAttrSize& GetMinSize() { return m_minSize; }
    const wxTextAttrSize& GetMinSize() const { return m_minSize; }

    /**
        Returns the object maximum size.
    */

    wxTextAttrSize& GetMaxSize() { return m_maxSize; }
    const wxTextAttrSize& GetMaxSize() const { return m_maxSize; }

    /**
        Sets the object size.
    */
    void SetSize(const wxTextAttrSize& sz) { m_size = sz; }

    /**
        Sets the object minimum size.
    */
    void SetMinSize(const wxTextAttrSize& sz) { m_minSize = sz; }

    /**
        Sets the object maximum size.
    */
    void SetMaxSize(const wxTextAttrSize& sz) { m_maxSize = sz; }

    /**
        Returns the object width.
    */
    wxTextAttrDimension& GetWidth() { return m_size.m_width; }
    const wxTextAttrDimension& GetWidth() const { return m_size.m_width; }

    /**
        Returns the object height.
    */
    wxTextAttrDimension& GetHeight() { return m_size.m_height; }
    const wxTextAttrDimension& GetHeight() const { return m_size.m_height; }

    /**
        Returns the box style name.
    */
    const wxString& GetBoxStyleName() const { return m_boxStyleName; }

    /**
        Sets the box style name.
    */
    void SetBoxStyleName(const wxString& name) { m_boxStyleName = name; AddFlag(wxTEXT_BOX_ATTR_BOX_STYLE_NAME); }

    /**
        Returns @true if the box style name is present.
    */
    bool HasBoxStyleName() const { return HasFlag(wxTEXT_BOX_ATTR_BOX_STYLE_NAME); }

public:

    int                             m_flags;

    wxTextAttrDimensions            m_margins;
    wxTextAttrDimensions            m_padding;
    wxTextAttrDimensions            m_position;

    wxTextAttrSize                  m_size;
    wxTextAttrSize                  m_minSize;
    wxTextAttrSize                  m_maxSize;

    wxTextAttrBorders               m_border;
    wxTextAttrBorders               m_outline;

    wxTextBoxAttrFloatStyle         m_floatMode;
    wxTextBoxAttrClearStyle         m_clearMode;
    wxTextBoxAttrCollapseMode       m_collapseMode;
    wxTextBoxAttrVerticalAlignment  m_verticalAlignment;
    wxString                        m_boxStyleName;
};

/**
    @class wxRichTextAttr
    A class representing enhanced attributes for rich text objects.
    This adds a wxTextBoxAttr member to the basic wxTextAttr class.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextAttr, wxTextBoxAttr, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextAttr: public wxTextAttr
{
public:
    /**
        Constructor taking a wxTextAttr.
    */
    wxRichTextAttr(const wxTextAttr& attr) { wxTextAttr::Copy(attr); }

    /**
        Copy constructor.
    */
    wxRichTextAttr(const wxRichTextAttr& attr): wxTextAttr() { Copy(attr); }

    /**
        Default constructor.
    */
    wxRichTextAttr() {}

    /**
        Copy function.
    */
    void Copy(const wxRichTextAttr& attr);

    /**
        Assignment operator.
    */
    void operator=(const wxRichTextAttr& attr) { Copy(attr); }

    /**
        Assignment operator.
    */
    void operator=(const wxTextAttr& attr) { wxTextAttr::Copy(attr); }

    /**
        Equality test.
    */
    bool operator==(const wxRichTextAttr& attr) const;

    /**
        Partial equality test. If @a weakTest is @true, attributes of this object do not
        have to be present if those attributes of @a attr are present. If @a weakTest is
        @false, the function will fail if an attribute is present in @a attr but not
        in this object.
    */
    bool EqPartial(const wxRichTextAttr& attr, bool weakTest = true) const;

    /**
        Merges the given attributes. If @a compareWith
        is non-NULL, then it will be used to mask out those attributes that are the same in style
        and @a compareWith, for situations where we don't want to explicitly set inherited attributes.
    */
    bool Apply(const wxRichTextAttr& style, const wxRichTextAttr* compareWith = NULL);

    /**
        Collects the attributes that are common to a range of content, building up a note of
        which attributes are absent in some objects and which clash in some objects.
    */
    void CollectCommonAttributes(const wxRichTextAttr& attr, wxRichTextAttr& clashingAttr, wxRichTextAttr& absentAttr);

    /**
        Removes the specified attributes from this object.
    */
    bool RemoveStyle(const wxRichTextAttr& attr);

    /**
        Returns the text box attributes.
    */
    wxTextBoxAttr& GetTextBoxAttr() { return m_textBoxAttr; }
    const wxTextBoxAttr& GetTextBoxAttr() const { return m_textBoxAttr; }

    /**
        Set the text box attributes.
    */
    void SetTextBoxAttr(const wxTextBoxAttr& attr) { m_textBoxAttr = attr; }

    /**
        Returns @true if no attributes are set.
    */
    bool IsDefault() const { return (GetFlags() == 0) && m_textBoxAttr.IsDefault(); }

    wxTextBoxAttr    m_textBoxAttr;
};

WX_DECLARE_USER_EXPORTED_OBJARRAY(wxRichTextAttr, wxRichTextAttrArray, WXDLLIMPEXP_RICHTEXT);

WX_DECLARE_USER_EXPORTED_OBJARRAY(wxVariant, wxRichTextVariantArray, WXDLLIMPEXP_RICHTEXT);

WX_DECLARE_USER_EXPORTED_OBJARRAY(wxRect, wxRichTextRectArray, WXDLLIMPEXP_RICHTEXT);

/**
    @class wxRichTextProperties
    A simple property class using wxVariants. This is used to give each rich text object the
    ability to store custom properties that can be used by the application.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextBuffer, wxRichTextObject, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextProperties: public wxObject
{
DECLARE_DYNAMIC_CLASS(wxRichTextProperties)
public:

    /**
        Default constructor.
    */
    wxRichTextProperties() {}

    /**
        Copy constructor.
    */
    wxRichTextProperties(const wxRichTextProperties& props): wxObject() { Copy(props); }

    /**
        Assignment operator.
    */
    void operator=(const wxRichTextProperties& props) { Copy(props); }

    /**
        Equality operator.
    */
    bool operator==(const wxRichTextProperties& props) const;

    /**
        Copies from @a props.
    */
    void Copy(const wxRichTextProperties& props) { m_properties = props.m_properties; }

    /**
        Returns the variant at the given index.
    */
    const wxVariant& operator[](size_t idx) const { return m_properties[idx]; }

    /**
        Returns the variant at the given index.
    */
    wxVariant& operator[](size_t idx) { return m_properties[idx]; }

    /**
        Clears the properties.
    */
    void Clear() { m_properties.Clear(); }

    /**
        Returns the array of variants implementing the properties.
    */
    const wxRichTextVariantArray& GetProperties() const { return m_properties; }

    /**
        Returns the array of variants implementing the properties.
    */
    wxRichTextVariantArray& GetProperties() { return m_properties; }

    /**
        Sets the array of variants.
    */
    void SetProperties(const wxRichTextVariantArray& props) { m_properties = props; }

    /**
        Returns all the property names.
    */
    wxArrayString GetPropertyNames() const;

    /**
        Returns a count of the properties.
    */
    size_t GetCount() const { return m_properties.GetCount(); }

    /**
        Returns @true if the given property is found.
    */
    bool HasProperty(const wxString& name) const { return Find(name) != -1; }

    /**
        Finds the given property.
    */
    int Find(const wxString& name) const;

    /**
        Removes the given property.
    */
    bool Remove(const wxString& name);

    /**
        Gets the property variant by name.
    */
    const wxVariant& GetProperty(const wxString& name) const;

    /**
        Finds or creates a property with the given name, returning a pointer to the variant.
    */
    wxVariant* FindOrCreateProperty(const wxString& name);

    /**
        Gets the value of the named property as a string.
    */
    wxString GetPropertyString(const wxString& name) const;

    /**
        Gets the value of the named property as a long integer.
    */
    long GetPropertyLong(const wxString& name) const;

    /**
        Gets the value of the named property as a boolean.
    */
    bool GetPropertyBool(const wxString& name) const;

    /**
        Gets the value of the named property as a double.
    */
    double GetPropertyDouble(const wxString& name) const;

    /**
        Sets the property by passing a variant which contains a name and value.
    */
    void SetProperty(const wxVariant& variant);

    /**
        Sets a property by name and variant.
    */
    void SetProperty(const wxString& name, const wxVariant& variant);

    /**
        Sets a property by name and string value.
    */
    void SetProperty(const wxString& name, const wxString& value);

    /**
        Sets a property by name and wxChar* value.
    */
    void SetProperty(const wxString& name, const wxChar* value) { SetProperty(name, wxString(value)); }

    /**
        Sets  property by name and long integer value.
    */
    void SetProperty(const wxString& name, long value);

    /**
        Sets  property by name and double value.
    */
    void SetProperty(const wxString& name, double value);

    /**
        Sets  property by name and boolean value.
    */
    void SetProperty(const wxString& name, bool value);

    /**
        Removes the given properties from these properties.
    */
    void RemoveProperties(const wxRichTextProperties& properties);

    /**
        Merges the given properties with these properties.
    */
    void MergeProperties(const wxRichTextProperties& properties);

protected:
    wxRichTextVariantArray  m_properties;
};


/**
    @class wxRichTextFontTable
    Manages quick access to a pool of fonts for rendering rich text.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextBuffer, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextFontTable: public wxObject
{
public:
    /**
        Default constructor.
    */
    wxRichTextFontTable();

    /**
        Copy constructor.
    */
    wxRichTextFontTable(const wxRichTextFontTable& table);
    virtual ~wxRichTextFontTable();

    /**
        Returns @true if the font table is valid.
    */
    bool IsOk() const { return m_refData != NULL; }

    /**
        Finds a font for the given attribute object.
    */
    wxFont FindFont(const wxRichTextAttr& fontSpec);

    /**
        Clears the font table.
    */
    void Clear();

    /**
        Assignment operator.
    */
    void operator= (const wxRichTextFontTable& table);

    /**
        Equality operator.
    */
    bool operator == (const wxRichTextFontTable& table) const;

    /**
        Inequality operator.
    */
    bool operator != (const wxRichTextFontTable& table) const { return !(*this == table); }

    /**
        Set the font scale factor.
    */
    void SetFontScale(double fontScale);

protected:

    double m_fontScale;

    DECLARE_DYNAMIC_CLASS(wxRichTextFontTable)
};

/**
    @class wxRichTextRange

    This stores beginning and end positions for a range of data.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextBuffer, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextRange
{
public:
// Constructors

    /**
        Default constructor.
    */
    wxRichTextRange() { m_start = 0; m_end = 0; }

    /**
        Constructor taking start and end positions.
    */
    wxRichTextRange(long start, long end) { m_start = start; m_end = end; }

    /**
        Copy constructor.
    */
    wxRichTextRange(const wxRichTextRange& range) { m_start = range.m_start; m_end = range.m_end; }
    ~wxRichTextRange() {}

    /**
        Assigns @a range to this range.
    */
    void operator =(const wxRichTextRange& range) { m_start = range.m_start; m_end = range.m_end; }

    /**
        Equality operator. Returns @true if @a range is the same as this range.
    */
    bool operator ==(const wxRichTextRange& range) const { return (m_start == range.m_start && m_end == range.m_end); }

    /**
        Inequality operator.
    */
    bool operator !=(const wxRichTextRange& range) const { return (m_start != range.m_start || m_end != range.m_end); }

    /**
        Subtracts a range from this range.
    */
    wxRichTextRange operator -(const wxRichTextRange& range) const { return wxRichTextRange(m_start - range.m_start, m_end - range.m_end); }

    /**
        Adds a range to this range.
    */
    wxRichTextRange operator +(const wxRichTextRange& range) const { return wxRichTextRange(m_start + range.m_start, m_end + range.m_end); }

    /**
        Sets the range start and end positions.
    */
    void SetRange(long start, long end) { m_start = start; m_end = end; }

    /**
        Sets the start position.
    */
    void SetStart(long start) { m_start = start; }

    /**
        Returns the start position.
    */
    long GetStart() const { return m_start; }

    /**
        Sets the end position.
    */
    void SetEnd(long end) { m_end = end; }

    /**
        Gets the end position.
    */
    long GetEnd() const { return m_end; }

    /**
        Returns true if this range is completely outside @a range.
    */
    bool IsOutside(const wxRichTextRange& range) const { return range.m_start > m_end || range.m_end < m_start; }

    /**
        Returns true if this range is completely within @a range.
    */
    bool IsWithin(const wxRichTextRange& range) const { return m_start >= range.m_start && m_end <= range.m_end; }

    /**
        Returns true if @a pos was within the range. Does not match if the range is empty.
    */
    bool Contains(long pos) const { return pos >= m_start && pos <= m_end ; }

    /**
        Limit this range to be within @a range.
    */
    bool LimitTo(const wxRichTextRange& range) ;

    /**
        Gets the length of the range.
    */
    long GetLength() const { return m_end - m_start + 1; }

    /**
        Swaps the start and end.
    */
    void Swap() { long tmp = m_start; m_start = m_end; m_end = tmp; }

    /**
        Converts the API-standard range, whose end is one past the last character in
        the range, to the internal form, which uses the first and last character
        positions of the range. In other words, one is subtracted from the end position.
        (n, n) is the range of a single character.
    */
    wxRichTextRange ToInternal() const { return wxRichTextRange(m_start, m_end-1); }

    /**
        Converts the internal range, which uses the first and last character positions
        of the range, to the API-standard range, whose end is one past the last
        character in the range. In other words, one is added to the end position.
        (n, n+1) is the range of a single character.
    */
    wxRichTextRange FromInternal() const { return wxRichTextRange(m_start, m_end+1); }

protected:
    long m_start;
    long m_end;
};

WX_DECLARE_USER_EXPORTED_OBJARRAY(wxRichTextRange, wxRichTextRangeArray, WXDLLIMPEXP_RICHTEXT);

#define wxRICHTEXT_ALL  wxRichTextRange(-2, -2)
#define wxRICHTEXT_NONE  wxRichTextRange(-1, -1)

#define wxRICHTEXT_NO_SELECTION wxRichTextRange(-2, -2)

/**
    @class wxRichTextSelection

    Stores selection information. The selection does not have to be contiguous, though currently non-contiguous
    selections are only supported for a range of table cells (a geometric block of cells can consist
    of a set of non-contiguous positions).

    The selection consists of an array of ranges, and the container that is the context for the selection. It
    follows that a single selection object can only represent ranges with the same parent container.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextBuffer, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextSelection
{
public:
    /**
        Copy constructor.
    */
    wxRichTextSelection(const wxRichTextSelection& sel) { Copy(sel); }

    /**
        Creates a selection from a range and a container.
    */
    wxRichTextSelection(const wxRichTextRange& range, wxRichTextParagraphLayoutBox* container) { m_ranges.Add(range); m_container = container; }

    /**
        Default constructor.
    */
    wxRichTextSelection() { Reset(); }

    /**
        Resets the selection.
    */
    void Reset() { m_ranges.Clear(); m_container = NULL; }

    /**
        Sets the selection.
    */

    void Set(const wxRichTextRange& range, wxRichTextParagraphLayoutBox* container)
    { m_ranges.Clear(); m_ranges.Add(range); m_container = container; }

    /**
        Adds a range to the selection.
    */
    void Add(const wxRichTextRange& range)
    { m_ranges.Add(range); }

    /**
        Sets the selections from an array of ranges and a container object.
    */
    void Set(const wxRichTextRangeArray& ranges, wxRichTextParagraphLayoutBox* container)
    { m_ranges = ranges; m_container = container; }

    /**
        Copies from @a sel.
    */
    void Copy(const wxRichTextSelection& sel)
    { m_ranges = sel.m_ranges; m_container = sel.m_container; }

    /**
        Assignment operator.
    */
    void operator=(const wxRichTextSelection& sel) { Copy(sel); }

    /**
        Equality operator.
    */
    bool operator==(const wxRichTextSelection& sel) const;

    /**
        Index operator.
    */
    wxRichTextRange operator[](size_t i) const { return GetRange(i); }

    /**
        Returns the selection ranges.
    */
    wxRichTextRangeArray& GetRanges() { return m_ranges; }

    /**
        Returns the selection ranges.
    */
    const wxRichTextRangeArray& GetRanges() const { return m_ranges; }

    /**
        Sets the selection ranges.
    */
    void SetRanges(const wxRichTextRangeArray& ranges) { m_ranges = ranges; }

    /**
        Returns the number of ranges in the selection.
    */
    size_t GetCount() const { return m_ranges.GetCount(); }

    /**
        Returns the range at the given index.

    */
    wxRichTextRange GetRange(size_t i) const { return m_ranges[i]; }

    /**
        Returns the first range if there is one, otherwise wxRICHTEXT_NO_SELECTION.
    */
    wxRichTextRange GetRange() const { return (m_ranges.GetCount() > 0) ? (m_ranges[0]) : wxRICHTEXT_NO_SELECTION; }

    /**
        Sets a single range.
    */
    void SetRange(const wxRichTextRange& range) { m_ranges.Clear(); m_ranges.Add(range); }

    /**
        Returns the container for which the selection is valid.
    */
    wxRichTextParagraphLayoutBox* GetContainer() const { return m_container; }

    /**
        Sets the container for which the selection is valid.
    */
    void SetContainer(wxRichTextParagraphLayoutBox* container) { m_container = container; }

    /**
        Returns @true if the selection is valid.
    */
    bool IsValid() const { return m_ranges.GetCount() > 0 && GetContainer(); }

    /**
        Returns the selection appropriate to the specified object, if any; returns an empty array if none
        at the level of the object's container.
    */
    wxRichTextRangeArray GetSelectionForObject(wxRichTextObject* obj) const;

    /**
        Returns @true if the given position is within the selection.
    */
    bool WithinSelection(long pos, wxRichTextObject* obj) const;

    /**
        Returns @true if the given position is within the selection.

    */
    bool WithinSelection(long pos) const { return WithinSelection(pos, m_ranges); }

    /**
        Returns @true if the given position is within the selection range.
    */
    static bool WithinSelection(long pos, const wxRichTextRangeArray& ranges);

    /**
        Returns @true if the given range is within the selection range.
    */
    static bool WithinSelection(const wxRichTextRange& range, const wxRichTextRangeArray& ranges);

    wxRichTextRangeArray            m_ranges;
    wxRichTextParagraphLayoutBox*   m_container;
};

/**
    @class wxRichTextDrawingContext

    A class for passing information to drawing and measuring functions.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextBuffer, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextDrawingContext: public wxObject
{
    DECLARE_CLASS(wxRichTextDrawingContext)
public:

    /**
        Pass the buffer to the context so the context can retrieve information
        such as virtual attributes.
    */
    wxRichTextDrawingContext(wxRichTextBuffer* buffer);

    void Init() { m_buffer = NULL; m_enableVirtualAttributes = true; }

    /**
        Does this object have virtual attributes?
        Virtual attributes can be provided for visual cues without
        affecting the actual styling.
    */
    bool HasVirtualAttributes(wxRichTextObject* obj) const;

    /**
        Returns the virtual attributes for this object.
        Virtual attributes can be provided for visual cues without
        affecting the actual styling.
    */
    wxRichTextAttr GetVirtualAttributes(wxRichTextObject* obj) const;

    /**
        Applies any virtual attributes relevant to this object.
    */
    bool ApplyVirtualAttributes(wxRichTextAttr& attr, wxRichTextObject* obj) const;

    /**
        Gets the count for mixed virtual attributes for individual positions within the object.
        For example, individual characters within a text object may require special highlighting.
    */
    int GetVirtualSubobjectAttributesCount(wxRichTextObject* obj) const;

    /**
        Gets the mixed virtual attributes for individual positions within the object.
        For example, individual characters within a text object may require special highlighting.
        The function is passed the count returned by GetVirtualSubobjectAttributesCount.
    */
    int GetVirtualSubobjectAttributes(wxRichTextObject* obj, wxArrayInt& positions, wxRichTextAttrArray& attributes) const;

    /**
        Do we have virtual text for this object? Virtual text allows an application
        to replace characters in an object for editing and display purposes, for example
        for highlighting special characters.
    */
    bool HasVirtualText(const wxRichTextPlainText* obj) const;

    /**
        Gets the virtual text for this object.
    */
    bool GetVirtualText(const wxRichTextPlainText* obj, wxString& text) const;

    /**
        Enables virtual attribute processing.
    */

    void EnableVirtualAttributes(bool b) { m_enableVirtualAttributes = b; }

    /**
        Returns @true if virtual attribute processing is enabled.
    */

    bool GetVirtualAttributesEnabled() const { return m_enableVirtualAttributes; }

    wxRichTextBuffer*   m_buffer;
    bool                m_enableVirtualAttributes;
};

/**
    @class wxRichTextObject

    This is the base for drawable rich text objects.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextBuffer, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextObject: public wxObject
{
    DECLARE_CLASS(wxRichTextObject)
public:
    /**
        Constructor, taking an optional parent pointer.
    */
    wxRichTextObject(wxRichTextObject* parent = NULL);

    virtual ~wxRichTextObject();

// Overridables

    /**
        Draw the item, within the given range. Some objects may ignore the range (for
        example paragraphs) while others must obey it (lines, to implement wrapping)
    */
    virtual bool Draw(wxDC& dc, wxRichTextDrawingContext& context, const wxRichTextRange& range, const wxRichTextSelection& selection, const wxRect& rect, int descent, int style) = 0;

    /**
        Lay the item out at the specified position with the given size constraint.
        Layout must set the cached size. @rect is the available space for the object,
        and @a parentRect is the container that is used to determine a relative size
        or position (for example if a text box must be 50% of the parent text box).
    */
    virtual bool Layout(wxDC& dc, wxRichTextDrawingContext& context, const wxRect& rect, const wxRect& parentRect, int style) = 0;

    /**
        Hit-testing: returns a flag indicating hit test details, plus
        information about position. @a contextObj is returned to specify what object
        position is relevant to, since otherwise there's an ambiguity.
        @ obj might not be a child of @a contextObj, since we may be referring to the container itself
        if we have no hit on a child - for example if we click outside an object.

        The function puts the position in @a textPosition if one is found.
        @a pt is in logical units (a zero y position is at the beginning of the buffer).

        Pass wxRICHTEXT_HITTEST_NO_NESTED_OBJECTS if you only want to consider objects
        directly under the object you are calling HitTest on. Otherwise, it will recurse
        and potentially find a nested object.

        @return One of the ::wxRichTextHitTestFlags values.
    */

    virtual int HitTest(wxDC& dc, wxRichTextDrawingContext& context, const wxPoint& pt, long& textPosition, wxRichTextObject** obj, wxRichTextObject** contextObj, int flags = 0);

    /**
        Finds the absolute position and row height for the given character position.
    */
    virtual bool FindPosition(wxDC& WXUNUSED(dc), wxRichTextDrawingContext& WXUNUSED(context), long WXUNUSED(index), wxPoint& WXUNUSED(pt), int* WXUNUSED(height), bool WXUNUSED(forceLineStart)) { return false; }

    /**
        Returns the best size, i.e. the ideal starting size for this object irrespective
        of available space. For a short text string, it will be the size that exactly encloses
        the text. For a longer string, it might use the parent width for example.
    */
    virtual wxSize GetBestSize() const { return m_size; }

    /**
        Returns the object size for the given range. Returns @false if the range
        is invalid for this object.
    */

    virtual bool GetRangeSize(const wxRichTextRange& range, wxSize& size, int& descent, wxDC& dc, wxRichTextDrawingContext& context, int flags, const wxPoint& position = wxPoint(0,0), const wxSize& parentSize = wxDefaultSize, wxArrayInt* partialExtents = NULL) const  = 0;

    /**
        Do a split from @a pos, returning an object containing the second part, and setting
        the first part in 'this'.
    */
    virtual wxRichTextObject* DoSplit(long WXUNUSED(pos)) { return NULL; }

    /**
        Calculates the range of the object. By default, guess that the object is 1 unit long.
    */
    virtual void CalculateRange(long start, long& end) { end = start ; m_range.SetRange(start, end); }

    /**
        Deletes the given range.
    */
    virtual bool DeleteRange(const wxRichTextRange& WXUNUSED(range)) { return false; }

    /**
        Returns @true if the object is empty.
    */
    virtual bool IsEmpty() const { return false; }

    /**
        Returns @true if this class of object is floatable.
    */
    virtual bool IsFloatable() const { return false; }

    /**
        Returns @true if this object is currently floating.
    */
    virtual bool IsFloating() const { return GetAttributes().GetTextBoxAttr().IsFloating(); }

    /**
        Returns the floating direction.
    */
    virtual int GetFloatDirection() const { return GetAttributes().GetTextBoxAttr().GetFloatMode(); }

    /**
        Returns any text in this object for the given range.
    */
    virtual wxString GetTextForRange(const wxRichTextRange& WXUNUSED(range)) const { return wxEmptyString; }

    /**
        Returns @true if this object can merge itself with the given one.
    */
    virtual bool CanMerge(wxRichTextObject* WXUNUSED(object), wxRichTextDrawingContext& WXUNUSED(context)) const { return false; }

    /**
        Returns @true if this object merged itself with the given one.
        The calling code will then delete the given object.
    */
    virtual bool Merge(wxRichTextObject* WXUNUSED(object), wxRichTextDrawingContext& WXUNUSED(context)) { return false; }

    /**
        JACS
        Returns @true if this object can potentially be split, by virtue of having
        different virtual attributes for individual sub-objects.
    */
    virtual bool CanSplit(wxRichTextDrawingContext& WXUNUSED(context)) const { return false; }

    /**
        Returns the final object in the split objects if this object was split due to differences between sub-object virtual attributes.
        Returns itself if it was not split.
    */
    virtual wxRichTextObject* Split(wxRichTextDrawingContext& WXUNUSED(context)) { return this; }

    /**
        Dump object data to the given output stream for debugging.
    */
    virtual void Dump(wxTextOutputStream& stream);

    /**
        Returns @true if we can edit the object's properties via a GUI.
    */
    virtual bool CanEditProperties() const { return false; }

    /**
        Edits the object's properties via a GUI.
    */
    virtual bool EditProperties(wxWindow* WXUNUSED(parent), wxRichTextBuffer* WXUNUSED(buffer)) { return false; }

    /**
        Returns the label to be used for the properties context menu item.
    */
    virtual wxString GetPropertiesMenuLabel() const { return wxEmptyString; }

    /**
        Returns @true if objects of this class can accept the focus, i.e. a call to SetFocusObject
        is possible. For example, containers supporting text, such as a text box object, can accept the focus,
        but a table can't (set the focus to individual cells instead).
    */
    virtual bool AcceptsFocus() const { return false; }

#if wxUSE_XML
    /**
        Imports this object from XML.
    */
    virtual bool ImportFromXML(wxRichTextBuffer* buffer, wxXmlNode* node, wxRichTextXMLHandler* handler, bool* recurse);
#endif

#if wxRICHTEXT_HAVE_DIRECT_OUTPUT
    /**
        Exports this object directly to the given stream, bypassing the creation of a wxXmlNode hierarchy.
        This method is considerably faster than creating a tree first. However, both versions of ExportXML must be
        implemented so that if the tree method is made efficient in the future, we can deprecate the
        more verbose direct output method. Compiled only if wxRICHTEXT_HAVE_DIRECT_OUTPUT is defined (on by default).
    */
    virtual bool ExportXML(wxOutputStream& stream, int indent, wxRichTextXMLHandler* handler);
#endif

#if wxRICHTEXT_HAVE_XMLDOCUMENT_OUTPUT
    /**
        Exports this object to the given parent node, usually creating at least one child node.
        This method is less efficient than the direct-to-stream method but is retained to allow for
        switching to this method if we make it more efficient. Compiled only if wxRICHTEXT_HAVE_XMLDOCUMENT_OUTPUT is defined
        (on by default).
    */
    virtual bool ExportXML(wxXmlNode* parent, wxRichTextXMLHandler* handler);
#endif

    /**
        Returns @true if this object takes note of paragraph attributes (text and image objects don't).
    */
    virtual bool UsesParagraphAttributes() const { return true; }

    /**
        Returns the XML node name of this object. This must be overridden for wxXmlNode-base XML export to work.
    */
    virtual wxString GetXMLNodeName() const { return wxT("unknown"); }

    /**
        Invalidates the object at the given range. With no argument, invalidates the whole object.
    */
    virtual void Invalidate(const wxRichTextRange& invalidRange = wxRICHTEXT_ALL);

    /**
        Returns @true if this object can handle the selections of its children, fOr example a table.
        Required for composite selection handling to work.
    */
    virtual bool HandlesChildSelections() const { return false; }

    /**
        Returns a selection object specifying the selections between start and end character positions.
        For example, a table would deduce what cells (of range length 1) are selected when dragging across the table.
    */
    virtual wxRichTextSelection GetSelection(long WXUNUSED(start), long WXUNUSED(end)) const { return wxRichTextSelection(); }

// Accessors

    /**
        Gets the cached object size as calculated by Layout.
    */
    virtual wxSize GetCachedSize() const { return m_size; }

    /**
        Sets the cached object size as calculated by Layout.
    */
    virtual void SetCachedSize(const wxSize& sz) { m_size = sz; }

    /**
        Gets the maximum object size as calculated by Layout. This allows
        us to fit an object to its contents or allocate extra space if required.
    */
    virtual wxSize GetMaxSize() const { return m_maxSize; }

    /**
        Sets the maximum object size as calculated by Layout. This allows
        us to fit an object to its contents or allocate extra space if required.
    */
    virtual void SetMaxSize(const wxSize& sz) { m_maxSize = sz; }

    /**
        Gets the minimum object size as calculated by Layout. This allows
        us to constrain an object to its absolute minimum size if necessary.
    */
    virtual wxSize GetMinSize() const { return m_minSize; }

    /**
        Sets the minimum object size as calculated by Layout. This allows
        us to constrain an object to its absolute minimum size if necessary.
    */
    virtual void SetMinSize(const wxSize& sz) { m_minSize = sz; }

    /**
        Gets the 'natural' size for an object. For an image, it would be the
        image size.
    */
    virtual wxTextAttrSize GetNaturalSize() const { return wxTextAttrSize(); }

    /**
        Returns the object position in pixels.
    */
    virtual wxPoint GetPosition() const { return m_pos; }

    /**
        Sets the object position in pixels.
    */
    virtual void SetPosition(const wxPoint& pos) { m_pos = pos; }

    /**
        Returns the absolute object position, by traversing up the child/parent hierarchy.
        TODO: may not be needed, if all object positions are in fact relative to the
        top of the coordinate space.
    */
    virtual wxPoint GetAbsolutePosition() const;

    /**
        Returns the rectangle enclosing the object.
    */
    virtual wxRect GetRect() const { return wxRect(GetPosition(), GetCachedSize()); }

    /**
        Sets the object's range within its container.
    */
    void SetRange(const wxRichTextRange& range) { m_range = range; }

    /**
        Returns the object's range.
    */
    const wxRichTextRange& GetRange() const { return m_range; }

    /**
        Returns the object's range.
    */
    wxRichTextRange& GetRange() { return m_range; }

    /**
        Set the object's own range, for a top-level object with its own position space.
    */
    void SetOwnRange(const wxRichTextRange& range) { m_ownRange = range; }

    /**
        Returns the object's own range (valid if top-level).
    */
    const wxRichTextRange& GetOwnRange() const { return m_ownRange; }

    /**
        Returns the object's own range (valid if top-level).
    */
    wxRichTextRange& GetOwnRange() { return m_ownRange; }

    /**
        Returns the object's own range only if a top-level object.
    */
    wxRichTextRange GetOwnRangeIfTopLevel() const { return IsTopLevel() ? m_ownRange : m_range; }

    /**
        Returns @true if this object is composite.
    */
    virtual bool IsComposite() const { return false; }

    /**
        Returns @true if no user editing can be done inside the object. This returns @true for simple objects,
        @false for most composite objects, but @true for fields, which if composite, should not be user-edited.
    */
    virtual bool IsAtomic() const { return true; }

    /**
        Returns a pointer to the parent object.
    */
    virtual wxRichTextObject* GetParent() const { return m_parent; }

    /**
        Sets the pointer to the parent object.
    */
    virtual void SetParent(wxRichTextObject* parent) { m_parent = parent; }

    /**
        Returns the top-level container of this object.
        May return itself if it's a container; use GetParentContainer to return
        a different container.
    */
    virtual wxRichTextParagraphLayoutBox* GetContainer() const;

    /**
        Returns the top-level container of this object.
        Returns a different container than itself, unless there's no parent, in which case it will return NULL.
    */
    virtual wxRichTextParagraphLayoutBox* GetParentContainer() const { return GetParent() ? GetParent()->GetContainer() : GetContainer(); }

    /**
        Set the margin around the object, in pixels.
    */
    virtual void SetMargins(int margin);

    /**
        Set the margin around the object, in pixels.
    */
    virtual void SetMargins(int leftMargin, int rightMargin, int topMargin, int bottomMargin);

    /**
        Returns the left margin of the object, in pixels.
    */
    virtual int GetLeftMargin() const;

    /**
        Returns the right margin of the object, in pixels.
    */
    virtual int GetRightMargin() const;

    /**
        Returns the top margin of the object, in pixels.
    */
    virtual int GetTopMargin() const;

    /**
        Returns the bottom margin of the object, in pixels.
    */
    virtual int GetBottomMargin() const;

    /**
        Calculates the available content space in the given rectangle, given the
        margins, border and padding specified in the object's attributes.
    */
    virtual wxRect GetAvailableContentArea(wxDC& dc, wxRichTextDrawingContext& context, const wxRect& outerRect) const;

    /**
        Lays out the object first with a given amount of space, and then if no width was specified in attr,
        lays out the object again using the minimum size. @a availableParentSpace is the maximum space
        for the object, whereas @a availableContainerSpace is the container with which relative positions and
        sizes should be computed. For example, a text box whose space has already been constrained
        in a previous layout pass to @a availableParentSpace, but should have a width of 50% of @a availableContainerSpace.
        (If these two rects were the same, a 2nd pass could see the object getting too small.)
    */
    virtual bool LayoutToBestSize(wxDC& dc, wxRichTextDrawingContext& context, wxRichTextBuffer* buffer,
                    const wxRichTextAttr& parentAttr, const wxRichTextAttr& attr,
                    const wxRect& availableParentSpace, const wxRect& availableContainerSpace, int style);

    /**
        Adjusts the attributes for virtual attribute provision, collapsed borders, etc.
    */
    virtual bool AdjustAttributes(wxRichTextAttr& attr, wxRichTextDrawingContext& context);

    /**
        Sets the object's attributes.
    */
    void SetAttributes(const wxRichTextAttr& attr) { m_attributes = attr; }

    /**
        Returns the object's attributes.
    */
    const wxRichTextAttr& GetAttributes() const { return m_attributes; }

    /**
        Returns the object's attributes.
    */
    wxRichTextAttr& GetAttributes() { return m_attributes; }

    /**
        Returns the object's properties.
    */
    wxRichTextProperties& GetProperties() { return m_properties; }

    /**
        Returns the object's properties.
    */
    const wxRichTextProperties& GetProperties() const { return m_properties; }

    /**
        Sets the object's properties.
    */
    void SetProperties(const wxRichTextProperties& props) { m_properties = props; }

    /**
        Sets the stored descent value.
    */
    void SetDescent(int descent) { m_descent = descent; }

    /**
        Returns the stored descent value.
    */
    int GetDescent() const { return m_descent; }

    /**
        Returns the containing buffer.
    */
    wxRichTextBuffer* GetBuffer() const;

    /**
        Sets the identifying name for this object as a property using the "name" key.
    */
    void SetName(const wxString& name) { m_properties.SetProperty(wxT("name"), name); }

    /**
        Returns the identifying name for this object from the properties, using the "name" key.
    */
    wxString GetName() const { return m_properties.GetPropertyString(wxT("name")); }

    /**
        Returns @true if this object is top-level, i.e. contains its own paragraphs, such as a text box.
    */
    virtual bool IsTopLevel() const { return false; }

    /**
        Returns @true if the object will be shown, @false otherwise.
    */
    bool IsShown() const { return m_show; }

// Operations

    /**
        Call to show or hide this object. This function does not cause the content to be
        laid out or redrawn.
    */
    virtual void Show(bool show) { m_show = show; }

    /**
        Clones the object.
    */
    virtual wxRichTextObject* Clone() const { return NULL; }

    /**
        Copies the object.
    */
    void Copy(const wxRichTextObject& obj);

    /**
        Reference-counting allows us to use the same object in multiple
        lists (not yet used).
    */

    void Reference() { m_refCount ++; }

    /**
        Reference-counting allows us to use the same object in multiple
        lists (not yet used).
    */
    void Dereference();

    /**
        Moves the object recursively, by adding the offset from old to new.
    */
    virtual void Move(const wxPoint& pt);

    /**
        Converts units in tenths of a millimetre to device units.
    */
    int ConvertTenthsMMToPixels(wxDC& dc, int units) const;

    /**
        Converts units in tenths of a millimetre to device units.
    */
    static int ConvertTenthsMMToPixels(int ppi, int units, double scale = 1.0);

    /**
        Convert units in pixels to tenths of a millimetre.
    */
    int ConvertPixelsToTenthsMM(wxDC& dc, int pixels) const;

    /**
        Convert units in pixels to tenths of a millimetre.
    */
    static int ConvertPixelsToTenthsMM(int ppi, int pixels, double scale = 1.0);

    /**
        Draws the borders and background for the given rectangle and attributes.
        @a boxRect is taken to be the outer margin box, not the box around the content.
    */
    static bool DrawBoxAttributes(wxDC& dc, wxRichTextBuffer* buffer, const wxRichTextAttr& attr, const wxRect& boxRect, int flags = 0, wxRichTextObject* obj = NULL);

    /**
        Draws a border.
    */
    static bool DrawBorder(wxDC& dc, wxRichTextBuffer* buffer, const wxTextAttrBorders& attr, const wxRect& rect, int flags = 0);

    /**
        Returns the various rectangles of the box model in pixels. You can either specify @a contentRect (inner)
        or @a marginRect (outer), and the other must be the default rectangle (no width or height).
        Note that the outline doesn't affect the position of the rectangle, it's drawn in whatever space
        is available.
    */
    static bool GetBoxRects(wxDC& dc, wxRichTextBuffer* buffer, const wxRichTextAttr& attr, wxRect& marginRect, wxRect& borderRect, wxRect& contentRect, wxRect& paddingRect, wxRect& outlineRect);

    /**
        Returns the total margin for the object in pixels, taking into account margin, padding and border size.
    */
    static bool GetTotalMargin(wxDC& dc, wxRichTextBuffer* buffer, const wxRichTextAttr& attr, int& leftMargin, int& rightMargin,
        int& topMargin, int& bottomMargin);

    /**
        Returns the rectangle which the child has available to it given restrictions specified in the
        child attribute, e.g. 50% width of the parent, 400 pixels, x position 20% of the parent, etc.
        availableContainerSpace might be a parent that the cell has to compute its width relative to.
        E.g. a cell that's 50% of its parent.
    */
    static wxRect AdjustAvailableSpace(wxDC& dc, wxRichTextBuffer* buffer, const wxRichTextAttr& parentAttr, const wxRichTextAttr& childAttr,
        const wxRect& availableParentSpace, const wxRect& availableContainerSpace);

protected:
    wxSize                  m_size;
    wxSize                  m_maxSize;
    wxSize                  m_minSize;
    wxPoint                 m_pos;
    int                     m_descent; // Descent for this object (if any)
    int                     m_refCount;
    bool                    m_show;
    wxRichTextObject*       m_parent;

    // The range of this object (start position to end position)
    wxRichTextRange         m_range;

    // The internal range of this object, if it's a top-level object with its own range space
    wxRichTextRange         m_ownRange;

    // Attributes
    wxRichTextAttr          m_attributes;

    // Properties
    wxRichTextProperties    m_properties;
};

WX_DECLARE_LIST_WITH_DECL( wxRichTextObject, wxRichTextObjectList, class WXDLLIMPEXP_RICHTEXT );

/**
    @class wxRichTextCompositeObject

    Objects of this class can contain other objects.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextObject, wxRichTextBuffer, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextCompositeObject: public wxRichTextObject
{
    DECLARE_CLASS(wxRichTextCompositeObject)
public:
// Constructors

    wxRichTextCompositeObject(wxRichTextObject* parent = NULL);
    virtual ~wxRichTextCompositeObject();

// Overridables

    virtual int HitTest(wxDC& dc, wxRichTextDrawingContext& context, const wxPoint& pt, long& textPosition, wxRichTextObject** obj, wxRichTextObject** contextObj, int flags = 0);

    virtual bool FindPosition(wxDC& dc, wxRichTextDrawingContext& context, long index, wxPoint& pt, int* height, bool forceLineStart);

    virtual void CalculateRange(long start, long& end);

    virtual bool DeleteRange(const wxRichTextRange& range);

    virtual wxString GetTextForRange(const wxRichTextRange& range) const;

    virtual bool GetRangeSize(const wxRichTextRange& range, wxSize& size, int& descent, wxDC& dc, wxRichTextDrawingContext& context, int flags, const wxPoint& position = wxPoint(0,0), const wxSize& parentSize = wxDefaultSize, wxArrayInt* partialExtents = NULL) const;

    virtual void Dump(wxTextOutputStream& stream);

    virtual void Invalidate(const wxRichTextRange& invalidRange = wxRICHTEXT_ALL);

// Accessors

    /**
        Returns the children.
    */
    wxRichTextObjectList& GetChildren() { return m_children; }
    /**
        Returns the children.
    */
    const wxRichTextObjectList& GetChildren() const { return m_children; }

    /**
        Returns the number of children.
    */
    size_t GetChildCount() const ;

    /**
        Returns the nth child.
    */
    wxRichTextObject* GetChild(size_t n) const ;

    /**
        Returns @true if this object is composite.
    */
    virtual bool IsComposite() const { return true; }

    /**
        Returns @true if no user editing can be done inside the object. This returns @true for simple objects,
        @false for most composite objects, but @true for fields, which if composite, should not be user-edited.
    */
    virtual bool IsAtomic() const { return false; }

    /**
        Returns true if the buffer is empty.
    */
    virtual bool IsEmpty() const { return GetChildCount() == 0; }

    /**
        Returns the child object at the given character position.
    */
    virtual wxRichTextObject* GetChildAtPosition(long pos) const;

// Operations

    void Copy(const wxRichTextCompositeObject& obj);

    void operator= (const wxRichTextCompositeObject& obj) { Copy(obj); }

    /**
        Appends a child, returning the position.
    */
    size_t AppendChild(wxRichTextObject* child) ;

    /**
        Inserts the child in front of the given object, or at the beginning.
    */
    bool InsertChild(wxRichTextObject* child, wxRichTextObject* inFrontOf) ;

    /**
        Removes and optionally deletes the specified child.
    */
    bool RemoveChild(wxRichTextObject* child, bool deleteChild = false) ;

    /**
        Deletes all the children.
    */
    bool DeleteChildren() ;

    /**
        Recursively merges all pieces that can be merged.
    */
    bool Defragment(wxRichTextDrawingContext& context, const wxRichTextRange& range = wxRICHTEXT_ALL);

    /**
        Moves the object recursively, by adding the offset from old to new.
    */
    virtual void Move(const wxPoint& pt);

protected:
    wxRichTextObjectList    m_children;
};

/**
    @class wxRichTextParagraphLayoutBox

    This class knows how to lay out paragraphs.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextCompositeObject, wxRichTextObject, wxRichTextBuffer, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextParagraphLayoutBox: public wxRichTextCompositeObject
{
    DECLARE_DYNAMIC_CLASS(wxRichTextParagraphLayoutBox)
public:
// Constructors

    wxRichTextParagraphLayoutBox(wxRichTextObject* parent = NULL);
    wxRichTextParagraphLayoutBox(const wxRichTextParagraphLayoutBox& obj): wxRichTextCompositeObject() { Init(); Copy(obj); }
    ~wxRichTextParagraphLayoutBox();

// Overridables

    virtual int HitTest(wxDC& dc, wxRichTextDrawingContext& context, const wxPoint& pt, long& textPosition, wxRichTextObject** obj, wxRichTextObject** contextObj, int flags = 0);

    virtual bool Draw(wxDC& dc, wxRichTextDrawingContext& context, const wxRichTextRange& range, const wxRichTextSelection& selection, const wxRect& rect, int descent, int style);

    virtual bool Layout(wxDC& dc, wxRichTextDrawingContext& context, const wxRect& rect, const wxRect& parentRect, int style);

    virtual bool GetRangeSize(const wxRichTextRange& range, wxSize& size, int& descent, wxDC& dc, wxRichTextDrawingContext& context, int flags, const wxPoint& position = wxPoint(0,0), const wxSize& parentSize = wxDefaultSize, wxArrayInt* partialExtents = NULL) const;

    virtual bool DeleteRange(const wxRichTextRange& range);

    virtual wxString GetTextForRange(const wxRichTextRange& range) const;

#if wxUSE_XML
    virtual bool ImportFromXML(wxRichTextBuffer* buffer, wxXmlNode* node, wxRichTextXMLHandler* handler, bool* recurse);
#endif

#if wxRICHTEXT_HAVE_DIRECT_OUTPUT
    virtual bool ExportXML(wxOutputStream& stream, int indent, wxRichTextXMLHandler* handler);
#endif

#if wxRICHTEXT_HAVE_XMLDOCUMENT_OUTPUT
    virtual bool ExportXML(wxXmlNode* parent, wxRichTextXMLHandler* handler);
#endif

    virtual wxString GetXMLNodeName() const { return wxT("paragraphlayout"); }

    virtual bool AcceptsFocus() const { return true; }

// Accessors

    /**
        Associates a control with the buffer, for operations that for example require refreshing the window.
    */
    void SetRichTextCtrl(wxRichTextCtrl* ctrl) { m_ctrl = ctrl; }

    /**
        Returns the associated control.
    */
    wxRichTextCtrl* GetRichTextCtrl() const { return m_ctrl; }

    /**
        Sets a flag indicating whether the last paragraph is partial or complete.
    */
    void SetPartialParagraph(bool partialPara) { m_partialParagraph = partialPara; }

    /**
        Returns a flag indicating whether the last paragraph is partial or complete.
    */
    bool GetPartialParagraph() const { return m_partialParagraph; }

    /**
        Returns the style sheet associated with the overall buffer.
    */
    virtual wxRichTextStyleSheet* GetStyleSheet() const;

    virtual bool IsTopLevel() const { return true; }

// Operations

    /**
        Submits a command to insert paragraphs.
    */
    bool InsertParagraphsWithUndo(wxRichTextBuffer* buffer, long pos, const wxRichTextParagraphLayoutBox& paragraphs, wxRichTextCtrl* ctrl, int flags = 0);

    /**
        Submits a command to insert the given text.
    */
    bool InsertTextWithUndo(wxRichTextBuffer* buffer, long pos, const wxString& text, wxRichTextCtrl* ctrl, int flags = 0);

    /**
        Submits a command to insert the given text.
    */
    bool InsertNewlineWithUndo(wxRichTextBuffer* buffer, long pos, wxRichTextCtrl* ctrl, int flags = 0);

    /**
        Submits a command to insert the given image.
    */
    bool InsertImageWithUndo(wxRichTextBuffer* buffer, long pos, const wxRichTextImageBlock& imageBlock,
                                                        wxRichTextCtrl* ctrl, int flags, const wxRichTextAttr& textAttr);

    /**
        Submits a command to insert the given field. Field data can be included in properties.

        @see wxRichTextField, wxRichTextFieldType, wxRichTextFieldTypeStandard
    */
    wxRichTextField* InsertFieldWithUndo(wxRichTextBuffer* buffer, long pos, const wxString& fieldType,
                                                        const wxRichTextProperties& properties,
                                                        wxRichTextCtrl* ctrl, int flags,
                                                        const wxRichTextAttr& textAttr);

    /**
        Returns the style that is appropriate for a new paragraph at this position.
        If the previous paragraph has a paragraph style name, looks up the next-paragraph
        style.
    */
    wxRichTextAttr GetStyleForNewParagraph(wxRichTextBuffer* buffer, long pos, bool caretPosition = false, bool lookUpNewParaStyle=false) const;

    /**
        Inserts an object.
    */
    wxRichTextObject* InsertObjectWithUndo(wxRichTextBuffer* buffer, long pos, wxRichTextObject *object, wxRichTextCtrl* ctrl, int flags = 0);

    /**
        Submits a command to delete this range.
    */
    bool DeleteRangeWithUndo(const wxRichTextRange& range, wxRichTextCtrl* ctrl, wxRichTextBuffer* buffer);

    /**
        Draws the floating objects in this buffer.
    */
    void DrawFloats(wxDC& dc, wxRichTextDrawingContext& context, const wxRichTextRange& range, const wxRichTextSelection& selection, const wxRect& rect, int descent, int style);

    /**
        Moves an anchored object to another paragraph.
    */
    void MoveAnchoredObjectToParagraph(wxRichTextParagraph* from, wxRichTextParagraph* to, wxRichTextObject* obj);

    /**
        Initializes the object.
    */
    void Init();

    /**
        Clears all the children.
    */
    virtual void Clear();

    /**
        Clears and initializes with one blank paragraph.
    */
    virtual void Reset();

    /**
        Convenience function to add a paragraph of text.
    */
    virtual wxRichTextRange AddParagraph(const wxString& text, wxRichTextAttr* paraStyle = NULL);

    /**
        Convenience function to add an image.
    */
    virtual wxRichTextRange AddImage(const wxImage& image, wxRichTextAttr* paraStyle = NULL);

    /**
        Adds multiple paragraphs, based on newlines.
    */
    virtual wxRichTextRange AddParagraphs(const wxString& text, wxRichTextAttr* paraStyle = NULL);

    /**
        Returns the line at the given position. If @a caretPosition is true, the position is
        a caret position, which is normally a smaller number.
    */
    virtual wxRichTextLine* GetLineAtPosition(long pos, bool caretPosition = false) const;

    /**
        Returns the line at the given y pixel position, or the last line.
    */
    virtual wxRichTextLine* GetLineAtYPosition(int y) const;

    /**
        Returns the paragraph at the given character or caret position.
    */
    virtual wxRichTextParagraph* GetParagraphAtPosition(long pos, bool caretPosition = false) const;

    /**
        Returns the line size at the given position.
    */
    virtual wxSize GetLineSizeAtPosition(long pos, bool caretPosition = false) const;

    /**
        Given a position, returns the number of the visible line (potentially many to a paragraph),
        starting from zero at the start of the buffer. We also have to pass a bool (@a startOfLine)
        that indicates whether the caret is being shown at the end of the previous line or at the start
        of the next, since the caret can be shown at two visible positions for the same underlying
        position.
    */
    virtual long GetVisibleLineNumber(long pos, bool caretPosition = false, bool startOfLine = false) const;

    /**
        Given a line number, returns the corresponding wxRichTextLine object.
    */
    virtual wxRichTextLine* GetLineForVisibleLineNumber(long lineNumber) const;

    /**
        Returns the leaf object in a paragraph at this position.
    */
    virtual wxRichTextObject* GetLeafObjectAtPosition(long position) const;

    /**
        Returns the paragraph by number.
    */
    virtual wxRichTextParagraph* GetParagraphAtLine(long paragraphNumber) const;

    /**
        Returns the paragraph for a given line.
    */
    virtual wxRichTextParagraph* GetParagraphForLine(wxRichTextLine* line) const;

    /**
        Returns the length of the paragraph.
    */
    virtual int GetParagraphLength(long paragraphNumber) const;

    /**
        Returns the number of paragraphs.
    */
    virtual int GetParagraphCount() const { return static_cast<int>(GetChildCount()); }

    /**
        Returns the number of visible lines.
    */
    virtual int GetLineCount() const;

    /**
        Returns the text of the paragraph.
    */
    virtual wxString GetParagraphText(long paragraphNumber) const;

    /**
        Converts zero-based line column and paragraph number to a position.
    */
    virtual long XYToPosition(long x, long y) const;

    /**
        Converts a zero-based position to line column and paragraph number.
    */
    virtual bool PositionToXY(long pos, long* x, long* y) const;

    /**
        Sets the attributes for the given range. Pass flags to determine how the
        attributes are set.

        The end point of range is specified as the last character position of the span
        of text. So, for example, to set the style for a character at position 5,
        use the range (5,5).
        This differs from the wxRichTextCtrl API, where you would specify (5,6).

        @a flags may contain a bit list of the following values:
        - wxRICHTEXT_SETSTYLE_NONE: no style flag.
        - wxRICHTEXT_SETSTYLE_WITH_UNDO: specifies that this operation should be
          undoable.
        - wxRICHTEXT_SETSTYLE_OPTIMIZE: specifies that the style should not be applied
          if the combined style at this point is already the style in question.
        - wxRICHTEXT_SETSTYLE_PARAGRAPHS_ONLY: specifies that the style should only be
          applied to paragraphs, and not the content.
          This allows content styling to be preserved independently from that
          of e.g. a named paragraph style.
        - wxRICHTEXT_SETSTYLE_CHARACTERS_ONLY: specifies that the style should only be
          applied to characters, and not the paragraph.
          This allows content styling to be preserved independently from that
          of e.g. a named paragraph style.
        - wxRICHTEXT_SETSTYLE_RESET: resets (clears) the existing style before applying
          the new style.
        - wxRICHTEXT_SETSTYLE_REMOVE: removes the specified style.
          Only the style flags are used in this operation.
    */
    virtual bool SetStyle(const wxRichTextRange& range, const wxRichTextAttr& style, int flags = wxRICHTEXT_SETSTYLE_WITH_UNDO);

    /**
        Sets the attributes for the given object only, for example the box attributes for a text box.
    */
    virtual void SetStyle(wxRichTextObject *obj, const wxRichTextAttr& textAttr, int flags = wxRICHTEXT_SETSTYLE_WITH_UNDO);

    /**
        Returns the combined text attributes for this position.

        This function gets the @e uncombined style - that is, the attributes associated
        with the paragraph or character content, and not necessarily the combined
        attributes you see on the screen. To get the combined attributes, use GetStyle().
        If you specify (any) paragraph attribute in @e style's flags, this function
        will fetch the paragraph attributes.
        Otherwise, it will return the character attributes.
    */
    virtual bool GetStyle(long position, wxRichTextAttr& style);

    /**
        Returns the content (uncombined) attributes for this position.
    */
    virtual bool GetUncombinedStyle(long position, wxRichTextAttr& style);

    /**
        Implementation helper for GetStyle. If combineStyles is true, combine base, paragraph and
        context attributes.
    */
    virtual bool DoGetStyle(long position, wxRichTextAttr& style, bool combineStyles = true);

    /**
        This function gets a style representing the common, combined attributes in the
        given range.
        Attributes which have different values within the specified range will not be
        included the style flags.

        The function is used to get the attributes to display in the formatting dialog:
        the user can edit the attributes common to the selection, and optionally specify the
        values of further attributes to be applied uniformly.

        To apply the edited attributes, you can use SetStyle() specifying
        the wxRICHTEXT_SETSTYLE_OPTIMIZE flag, which will only apply attributes that
        are different from the @e combined attributes within the range.
        So, the user edits the effective, displayed attributes for the range,
        but his choice won't be applied unnecessarily to content. As an example,
        say the style for a paragraph specifies bold, but the paragraph text doesn't
        specify a weight.
        The combined style is bold, and this is what the user will see on-screen and
        in the formatting dialog. The user now specifies red text, in addition to bold.
        When applying with SetStyle(), the content font weight attributes won't be
        changed to bold because this is already specified by the paragraph.
        However the text colour attributes @e will be changed to show red.
    */
    virtual bool GetStyleForRange(const wxRichTextRange& range, wxRichTextAttr& style);

    /**
        Combines @a style with @a currentStyle for the purpose of summarising the attributes of a range of
        content.
    */
    bool CollectStyle(wxRichTextAttr& currentStyle, const wxRichTextAttr& style, wxRichTextAttr& clashingAttr, wxRichTextAttr& absentAttr);

    //@{
    /**
        Sets the list attributes for the given range, passing flags to determine how
        the attributes are set.
        Either the style definition or the name of the style definition (in the current
        sheet) can be passed.

        @a flags is a bit list of the following:
        - wxRICHTEXT_SETSTYLE_WITH_UNDO: specifies that this command will be undoable.
        - wxRICHTEXT_SETSTYLE_RENUMBER: specifies that numbering should start from
          @a startFrom, otherwise existing attributes are used.
        - wxRICHTEXT_SETSTYLE_SPECIFY_LEVEL: specifies that @a listLevel should be used
          as the level for all paragraphs, otherwise the current indentation will be used.

        @see NumberList(), PromoteList(), ClearListStyle().
    */
    virtual bool SetListStyle(const wxRichTextRange& range, wxRichTextListStyleDefinition* def, int flags = wxRICHTEXT_SETSTYLE_WITH_UNDO, int startFrom = 1, int specifiedLevel = -1);
    virtual bool SetListStyle(const wxRichTextRange& range, const wxString& defName, int flags = wxRICHTEXT_SETSTYLE_WITH_UNDO, int startFrom = 1, int specifiedLevel = -1);
    //@}

    /**
        Clears the list style from the given range, clearing list-related attributes
        and applying any named paragraph style associated with each paragraph.

        @a flags is a bit list of the following:
        - wxRICHTEXT_SETSTYLE_WITH_UNDO: specifies that this command will be undoable.

        @see SetListStyle(), PromoteList(), NumberList()
    */
    virtual bool ClearListStyle(const wxRichTextRange& range, int flags = wxRICHTEXT_SETSTYLE_WITH_UNDO);

    //@{
    /**
        Numbers the paragraphs in the given range.

        Pass flags to determine how the attributes are set.
        Either the style definition or the name of the style definition (in the current
        sheet) can be passed.

        @a flags is a bit list of the following:
        - wxRICHTEXT_SETSTYLE_WITH_UNDO: specifies that this command will be undoable.
        - wxRICHTEXT_SETSTYLE_RENUMBER: specifies that numbering should start from
          @a startFrom, otherwise existing attributes are used.
        - wxRICHTEXT_SETSTYLE_SPECIFY_LEVEL: specifies that @a listLevel should be used
          as the level for all paragraphs, otherwise the current indentation will be used.

        @a def can be NULL to indicate that the existing list style should be used.

        @see SetListStyle(), PromoteList(), ClearListStyle()
    */
    virtual bool NumberList(const wxRichTextRange& range, wxRichTextListStyleDefinition* def = NULL, int flags = wxRICHTEXT_SETSTYLE_WITH_UNDO, int startFrom = 1, int specifiedLevel = -1);
    virtual bool NumberList(const wxRichTextRange& range, const wxString& defName, int flags = wxRICHTEXT_SETSTYLE_WITH_UNDO, int startFrom = 1, int specifiedLevel = -1);
    //@}

    //@{
    /**
        Promotes the list items within the given range.
        A positive @a promoteBy produces a smaller indent, and a negative number
        produces a larger indent. Pass flags to determine how the attributes are set.
        Either the style definition or the name of the style definition (in the current
        sheet) can be passed.

        @a flags is a bit list of the following:
        - wxRICHTEXT_SETSTYLE_WITH_UNDO: specifies that this command will be undoable.
        - wxRICHTEXT_SETSTYLE_RENUMBER: specifies that numbering should start from
          @a startFrom, otherwise existing attributes are used.
        - wxRICHTEXT_SETSTYLE_SPECIFY_LEVEL: specifies that @a listLevel should be used
          as the level for all paragraphs, otherwise the current indentation will be used.

        @see SetListStyle(), SetListStyle(), ClearListStyle()
    */
    virtual bool PromoteList(int promoteBy, const wxRichTextRange& range, wxRichTextListStyleDefinition* def = NULL, int flags = wxRICHTEXT_SETSTYLE_WITH_UNDO, int specifiedLevel = -1);
    virtual bool PromoteList(int promoteBy, const wxRichTextRange& range, const wxString& defName, int flags = wxRICHTEXT_SETSTYLE_WITH_UNDO, int specifiedLevel = -1);
    //@}

    /**
        Helper for NumberList and PromoteList, that does renumbering and promotion simultaneously
        @a def can be NULL/empty to indicate that the existing list style should be used.
    */
    virtual bool DoNumberList(const wxRichTextRange& range, const wxRichTextRange& promotionRange, int promoteBy, wxRichTextListStyleDefinition* def, int flags = wxRICHTEXT_SETSTYLE_WITH_UNDO, int startFrom = 1, int specifiedLevel = -1);

    /**
        Fills in the attributes for numbering a paragraph after previousParagraph.
    */
    virtual bool FindNextParagraphNumber(wxRichTextParagraph* previousParagraph, wxRichTextAttr& attr) const;

    /**
        Sets the properties for the given range, passing flags to determine how the
        attributes are set. You can merge properties or replace them.

        The end point of range is specified as the last character position of the span
        of text, plus one. So, for example, to set the properties for a character at
        position 5, use the range (5,6).

        @a flags may contain a bit list of the following values:
        - wxRICHTEXT_SETPROPERTIES_NONE: no flag.
        - wxRICHTEXT_SETPROPERTIES_WITH_UNDO: specifies that this operation should be
          undoable.
        - wxRICHTEXT_SETPROPERTIES_PARAGRAPHS_ONLY: specifies that the properties should only be
          applied to paragraphs, and not the content.
        - wxRICHTEXT_SETPROPERTIES_CHARACTERS_ONLY: specifies that the properties should only be
          applied to characters, and not the paragraph.
        - wxRICHTEXT_SETPROPERTIES_RESET: resets (clears) the existing properties before applying
          the new properties.
        - wxRICHTEXT_SETPROPERTIES_REMOVE: removes the specified properties.
    */
    virtual bool SetProperties(const wxRichTextRange& range, const wxRichTextProperties& properties, int flags = wxRICHTEXT_SETPROPERTIES_WITH_UNDO);

    /**
        Sets with undo the properties for the given object.
    */
    virtual bool SetObjectPropertiesWithUndo(wxRichTextObject& obj, const wxRichTextProperties& properties, wxRichTextObject* objToSet = NULL);

    /**
        Test if this whole range has character attributes of the specified kind. If any
        of the attributes are different within the range, the test fails. You
        can use this to implement, for example, bold button updating. style must have
        flags indicating which attributes are of interest.
    */
    virtual bool HasCharacterAttributes(const wxRichTextRange& range, const wxRichTextAttr& style) const;

    /**
        Test if this whole range has paragraph attributes of the specified kind. If any
        of the attributes are different within the range, the test fails. You
        can use this to implement, for example, centering button updating. style must have
        flags indicating which attributes are of interest.
    */
    virtual bool HasParagraphAttributes(const wxRichTextRange& range, const wxRichTextAttr& style) const;

    virtual wxRichTextObject* Clone() const { return new wxRichTextParagraphLayoutBox(*this); }

    /**
        Prepares the content just before insertion (or after buffer reset).
        Currently is only called if undo mode is on.
    */
    virtual void PrepareContent(wxRichTextParagraphLayoutBox& container);

    /**
        Insert fragment into this box at the given position. If partialParagraph is true,
        it is assumed that the last (or only) paragraph is just a piece of data with no paragraph
        marker.
    */
    virtual bool InsertFragment(long position, wxRichTextParagraphLayoutBox& fragment);

    /**
        Make a copy of the fragment corresponding to the given range, putting it in @a fragment.
    */
    virtual bool CopyFragment(const wxRichTextRange& range, wxRichTextParagraphLayoutBox& fragment);

    /**
        Apply the style sheet to the buffer, for example if the styles have changed.
    */
    virtual bool ApplyStyleSheet(wxRichTextStyleSheet* styleSheet);

    void Copy(const wxRichTextParagraphLayoutBox& obj);

    void operator= (const wxRichTextParagraphLayoutBox& obj) { Copy(obj); }

    /**
        Calculate ranges.
    */
    virtual void UpdateRanges();

    /**
        Get all the text.
    */
    virtual wxString GetText() const;

    /**
        Sets the default style, affecting the style currently being applied
        (for example, setting the default style to bold will cause subsequently
        inserted text to be bold).

        This is not cumulative - setting the default style will replace the previous
        default style.

        Setting it to a default attribute object makes new content take on the 'basic' style.
    */
    virtual bool SetDefaultStyle(const wxRichTextAttr& style);

    /**
        Returns the current default style, affecting the style currently being applied
        (for example, setting the default style to bold will cause subsequently
        inserted text to be bold).
    */
    virtual const wxRichTextAttr& GetDefaultStyle() const { return m_defaultAttributes; }

    /**
        Sets the basic (overall) style. This is the style of the whole
        buffer before further styles are applied, unlike the default style, which
        only affects the style currently being applied (for example, setting the default
        style to bold will cause subsequently inserted text to be bold).
    */
    virtual void SetBasicStyle(const wxRichTextAttr& style) { m_attributes = style; }

    /**
        Returns the basic (overall) style.

        This is the style of the whole buffer before further styles are applied,
        unlike the default style, which only affects the style currently being
        applied (for example, setting the default style to bold will cause
        subsequently inserted text to be bold).
    */
    virtual const wxRichTextAttr& GetBasicStyle() const { return m_attributes; }

    /**
        Invalidates the buffer. With no argument, invalidates whole buffer.
    */
    virtual void Invalidate(const wxRichTextRange& invalidRange = wxRICHTEXT_ALL);

    /**
        Do the (in)validation for this object only.
    */
    virtual void DoInvalidate(const wxRichTextRange& invalidRange);

    /**
        Do the (in)validation both up and down the hierarchy.
    */
    virtual void InvalidateHierarchy(const wxRichTextRange& invalidRange = wxRICHTEXT_ALL);

    /**
        Gather information about floating objects. If untilObj is non-NULL,
        will stop getting information if the current object is this, since we
        will collect the rest later.
    */
    virtual bool UpdateFloatingObjects(const wxRect& availableRect, wxRichTextObject* untilObj = NULL);

    /**
        Get invalid range, rounding to entire paragraphs if argument is true.
    */
    wxRichTextRange GetInvalidRange(bool wholeParagraphs = false) const;

    /**
        Returns @true if this object needs layout.
    */
    bool IsDirty() const { return m_invalidRange != wxRICHTEXT_NONE; }

    /**
        Returns the wxRichTextFloatCollector of this object.
    */
    wxRichTextFloatCollector* GetFloatCollector() { return m_floatCollector; }

    /**
        Returns the number of floating objects at this level.
    */
    int GetFloatingObjectCount() const;

    /**
        Returns a list of floating objects.
    */
    bool GetFloatingObjects(wxRichTextObjectList& objects) const;

protected:
    wxRichTextCtrl* m_ctrl;
    wxRichTextAttr  m_defaultAttributes;

    // The invalidated range that will need full layout
    wxRichTextRange m_invalidRange;

    // Is the last paragraph partial or complete?
    bool            m_partialParagraph;

    // The floating layout state
    wxRichTextFloatCollector* m_floatCollector;
};

/**
    @class wxRichTextBox

    This class implements a floating or inline text box, containing paragraphs.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextParagraphLayoutBox, wxRichTextObject, wxRichTextBuffer, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextBox: public wxRichTextParagraphLayoutBox
{
    DECLARE_DYNAMIC_CLASS(wxRichTextBox)
public:
// Constructors

    /**
        Default constructor; optionally pass the parent object.
    */

    wxRichTextBox(wxRichTextObject* parent = NULL);

    /**
        Copy constructor.
    */

    wxRichTextBox(const wxRichTextBox& obj): wxRichTextParagraphLayoutBox() { Copy(obj); }

// Overridables

    virtual bool Draw(wxDC& dc, wxRichTextDrawingContext& context, const wxRichTextRange& range, const wxRichTextSelection& selection, const wxRect& rect, int descent, int style);

    virtual wxString GetXMLNodeName() const { return wxT("textbox"); }

    virtual bool CanEditProperties() const { return true; }

    virtual bool EditProperties(wxWindow* parent, wxRichTextBuffer* buffer);

    virtual wxString GetPropertiesMenuLabel() const { return wxGetTranslation("&Box"); }

// Accessors

// Operations

    virtual wxRichTextObject* Clone() const { return new wxRichTextBox(*this); }

    void Copy(const wxRichTextBox& obj);

protected:
};

/**
    @class wxRichTextField

    This class implements the general concept of a field, an object that represents
    additional functionality such as a footnote, a bookmark, a page number, a table
    of contents, and so on. Extra information (such as a bookmark name) can be stored
    in the object properties.

    Drawing, layout, and property editing is delegated to classes derived
    from wxRichTextFieldType, such as instances of wxRichTextFieldTypeStandard; this makes
    the use of fields an efficient method of introducing extra functionality, since
    most of the information required to draw a field (such as a bitmap) is kept centrally
    in a single field type definition.

    The FieldType property, accessed by SetFieldType/GetFieldType, is used to retrieve
    the field type definition. So be careful not to overwrite this property.

    wxRichTextField is derived from wxRichTextParagraphLayoutBox, which means that it
    can contain its own read-only content, refreshed when the application calls the UpdateField
    function. Whether a field is treated as a composite or a single graphic is determined
    by the field type definition. If using wxRichTextFieldTypeStandard, passing the display
    type wxRICHTEXT_FIELD_STYLE_COMPOSITE to the field type definition causes the field
    to behave like a composite; the other display styles display a simple graphic.
    When implementing a composite field, you will still need to derive from wxRichTextFieldTypeStandard
    or wxRichTextFieldType, if only to implement UpdateField to refresh the field content
    appropriately. wxRichTextFieldTypeStandard is only one possible implementation, but
    covers common needs especially for simple, static fields using text or a bitmap.

    Register field types on application initialisation with the static function
    wxRichTextBuffer::AddFieldType. They will be deleted automatically on 
    application exit.

    An application can write a field to a control with wxRichTextCtrl::WriteField,
    taking a field type, the properties for the field, and optional attributes.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextFieldTypeStandard, wxRichTextFieldType, wxRichTextParagraphLayoutBox, wxRichTextProperties, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextField: public wxRichTextParagraphLayoutBox
{
    DECLARE_DYNAMIC_CLASS(wxRichTextField)
public:
// Constructors

    /**
        Default constructor; optionally pass the parent object.
    */

    wxRichTextField(const wxString& fieldType = wxEmptyString, wxRichTextObject* parent = NULL);

    /**
        Copy constructor.
    */

    wxRichTextField(const wxRichTextField& obj): wxRichTextParagraphLayoutBox() { Copy(obj); }

// Overridables

    virtual bool Draw(wxDC& dc, wxRichTextDrawingContext& context, const wxRichTextRange& range, const wxRichTextSelection& selection, const wxRect& rect, int descent, int style);

    virtual bool Layout(wxDC& dc, wxRichTextDrawingContext& context, const wxRect& rect, const wxRect& parentRect, int style);

    virtual bool GetRangeSize(const wxRichTextRange& range, wxSize& size, int& descent, wxDC& dc, wxRichTextDrawingContext& context, int flags, const wxPoint& position = wxPoint(0,0), const wxSize& parentSize = wxDefaultSize, wxArrayInt* partialExtents = NULL) const;

    virtual wxString GetXMLNodeName() const { return wxT("field"); }

    virtual bool CanEditProperties() const;

    virtual bool EditProperties(wxWindow* parent, wxRichTextBuffer* buffer);

    virtual wxString GetPropertiesMenuLabel() const;

    virtual bool AcceptsFocus() const { return false; }

    virtual void CalculateRange(long start, long& end);

    /**
        If a field has children, we don't want the user to be able to edit it.
    */
    virtual bool IsAtomic() const { return true; }

    virtual bool IsEmpty() const { return false; }

    virtual bool IsTopLevel() const;

// Accessors

    void SetFieldType(const wxString& fieldType) { GetProperties().SetProperty(wxT("FieldType"), fieldType); }
    wxString GetFieldType() const { return GetProperties().GetPropertyString(wxT("FieldType")); }

// Operations

    /**
        Update the field; delegated to the associated field type. This would typically expand the field to its value,
        if this is a dynamically changing and/or composite field.
     */
    virtual bool UpdateField(wxRichTextBuffer* buffer);

    virtual wxRichTextObject* Clone() const { return new wxRichTextField(*this); }

    void Copy(const wxRichTextField& obj);

protected:
};

/**
    @class wxRichTextFieldType

    The base class for custom field types. Each type definition handles one
    field type. Override functions to provide drawing, layout, updating and
    property editing functionality for a field.

    Register field types on application initialisation with the static function
    wxRichTextBuffer::AddFieldType. They will be deleted automatically on
    application exit.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextFieldTypeStandard, wxRichTextField, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextFieldType: public wxObject
{
    DECLARE_CLASS(wxRichTextFieldType)
public:
    /**
        Creates a field type definition.
    */
    wxRichTextFieldType(const wxString& name = wxEmptyString)
        : m_name(name)
        { }

    /**
        Copy constructor.
    */
    wxRichTextFieldType(const wxRichTextFieldType& fieldType)
        : wxObject(fieldType)
    { Copy(fieldType); }

    void Copy(const wxRichTextFieldType& fieldType) { m_name = fieldType.m_name; }

    /**
        Draw the item, within the given range. Some objects may ignore the range (for
        example paragraphs) while others must obey it (lines, to implement wrapping)
    */
    virtual bool Draw(wxRichTextField* obj, wxDC& dc, wxRichTextDrawingContext& context, const wxRichTextRange& range, const wxRichTextSelection& selection, const wxRect& rect, int descent, int style) = 0;

    /**
        Lay the item out at the specified position with the given size constraint.
        Layout must set the cached size. @rect is the available space for the object,
        and @a parentRect is the container that is used to determine a relative size
        or position (for example if a text box must be 50% of the parent text box).
    */
    virtual bool Layout(wxRichTextField* obj, wxDC& dc, wxRichTextDrawingContext& context, const wxRect& rect, const wxRect& parentRect, int style) = 0;

    /**
        Returns the object size for the given range. Returns @false if the range
        is invalid for this object.
    */
    virtual bool GetRangeSize(wxRichTextField* obj, const wxRichTextRange& range, wxSize& size, int& descent, wxDC& dc, wxRichTextDrawingContext& context, int flags, const wxPoint& position = wxPoint(0,0), const wxSize& parentSize = wxDefaultSize, wxArrayInt* partialExtents = NULL) const = 0;

    /**
        Returns @true if we can edit the object's properties via a GUI.
    */
    virtual bool CanEditProperties(wxRichTextField* WXUNUSED(obj)) const { return false; }

    /**
        Edits the object's properties via a GUI.
    */
    virtual bool EditProperties(wxRichTextField* WXUNUSED(obj), wxWindow* WXUNUSED(parent), wxRichTextBuffer* WXUNUSED(buffer)) { return false; }

    /**
        Returns the label to be used for the properties context menu item.
    */
    virtual wxString GetPropertiesMenuLabel(wxRichTextField* WXUNUSED(obj)) const { return wxEmptyString; }

    /**
        Update the field. This would typically expand the field to its value,
        if this is a dynamically changing and/or composite field.
     */
    virtual bool UpdateField(wxRichTextBuffer* WXUNUSED(buffer), wxRichTextField* WXUNUSED(obj)) { return false; }

    /**
        Returns @true if this object is top-level, i.e. contains its own paragraphs, such as a text box.
    */
    virtual bool IsTopLevel(wxRichTextField* WXUNUSED(obj)) const { return true; }

    /**
        Sets the field type name. There should be a unique name per field type object.
    */
    void SetName(const wxString& name) { m_name = name; }

    /**
        Returns the field type name. There should be a unique name per field type object.
    */
    wxString GetName() const { return m_name; }

protected:

    wxString  m_name;
};

WX_DECLARE_STRING_HASH_MAP(wxRichTextFieldType*, wxRichTextFieldTypeHashMap);

/**
    @class wxRichTextFieldTypeStandard

    A field type that can handle fields with text or bitmap labels, with a small range
    of styles for implementing rectangular fields and fields that can be used for start
    and end tags.

    The border, text and background colours can be customised; the default is
    white text on a black background.

    The following display styles can be used.

    @beginStyleTable
    @style{wxRICHTEXT_FIELD_STYLE_COMPOSITE}
           Creates a composite field; you will probably need to derive a new class to implement UpdateField.
    @style{wxRICHTEXT_FIELD_STYLE_RECTANGLE}
           Shows a rounded rectangle background.
    @style{wxRICHTEXT_FIELD_STYLE_NO_BORDER}
           Suppresses the background and border; mostly used with a bitmap label.
    @style{wxRICHTEXT_FIELD_STYLE_START_TAG}
           Shows a start tag background, with the pointy end facing right.
    @style{wxRICHTEXT_FIELD_STYLE_END_TAG}
           Shows an end tag background, with the pointy end facing left.
    @endStyleTable

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextFieldType, wxRichTextField, wxRichTextBuffer, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextFieldTypeStandard: public wxRichTextFieldType
{
    DECLARE_CLASS(wxRichTextFieldTypeStandard)
public:

    // Display style types
    enum { wxRICHTEXT_FIELD_STYLE_COMPOSITE = 0x01,
           wxRICHTEXT_FIELD_STYLE_RECTANGLE = 0x02,
           wxRICHTEXT_FIELD_STYLE_NO_BORDER = 0x04,
           wxRICHTEXT_FIELD_STYLE_START_TAG = 0x08,
           wxRICHTEXT_FIELD_STYLE_END_TAG = 0x10
         };

    /**
        Constructor, creating a field type definition with a text label.

        @param parent
            The name of the type definition. This must be unique, and is the type
            name used when adding a field to a control.
        @param label
            The text label to be shown on the field.
        @param displayStyle
            The display style: one of wxRICHTEXT_FIELD_STYLE_RECTANGLE,
            wxRICHTEXT_FIELD_STYLE_NO_BORDER, wxRICHTEXT_FIELD_STYLE_START_TAG,
            wxRICHTEXT_FIELD_STYLE_END_TAG.

    */
    wxRichTextFieldTypeStandard(const wxString& name, const wxString& label, int displayStyle = wxRICHTEXT_FIELD_STYLE_RECTANGLE);

    /**
        Constructor, creating a field type definition with a bitmap label.

        @param parent
            The name of the type definition. This must be unique, and is the type
            name used when adding a field to a control.
        @param label
            The bitmap label to be shown on the field.
        @param displayStyle
            The display style: one of wxRICHTEXT_FIELD_STYLE_RECTANGLE,
            wxRICHTEXT_FIELD_STYLE_NO_BORDER, wxRICHTEXT_FIELD_STYLE_START_TAG,
            wxRICHTEXT_FIELD_STYLE_END_TAG.

    */
    wxRichTextFieldTypeStandard(const wxString& name, const wxBitmap& bitmap, int displayStyle = wxRICHTEXT_FIELD_STYLE_NO_BORDER);

    /**
        The default constructor.

    */
    wxRichTextFieldTypeStandard() { Init(); }

    /**
        The copy constructor.

    */
    wxRichTextFieldTypeStandard(const wxRichTextFieldTypeStandard& field)
        : wxRichTextFieldType(field)
    { Copy(field); }

    /**
        Initialises the object.
    */
    void Init();

    /**
        Copies the object.
    */
    void Copy(const wxRichTextFieldTypeStandard& field);

    /**
        The assignment operator.
    */
    void operator=(const wxRichTextFieldTypeStandard& field) { Copy(field); }

    /**
        Draw the item, within the given range. Some objects may ignore the range (for
        example paragraphs) while others must obey it (lines, to implement wrapping)
    */
    virtual bool Draw(wxRichTextField* obj, wxDC& dc, wxRichTextDrawingContext& context, const wxRichTextRange& range, const wxRichTextSelection& selection, const wxRect& rect, int descent, int style);

    /**
        Lay the item out at the specified position with the given size constraint.
        Layout must set the cached size. @rect is the available space for the object,
        and @a parentRect is the container that is used to determine a relative size
        or position (for example if a text box must be 50% of the parent text box).
    */
    virtual bool Layout(wxRichTextField* obj, wxDC& dc, wxRichTextDrawingContext& context, const wxRect& rect, const wxRect& parentRect, int style);

    /**
        Returns the object size for the given range. Returns @false if the range
        is invalid for this object.
    */
    virtual bool GetRangeSize(wxRichTextField* obj, const wxRichTextRange& range, wxSize& size, int& descent, wxDC& dc, wxRichTextDrawingContext& context, int flags, const wxPoint& position = wxPoint(0,0), const wxSize& parentSize = wxDefaultSize, wxArrayInt* partialExtents = NULL) const;

    /**
        Get the size of the field, given the label, font size, and so on.
    */
    wxSize GetSize(wxRichTextField* obj, wxDC& dc, wxRichTextDrawingContext& context, int style) const;

    /**
        Returns @true if the display type is wxRICHTEXT_FIELD_STYLE_COMPOSITE, @false otherwise.
    */
    virtual bool IsTopLevel(wxRichTextField* WXUNUSED(obj)) const { return (GetDisplayStyle() & wxRICHTEXT_FIELD_STYLE_COMPOSITE) != 0; }

    /**
        Sets the text label for fields of this type.
    */
    void SetLabel(const wxString& label) { m_label = label; }

    /**
        Returns the text label for fields of this type.
    */
    const wxString& GetLabel() const { return m_label; }

    /**
        Sets the bitmap label for fields of this type.
    */
    void SetBitmap(const wxBitmap& bitmap) { m_bitmap = bitmap; }

    /**
        Gets the bitmap label for fields of this type.
    */
    const wxBitmap& GetBitmap() const { return m_bitmap; }

    /**
        Gets the display style for fields of this type.
    */
    int GetDisplayStyle() const { return m_displayStyle; }

    /**
        Sets the display style for fields of this type.
    */
    void SetDisplayStyle(int displayStyle) { m_displayStyle = displayStyle; }

    /**
        Gets the font used for drawing the text label.
    */
    const wxFont& GetFont() const { return m_font; }

    /**
        Sets the font used for drawing the text label.
    */
    void SetFont(const wxFont& font) { m_font = font; }

    /**
        Gets the colour used for drawing the text label.
    */
    const wxColour& GetTextColour() const { return m_textColour; }

    /**
        Sets the colour used for drawing the text label.
    */
    void SetTextColour(const wxColour& colour) { m_textColour = colour; }

    /**
        Gets the colour used for drawing the field border.
    */
    const wxColour& GetBorderColour() const { return m_borderColour; }

    /**
        Sets the colour used for drawing the field border.
    */
    void SetBorderColour(const wxColour& colour) { m_borderColour = colour; }

    /**
        Gets the colour used for drawing the field background.
    */
    const wxColour& GetBackgroundColour() const { return m_backgroundColour; }

    /**
        Sets the colour used for drawing the field background.
    */
    void SetBackgroundColour(const wxColour& colour) { m_backgroundColour = colour; }

    /**
        Sets the vertical padding (the distance between the border and the text).
    */
    void SetVerticalPadding(int padding) { m_verticalPadding = padding; }

    /**
        Gets the vertical padding (the distance between the border and the text).
    */
    int GetVerticalPadding() const { return m_verticalPadding; }

    /**
        Sets the horizontal padding (the distance between the border and the text).
    */
    void SetHorizontalPadding(int padding) { m_horizontalPadding = padding; }

    /**
        Sets the horizontal padding (the distance between the border and the text).
    */
    int GetHorizontalPadding() const { return m_horizontalPadding; }

    /**
        Sets the horizontal margin surrounding the field object.
    */
    void SetHorizontalMargin(int margin) { m_horizontalMargin = margin; }

    /**
        Gets the horizontal margin surrounding the field object.
    */
    int GetHorizontalMargin() const { return m_horizontalMargin; }

    /**
        Sets the vertical margin surrounding the field object.
    */
    void SetVerticalMargin(int margin) { m_verticalMargin = margin; }

    /**
        Gets the vertical margin surrounding the field object.
    */
    int GetVerticalMargin() const { return m_verticalMargin; }

protected:

    wxString    m_label;
    int         m_displayStyle;
    wxFont      m_font;
    wxColour    m_textColour;
    wxColour    m_borderColour;
    wxColour    m_backgroundColour;
    int         m_verticalPadding;
    int         m_horizontalPadding;
    int         m_horizontalMargin;
    int         m_verticalMargin;
    wxBitmap    m_bitmap;
};

/**
    @class wxRichTextLine

    This object represents a line in a paragraph, and stores
    offsets from the start of the paragraph representing the
    start and end positions of the line.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextBuffer, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextLine
{
public:
// Constructors

    wxRichTextLine(wxRichTextParagraph* parent);
    wxRichTextLine(const wxRichTextLine& obj) { Init( NULL); Copy(obj); }
    virtual ~wxRichTextLine() {}

// Overridables

// Accessors

    /**
        Sets the range associated with this line.
    */
    void SetRange(const wxRichTextRange& range) { m_range = range; }
    /**
        Sets the range associated with this line.
    */
    void SetRange(long from, long to) { m_range = wxRichTextRange(from, to); }

    /**
        Returns the parent paragraph.
    */
    wxRichTextParagraph* GetParent() { return m_parent; }

    /**
        Returns the range.
    */
    const wxRichTextRange& GetRange() const { return m_range; }
    /**
        Returns the range.
    */
    wxRichTextRange& GetRange() { return m_range; }

    /**
        Returns the absolute range.
    */
    wxRichTextRange GetAbsoluteRange() const;

    /**
        Returns the line size as calculated by Layout.
    */
    virtual wxSize GetSize() const { return m_size; }

    /**
        Sets the line size as calculated by Layout.
    */
    virtual void SetSize(const wxSize& sz) { m_size = sz; }

    /**
        Returns the object position relative to the parent.
    */
    virtual wxPoint GetPosition() const { return m_pos; }

    /**
        Sets the object position relative to the parent.
    */
    virtual void SetPosition(const wxPoint& pos) { m_pos = pos; }

    /**
        Returns the absolute object position.
    */
    virtual wxPoint GetAbsolutePosition() const;

    /**
        Returns the rectangle enclosing the line.
    */
    virtual wxRect GetRect() const { return wxRect(GetAbsolutePosition(), GetSize()); }

    /**
        Sets the stored descent.
    */
    void SetDescent(int descent) { m_descent = descent; }

    /**
        Returns the stored descent.
    */
    int GetDescent() const { return m_descent; }

#if wxRICHTEXT_USE_OPTIMIZED_LINE_DRAWING
    wxArrayInt& GetObjectSizes() { return m_objectSizes; }
    const wxArrayInt& GetObjectSizes() const { return m_objectSizes; }
#endif

// Operations

    /**
        Initialises the object.
    */
    void Init(wxRichTextParagraph* parent);

    /**
        Copies from @a obj.
    */
    void Copy(const wxRichTextLine& obj);

    virtual wxRichTextLine* Clone() const { return new wxRichTextLine(*this); }

protected:

    // The range of the line (start position to end position)
    // This is relative to the parent paragraph.
    wxRichTextRange     m_range;

    // Size and position measured relative to top of paragraph
    wxPoint             m_pos;
    wxSize              m_size;

    // Maximum descent for this line (location of text baseline)
    int                 m_descent;

    // The parent object
    wxRichTextParagraph* m_parent;

#if wxRICHTEXT_USE_OPTIMIZED_LINE_DRAWING
    wxArrayInt          m_objectSizes;
#endif
};

WX_DECLARE_LIST_WITH_DECL( wxRichTextLine, wxRichTextLineList , class WXDLLIMPEXP_RICHTEXT );

/**
    @class wxRichTextParagraph

    This object represents a single paragraph containing various objects such as text content, images, and further paragraph layout objects.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextBuffer, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextParagraph: public wxRichTextCompositeObject
{
    DECLARE_DYNAMIC_CLASS(wxRichTextParagraph)
public:
// Constructors

    /**
        Constructor taking a parent and style.
    */
    wxRichTextParagraph(wxRichTextObject* parent = NULL, wxRichTextAttr* style = NULL);
    /**
        Constructor taking a text string, a parent and paragraph and character attributes.
    */
    wxRichTextParagraph(const wxString& text, wxRichTextObject* parent = NULL, wxRichTextAttr* paraStyle = NULL, wxRichTextAttr* charStyle = NULL);
    virtual ~wxRichTextParagraph();
    wxRichTextParagraph(const wxRichTextParagraph& obj): wxRichTextCompositeObject() { Copy(obj); }

// Overridables

    virtual bool Draw(wxDC& dc, wxRichTextDrawingContext& context, const wxRichTextRange& range, const wxRichTextSelection& selection, const wxRect& rect, int descent, int style);

    virtual bool Layout(wxDC& dc, wxRichTextDrawingContext& context, const wxRect& rect, const wxRect& parentRect, int style);

    virtual bool GetRangeSize(const wxRichTextRange& range, wxSize& size, int& descent, wxDC& dc, wxRichTextDrawingContext& context, int flags, const wxPoint& position = wxPoint(0,0), const wxSize& parentSize = wxDefaultSize, wxArrayInt* partialExtents = NULL) const;

    virtual bool FindPosition(wxDC& dc, wxRichTextDrawingContext& context, long index, wxPoint& pt, int* height, bool forceLineStart);

    virtual int HitTest(wxDC& dc, wxRichTextDrawingContext& context, const wxPoint& pt, long& textPosition, wxRichTextObject** obj, wxRichTextObject** contextObj, int flags = 0);

    virtual void CalculateRange(long start, long& end);

    virtual wxString GetXMLNodeName() const { return wxT("paragraph"); }

// Accessors

    /**
        Returns the cached lines.
    */
    wxRichTextLineList& GetLines() { return m_cachedLines; }

// Operations

    /**
        Copies the object.
    */
    void Copy(const wxRichTextParagraph& obj);

    virtual wxRichTextObject* Clone() const { return new wxRichTextParagraph(*this); }

    /**
        Clears the cached lines.
    */
    void ClearLines();

// Implementation

    /**
        Applies paragraph styles such as centering to the wrapped lines.
    */
    virtual void ApplyParagraphStyle(wxRichTextLine* line, const wxRichTextAttr& attr, const wxRect& rect, wxDC& dc);

    /**
        Inserts text at the given position.
    */
    virtual bool InsertText(long pos, const wxString& text);

    /**
        Splits an object at this position if necessary, and returns
        the previous object, or NULL if inserting at the beginning.
    */
    virtual wxRichTextObject* SplitAt(long pos, wxRichTextObject** previousObject = NULL);

    /**
        Moves content to a list from this point.
    */
    virtual void MoveToList(wxRichTextObject* obj, wxList& list);

    /**
        Adds content back from a list.
    */
    virtual void MoveFromList(wxList& list);

    /**
        Returns the plain text searching from the start or end of the range.
        The resulting string may be shorter than the range given.
    */
    bool GetContiguousPlainText(wxString& text, const wxRichTextRange& range, bool fromStart = true);

    /**
        Finds a suitable wrap position. @a wrapPosition is the last position in the line to the left
        of the split.
    */
    bool FindWrapPosition(const wxRichTextRange& range, wxDC& dc, wxRichTextDrawingContext& context, int availableSpace, long& wrapPosition, wxArrayInt* partialExtents);

    /**
        Finds the object at the given position.
    */
    wxRichTextObject* FindObjectAtPosition(long position);

    /**
        Returns the bullet text for this paragraph.
    */
    wxString GetBulletText();

    /**
        Allocates or reuses a line object.
    */
    wxRichTextLine* AllocateLine(int pos);

    /**
        Clears remaining unused line objects, if any.
    */
    bool ClearUnusedLines(int lineCount);

    /**
        Returns combined attributes of the base style, paragraph style and character style. We use this to dynamically
        retrieve the actual style.
    */
    wxRichTextAttr GetCombinedAttributes(const wxRichTextAttr& contentStyle, bool includingBoxAttr = false) const;

    /**
        Returns the combined attributes of the base style and paragraph style.
    */
    wxRichTextAttr GetCombinedAttributes(bool includingBoxAttr = false) const;

    /**
        Returns the first position from pos that has a line break character.
    */
    long GetFirstLineBreakPosition(long pos);

    /**
        Creates a default tabstop array.
    */
    static void InitDefaultTabs();

    /**
        Clears the default tabstop array.
    */
    static void ClearDefaultTabs();

    /**
        Returns the default tabstop array.
    */
    static const wxArrayInt& GetDefaultTabs() { return sm_defaultTabs; }

    /**
        Lays out the floating objects.
    */
    void LayoutFloat(wxDC& dc, wxRichTextDrawingContext& context, const wxRect& rect, const wxRect& parentRect, int style, wxRichTextFloatCollector* floatCollector);

protected:

    // The lines that make up the wrapped paragraph
    wxRichTextLineList m_cachedLines;

    // Default tabstops
    static wxArrayInt  sm_defaultTabs;

friend class wxRichTextFloatCollector;
};

/**
    @class wxRichTextPlainText

    This object represents a single piece of text.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextBuffer, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextPlainText: public wxRichTextObject
{
    DECLARE_DYNAMIC_CLASS(wxRichTextPlainText)
public:
// Constructors

    /**
        Constructor.
    */
    wxRichTextPlainText(const wxString& text = wxEmptyString, wxRichTextObject* parent = NULL, wxRichTextAttr* style = NULL);

    /**
        Copy constructor.
    */
    wxRichTextPlainText(const wxRichTextPlainText& obj): wxRichTextObject() { Copy(obj); }

// Overridables

    virtual bool Draw(wxDC& dc, wxRichTextDrawingContext& context, const wxRichTextRange& range, const wxRichTextSelection& selection, const wxRect& rect, int descent, int style);

    virtual bool Layout(wxDC& dc, wxRichTextDrawingContext& context, const wxRect& rect, const wxRect& parentRect, int style);

    virtual bool AdjustAttributes(wxRichTextAttr& attr, wxRichTextDrawingContext& context);

    virtual bool GetRangeSize(const wxRichTextRange& range, wxSize& size, int& descent, wxDC& dc, wxRichTextDrawingContext& context, int flags, const wxPoint& position = wxPoint(0,0), const wxSize& parentSize = wxDefaultSize, wxArrayInt* partialExtents = NULL) const;

    virtual wxString GetTextForRange(const wxRichTextRange& range) const;

    virtual wxRichTextObject* DoSplit(long pos);

    virtual void CalculateRange(long start, long& end);

    virtual bool DeleteRange(const wxRichTextRange& range);

    virtual bool IsEmpty() const { return m_text.empty(); }

    virtual bool CanMerge(wxRichTextObject* object, wxRichTextDrawingContext& context) const;

    virtual bool Merge(wxRichTextObject* object, wxRichTextDrawingContext& context);

    virtual void Dump(wxTextOutputStream& stream);

    virtual bool CanSplit(wxRichTextDrawingContext& context) const;

    virtual wxRichTextObject* Split(wxRichTextDrawingContext& context);

    /**
        Get the first position from pos that has a line break character.
    */
    long GetFirstLineBreakPosition(long pos);

    /// Does this object take note of paragraph attributes? Text and image objects don't.
    virtual bool UsesParagraphAttributes() const { return false; }

#if wxUSE_XML
    virtual bool ImportFromXML(wxRichTextBuffer* buffer, wxXmlNode* node, wxRichTextXMLHandler* handler, bool* recurse);
#endif

#if wxRICHTEXT_HAVE_DIRECT_OUTPUT
    virtual bool ExportXML(wxOutputStream& stream, int indent, wxRichTextXMLHandler* handler);
#endif

#if wxRICHTEXT_HAVE_XMLDOCUMENT_OUTPUT
    virtual bool ExportXML(wxXmlNode* parent, wxRichTextXMLHandler* handler);
#endif

    virtual wxString GetXMLNodeName() const { return wxT("text"); }

// Accessors

    /**
        Returns the text.
    */
    const wxString& GetText() const { return m_text; }

    /**
        Sets the text.
    */
    void SetText(const wxString& text) { m_text = text; }

// Operations

    // Copies the text object,
    void Copy(const wxRichTextPlainText& obj);

    // Clones the text object.
    virtual wxRichTextObject* Clone() const { return new wxRichTextPlainText(*this); }

private:
    bool DrawTabbedString(wxDC& dc, const wxRichTextAttr& attr, const wxRect& rect, wxString& str, wxCoord& x, wxCoord& y, bool selected);

protected:
    wxString    m_text;
};

/**
    @class wxRichTextImageBlock

    This class stores information about an image, in binary in-memory form.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextBuffer, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextImageBlock: public wxObject
{
public:
    /**
        Constructor.
    */
    wxRichTextImageBlock();

    /**
        Copy constructor.
    */
    wxRichTextImageBlock(const wxRichTextImageBlock& block);
    virtual ~wxRichTextImageBlock();

    /**
        Initialises the block.
    */
    void Init();

    /**
        Clears the block.
    */

    void Clear();

    /**
        Load the original image into a memory block.
        If the image is not a JPEG, we must convert it into a JPEG
        to conserve space.
        If it's not a JPEG we can make use of @a image, already scaled, so we don't have to
        load the image a second time.
    */
    virtual bool MakeImageBlock(const wxString& filename, wxBitmapType imageType,
                                wxImage& image, bool convertToJPEG = true);

    /**
        Make an image block from the wxImage in the given
        format.
    */
    virtual bool MakeImageBlock(wxImage& image, wxBitmapType imageType, int quality = 80);

    /**
        Uses a const wxImage for efficiency, but can't set quality (only relevant for JPEG)
    */
    virtual bool MakeImageBlockDefaultQuality(const wxImage& image, wxBitmapType imageType);

    /**
        Makes the image block.
    */
    virtual bool DoMakeImageBlock(const wxImage& image, wxBitmapType imageType);

    /**
        Writes the block to a file.
    */
    bool Write(const wxString& filename);

    /**
        Writes the data in hex to a stream.
    */
    bool WriteHex(wxOutputStream& stream);

    /**
        Reads the data in hex from a stream.
    */
    bool ReadHex(wxInputStream& stream, int length, wxBitmapType imageType);

    /**
        Copy from @a block.
    */
    void Copy(const wxRichTextImageBlock& block);

    // Load a wxImage from the block
    /**
    */
    bool Load(wxImage& image);

// Operators

    /**
        Assignment operation.
    */
    void operator=(const wxRichTextImageBlock& block);

// Accessors

    /**
        Returns the raw data.
    */
    unsigned char* GetData() const { return m_data; }

    /**
        Returns the data size in bytes.
    */
    size_t GetDataSize() const { return m_dataSize; }

    /**
        Returns the image type.
    */
    wxBitmapType GetImageType() const { return m_imageType; }

    /**
    */
    void SetData(unsigned char* image) { m_data = image; }

    /**
        Sets the data size.
    */
    void SetDataSize(size_t size) { m_dataSize = size; }

    /**
        Sets the image type.
    */
    void SetImageType(wxBitmapType imageType) { m_imageType = imageType; }

    /**
        Returns @true if the data is non-NULL.
    */
    bool IsOk() const { return GetData() != NULL; }
    bool Ok() const { return IsOk(); }

    /**
        Gets the extension for the block's type.
    */
    wxString GetExtension() const;

/// Implementation

    /**
        Allocates and reads from a stream as a block of memory.
    */
    static unsigned char* ReadBlock(wxInputStream& stream, size_t size);

    /**
        Allocates and reads from a file as a block of memory.
    */
    static unsigned char* ReadBlock(const wxString& filename, size_t size);

    /**
        Writes a memory block to stream.
    */
    static bool WriteBlock(wxOutputStream& stream, unsigned char* block, size_t size);

    /**
        Writes a memory block to a file.
    */
    static bool WriteBlock(const wxString& filename, unsigned char* block, size_t size);

protected:
    // Size in bytes of the image stored.
    // This is in the raw, original form such as a JPEG file.
    unsigned char*      m_data;
    size_t              m_dataSize;
    wxBitmapType        m_imageType;
};

/**
    @class wxRichTextImage

    This class implements a graphic object.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextBuffer, wxRichTextCtrl, wxRichTextImageBlock
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextImage: public wxRichTextObject
{
    DECLARE_DYNAMIC_CLASS(wxRichTextImage)
public:
// Constructors

    /**
        Default constructor.
    */
    wxRichTextImage(wxRichTextObject* parent = NULL): wxRichTextObject(parent) { Init(); }

    /**
        Creates a wxRichTextImage from a wxImage.
    */
    wxRichTextImage(const wxImage& image, wxRichTextObject* parent = NULL, wxRichTextAttr* charStyle = NULL);

    /**
        Creates a wxRichTextImage from an image block.
    */
    wxRichTextImage(const wxRichTextImageBlock& imageBlock, wxRichTextObject* parent = NULL, wxRichTextAttr* charStyle = NULL);

    /**
        Copy constructor.
    */
    wxRichTextImage(const wxRichTextImage& obj): wxRichTextObject(obj) { Copy(obj); }

    /**
        Destructor.
    */
    ~wxRichTextImage();

    /**
        Initialisation.
    */
    void Init();

// Overridables

    virtual bool Draw(wxDC& dc, wxRichTextDrawingContext& context, const wxRichTextRange& range, const wxRichTextSelection& selection, const wxRect& rect, int descent, int style);

    virtual bool Layout(wxDC& dc, wxRichTextDrawingContext& context, const wxRect& rect, const wxRect& parentRect, int style);

    virtual bool GetRangeSize(const wxRichTextRange& range, wxSize& size, int& descent, wxDC& dc, wxRichTextDrawingContext& context, int flags, const wxPoint& position = wxPoint(0,0), const wxSize& parentSize = wxDefaultSize, wxArrayInt* partialExtents = NULL) const;

    /**
        Returns the 'natural' size for this object - the image size.
    */
    virtual wxTextAttrSize GetNaturalSize() const;

    virtual bool IsEmpty() const { return false; /* !m_imageBlock.IsOk(); */ }

    virtual bool CanEditProperties() const { return true; }

    virtual bool EditProperties(wxWindow* parent, wxRichTextBuffer* buffer);

    virtual wxString GetPropertiesMenuLabel() const { return wxGetTranslation("&Picture"); }

    virtual bool UsesParagraphAttributes() const { return false; }

#if wxUSE_XML
    virtual bool ImportFromXML(wxRichTextBuffer* buffer, wxXmlNode* node, wxRichTextXMLHandler* handler, bool* recurse);
#endif

#if wxRICHTEXT_HAVE_DIRECT_OUTPUT
    virtual bool ExportXML(wxOutputStream& stream, int indent, wxRichTextXMLHandler* handler);
#endif

#if wxRICHTEXT_HAVE_XMLDOCUMENT_OUTPUT
    virtual bool ExportXML(wxXmlNode* parent, wxRichTextXMLHandler* handler);
#endif

    // Images can be floatable (optionally).
    virtual bool IsFloatable() const { return true; }

    virtual wxString GetXMLNodeName() const { return wxT("image"); }

// Accessors

    /**
        Returns the image cache (a scaled bitmap).
    */
    const wxBitmap& GetImageCache() const { return m_imageCache; }

    /**
        Sets the image cache.
    */
    void SetImageCache(const wxBitmap& bitmap) { m_imageCache = bitmap; m_originalImageSize = wxSize(bitmap.GetWidth(), bitmap.GetHeight()); }

    /**
        Resets the image cache.
    */
    void ResetImageCache() { m_imageCache = wxNullBitmap; m_originalImageSize = wxSize(-1, -1); }

    /**
        Returns the image block containing the raw data.
    */
    wxRichTextImageBlock& GetImageBlock() { return m_imageBlock; }

// Operations

    /**
        Copies the image object.
    */
    void Copy(const wxRichTextImage& obj);

    /**
        Clones the image object.
    */
    virtual wxRichTextObject* Clone() const { return new wxRichTextImage(*this); }

    /**
        Creates a cached image at the required size.
    */
    virtual bool LoadImageCache(wxDC& dc, bool resetCache = false, const wxSize& parentSize = wxDefaultSize);

    /**
        Gets the original image size.
    */
    wxSize GetOriginalImageSize() const { return m_originalImageSize; }

    /**
        Sets the original image size.
    */
    void SetOriginalImageSize(const wxSize& sz) { m_originalImageSize = sz; }

protected:
    wxRichTextImageBlock    m_imageBlock;
    wxBitmap                m_imageCache;
    wxSize                  m_originalImageSize;
};

class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextCommand;
class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextAction;

/**
    @class wxRichTextBuffer

    This is a kind of paragraph layout box, used to represent the whole buffer.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextParagraphLayoutBox, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextBuffer: public wxRichTextParagraphLayoutBox
{
    DECLARE_DYNAMIC_CLASS(wxRichTextBuffer)
public:
// Constructors

    /**
        Default constructor.
    */
    wxRichTextBuffer() { Init(); }

    /**
        Copy constructor.
    */
    wxRichTextBuffer(const wxRichTextBuffer& obj): wxRichTextParagraphLayoutBox() { Init(); Copy(obj); }

    virtual ~wxRichTextBuffer() ;

// Accessors

    /**
        Returns the command processor.
        A text buffer always creates its own command processor when it is initialized.
    */
    wxCommandProcessor* GetCommandProcessor() const { return m_commandProcessor; }

    /**
        Sets style sheet, if any. This will allow the application to use named character and paragraph
        styles found in the style sheet.

        Neither the buffer nor the control owns the style sheet so must be deleted by the application.
    */
    void SetStyleSheet(wxRichTextStyleSheet* styleSheet) { m_styleSheet = styleSheet; }

    /**
        Returns the style sheet.
    */
    virtual wxRichTextStyleSheet* GetStyleSheet() const { return m_styleSheet; }

    /**
        Sets the style sheet and sends a notification of the change.
    */
    bool SetStyleSheetAndNotify(wxRichTextStyleSheet* sheet);

    /**
        Pushes the style sheet to the top of the style sheet stack.
    */
    bool PushStyleSheet(wxRichTextStyleSheet* styleSheet);

    /**
        Pops the style sheet from the top of the style sheet stack.
    */
    wxRichTextStyleSheet* PopStyleSheet();

    /**
        Returns the table storing fonts, for quick access and font reuse.
    */
    wxRichTextFontTable& GetFontTable() { return m_fontTable; }

    /**
        Returns the table storing fonts, for quick access and font reuse.
    */
    const wxRichTextFontTable& GetFontTable() const { return m_fontTable; }

    /**
        Sets table storing fonts, for quick access and font reuse.
    */
    void SetFontTable(const wxRichTextFontTable& table) { m_fontTable = table; }

    /**
        Sets the scale factor for displaying fonts, for example for more comfortable
        editing.
    */
    void SetFontScale(double fontScale);

    /**
        Returns the scale factor for displaying fonts, for example for more comfortable
        editing.
    */
    double GetFontScale() const { return m_fontScale; }

    /**
        Sets the scale factor for displaying certain dimensions such as indentation and
        inter-paragraph spacing. This can be useful when editing in a small control
        where you still want legible text, but a minimum of wasted white space.
    */
    void SetDimensionScale(double dimScale);

    /**
        Returns the scale factor for displaying certain dimensions such as indentation
        and inter-paragraph spacing.
    */
    double GetDimensionScale() const { return m_dimensionScale; }

// Operations

    /**
        Initialisation.
    */
    void Init();

    /**
        Clears the buffer, adds an empty paragraph, and clears the command processor.
    */
    virtual void ResetAndClearCommands();

#if wxUSE_FFILE && wxUSE_STREAMS
    //@{
    /**
        Loads content from a file.
        Not all handlers will implement file loading.
    */
    virtual bool LoadFile(const wxString& filename, wxRichTextFileType type = wxRICHTEXT_TYPE_ANY);
    //@}

    //@{
    /**
        Saves content to a file.
        Not all handlers will implement file saving.
    */
    virtual bool SaveFile(const wxString& filename, wxRichTextFileType type = wxRICHTEXT_TYPE_ANY);
    //@}
#endif // wxUSE_FFILE

#if wxUSE_STREAMS
    //@{
    /**
        Loads content from a stream.
        Not all handlers will implement loading from a stream.
    */
    virtual bool LoadFile(wxInputStream& stream, wxRichTextFileType type = wxRICHTEXT_TYPE_ANY);
    //@}

    //@{
    /**
        Saves content to a stream.
        Not all handlers will implement saving to a stream.
    */
    virtual bool SaveFile(wxOutputStream& stream, wxRichTextFileType type = wxRICHTEXT_TYPE_ANY);
    //@}
#endif // wxUSE_STREAMS

    /**
        Sets the handler flags, controlling loading and saving.
    */
    void SetHandlerFlags(int flags) { m_handlerFlags = flags; }

    /**
        Gets the handler flags, controlling loading and saving.
    */
    int GetHandlerFlags() const { return m_handlerFlags; }

    /**
        Convenience function to add a paragraph of text.
    */
    virtual wxRichTextRange AddParagraph(const wxString& text, wxRichTextAttr* paraStyle = NULL) { Modify(); return wxRichTextParagraphLayoutBox::AddParagraph(text, paraStyle); }

    /**
        Begin collapsing undo/redo commands. Note that this may not work properly
        if combining commands that delete or insert content, changing ranges for
        subsequent actions.

        @a cmdName should be the name of the combined command that will appear
        next to Undo and Redo in the edit menu.
    */
    virtual bool BeginBatchUndo(const wxString& cmdName);

    /**
        End collapsing undo/redo commands.
    */
    virtual bool EndBatchUndo();

    /**
        Returns @true if we are collapsing commands.
    */
    virtual bool BatchingUndo() const { return m_batchedCommandDepth > 0; }

    /**
        Submit the action immediately, or delay according to whether collapsing is on.
    */
    virtual bool SubmitAction(wxRichTextAction* action);

    /**
        Returns the collapsed command.
    */
    virtual wxRichTextCommand* GetBatchedCommand() const { return m_batchedCommand; }

    /**
        Begin suppressing undo/redo commands. The way undo is suppressed may be implemented
        differently by each command. If not dealt with by a command implementation, then
        it will be implemented automatically by not storing the command in the undo history
        when the action is submitted to the command processor.
    */
    virtual bool BeginSuppressUndo();

    /**
        End suppressing undo/redo commands.
    */
    virtual bool EndSuppressUndo();

    /**
        Are we suppressing undo??
    */
    virtual bool SuppressingUndo() const { return m_suppressUndo > 0; }

    /**
        Copy the range to the clipboard.
    */
    virtual bool CopyToClipboard(const wxRichTextRange& range);

    /**
        Paste the clipboard content to the buffer.
    */
    virtual bool PasteFromClipboard(long position);

    /**
        Returns @true if we can paste from the clipboard.
    */
    virtual bool CanPasteFromClipboard() const;

    /**
        Begin using a style.
    */
    virtual bool BeginStyle(const wxRichTextAttr& style);

    /**
        End the style.
    */
    virtual bool EndStyle();

    /**
        End all styles.
    */
    virtual bool EndAllStyles();

    /**
        Clears the style stack.
    */
    virtual void ClearStyleStack();

    /**
        Returns the size of the style stack, for example to check correct nesting.
    */
    virtual size_t GetStyleStackSize() const { return m_attributeStack.GetCount(); }

    /**
        Begins using bold.
    */
    bool BeginBold();

    /**
        Ends using bold.
    */
    bool EndBold() { return EndStyle(); }

    /**
        Begins using italic.
    */
    bool BeginItalic();

    /**
        Ends using italic.
    */
    bool EndItalic() { return EndStyle(); }

    /**
        Begins using underline.
    */
    bool BeginUnderline();

    /**
        Ends using underline.
    */
    bool EndUnderline() { return EndStyle(); }

    /**
        Begins using point size.
    */
    bool BeginFontSize(int pointSize);

    /**
        Ends using point size.
    */
    bool EndFontSize() { return EndStyle(); }

    /**
        Begins using this font.
    */
    bool BeginFont(const wxFont& font);

    /**
        Ends using a font.
    */
    bool EndFont() { return EndStyle(); }

    /**
        Begins using this colour.
    */
    bool BeginTextColour(const wxColour& colour);

    /**
        Ends using a colour.
    */
    bool EndTextColour() { return EndStyle(); }

    /**
        Begins using alignment.
    */
    bool BeginAlignment(wxTextAttrAlignment alignment);

    /**
        Ends alignment.
    */
    bool EndAlignment() { return EndStyle(); }

    /**
        Begins using @a leftIndent for the left indent, and optionally @a leftSubIndent for
        the sub-indent. Both are expressed in tenths of a millimetre.

        The sub-indent is an offset from the left of the paragraph, and is used for all
        but the first line in a paragraph. A positive value will cause the first line to appear
        to the left of the subsequent lines, and a negative value will cause the first line to be
        indented relative to the subsequent lines.
    */
    bool BeginLeftIndent(int leftIndent, int leftSubIndent = 0);

    /**
        Ends left indent.
    */
    bool EndLeftIndent() { return EndStyle(); }

    /**
        Begins a right indent, specified in tenths of a millimetre.
    */
    bool BeginRightIndent(int rightIndent);

    /**
        Ends right indent.
    */
    bool EndRightIndent() { return EndStyle(); }

    /**
        Begins paragraph spacing; pass the before-paragraph and after-paragraph spacing
        in tenths of a millimetre.
    */
    bool BeginParagraphSpacing(int before, int after);

    /**
        Ends paragraph spacing.
    */
    bool EndParagraphSpacing() { return EndStyle(); }

    /**
        Begins line spacing using the specified value. @e spacing is a multiple, where
        10 means single-spacing, 15 means 1.5 spacing, and 20 means double spacing.

        The ::wxTextAttrLineSpacing enumeration values are defined for convenience.
    */
    bool BeginLineSpacing(int lineSpacing);

    /**
        Ends line spacing.
    */
    bool EndLineSpacing() { return EndStyle(); }

    /**
        Begins numbered bullet.

        This call will be needed for each item in the list, and the
        application should take care of incrementing the numbering.

        @a bulletNumber is a number, usually starting with 1.
        @a leftIndent and @a leftSubIndent are values in tenths of a millimetre.
        @a bulletStyle is a bitlist of the following values:

        wxRichTextBuffer uses indentation to render a bulleted item.
        The left indent is the distance between the margin and the bullet.
        The content of the paragraph, including the first line, starts
        at leftMargin + leftSubIndent.
        So the distance between the left edge of the bullet and the
        left of the actual paragraph is leftSubIndent.
    */
    bool BeginNumberedBullet(int bulletNumber, int leftIndent, int leftSubIndent, int bulletStyle = wxTEXT_ATTR_BULLET_STYLE_ARABIC|wxTEXT_ATTR_BULLET_STYLE_PERIOD);

    /**
        Ends numbered bullet.
    */
    bool EndNumberedBullet() { return EndStyle(); }

    /**
        Begins applying a symbol bullet, using a character from the current font.

        See BeginNumberedBullet() for an explanation of how indentation is used
        to render the bulleted paragraph.
    */
    bool BeginSymbolBullet(const wxString& symbol, int leftIndent, int leftSubIndent, int bulletStyle = wxTEXT_ATTR_BULLET_STYLE_SYMBOL);

    /**
        Ends symbol bullet.
    */
    bool EndSymbolBullet() { return EndStyle(); }

    /**
        Begins applying a standard bullet, using one of the standard bullet names
        (currently @c standard/circle or @c standard/square.

        See BeginNumberedBullet() for an explanation of how indentation is used to
        render the bulleted paragraph.
    */
    bool BeginStandardBullet(const wxString& bulletName, int leftIndent, int leftSubIndent, int bulletStyle = wxTEXT_ATTR_BULLET_STYLE_STANDARD);

    /**
        Ends standard bullet.
    */
    bool EndStandardBullet() { return EndStyle(); }

    /**
        Begins named character style.
    */
    bool BeginCharacterStyle(const wxString& characterStyle);

    /**
        Ends named character style.
    */
    bool EndCharacterStyle() { return EndStyle(); }

    /**
        Begins named paragraph style.
    */
    bool BeginParagraphStyle(const wxString& paragraphStyle);

    /**
        Ends named character style.
    */
    bool EndParagraphStyle() { return EndStyle(); }

    /**
        Begins named list style.

        Optionally, you can also pass a level and a number.
    */
    bool BeginListStyle(const wxString& listStyle, int level = 1, int number = 1);

    /**
        Ends named character style.
    */
    bool EndListStyle() { return EndStyle(); }

    /**
        Begins applying wxTEXT_ATTR_URL to the content.

        Pass a URL and optionally, a character style to apply, since it is common
        to mark a URL with a familiar style such as blue text with underlining.
    */
    bool BeginURL(const wxString& url, const wxString& characterStyle = wxEmptyString);

    /**
        Ends URL.
    */
    bool EndURL() { return EndStyle(); }

// Event handling

    /**
        Adds an event handler.

        A buffer associated with a control has the control as the only event handler,
        but the application is free to add more if further notification is required.
        All handlers are notified of an event originating from the buffer, such as
        the replacement of a style sheet during loading.

        The buffer never deletes any of the event handlers, unless RemoveEventHandler()
        is called with @true as the second argument.
    */
    bool AddEventHandler(wxEvtHandler* handler);

    /**
        Removes an event handler from the buffer's list of handlers, deleting the
        object if @a deleteHandler is @true.
    */
    bool RemoveEventHandler(wxEvtHandler* handler, bool deleteHandler = false);

    /**
        Clear event handlers.
    */
    void ClearEventHandlers();

    /**
        Send event to event handlers. If sendToAll is true, will send to all event handlers,
        otherwise will stop at the first successful one.
    */
    bool SendEvent(wxEvent& event, bool sendToAll = true);

// Implementation

    virtual int HitTest(wxDC& dc, wxRichTextDrawingContext& context, const wxPoint& pt, long& textPosition, wxRichTextObject** obj, wxRichTextObject** contextObj, int flags = 0);

    /**
        Copies the buffer.
    */
    void Copy(const wxRichTextBuffer& obj);

    /**
        Assignment operator.
    */
    void operator= (const wxRichTextBuffer& obj) { Copy(obj); }

    /**
        Clones the buffer.
    */
    virtual wxRichTextObject* Clone() const { return new wxRichTextBuffer(*this); }

    /**
        Submits a command to insert paragraphs.
    */
    bool InsertParagraphsWithUndo(long pos, const wxRichTextParagraphLayoutBox& paragraphs, wxRichTextCtrl* ctrl, int flags = 0);

    /**
        Submits a command to insert the given text.
    */
    bool InsertTextWithUndo(long pos, const wxString& text, wxRichTextCtrl* ctrl, int flags = 0);

    /**
        Submits a command to insert a newline.
    */
    bool InsertNewlineWithUndo(long pos, wxRichTextCtrl* ctrl, int flags = 0);

    /**
        Submits a command to insert the given image.
    */
    bool InsertImageWithUndo(long pos, const wxRichTextImageBlock& imageBlock, wxRichTextCtrl* ctrl, int flags = 0,
            const wxRichTextAttr& textAttr = wxRichTextAttr());

    /**
        Submits a command to insert an object.
    */
    wxRichTextObject* InsertObjectWithUndo(long pos, wxRichTextObject *object, wxRichTextCtrl* ctrl, int flags);

    /**
        Submits a command to delete this range.
    */
    bool DeleteRangeWithUndo(const wxRichTextRange& range, wxRichTextCtrl* ctrl);

    /**
        Mark modified.
    */
    void Modify(bool modify = true) { m_modified = modify; }

    /**
        Returns @true if the buffer was modified.
    */
    bool IsModified() const { return m_modified; }

    //@{
    /**
        Dumps contents of buffer for debugging purposes.
    */
    virtual void Dump();
    virtual void Dump(wxTextOutputStream& stream) { wxRichTextParagraphLayoutBox::Dump(stream); }
    //@}

    /**
        Returns the file handlers.
    */
    static wxList& GetHandlers() { return sm_handlers; }

    /**
        Adds a file handler to the end.
    */
    static void AddHandler(wxRichTextFileHandler *handler);

    /**
        Inserts a file handler at the front.
    */
    static void InsertHandler(wxRichTextFileHandler *handler);

    /**
        Removes a file handler.
    */
    static bool RemoveHandler(const wxString& name);

    /**
        Finds a file handler by name.
    */
    static wxRichTextFileHandler *FindHandler(const wxString& name);

    /**
        Finds a file handler by extension and type.
    */
    static wxRichTextFileHandler *FindHandler(const wxString& extension, wxRichTextFileType imageType);

    /**
        Finds a handler by filename or, if supplied, type.
    */
    static wxRichTextFileHandler *FindHandlerFilenameOrType(const wxString& filename,
                                                            wxRichTextFileType imageType);

    /**
        Finds a handler by type.
    */
    static wxRichTextFileHandler *FindHandler(wxRichTextFileType imageType);

    /**
        Gets a wildcard incorporating all visible handlers. If @a types is present,
        it will be filled with the file type corresponding to each filter. This can be
        used to determine the type to pass to LoadFile given a selected filter.
    */
    static wxString GetExtWildcard(bool combine = false, bool save = false, wxArrayInt* types = NULL);

    /**
        Clean up file handlers.
    */
    static void CleanUpHandlers();

    /**
        Initialise the standard file handlers.
        Currently, only the plain text loading/saving handler is initialised by default.
    */
    static void InitStandardHandlers();

    /**
        Returns the drawing handlers.
    */
    static wxList& GetDrawingHandlers() { return sm_drawingHandlers; }

    /**
        Adds a drawing handler to the end.
    */
    static void AddDrawingHandler(wxRichTextDrawingHandler *handler);

    /**
        Inserts a drawing handler at the front.
    */
    static void InsertDrawingHandler(wxRichTextDrawingHandler *handler);

    /**
        Removes a drawing handler.
    */
    static bool RemoveDrawingHandler(const wxString& name);

    /**
        Finds a drawing handler by name.
    */
    static wxRichTextDrawingHandler *FindDrawingHandler(const wxString& name);

    /**
        Clean up drawing handlers.
    */
    static void CleanUpDrawingHandlers();

    /**
        Returns the field types.
    */
    static wxRichTextFieldTypeHashMap& GetFieldTypes() { return sm_fieldTypes; }

    /**
        Adds a field type.

        @see RemoveFieldType(), FindFieldType(), wxRichTextField, wxRichTextFieldType, wxRichTextFieldTypeStandard

    */
    static void AddFieldType(wxRichTextFieldType *fieldType);

    /**
        Removes a field type by name.

        @see AddFieldType(), FindFieldType(), wxRichTextField, wxRichTextFieldType, wxRichTextFieldTypeStandard
    */
    static bool RemoveFieldType(const wxString& name);

    /**
        Finds a field type by name.

        @see RemoveFieldType(), AddFieldType(), wxRichTextField, wxRichTextFieldType, wxRichTextFieldTypeStandard
    */
    static wxRichTextFieldType *FindFieldType(const wxString& name);

    /**
        Cleans up field types.
    */
    static void CleanUpFieldTypes();

    /**
        Returns the renderer object.
    */
    static wxRichTextRenderer* GetRenderer() { return sm_renderer; }

    /**
        Sets @a renderer as the object to be used to render certain aspects of the
        content, such as bullets.

        You can override default rendering by deriving a new class from
        wxRichTextRenderer or wxRichTextStdRenderer, overriding one or more
        virtual functions, and setting an instance of the class using this function.
    */
    static void SetRenderer(wxRichTextRenderer* renderer);

    /**
        Returns the minimum margin between bullet and paragraph in 10ths of a mm.
    */
    static int GetBulletRightMargin() { return sm_bulletRightMargin; }

    /**
        Sets the minimum margin between bullet and paragraph in 10ths of a mm.
    */
    static void SetBulletRightMargin(int margin) { sm_bulletRightMargin = margin; }

    /**
        Returns the factor to multiply by character height to get a reasonable bullet size.
    */
    static float GetBulletProportion() { return sm_bulletProportion; }

    /**
        Sets the factor to multiply by character height to get a reasonable bullet size.
    */
    static void SetBulletProportion(float prop) { sm_bulletProportion = prop; }

    /**
        Returns the scale factor for calculating dimensions.
    */
    double GetScale() const { return m_scale; }

    /**
        Sets the scale factor for calculating dimensions.
    */
    void SetScale(double scale) { m_scale = scale; }

    /**
        Sets the floating layout mode. Pass @false to speed up editing by not performing
        floating layout. This setting affects all buffers.

    */
    static void SetFloatingLayoutMode(bool mode) { sm_floatingLayoutMode = mode; }

    /**
        Returns the floating layout mode. The default is @true, where objects
        are laid out according to their floating status.
    */
    static bool GetFloatingLayoutMode() { return sm_floatingLayoutMode; }

protected:

    /// Command processor
    wxCommandProcessor*     m_commandProcessor;

    /// Table storing fonts
    wxRichTextFontTable     m_fontTable;

    /// Has been modified?
    bool                    m_modified;

    /// Collapsed command stack
    int                     m_batchedCommandDepth;

    /// Name for collapsed command
    wxString                m_batchedCommandsName;

    /// Current collapsed command accumulating actions
    wxRichTextCommand*      m_batchedCommand;

    /// Whether to suppress undo
    int                     m_suppressUndo;

    /// Style sheet, if any
    wxRichTextStyleSheet*   m_styleSheet;

    /// List of event handlers that will be notified of events
    wxList                  m_eventHandlers;

    /// Stack of attributes for convenience functions
    wxList                  m_attributeStack;

    /// Flags to be passed to handlers
    int                     m_handlerFlags;

    /// File handlers
    static wxList           sm_handlers;

    /// Drawing handlers
    static wxList           sm_drawingHandlers;

    /// Field types
    static wxRichTextFieldTypeHashMap sm_fieldTypes;

    /// Renderer
    static wxRichTextRenderer* sm_renderer;

    /// Minimum margin between bullet and paragraph in 10ths of a mm
    static int              sm_bulletRightMargin;

    /// Factor to multiply by character height to get a reasonable bullet size
    static float            sm_bulletProportion;

    /// Floating layout mode, @true by default
    static bool             sm_floatingLayoutMode;

    /// Scaling factor in use: needed to calculate correct dimensions when printing
    double                  m_scale;

    /// Font scale for adjusting the text size when editing
    double                  m_fontScale;

    /// Dimension scale for reducing redundant whitespace when editing
    double                  m_dimensionScale;
};

/**
    @class wxRichTextCell

    wxRichTextCell is the cell in a table.
 */

class WXDLLIMPEXP_RICHTEXT wxRichTextCell: public wxRichTextBox
{
    DECLARE_DYNAMIC_CLASS(wxRichTextCell)
public:
// Constructors

    /**
        Default constructor; optionally pass the parent object.
    */

    wxRichTextCell(wxRichTextObject* parent = NULL);

    /**
        Copy constructor.
    */

    wxRichTextCell(const wxRichTextCell& obj): wxRichTextBox() { Copy(obj); }

// Overridables

    virtual bool Draw(wxDC& dc, wxRichTextDrawingContext& context, const wxRichTextRange& range, const wxRichTextSelection& selection, const wxRect& rect, int descent, int style);

    virtual int HitTest(wxDC& dc, wxRichTextDrawingContext& context, const wxPoint& pt, long& textPosition, wxRichTextObject** obj, wxRichTextObject** contextObj, int flags = 0);

    virtual bool AdjustAttributes(wxRichTextAttr& attr, wxRichTextDrawingContext& context);

    virtual wxString GetXMLNodeName() const { return wxT("cell"); }

    virtual bool CanEditProperties() const { return true; }

    virtual bool EditProperties(wxWindow* parent, wxRichTextBuffer* buffer);

    virtual wxString GetPropertiesMenuLabel() const { return wxGetTranslation("&Cell"); }

// Accessors

    int GetColSpan() const;

    void SetColSpan(long span) { GetProperties().SetProperty(wxT("colspan"), span); }

    int GetRowSpan() const;

    void SetRowSpan(long span) { GetProperties().SetProperty(wxT("rowspan"), span); }

// Operations

    virtual wxRichTextObject* Clone() const { return new wxRichTextCell(*this); }

    void Copy(const wxRichTextCell& obj);

protected:
};

/**
    @class wxRichTextTable

    wxRichTextTable represents a table with arbitrary columns and rows.
 */

WX_DEFINE_ARRAY_PTR(wxRichTextObject*, wxRichTextObjectPtrArray);
WX_DECLARE_USER_EXPORTED_OBJARRAY(wxRichTextObjectPtrArray, wxRichTextObjectPtrArrayArray, WXDLLIMPEXP_RICHTEXT);

class WXDLLIMPEXP_RICHTEXT wxRichTextTable: public wxRichTextBox
{
    DECLARE_DYNAMIC_CLASS(wxRichTextTable)
public:

// Constructors

    /**
        Default constructor; optionally pass the parent object.
    */

    wxRichTextTable(wxRichTextObject* parent = NULL);

    /**
        Copy constructor.
    */

    wxRichTextTable(const wxRichTextTable& obj): wxRichTextBox() { Copy(obj); }

// Overridables

    virtual bool Draw(wxDC& dc, wxRichTextDrawingContext& context, const wxRichTextRange& range, const wxRichTextSelection& selection, const wxRect& rect, int descent, int style);

    virtual int HitTest(wxDC& dc, wxRichTextDrawingContext& context, const wxPoint& pt, long& textPosition, wxRichTextObject** obj, wxRichTextObject** contextObj, int flags = 0);

    virtual bool AdjustAttributes(wxRichTextAttr& attr, wxRichTextDrawingContext& context);

    virtual wxString GetXMLNodeName() const { return wxT("table"); }

    virtual bool Layout(wxDC& dc, wxRichTextDrawingContext& context, const wxRect& rect, const wxRect& parentRect, int style);

    virtual bool GetRangeSize(const wxRichTextRange& range, wxSize& size, int& descent, wxDC& dc, wxRichTextDrawingContext& context, int flags, const wxPoint& position = wxPoint(0,0), const wxSize& parentSize = wxDefaultSize, wxArrayInt* partialExtents = NULL) const;

    virtual bool DeleteRange(const wxRichTextRange& range);

    virtual wxString GetTextForRange(const wxRichTextRange& range) const;

#if wxUSE_XML
    virtual bool ImportFromXML(wxRichTextBuffer* buffer, wxXmlNode* node, wxRichTextXMLHandler* handler, bool* recurse);
#endif

#if wxRICHTEXT_HAVE_DIRECT_OUTPUT
    virtual bool ExportXML(wxOutputStream& stream, int indent, wxRichTextXMLHandler* handler);
#endif

#if wxRICHTEXT_HAVE_XMLDOCUMENT_OUTPUT
    virtual bool ExportXML(wxXmlNode* parent, wxRichTextXMLHandler* handler);
#endif

    virtual bool FindPosition(wxDC& dc, wxRichTextDrawingContext& context, long index, wxPoint& pt, int* height, bool forceLineStart);

    virtual void CalculateRange(long start, long& end);

    // Can this object handle the selections of its children? FOr example, a table.
    virtual bool HandlesChildSelections() const { return true; }

    /// Returns a selection object specifying the selections between start and end character positions.
    /// For example, a table would deduce what cells (of range length 1) are selected when dragging across the table.
    virtual wxRichTextSelection GetSelection(long start, long end) const;

    virtual bool CanEditProperties() const { return true; }

    virtual bool EditProperties(wxWindow* parent, wxRichTextBuffer* buffer);

    virtual wxString GetPropertiesMenuLabel() const { return wxGetTranslation("&Table"); }

    // Returns true if objects of this class can accept the focus, i.e. a call to SetFocusObject
    // is possible. For example, containers supporting text, such as a text box object, can accept the focus,
    // but a table can't (set the focus to individual cells instead).
    virtual bool AcceptsFocus() const { return false; }

// Accessors

    /**
        Returns the cells array.
    */
    const wxRichTextObjectPtrArrayArray& GetCells() const { return m_cells; }

    /**
        Returns the cells array.
    */
    wxRichTextObjectPtrArrayArray& GetCells() { return m_cells; }

    /**
        Returns the row count.
    */
    int GetRowCount() const { return m_rowCount; }

    /**
        Sets the row count.
    */
    void SetRowCount(int count) { m_rowCount = count; }

    /**
        Returns the column count.
    */
    int GetColumnCount() const { return m_colCount; }

    /**
        Sets the column count.
    */
    void SetColumnCount(int count) { m_colCount = count; }

    /**
        Returns the cell at the given row/column position.
    */
    virtual wxRichTextCell* GetCell(int row, int col) const;

    /**
        Returns the cell at the given character position (in the range of the table).
    */
    virtual wxRichTextCell* GetCell(long pos) const;

    /**
        Returns the row/column for a given character position.
    */
    virtual bool GetCellRowColumnPosition(long pos, int& row, int& col) const;

    /**
        Returns the coordinates of the cell with keyboard focus, or (-1,-1) if none.
    */    
    virtual wxPosition GetFocusedCell() const;

// Operations

    /**
        Clears the table.
    */

    virtual void ClearTable();

    /**
        Creates a table of the given dimensions.
    */

    virtual bool CreateTable(int rows, int cols);

    /**
        Sets the attributes for the cells specified by the selection.
    */

    virtual bool SetCellStyle(const wxRichTextSelection& selection, const wxRichTextAttr& style, int flags = wxRICHTEXT_SETSTYLE_WITH_UNDO);

    /**
        Deletes rows from the given row position.
    */

    virtual bool DeleteRows(int startRow, int noRows = 1);

    /**
        Deletes columns from the given column position.
    */

    virtual bool DeleteColumns(int startCol, int noCols = 1);

    /**
        Adds rows from the given row position.
    */

    virtual bool AddRows(int startRow, int noRows = 1, const wxRichTextAttr& attr = wxRichTextAttr());

    /**
        Adds columns from the given column position.
    */

    virtual bool AddColumns(int startCol, int noCols = 1, const wxRichTextAttr& attr = wxRichTextAttr());

    // Makes a clone of this object.
    virtual wxRichTextObject* Clone() const { return new wxRichTextTable(*this); }

    // Copies this object.
    void Copy(const wxRichTextTable& obj);

protected:

    int m_rowCount;
    int m_colCount;

    // An array of rows, each of which is a wxRichTextObjectPtrArray containing
    // the cell objects. The cell objects are also children of this object.
    // Problem: if boxes are immediate children of a box, this will cause problems
    // with wxRichTextParagraphLayoutBox functions (and functions elsewhere) that
    // expect to find just paragraphs. May have to adjust the way we handle the
    // hierarchy to accept non-paragraph objects in a paragraph layout box.
    // We'll be overriding much wxRichTextParagraphLayoutBox functionality so this
    // may not be such a problem. Perhaps the table should derive from a different
    // class?
    wxRichTextObjectPtrArrayArray   m_cells;
};

/** @class wxRichTextTableBlock

    Stores the coordinates for a block of cells.
 */

class WXDLLIMPEXP_RICHTEXT wxRichTextTableBlock
{
public:
    wxRichTextTableBlock() { Init(); }
    wxRichTextTableBlock(int colStart, int colEnd, int rowStart, int rowEnd)
    { Init(); m_colStart = colStart; m_colEnd = colEnd; m_rowStart = rowStart; m_rowEnd = rowEnd; }
    wxRichTextTableBlock(const wxRichTextTableBlock& block) { Copy(block); }

    void Init() { m_colStart = 0; m_colEnd = 0; m_rowStart = 0; m_rowEnd = 0; }
    
    void Copy(const wxRichTextTableBlock& block)
    {
        m_colStart = block.m_colStart; m_colEnd = block.m_colEnd; m_rowStart = block.m_rowStart; m_rowEnd = block.m_rowEnd;
    }
    void operator=(const wxRichTextTableBlock& block) { Copy(block); }
    bool operator==(const wxRichTextTableBlock& block)
    { return m_colStart == block.m_colStart && m_colEnd == block.m_colEnd && m_rowStart == block.m_rowStart && m_rowEnd == block.m_rowEnd; }

    /// Computes the block given a table (perhaps about to be edited) and a rich text control
    /// that may have a selection. If no selection, the whole table is used. If just the whole content
    /// of one cell is selected, this cell only is used. If the cell contents is not selected and
    /// requireCellSelection is @false, the focused cell will count as a selected cell.
    bool ComputeBlockForSelection(wxRichTextTable* table, wxRichTextCtrl* ctrl, bool requireCellSelection = true);

    /// Does this block represent the whole table?
    bool IsWholeTable(wxRichTextTable* table) const;

    /// Returns the cell focused in the table, if any
    static wxRichTextCell* GetFocusedCell(wxRichTextCtrl* ctrl);

    int& ColStart() { return m_colStart; }
    int ColStart() const { return m_colStart; }

    int& ColEnd() { return m_colEnd; }
    int ColEnd() const { return m_colEnd; }

    int& RowStart() { return m_rowStart; }
    int RowStart() const { return m_rowStart; }

    int& RowEnd() { return m_rowEnd; }
    int RowEnd() const { return m_rowEnd; }

    int m_colStart, m_colEnd, m_rowStart, m_rowEnd;
};

/**
    The command identifiers for Do/Undo.
*/

enum wxRichTextCommandId
{
    wxRICHTEXT_INSERT,
    wxRICHTEXT_DELETE,
    wxRICHTEXT_CHANGE_ATTRIBUTES,
    wxRICHTEXT_CHANGE_STYLE,
    wxRICHTEXT_CHANGE_PROPERTIES,
    wxRICHTEXT_CHANGE_OBJECT
};

/**
    @class wxRichTextObjectAddress

    A class for specifying an object anywhere in an object hierarchy,
    without using a pointer, necessary since wxRTC commands may delete
    and recreate sub-objects so physical object addresses change. An array
    of positions (one per hierarchy level) is used.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextCommand
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextObjectAddress
{
public:
    /**
        Creates the address given a container and an object.
    */
    wxRichTextObjectAddress(wxRichTextParagraphLayoutBox* topLevelContainer, wxRichTextObject* obj) { Create(topLevelContainer, obj); }
    /**
    */
    wxRichTextObjectAddress() { Init(); }
    /**
    */
    wxRichTextObjectAddress(const wxRichTextObjectAddress& address) { Copy(address); }

    void Init() {}

    /**
        Copies the address.
    */
    void Copy(const wxRichTextObjectAddress& address) { m_address = address.m_address; }

    /**
        Assignment operator.
    */
    void operator=(const wxRichTextObjectAddress& address) { Copy(address); }

    /**
        Returns the object specified by the address, given a top level container.
    */
    wxRichTextObject* GetObject(wxRichTextParagraphLayoutBox* topLevelContainer) const;

    /**
        Creates the address given a container and an object.
    */
    bool Create(wxRichTextParagraphLayoutBox* topLevelContainer, wxRichTextObject* obj);

    /**
        Returns the array of integers representing the object address.
    */
    wxArrayInt& GetAddress() { return m_address; }

    /**
        Returns the array of integers representing the object address.
    */
    const wxArrayInt& GetAddress() const { return m_address; }

    /**
        Sets the address from an array of integers.
    */
    void SetAddress(const wxArrayInt& address) { m_address = address; }

protected:

    wxArrayInt  m_address;
};

class WXDLLIMPEXP_FWD_RICHTEXT wxRichTextAction;

/**
    @class wxRichTextCommand

    Implements a command on the undo/redo stack. A wxRichTextCommand object contains one or more wxRichTextAction
    objects, allowing aggregation of a number of operations into one command.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextAction
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextCommand: public wxCommand
{
public:
    /**
        Constructor for one action.
    */
    wxRichTextCommand(const wxString& name, wxRichTextCommandId id, wxRichTextBuffer* buffer,
        wxRichTextParagraphLayoutBox* container, wxRichTextCtrl* ctrl, bool ignoreFirstTime = false);

    /**
        Constructor for multiple actions.
    */
    wxRichTextCommand(const wxString& name);

    virtual ~wxRichTextCommand();

    /**
        Performs the command.
    */
    bool Do();

    /**
        Undoes the command.
    */
    bool Undo();

    /**
        Adds an action to the action list.
    */
    void AddAction(wxRichTextAction* action);

    /**
        Clears the action list.
    */
    void ClearActions();

    /**
        Returns the action list.
    */
    wxList& GetActions() { return m_actions; }

protected:

    wxList  m_actions;
};

/**
    @class wxRichTextAction

    Implements a part of a command.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextCommand
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextAction: public wxObject
{
public:
    /**
        Constructor. @a buffer is the top-level buffer, while @a container is the object within
        which the action is taking place. In the simplest case, they are the same.
    */
    wxRichTextAction(wxRichTextCommand* cmd, const wxString& name, wxRichTextCommandId id,
        wxRichTextBuffer* buffer, wxRichTextParagraphLayoutBox* container,
        wxRichTextCtrl* ctrl, bool ignoreFirstTime = false);

    virtual ~wxRichTextAction();

    /**
        Performs the action.
    */
    bool Do();

    /**
        Undoes the action.
    */
    bool Undo();

    /**
        Updates the control appearance, optimizing if possible given information from the call to Layout.
    */
    void UpdateAppearance(long caretPosition, bool sendUpdateEvent = false,
                            wxArrayInt* optimizationLineCharPositions = NULL, wxArrayInt* optimizationLineYPositions = NULL, bool isDoCmd = true);

    /**
        Replaces the buffer paragraphs with the given fragment.
    */
    void ApplyParagraphs(const wxRichTextParagraphLayoutBox& fragment);

    /**
        Returns the new fragments.
    */
    wxRichTextParagraphLayoutBox& GetNewParagraphs() { return m_newParagraphs; }

    /**
        Returns the old fragments.
    */
    wxRichTextParagraphLayoutBox& GetOldParagraphs() { return m_oldParagraphs; }

    /**
        Returns the attributes, for single-object commands.
    */
    wxRichTextAttr& GetAttributes() { return m_attributes; }

    /**
        Returns the object to replace the one at the position defined by the container address
        and the action's range start position.
    */
    wxRichTextObject* GetObject() const { return m_object; }

    /**
        Stores the object to replace the one at the position defined by the container address
        without making an address for it (cf SetObject() and MakeObject()).
    */
    void StoreObject(wxRichTextObject* obj) { m_object = obj; }

    /**
        Sets the object to replace the one at the position defined by the container address
        and the action's range start position.
    */
    void SetObject(wxRichTextObject* obj) { m_object = obj; m_objectAddress.Create(m_buffer, m_object); }

    /**
        Makes an address from the given object.
    */
    void MakeObject(wxRichTextObject* obj) { m_objectAddress.Create(m_buffer, obj); }

    /**
        Sets the existing and new objects, for use with wxRICHTEXT_CHANGE_OBJECT.
    */
    void SetOldAndNewObjects(wxRichTextObject* oldObj, wxRichTextObject* newObj) { SetObject(oldObj); StoreObject(newObj); }

    /**
        Calculate arrays for refresh optimization.
    */
    void CalculateRefreshOptimizations(wxArrayInt& optimizationLineCharPositions, wxArrayInt& optimizationLineYPositions);

    /**
        Sets the position used for e.g. insertion.
    */
    void SetPosition(long pos) { m_position = pos; }

    /**
        Returns the position used for e.g. insertion.
    */
    long GetPosition() const { return m_position; }

    /**
        Sets the range for e.g. deletion.
    */
    void SetRange(const wxRichTextRange& range) { m_range = range; }

    /**
        Returns the range for e.g. deletion.
    */
    const wxRichTextRange& GetRange() const { return m_range; }

    /**
        Returns the address (nested position) of the container within the buffer being manipulated.
    */
    wxRichTextObjectAddress& GetContainerAddress() { return m_containerAddress; }

    /**
        Returns the address (nested position) of the container within the buffer being manipulated.
    */
    const wxRichTextObjectAddress& GetContainerAddress() const { return m_containerAddress; }

    /**
        Sets the address (nested position) of the container within the buffer being manipulated.
    */
    void SetContainerAddress(const wxRichTextObjectAddress& address) { m_containerAddress = address; }

    /**
        Sets the address (nested position) of the container within the buffer being manipulated.
    */
    void SetContainerAddress(wxRichTextParagraphLayoutBox* container, wxRichTextObject* obj) { m_containerAddress.Create(container, obj); }

    /**
        Returns the container that this action refers to, using the container address and top-level buffer.
    */
    wxRichTextParagraphLayoutBox* GetContainer() const;

    /**
        Returns the action name.
    */
    const wxString& GetName() const { return m_name; }

    /**
        Instructs the first Do() command should be skipped as it's already been applied.
    */
    void SetIgnoreFirstTime(bool b) { m_ignoreThis = b; }

    /**
        Returns true if the first Do() command should be skipped as it's already been applied.
    */
    bool GetIgnoreFirstTime() const { return m_ignoreThis; }

protected:
    // Action name
    wxString                        m_name;

    // Buffer
    wxRichTextBuffer*               m_buffer;

    // The address (nested position) of the container being manipulated.
    // This is necessary because objects are deleted, and we can't
    // therefore store actual pointers.
    wxRichTextObjectAddress         m_containerAddress;

    // Control
    wxRichTextCtrl*                 m_ctrl;

    // Stores the new paragraphs
    wxRichTextParagraphLayoutBox    m_newParagraphs;

    // Stores the old paragraphs
    wxRichTextParagraphLayoutBox    m_oldParagraphs;

    // Stores an object to replace the one at the position
    // defined by the container address and the action's range start position.
    wxRichTextObject*               m_object;

    // Stores the attributes
    wxRichTextAttr                  m_attributes;

    // The address of the object being manipulated (used for changing an individual object or its attributes)
    wxRichTextObjectAddress         m_objectAddress;

    // Stores the old attributes
    // wxRichTextAttr                  m_oldAttributes;

    // The affected range
    wxRichTextRange                 m_range;

    // The insertion point for this command
    long                            m_position;

    // Ignore 1st 'Do' operation because we already did it
    bool                            m_ignoreThis;

    // The command identifier
    wxRichTextCommandId             m_cmdId;
};

/*!
 * Handler flags
 */

// Include style sheet when loading and saving
#define wxRICHTEXT_HANDLER_INCLUDE_STYLESHEET       0x0001

// Save images to memory file system in HTML handler
#define wxRICHTEXT_HANDLER_SAVE_IMAGES_TO_MEMORY    0x0010

// Save images to files in HTML handler
#define wxRICHTEXT_HANDLER_SAVE_IMAGES_TO_FILES     0x0020

// Save images as inline base64 data in HTML handler
#define wxRICHTEXT_HANDLER_SAVE_IMAGES_TO_BASE64    0x0040

// Don't write header and footer (or BODY), so we can include the fragment
// in a larger document
#define wxRICHTEXT_HANDLER_NO_HEADER_FOOTER         0x0080

// Convert the more common face names to names that will work on the current platform
// in a larger document
#define wxRICHTEXT_HANDLER_CONVERT_FACENAMES        0x0100

/**
    @class wxRichTextFileHandler

    The base class for file handlers.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextBuffer, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextFileHandler: public wxObject
{
    DECLARE_CLASS(wxRichTextFileHandler)
public:
    /**
        Creates a file handler object.
    */
    wxRichTextFileHandler(const wxString& name = wxEmptyString, const wxString& ext = wxEmptyString, int type = 0)
        : m_name(name), m_extension(ext), m_type(type), m_flags(0), m_visible(true)
        { }

#if wxUSE_STREAMS
    /**
        Loads the buffer from a stream.
        Not all handlers will implement file loading.
    */
    bool LoadFile(wxRichTextBuffer *buffer, wxInputStream& stream)
    { return DoLoadFile(buffer, stream); }

    /**
        Saves the buffer to a stream.
        Not all handlers will implement file saving.
    */
    bool SaveFile(wxRichTextBuffer *buffer, wxOutputStream& stream)
    { return DoSaveFile(buffer, stream); }
#endif

#if wxUSE_FFILE && wxUSE_STREAMS
    /**
        Loads the buffer from a file.
    */
    virtual bool LoadFile(wxRichTextBuffer *buffer, const wxString& filename);

    /**
        Saves the buffer to a file.
    */
    virtual bool SaveFile(wxRichTextBuffer *buffer, const wxString& filename);
#endif // wxUSE_STREAMS && wxUSE_STREAMS

    /**
        Returns @true if we handle this filename (if using files). By default, checks the extension.
    */
    virtual bool CanHandle(const wxString& filename) const;

    /**
        Returns @true if we can save using this handler.
    */
    virtual bool CanSave() const { return false; }

    /**
        Returns @true if we can load using this handler.
    */
    virtual bool CanLoad() const { return false; }

    /**
        Returns @true if this handler should be visible to the user.
    */
    virtual bool IsVisible() const { return m_visible; }

    /**
        Sets whether the handler should be visible to the user (via the application's
        load and save dialogs).
    */
    virtual void SetVisible(bool visible) { m_visible = visible; }

    /**
        Sets the name of the handler.
    */
    void SetName(const wxString& name) { m_name = name; }

    /**
        Returns the name of the handler.
    */
    wxString GetName() const { return m_name; }

    /**
        Sets the default extension to recognise.
    */
    void SetExtension(const wxString& ext) { m_extension = ext; }

    /**
        Returns the default extension to recognise.
    */
    wxString GetExtension() const { return m_extension; }

    /**
        Sets the handler type.
    */
    void SetType(int type) { m_type = type; }

    /**
        Returns the handler type.
    */
    int GetType() const { return m_type; }

    /**
        Sets flags that change the behaviour of loading or saving.
        See the documentation for each handler class to see what flags are relevant
        for each handler.

        You call this function directly if you are using a file handler explicitly
        (without going through the text control or buffer LoadFile/SaveFile API).
        Or, you can call the control or buffer's SetHandlerFlags function to set
        the flags that will be used for subsequent load and save operations.
    */
    void SetFlags(int flags) { m_flags = flags; }

    /**
        Returns flags controlling how loading and saving is done.
    */
    int GetFlags() const { return m_flags; }

    /**
        Sets the encoding to use when saving a file. If empty, a suitable encoding is chosen.
    */
    void SetEncoding(const wxString& encoding) { m_encoding = encoding; }

    /**
        Returns the encoding to use when saving a file. If empty, a suitable encoding is chosen.
    */
    const wxString& GetEncoding() const { return m_encoding; }

protected:

#if wxUSE_STREAMS
    /**
        Override to load content from @a stream into @a buffer.
    */
    virtual bool DoLoadFile(wxRichTextBuffer *buffer, wxInputStream& stream) = 0;

    /**
        Override to save content to @a stream from @a buffer.
    */
    virtual bool DoSaveFile(wxRichTextBuffer *buffer, wxOutputStream& stream) = 0;
#endif

    wxString  m_name;
    wxString  m_encoding;
    wxString  m_extension;
    int       m_type;
    int       m_flags;
    bool      m_visible;
};

/**
    @class wxRichTextPlainTextHandler

    Implements saving a buffer to plain text.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextFileHandler, wxRichTextBuffer, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextPlainTextHandler: public wxRichTextFileHandler
{
    DECLARE_CLASS(wxRichTextPlainTextHandler)
public:
    wxRichTextPlainTextHandler(const wxString& name = wxT("Text"),
                               const wxString& ext = wxT("txt"),
                               wxRichTextFileType type = wxRICHTEXT_TYPE_TEXT)
        : wxRichTextFileHandler(name, ext, type)
        { }

    // Can we save using this handler?
    virtual bool CanSave() const { return true; }

    // Can we load using this handler?
    virtual bool CanLoad() const { return true; }

protected:

#if wxUSE_STREAMS
    virtual bool DoLoadFile(wxRichTextBuffer *buffer, wxInputStream& stream);
    virtual bool DoSaveFile(wxRichTextBuffer *buffer, wxOutputStream& stream);
#endif

};

/**
    @class wxRichTextDrawingHandler

    The base class for custom drawing handlers.
    Currently, drawing handlers can provide virtual attributes.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextBuffer, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextDrawingHandler: public wxObject
{
    DECLARE_CLASS(wxRichTextDrawingHandler)
public:
    /**
        Creates a drawing handler object.
    */
    wxRichTextDrawingHandler(const wxString& name = wxEmptyString)
        : m_name(name)
        { }

    /**
        Returns @true if this object has virtual attributes that we can provide.
    */
    virtual bool HasVirtualAttributes(wxRichTextObject* obj) const = 0;

    /**
        Provides virtual attributes that we can provide.
    */
    virtual bool GetVirtualAttributes(wxRichTextAttr& attr, wxRichTextObject* obj) const = 0;

    /**
        Gets the count for mixed virtual attributes for individual positions within the object.
        For example, individual characters within a text object may require special highlighting.
    */
    virtual int GetVirtualSubobjectAttributesCount(wxRichTextObject* obj) const = 0;

    /**
        Gets the mixed virtual attributes for individual positions within the object.
        For example, individual characters within a text object may require special highlighting.
        Returns the number of virtual attributes found.
    */
    virtual int GetVirtualSubobjectAttributes(wxRichTextObject* obj, wxArrayInt& positions, wxRichTextAttrArray& attributes) const = 0;

    /**
        Do we have virtual text for this object? Virtual text allows an application
        to replace characters in an object for editing and display purposes, for example
        for highlighting special characters.
    */
    virtual bool HasVirtualText(const wxRichTextPlainText* obj) const = 0;

    /**
        Gets the virtual text for this object.
    */
    virtual bool GetVirtualText(const wxRichTextPlainText* obj, wxString& text) const = 0;

    /**
        Sets the name of the handler.
    */
    void SetName(const wxString& name) { m_name = name; }

    /**
        Returns the name of the handler.
    */
    wxString GetName() const { return m_name; }

protected:

    wxString  m_name;
};

#if wxUSE_DATAOBJ

/**
    @class wxRichTextBufferDataObject

    Implements a rich text data object for clipboard transfer.

    @library{wxrichtext}
    @category{richtext}

    @see wxDataObjectSimple, wxRichTextBuffer, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextBufferDataObject: public wxDataObjectSimple
{
public:
    /**
        The constructor doesn't copy the pointer, so it shouldn't go away while this object
        is alive.
    */
    wxRichTextBufferDataObject(wxRichTextBuffer* richTextBuffer = NULL);
    virtual ~wxRichTextBufferDataObject();

    /**
        After a call to this function, the buffer is owned by the caller and it
        is responsible for deleting it.
    */
    wxRichTextBuffer* GetRichTextBuffer();

    /**
        Returns the id for the new data format.
    */
    static const wxChar* GetRichTextBufferFormatId() { return ms_richTextBufferFormatId; }

    // base class pure virtuals

    virtual wxDataFormat GetPreferredFormat(Direction dir) const;
    virtual size_t GetDataSize() const;
    virtual bool GetDataHere(void *pBuf) const;
    virtual bool SetData(size_t len, const void *buf);

    // prevent warnings

    virtual size_t GetDataSize(const wxDataFormat&) const { return GetDataSize(); }
    virtual bool GetDataHere(const wxDataFormat&, void *buf) const { return GetDataHere(buf); }
    virtual bool SetData(const wxDataFormat&, size_t len, const void *buf) { return SetData(len, buf); }

private:
    wxDataFormat            m_formatRichTextBuffer;     // our custom format
    wxRichTextBuffer*       m_richTextBuffer;           // our data
    static const wxChar*    ms_richTextBufferFormatId;  // our format id
};

#endif

/**
    @class wxRichTextRenderer

    This class isolates some common drawing functionality.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextBuffer, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextRenderer: public wxObject
{
public:
    /**
        Constructor.
    */
    wxRichTextRenderer() {}
    virtual ~wxRichTextRenderer() {}

    /**
        Draws a standard bullet, as specified by the value of GetBulletName. This function should be overridden.
    */
    virtual bool DrawStandardBullet(wxRichTextParagraph* paragraph, wxDC& dc, const wxRichTextAttr& attr, const wxRect& rect) = 0;

    /**
        Draws a bullet that can be described by text, such as numbered or symbol bullets. This function should be overridden.
    */
    virtual bool DrawTextBullet(wxRichTextParagraph* paragraph, wxDC& dc, const wxRichTextAttr& attr, const wxRect& rect, const wxString& text) = 0;

    /**
        Draws a bitmap bullet, where the bullet bitmap is specified by the value of GetBulletName. This function should be overridden.
    */
    virtual bool DrawBitmapBullet(wxRichTextParagraph* paragraph, wxDC& dc, const wxRichTextAttr& attr, const wxRect& rect) = 0;

    /**
        Enumerate the standard bullet names currently supported. This function should be overridden.
    */
    virtual bool EnumerateStandardBulletNames(wxArrayString& bulletNames) = 0;
};

/**
    @class wxRichTextStdRenderer

    The standard renderer for drawing bullets.

    @library{wxrichtext}
    @category{richtext}

    @see wxRichTextRenderer, wxRichTextBuffer, wxRichTextCtrl
*/

class WXDLLIMPEXP_RICHTEXT wxRichTextStdRenderer: public wxRichTextRenderer
{
public:
    /**
        Constructor.
    */
    wxRichTextStdRenderer() {}

    // Draw a standard bullet, as specified by the value of GetBulletName
    virtual bool DrawStandardBullet(wxRichTextParagraph* paragraph, wxDC& dc, const wxRichTextAttr& attr, const wxRect& rect);

    // Draw a bullet that can be described by text, such as numbered or symbol bullets
    virtual bool DrawTextBullet(wxRichTextParagraph* paragraph, wxDC& dc, const wxRichTextAttr& attr, const wxRect& rect, const wxString& text);

    // Draw a bitmap bullet, where the bullet bitmap is specified by the value of GetBulletName
    virtual bool DrawBitmapBullet(wxRichTextParagraph* paragraph, wxDC& dc, const wxRichTextAttr& attr, const wxRect& rect);

    // Enumerate the standard bullet names currently supported
    virtual bool EnumerateStandardBulletNames(wxArrayString& bulletNames);
};

/*!
 * Utilities
 *
 */

inline bool wxRichTextHasStyle(int flags, int style)
{
    return ((flags & style) == style);
}

/// Compare two attribute objects
WXDLLIMPEXP_RICHTEXT bool wxTextAttrEq(const wxRichTextAttr& attr1, const wxRichTextAttr& attr2);
WXDLLIMPEXP_RICHTEXT bool wxTextAttrEq(const wxRichTextAttr& attr1, const wxRichTextAttr& attr2);

/// Apply one style to another
WXDLLIMPEXP_RICHTEXT bool wxRichTextApplyStyle(wxRichTextAttr& destStyle, const wxRichTextAttr& style, wxRichTextAttr* compareWith = NULL);

// Remove attributes
WXDLLIMPEXP_RICHTEXT bool wxRichTextRemoveStyle(wxRichTextAttr& destStyle, const wxRichTextAttr& style);

/// Combine two bitlists
WXDLLIMPEXP_RICHTEXT bool wxRichTextCombineBitlists(int& valueA, int valueB, int& flagsA, int flagsB);

/// Compare two bitlists
WXDLLIMPEXP_RICHTEXT bool wxRichTextBitlistsEqPartial(int valueA, int valueB, int flags);

/// Split into paragraph and character styles
WXDLLIMPEXP_RICHTEXT bool wxRichTextSplitParaCharStyles(const wxRichTextAttr& style, wxRichTextAttr& parStyle, wxRichTextAttr& charStyle);

/// Compare tabs
WXDLLIMPEXP_RICHTEXT bool wxRichTextTabsEq(const wxArrayInt& tabs1, const wxArrayInt& tabs2);

/// Convert a decimal to Roman numerals
WXDLLIMPEXP_RICHTEXT wxString wxRichTextDecimalToRoman(long n);

// Collects the attributes that are common to a range of content, building up a note of
// which attributes are absent in some objects and which clash in some objects.
WXDLLIMPEXP_RICHTEXT void wxTextAttrCollectCommonAttributes(wxTextAttr& currentStyle, const wxTextAttr& attr, wxTextAttr& clashingAttr, wxTextAttr& absentAttr);

WXDLLIMPEXP_RICHTEXT void wxRichTextModuleInit();

#endif
    // wxUSE_RICHTEXT

#endif
    // _WX_RICHTEXTBUFFER_H_

