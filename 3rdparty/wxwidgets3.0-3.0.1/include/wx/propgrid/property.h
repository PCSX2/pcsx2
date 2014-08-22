/////////////////////////////////////////////////////////////////////////////
// Name:        wx/propgrid/property.h
// Purpose:     wxPGProperty and related support classes
// Author:      Jaakko Salli
// Modified by:
// Created:     2008-08-23
// Copyright:   (c) Jaakko Salli
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PROPGRID_PROPERTY_H_
#define _WX_PROPGRID_PROPERTY_H_

#include "wx/defs.h"

#if wxUSE_PROPGRID

#include "wx/propgrid/propgriddefs.h"

// -----------------------------------------------------------------------

#define wxNullProperty  ((wxPGProperty*)NULL)


/** @class wxPGPaintData

    Contains information relayed to property's OnCustomPaint.
*/
struct wxPGPaintData
{
    /** wxPropertyGrid. */
    const wxPropertyGrid*   m_parent;

    /**
        Normally -1, otherwise index to drop-down list item that has to be
        drawn.
     */
    int                     m_choiceItem;

    /** Set to drawn width in OnCustomPaint (optional). */
    int                     m_drawnWidth;

    /**
        In a measure item call, set this to the height of item at m_choiceItem
        index.
     */
    int                     m_drawnHeight;
};


// space between vertical sides of a custom image
#define wxPG_CUSTOM_IMAGE_SPACINGY      1

// space between caption and selection rectangle,
#define wxPG_CAPRECTXMARGIN             2

// horizontally and vertically
#define wxPG_CAPRECTYMARGIN             1


/** @class wxPGCellRenderer

    Base class for wxPropertyGrid cell renderers.
*/
class WXDLLIMPEXP_PROPGRID wxPGCellRenderer : public wxObjectRefData
{
public:

    wxPGCellRenderer()
        : wxObjectRefData() { }
    virtual ~wxPGCellRenderer() { }

    // Render flags
    enum
    {
        // We are painting selected item
        Selected        = 0x00010000,

        // We are painting item in choice popup
        ChoicePopup     = 0x00020000,

        // We are rendering wxOwnerDrawnComboBox control
        // (or other owner drawn control, but that is only
        // officially supported one ATM).
        Control         = 0x00040000,

        // We are painting a disable property
        Disabled        = 0x00080000,

        // We are painting selected, disabled, or similar
        // item that dictates fore- and background colours,
        // overriding any cell values.
        DontUseCellFgCol    = 0x00100000,
        DontUseCellBgCol    = 0x00200000,
        DontUseCellColours  = DontUseCellFgCol |
                              DontUseCellBgCol
    };

    /**
        Returns @true if rendered something in the foreground (text or
        bitmap.
    */
    virtual bool Render( wxDC& dc,
                         const wxRect& rect,
                         const wxPropertyGrid* propertyGrid,
                         wxPGProperty* property,
                         int column,
                         int item,
                         int flags ) const = 0;

    /** Returns size of the image in front of the editable area.
        @remarks
        If property is NULL, then this call is for a custom value. In that case
        the item is index to wxPropertyGrid's custom values.
    */
    virtual wxSize GetImageSize( const wxPGProperty* property,
                                 int column,
                                 int item ) const;

    /** Paints property category selection rectangle.
    */
    virtual void DrawCaptionSelectionRect( wxDC& dc,
                                           int x, int y,
                                           int w, int h ) const;

    /** Utility to draw vertically centered text.
    */
    void DrawText( wxDC& dc,
                   const wxRect& rect,
                   int imageWidth,
                   const wxString& text ) const;

    /**
        Utility to draw editor's value, or vertically aligned text if editor is
        NULL.
    */
    void DrawEditorValue( wxDC& dc, const wxRect& rect,
                          int xOffset, const wxString& text,
                          wxPGProperty* property,
                          const wxPGEditor* editor ) const;

    /** Utility to render cell bitmap and set text colour plus bg brush
        colour.

        @return Returns image width, which, for instance, can be passed to
                DrawText.
    */
    int PreDrawCell( wxDC& dc,
                     const wxRect& rect,
                     const wxPGCell& cell,
                     int flags ) const;

    /**
        Utility to be called after drawing is done, to revert whatever
        changes PreDrawCell() did.

        @param flags
            Same as those passed to PreDrawCell().
    */
    void PostDrawCell( wxDC& dc,
                       const wxPropertyGrid* propGrid,
                       const wxPGCell& cell,
                       int flags ) const;
};


/**
    @class wxPGDefaultRenderer

    Default cell renderer, that can handles the common
    scenarios.
*/
class WXDLLIMPEXP_PROPGRID wxPGDefaultRenderer : public wxPGCellRenderer
{
public:
    virtual bool Render( wxDC& dc,
                         const wxRect& rect,
                         const wxPropertyGrid* propertyGrid,
                         wxPGProperty* property,
                         int column,
                         int item,
                         int flags ) const;

    virtual wxSize GetImageSize( const wxPGProperty* property,
                                 int column,
                                 int item ) const;

protected:
};


class WXDLLIMPEXP_PROPGRID wxPGCellData : public wxObjectRefData
{
    friend class wxPGCell;
public:
    wxPGCellData();

    void SetText( const wxString& text )
    {
        m_text = text;
        m_hasValidText = true;
    }
    void SetBitmap( const wxBitmap& bitmap ) { m_bitmap = bitmap; }
    void SetFgCol( const wxColour& col ) { m_fgCol = col; }
    void SetBgCol( const wxColour& col ) { m_bgCol = col; }
    void SetFont( const wxFont& font ) { m_font = font; }

protected:
    virtual ~wxPGCellData() { }

    wxString    m_text;
    wxBitmap    m_bitmap;
    wxColour    m_fgCol;
    wxColour    m_bgCol;
    wxFont      m_font;

    // True if m_text is valid and specified
    bool        m_hasValidText;
};


/**
    @class wxPGCell

    Base class for wxPropertyGrid cell information.
*/
class WXDLLIMPEXP_PROPGRID wxPGCell : public wxObject
{
public:
    wxPGCell();
    wxPGCell(const wxPGCell& other)
        : wxObject(other)
    {
    }

    wxPGCell( const wxString& text,
              const wxBitmap& bitmap = wxNullBitmap,
              const wxColour& fgCol = wxNullColour,
              const wxColour& bgCol = wxNullColour );

    virtual ~wxPGCell() { }

    wxPGCellData* GetData()
    {
        return (wxPGCellData*) m_refData;
    }

    const wxPGCellData* GetData() const
    {
        return (const wxPGCellData*) m_refData;
    }

    bool HasText() const
    {
        return (m_refData && GetData()->m_hasValidText);
    }

    /**
        Sets empty but valid data to this cell object.
    */
    void SetEmptyData();

    /**
        Merges valid data from srcCell into this.
    */
    void MergeFrom( const wxPGCell& srcCell );

    void SetText( const wxString& text );
    void SetBitmap( const wxBitmap& bitmap );
    void SetFgCol( const wxColour& col );

    /**
        Sets font of the cell.

        @remarks Because wxPropertyGrid does not support rows of
                 different height, it makes little sense to change
                 size of the font. Therefore it is recommended
                 to use return value of wxPropertyGrid::GetFont()
                 or wxPropertyGrid::GetCaptionFont() as a basis
                 for the font that, after modifications, is passed
                 to this member function.
    */
    void SetFont( const wxFont& font );

    void SetBgCol( const wxColour& col );

    const wxString& GetText() const { return GetData()->m_text; }
    const wxBitmap& GetBitmap() const { return GetData()->m_bitmap; }
    const wxColour& GetFgCol() const { return GetData()->m_fgCol; }

    /**
        Returns font of the cell. If no specific font is set for this
        cell, then the font will be invalid.
    */
    const wxFont& GetFont() const { return GetData()->m_font; }

    const wxColour& GetBgCol() const { return GetData()->m_bgCol; }

    wxPGCell& operator=( const wxPGCell& other )
    {
        if ( this != &other )
        {
            Ref(other);
        }
        return *this;
    }

    // Used mostly internally to figure out if this cell is supposed
    // to have default values when attached to a grid.
    bool IsInvalid() const
    {
        return ( m_refData == NULL );
    }

private:
    virtual wxObjectRefData *CreateRefData() const
        { return new wxPGCellData(); }

    virtual wxObjectRefData *CloneRefData(const wxObjectRefData *data) const;
};

// -----------------------------------------------------------------------

/** @class wxPGAttributeStorage

    wxPGAttributeStorage is somewhat optimized storage for
      key=variant pairs (ie. a map).
*/
class WXDLLIMPEXP_PROPGRID wxPGAttributeStorage
{
public:
    wxPGAttributeStorage();
    ~wxPGAttributeStorage();

    void Set( const wxString& name, const wxVariant& value );
    unsigned int GetCount() const { return (unsigned int) m_map.size(); }
    wxVariant FindValue( const wxString& name ) const
    {
        wxPGHashMapS2P::const_iterator it = m_map.find(name);
        if ( it != m_map.end() )
        {
            wxVariantData* data = (wxVariantData*) it->second;
            data->IncRef();
            return wxVariant(data, it->first);
        }
        return wxVariant();
    }

    typedef wxPGHashMapS2P::const_iterator const_iterator;
    const_iterator StartIteration() const
    {
        return m_map.begin();
    }
    bool GetNext( const_iterator& it, wxVariant& variant ) const
    {
        if ( it == m_map.end() )
            return false;

        wxVariantData* data = (wxVariantData*) it->second;
        data->IncRef();
        variant.SetData(data);
        variant.SetName(it->first);
        ++it;
        return true;
    }

protected:
    wxPGHashMapS2P  m_map;
};


// -----------------------------------------------------------------------

/** @section propgrid_propflags wxPGProperty Flags
    @{
*/

enum wxPGPropertyFlags
{

/** Indicates bold font.
*/
wxPG_PROP_MODIFIED                  = 0x0001,

/** Disables ('greyed' text and editor does not activate) property.
*/
wxPG_PROP_DISABLED                  = 0x0002,

/** Hider button will hide this property.
*/
wxPG_PROP_HIDDEN                    = 0x0004,

/** This property has custom paint image just in front of its value.
    If property only draws custom images into a popup list, then this
    flag should not be set.
*/
wxPG_PROP_CUSTOMIMAGE               = 0x0008,

/** Do not create text based editor for this property (but button-triggered
    dialog and choice are ok).
*/
wxPG_PROP_NOEDITOR                  = 0x0010,

/** Property is collapsed, ie. it's children are hidden.
*/
wxPG_PROP_COLLAPSED                 = 0x0020,

/**
    If property is selected, then indicates that validation failed for pending
    value.

    If property is not selected, that indicates that the actual property
    value has failed validation (NB: this behaviour is not currently supported,
    but may be used in future).
*/
wxPG_PROP_INVALID_VALUE             = 0x0040,

// 0x0080,

/** Switched via SetWasModified(). Temporary flag - only used when
    setting/changing property value.
*/
wxPG_PROP_WAS_MODIFIED              = 0x0200,

/**
    If set, then child properties (if any) are private, and should be
    "invisible" to the application.
*/
wxPG_PROP_AGGREGATE                 = 0x0400,

/** If set, then child properties (if any) are copies and should not
    be deleted in dtor.
*/
wxPG_PROP_CHILDREN_ARE_COPIES       = 0x0800,

/**
    Classifies this item as a non-category.

    Used for faster item type identification.
*/
wxPG_PROP_PROPERTY                  = 0x1000,

/**
    Classifies this item as a category.

    Used for faster item type identification.
*/
wxPG_PROP_CATEGORY                  = 0x2000,

/** Classifies this item as a property that has children, but is not aggregate
    (ie children are not private).
*/
wxPG_PROP_MISC_PARENT               = 0x4000,

/** Property is read-only. Editor is still created for wxTextCtrl-based
    property editors. For others, editor is not usually created because
    they do implement wxTE_READONLY style or equivalent.
*/
wxPG_PROP_READONLY                  = 0x8000,

//
// NB: FLAGS ABOVE 0x8000 CANNOT BE USED WITH PROPERTY ITERATORS
//

/** Property's value is composed from values of child properties.
    @remarks
    This flag cannot be used with property iterators.
*/
wxPG_PROP_COMPOSED_VALUE            = 0x00010000,

/** Common value of property is selectable in editor.
    @remarks
    This flag cannot be used with property iterators.
*/
wxPG_PROP_USES_COMMON_VALUE         = 0x00020000,

/** Property can be set to unspecified value via editor.
    Currently, this applies to following properties:
    - wxIntProperty, wxUIntProperty, wxFloatProperty, wxEditEnumProperty:
      Clear the text field

    @remarks
    This flag cannot be used with property iterators.

    @see wxPGProperty::SetAutoUnspecified()
*/
wxPG_PROP_AUTO_UNSPECIFIED          = 0x00040000,

/** Indicates the bit useable by derived properties.
*/
wxPG_PROP_CLASS_SPECIFIC_1          = 0x00080000,

/** Indicates the bit useable by derived properties.
*/
wxPG_PROP_CLASS_SPECIFIC_2          = 0x00100000,

/** Indicates that the property is being deleted and should be ignored.
*/
wxPG_PROP_BEING_DELETED             = 0x00200000

};

/** Topmost flag.
*/
#define wxPG_PROP_MAX               wxPG_PROP_AUTO_UNSPECIFIED

/** Property with children must have one of these set, otherwise iterators
    will not work correctly.
    Code should automatically take care of this, however.
*/
#define wxPG_PROP_PARENTAL_FLAGS \
    ((wxPGPropertyFlags)(wxPG_PROP_AGGREGATE | \
                         wxPG_PROP_CATEGORY | \
                         wxPG_PROP_MISC_PARENT))

/** @}
*/

// Combination of flags that can be stored by GetFlagsAsString
#define wxPG_STRING_STORED_FLAGS \
    (wxPG_PROP_DISABLED|wxPG_PROP_HIDDEN|wxPG_PROP_NOEDITOR|wxPG_PROP_COLLAPSED)

// -----------------------------------------------------------------------

/**
    @section propgrid_property_attributes wxPropertyGrid Property Attribute
    Identifiers.

    wxPGProperty::SetAttribute() and
    wxPropertyGridInterface::SetPropertyAttribute() accept one of these as
    attribute name argument.

    You can use strings instead of constants. However, some of these
    constants are redefined to use cached strings which may reduce
    your binary size by some amount.

    @{
*/

/** Set default value for property.
*/
#define wxPG_ATTR_DEFAULT_VALUE           wxS("DefaultValue")

/** Universal, int or double. Minimum value for numeric properties.
*/
#define wxPG_ATTR_MIN                     wxS("Min")

/** Universal, int or double. Maximum value for numeric properties.
*/
#define wxPG_ATTR_MAX                     wxS("Max")

/** Universal, string. When set, will be shown as text after the displayed
    text value. Alternatively, if third column is enabled, text will be shown
    there (for any type of property).
*/
#define wxPG_ATTR_UNITS                     wxS("Units")

/** When set, will be shown as 'greyed' text in property's value cell when
    the actual displayed value is blank.
*/
#define wxPG_ATTR_HINT                      wxS("Hint")

#if wxPG_COMPATIBILITY_1_4
/**
    @deprecated Use "Hint" (wxPG_ATTR_HINT) instead.
*/
#define wxPG_ATTR_INLINE_HELP               wxS("InlineHelp")
#endif

/** Universal, wxArrayString. Set to enable auto-completion in any
    wxTextCtrl-based property editor.
*/
#define wxPG_ATTR_AUTOCOMPLETE              wxS("AutoComplete")

/** wxBoolProperty and wxFlagsProperty specific. Value type is bool.
    Default value is False.

    When set to True, bool property will use check box instead of a
    combo box as its editor control. If you set this attribute
    for a wxFlagsProperty, it is automatically applied to child
    bool properties.
*/
#define wxPG_BOOL_USE_CHECKBOX              wxS("UseCheckbox")

/** wxBoolProperty and wxFlagsProperty specific. Value type is bool.
    Default value is False.

    Set to True for the bool property to cycle value on double click
    (instead of showing the popup listbox). If you set this attribute
    for a wxFlagsProperty, it is automatically applied to child
    bool properties.
*/
#define wxPG_BOOL_USE_DOUBLE_CLICK_CYCLING  wxS("UseDClickCycling")

/**
    wxFloatProperty (and similar) specific, int, default -1.

    Sets the (max) precision used when floating point value is rendered as
    text. The default -1 means infinite precision.
*/
#define wxPG_FLOAT_PRECISION                wxS("Precision")

/**
    The text will be echoed as asterisks (wxTE_PASSWORD will be passed to
    textctrl etc).
*/
#define wxPG_STRING_PASSWORD                wxS("Password")

/** Define base used by a wxUIntProperty. Valid constants are
    wxPG_BASE_OCT, wxPG_BASE_DEC, wxPG_BASE_HEX and wxPG_BASE_HEXL
    (lowercase characters).
*/
#define wxPG_UINT_BASE                      wxS("Base")

/** Define prefix rendered to wxUIntProperty. Accepted constants
    wxPG_PREFIX_NONE, wxPG_PREFIX_0x, and wxPG_PREFIX_DOLLAR_SIGN.
    <b>Note:</b> Only wxPG_PREFIX_NONE works with Decimal and Octal
    numbers.
*/
#define wxPG_UINT_PREFIX                    wxS("Prefix")

/**
    wxFileProperty/wxImageFileProperty specific, wxChar*, default is
    detected/varies.
    Sets the wildcard used in the triggered wxFileDialog. Format is the same.
*/
#define wxPG_FILE_WILDCARD                  wxS("Wildcard")

/** wxFileProperty/wxImageFileProperty specific, int, default 1.
    When 0, only the file name is shown (i.e. drive and directory are hidden).
*/
#define wxPG_FILE_SHOW_FULL_PATH            wxS("ShowFullPath")

/** Specific to wxFileProperty and derived properties, wxString, default empty.
    If set, then the filename is shown relative to the given path string.
*/
#define wxPG_FILE_SHOW_RELATIVE_PATH        wxS("ShowRelativePath")

/**
    Specific to wxFileProperty and derived properties, wxString, default is
    empty.

    Sets the initial path of where to look for files.
*/
#define wxPG_FILE_INITIAL_PATH              wxS("InitialPath")

/** Specific to wxFileProperty and derivatives, wxString, default is empty.
    Sets a specific title for the dir dialog.
*/
#define wxPG_FILE_DIALOG_TITLE              wxS("DialogTitle")

/** Specific to wxFileProperty and derivatives, long, default is 0.
    Sets a specific wxFileDialog style for the file dialog.
*/
#define wxPG_FILE_DIALOG_STYLE              wxS("DialogStyle")

/** Specific to wxDirProperty, wxString, default is empty.
    Sets a specific message for the dir dialog.
*/
#define wxPG_DIR_DIALOG_MESSAGE             wxS("DialogMessage")

/**
    wxArrayStringProperty's string delimiter character. If this is a quotation
    mark or hyphen, then strings will be quoted instead (with given
    character).

    Default delimiter is quotation mark.
*/
#define wxPG_ARRAY_DELIMITER                wxS("Delimiter")

/** Sets displayed date format for wxDateProperty.
*/
#define wxPG_DATE_FORMAT                    wxS("DateFormat")

/** Sets wxDatePickerCtrl window style used with wxDateProperty. Default
    is wxDP_DEFAULT | wxDP_SHOWCENTURY.
*/
#define wxPG_DATE_PICKER_STYLE              wxS("PickerStyle")

/** SpinCtrl editor, int or double. How much number changes when button is
    pressed (or up/down on keyboard).
*/
#define wxPG_ATTR_SPINCTRL_STEP             wxS("Step")

/** SpinCtrl editor, bool. If true, value wraps at Min/Max.
*/
#define wxPG_ATTR_SPINCTRL_WRAP             wxS("Wrap")

/**
    wxMultiChoiceProperty, int.
    If 0, no user strings allowed. If 1, user strings appear before list
    strings. If 2, user strings appear after list string.
*/
#define wxPG_ATTR_MULTICHOICE_USERSTRINGMODE    wxS("UserStringMode")

/**
    wxColourProperty and its kind, int, default 1.

    Setting this attribute to 0 hides custom colour from property's list of
    choices.
*/
#define wxPG_COLOUR_ALLOW_CUSTOM            wxS("AllowCustom")

/**
    wxColourProperty and its kind: Set to True in order to support editing
    alpha colour component.
*/
#define wxPG_COLOUR_HAS_ALPHA               wxS("HasAlpha")

/** @}
*/

// Redefine attribute macros to use cached strings
#undef wxPG_ATTR_DEFAULT_VALUE
#define wxPG_ATTR_DEFAULT_VALUE           wxPGGlobalVars->m_strDefaultValue
#undef wxPG_ATTR_MIN
#define wxPG_ATTR_MIN                     wxPGGlobalVars->m_strMin
#undef wxPG_ATTR_MAX
#define wxPG_ATTR_MAX                     wxPGGlobalVars->m_strMax
#undef wxPG_ATTR_UNITS
#define wxPG_ATTR_UNITS                   wxPGGlobalVars->m_strUnits
#undef wxPG_ATTR_HINT
#define wxPG_ATTR_HINT                    wxPGGlobalVars->m_strHint
#if wxPG_COMPATIBILITY_1_4
#undef wxPG_ATTR_INLINE_HELP
#define wxPG_ATTR_INLINE_HELP             wxPGGlobalVars->m_strInlineHelp
#endif

// -----------------------------------------------------------------------

/** @class wxPGChoiceEntry
    Data of a single wxPGChoices choice.
*/
class WXDLLIMPEXP_PROPGRID wxPGChoiceEntry : public wxPGCell
{
public:
    wxPGChoiceEntry();
    wxPGChoiceEntry(const wxPGChoiceEntry& other)
        : wxPGCell(other)
    {
        m_value = other.m_value;
    }
    wxPGChoiceEntry( const wxString& label,
                     int value = wxPG_INVALID_VALUE )
        : wxPGCell(), m_value(value)
    {
        SetText(label);
    }

    virtual ~wxPGChoiceEntry() { }

    void SetValue( int value ) { m_value = value; }
    int GetValue() const { return m_value; }

    wxPGChoiceEntry& operator=( const wxPGChoiceEntry& other )
    {
        if ( this != &other )
        {
            Ref(other);
        }
        m_value = other.m_value;
        return *this;
    }

protected:
    int m_value;
};


typedef void* wxPGChoicesId;

class WXDLLIMPEXP_PROPGRID wxPGChoicesData : public wxObjectRefData
{
    friend class wxPGChoices;
public:
    // Constructor sets m_refCount to 1.
    wxPGChoicesData();

    void CopyDataFrom( wxPGChoicesData* data );

    wxPGChoiceEntry& Insert( int index, const wxPGChoiceEntry& item );

    // Delete all entries
    void Clear();

    unsigned int GetCount() const
    {
        return (unsigned int) m_items.size();
    }

    const wxPGChoiceEntry& Item( unsigned int i ) const
    {
        wxASSERT_MSG( i < GetCount(), "invalid index" );
        return m_items[i];
    }

    wxPGChoiceEntry& Item( unsigned int i )
    {
        wxASSERT_MSG( i < GetCount(), "invalid index" );
        return m_items[i];
    }

private:
    wxVector<wxPGChoiceEntry>   m_items;

    virtual ~wxPGChoicesData();
};

#define wxPGChoicesEmptyData    ((wxPGChoicesData*)NULL)


/** @class wxPGChoices

    Helper class for managing choices of wxPropertyGrid properties.
    Each entry can have label, value, bitmap, text colour, and background
    colour.

    wxPGChoices uses reference counting, similar to other wxWidgets classes.
    This means that assignment operator and copy constructor only copy the
    reference and not the actual data. Use Copy() member function to create a
    real copy.

    @remarks If you do not specify value for entry, index is used.

    @library{wxpropgrid}
    @category{propgrid}
*/
class WXDLLIMPEXP_PROPGRID wxPGChoices
{
public:
    typedef long ValArrItem;

    /** Default constructor. */
    wxPGChoices()
    {
        Init();
    }

    /**
        Copy constructor, uses reference counting. To create a real copy,
        use Copy() member function instead.
    */
    wxPGChoices( const wxPGChoices& a )
    {
        if ( a.m_data != wxPGChoicesEmptyData )
        {
            m_data = a.m_data;
            m_data->IncRef();
        }
    }

    /**
        Constructor.

        @param labels
            Labels for choices

        @param values
            Values for choices. If NULL, indexes are used.
    */
    wxPGChoices( const wxChar* const* labels, const long* values = NULL )
    {
        Init();
        Set(labels,values);
    }

    /**
        Constructor.

        @param labels
            Labels for choices

        @param values
            Values for choices. If empty, indexes are used.
    */
    wxPGChoices( const wxArrayString& labels,
                 const wxArrayInt& values = wxArrayInt() )
    {
        Init();
        Set(labels,values);
    }

    /** Simple interface constructor. */
    wxPGChoices( wxPGChoicesData* data )
    {
        wxASSERT(data);
        m_data = data;
        data->IncRef();
    }

    /** Destructor. */
    ~wxPGChoices()
    {
        Free();
    }

    /**
        Adds to current.

        If did not have own copies, creates them now. If was empty, identical
        to set except that creates copies.

        @param labels
            Labels for added choices.

        @param values
            Values for added choices. If empty, relevant entry indexes are used.
    */
    void Add( const wxChar* const* labels, const ValArrItem* values = NULL );

    /** Version that works with wxArrayString and wxArrayInt. */
    void Add( const wxArrayString& arr, const wxArrayInt& arrint = wxArrayInt() );

    /**
        Adds a single choice.

        @param label
            Label for added choice.

        @param value
            Value for added choice. If unspecified, index is used.
    */
    wxPGChoiceEntry& Add( const wxString& label,
                          int value = wxPG_INVALID_VALUE );

    /** Adds a single item, with bitmap. */
    wxPGChoiceEntry& Add( const wxString& label,
                          const wxBitmap& bitmap,
                          int value = wxPG_INVALID_VALUE );

    /** Adds a single item with full entry information. */
    wxPGChoiceEntry& Add( const wxPGChoiceEntry& entry )
    {
        return Insert(entry, -1);
    }

    /** Adds single item. */
    wxPGChoiceEntry& AddAsSorted( const wxString& label,
                                  int value = wxPG_INVALID_VALUE );

    /**
        Assigns choices data, using reference counting. To create a real copy,
        use Copy() member function instead.
    */
    void Assign( const wxPGChoices& a )
    {
        AssignData(a.m_data);
    }

    void AssignData( wxPGChoicesData* data );

    /** Delete all choices. */
    void Clear();

    /**
        Returns a real copy of the choices.
    */
    wxPGChoices Copy() const
    {
        wxPGChoices dst;
        dst.EnsureData();
        dst.m_data->CopyDataFrom(m_data);
        return dst;
    }

    void EnsureData()
    {
        if ( m_data == wxPGChoicesEmptyData )
            m_data = new wxPGChoicesData();
    }

    /** Gets a unsigned number identifying this list. */
    wxPGChoicesId GetId() const { return (wxPGChoicesId) m_data; }

    const wxString& GetLabel( unsigned int ind ) const
    {
        return Item(ind).GetText();
    }

    unsigned int GetCount () const
    {
        if ( !m_data )
            return 0;

        return m_data->GetCount();
    }

    int GetValue( unsigned int ind ) const { return Item(ind).GetValue(); }

    /** Returns array of values matching the given strings. Unmatching strings
        result in wxPG_INVALID_VALUE entry in array.
    */
    wxArrayInt GetValuesForStrings( const wxArrayString& strings ) const;

    /** Returns array of indices matching given strings. Unmatching strings
        are added to 'unmatched', if not NULL.
    */
    wxArrayInt GetIndicesForStrings( const wxArrayString& strings,
                                     wxArrayString* unmatched = NULL ) const;

    int Index( const wxString& str ) const;
    int Index( int val ) const;

    /** Inserts single item. */
    wxPGChoiceEntry& Insert( const wxString& label,
                             int index,
                             int value = wxPG_INVALID_VALUE );

    /** Inserts a single item with full entry information. */
    wxPGChoiceEntry& Insert( const wxPGChoiceEntry& entry, int index );

    /** Returns false if this is a constant empty set of choices,
        which should not be modified.
    */
    bool IsOk() const
    {
        return ( m_data != wxPGChoicesEmptyData );
    }

    const wxPGChoiceEntry& Item( unsigned int i ) const
    {
        wxASSERT( IsOk() );
        return m_data->Item(i);
    }

    wxPGChoiceEntry& Item( unsigned int i )
    {
        wxASSERT( IsOk() );
        return m_data->Item(i);
    }

    /** Removes count items starting at position nIndex. */
    void RemoveAt(size_t nIndex, size_t count = 1);

    /** Does not create copies for itself.
        TODO: Deprecate.
    */
    void Set( const wxChar* const* labels, const long* values = NULL )
    {
        Free();
        Add(labels,values);
    }

    /** Version that works with wxArrayString and wxArrayInt. */
    void Set( const wxArrayString& labels,
              const wxArrayInt& values = wxArrayInt() )
    {
        Free();
        if ( &values )
            Add(labels,values);
        else
            Add(labels);
    }

    // Creates exclusive copy of current choices
    void AllocExclusive();

    // Returns data, increases refcount.
    wxPGChoicesData* GetData()
    {
        wxASSERT( m_data->GetRefCount() != -1 );
        m_data->IncRef();
        return m_data;
    }

    // Returns plain data ptr - no refcounting stuff is done.
    wxPGChoicesData* GetDataPtr() const { return m_data; }

    // Changes ownership of data to you.
    wxPGChoicesData* ExtractData()
    {
        wxPGChoicesData* data = m_data;
        m_data = wxPGChoicesEmptyData;
        return data;
    }

    wxArrayString GetLabels() const;

    void operator= (const wxPGChoices& a)
    {
        if (this != &a)
            AssignData(a.m_data);
    }

    wxPGChoiceEntry& operator[](unsigned int i)
    {
        return Item(i);
    }

    const wxPGChoiceEntry& operator[](unsigned int i) const
    {
        return Item(i);
    }

protected:
    wxPGChoicesData*    m_data;

    void Init();
    void Free();
};

// -----------------------------------------------------------------------

/** @class wxPGProperty

    wxPGProperty is base class for all wxPropertyGrid properties.

    NB: Full class overview is now only present in
        interface/wx/propgrid/property.h.

    @library{wxpropgrid}
    @category{propgrid}
*/
class WXDLLIMPEXP_PROPGRID wxPGProperty : public wxObject
{
    friend class wxPropertyGrid;
    friend class wxPropertyGridInterface;
    friend class wxPropertyGridPageState;
    friend class wxPropertyGridPopulator;
    friend class wxStringProperty;  // Proper "<composed>" support requires this

    DECLARE_ABSTRACT_CLASS(wxPGProperty)
public:
    typedef wxUint32 FlagType;

    /**
        Default constructor.
    */
    wxPGProperty();

    /**
        Constructor.

        All non-abstract property classes should have a constructor with
        the same first two arguments as this one.
    */
    wxPGProperty( const wxString& label, const wxString& name );

    /**
        Virtual destructor.
        It is customary for derived properties to implement this.
    */
    virtual ~wxPGProperty();

    /** This virtual function is called after m_value has been set.

        @remarks
        - If m_value was set to Null variant (ie. unspecified value),
          OnSetValue() will not be called.
        - m_value may be of any variant type. Typically properties internally
          support only one variant type, and as such OnSetValue() provides a
          good opportunity to convert
          supported values into internal type.
        - Default implementation does nothing.
    */
    virtual void OnSetValue();

    /** Override this to return something else than m_value as the value.
    */
    virtual wxVariant DoGetValue() const { return m_value; }

    /** Implement this function in derived class to check the value.
        Return true if it is ok. Returning false prevents property change events
        from occurring.

        @remarks
        - Default implementation always returns true.
    */
    virtual bool ValidateValue( wxVariant& value,
                                wxPGValidationInfo& validationInfo ) const;

    /**
        Converts text into wxVariant value appropriate for this property.

        @param variant
            On function entry this is the old value (should not be wxNullVariant
            in normal cases). Translated value must be assigned back to it.

        @param text
            Text to be translated into variant.

        @param argFlags
            If wxPG_FULL_VALUE is set, returns complete, storable value instead
            of displayable one (they may be different).
            If wxPG_COMPOSITE_FRAGMENT is set, text is interpreted as a part of
            composite property string value (as generated by ValueToString()
            called with this same flag).

        @return Returns @true if resulting wxVariant value was different.

        @remarks Default implementation converts semicolon delimited tokens into
                child values. Only works for properties with children.

                You might want to take into account that m_value is Null variant
                if property value is unspecified (which is usually only case if
                you explicitly enabled that sort behaviour).
    */
    virtual bool StringToValue( wxVariant& variant,
                                const wxString& text,
                                int argFlags = 0 ) const;

    /**
        Converts integer (possibly a choice selection) into wxVariant value
        appropriate for this property.

        @param variant
            On function entry this is the old value (should not be wxNullVariant
            in normal cases). Translated value must be assigned back to it.

        @param number
            Integer to be translated into variant.

        @param argFlags
            If wxPG_FULL_VALUE is set, returns complete, storable value instead
            of displayable one.

        @return Returns @true if resulting wxVariant value was different.

        @remarks
        - If property is not supposed to use choice or spinctrl or other editor
          with int-based value, it is not necessary to implement this method.
        - Default implementation simply assign given int to m_value.
        - If property uses choice control, and displays a dialog on some choice
          items, then it is preferred to display that dialog in IntToValue
          instead of OnEvent.
        - You might want to take into account that m_value is Null variant if
          property value is unspecified (which is usually only case if you
          explicitly enabled that sort behaviour).
    */
    virtual bool IntToValue( wxVariant& value,
                             int number,
                             int argFlags = 0 ) const;

    /**
        Converts property value into a text representation.

        @param value
            Value to be converted.

        @param argFlags
            If 0 (default value), then displayed string is returned.
            If wxPG_FULL_VALUE is set, returns complete, storable string value
            instead of displayable. If wxPG_EDITABLE_VALUE is set, returns
            string value that must be editable in textctrl. If
            wxPG_COMPOSITE_FRAGMENT is set, returns text that is appropriate to
            display as a part of string property's composite text
            representation.

        @remarks Default implementation calls GenerateComposedValue().
    */
    virtual wxString ValueToString( wxVariant& value, int argFlags = 0 ) const;

    /** Converts string to a value, and if successful, calls SetValue() on it.
        Default behaviour is to do nothing.
        @param text
        String to get the value from.
        @return
        true if value was changed.
    */
    bool SetValueFromString( const wxString& text, int flags = wxPG_PROGRAMMATIC_VALUE );

    /** Converts integer to a value, and if successful, calls SetValue() on it.
        Default behaviour is to do nothing.
        @param value
            Int to get the value from.
        @param flags
            If has wxPG_FULL_VALUE, then the value given is a actual value and
            not an index.
        @return
            True if value was changed.
    */
    bool SetValueFromInt( long value, int flags = 0 );

    /**
        Returns size of the custom painted image in front of property.

        This method must be overridden to return non-default value if
        OnCustomPaint is to be called.
        @param item
            Normally -1, but can be an index to the property's list of items.
        @remarks
        - Default behaviour is to return wxSize(0,0), which means no image.
        - Default image width or height is indicated with dimension -1.
        - You can also return wxPG_DEFAULT_IMAGE_SIZE, i.e. wxSize(-1, -1).
    */
    virtual wxSize OnMeasureImage( int item = -1 ) const;

    /**
        Events received by editor widgets are processed here.

        Note that editor class usually processes most events. Some, such as
        button press events of TextCtrlAndButton class, can be handled here.
        Also, if custom handling for regular events is desired, then that can
        also be done (for example, wxSystemColourProperty custom handles
        wxEVT_CHOICE to display colour picker dialog when
        'custom' selection is made).

        If the event causes value to be changed, SetValueInEvent()
        should be called to set the new value.

        @param event
        Associated wxEvent.
        @return
        Should return true if any changes in value should be reported.
        @remarks
        If property uses choice control, and displays a dialog on some choice
        items, then it is preferred to display that dialog in IntToValue
        instead of OnEvent.
    */
    virtual bool OnEvent( wxPropertyGrid* propgrid,
                          wxWindow* wnd_primary,
                          wxEvent& event );

    /**
        Called after value of a child property has been altered. Must return
        new value of the whole property (after any alterations warranted by
        child's new value).

        Note that this function is usually called at the time that value of
        this property, or given child property, is still pending for change,
        and as such, result of GetValue() or m_value should not be relied
        on.

        Sample pseudo-code implementation:

        @code
        wxVariant MyProperty::ChildChanged( wxVariant& thisValue,
                                            int childIndex,
                                            wxVariant& childValue ) const
        {
            // Acquire reference to actual type of data stored in variant
            // (TFromVariant only exists if wxPropertyGrid's wxVariant-macros
            // were used to create the variant class).
            T& data = TFromVariant(thisValue);

            // Copy childValue into data.
            switch ( childIndex )
            {
                case 0:
                    data.SetSubProp1( childvalue.GetLong() );
                    break;
                case 1:
                    data.SetSubProp2( childvalue.GetString() );
                    break;
                ...
            }

            // Return altered data
            return data;
        }
        @endcode

        @param thisValue
            Value of this property. Changed value should be returned (in
            previous versions of wxPropertyGrid it was only necessary to
            write value back to this argument).
        @param childIndex
            Index of child changed (you can use Item(childIndex) to get
            child property).
        @param childValue
            (Pending) value of the child property.

        @return
            Modified value of the whole property.
    */
    virtual wxVariant ChildChanged( wxVariant& thisValue,
                                    int childIndex,
                                    wxVariant& childValue ) const;

    /** Returns pointer to an instance of used editor.
    */
    virtual const wxPGEditor* DoGetEditorClass() const;

    /** Returns pointer to the wxValidator that should be used
        with the editor of this property (NULL for no validator).
        Setting validator explicitly via SetPropertyValidator
        will override this.

        In most situations, code like this should work well
        (macros are used to maintain one actual validator instance,
        so on the second call the function exits within the first
        macro):

        @code

        wxValidator* wxMyPropertyClass::DoGetValidator () const
        {
            WX_PG_DOGETVALIDATOR_ENTRY()

            wxMyValidator* validator = new wxMyValidator(...);

            ... prepare validator...

            WX_PG_DOGETVALIDATOR_EXIT(validator)
        }

        @endcode

        @remarks
        You can get common filename validator by returning
        wxFileProperty::GetClassValidator(). wxDirProperty,
        for example, uses it.
    */
    virtual wxValidator* DoGetValidator () const;

    /**
        Override to paint an image in front of the property value text or
        drop-down list item (but only if wxPGProperty::OnMeasureImage is
        overridden as well).

        If property's OnMeasureImage() returns size that has height != 0 but
        less than row height ( < 0 has special meanings), wxPropertyGrid calls
        this method to draw a custom image in a limited area in front of the
        editor control or value text/graphics, and if control has drop-down
        list, then the image is drawn there as well (even in the case
        OnMeasureImage() returned higher height than row height).

        NOTE: Following applies when OnMeasureImage() returns a "flexible"
        height ( using wxPG_FLEXIBLE_SIZE(W,H) macro), which implies variable
        height items: If rect.x is < 0, then this is a measure item call, which
        means that dc is invalid and only thing that should be done is to set
        paintdata.m_drawnHeight to the height of the image of item at index
        paintdata.m_choiceItem. This call may be done even as often as once
        every drop-down popup show.

        @param dc
            wxDC to paint on.
        @param rect
            Box reserved for custom graphics. Includes surrounding rectangle,
            if any. If x is < 0, then this is a measure item call (see above).
        @param paintdata
            wxPGPaintData structure with much useful data.

        @remarks
            - You can actually exceed rect width, but if you do so then
              paintdata.m_drawnWidth must be set to the full width drawn in
              pixels.
            - Due to technical reasons, rect's height will be default even if
              custom height was reported during measure call.
            - Brush is guaranteed to be default background colour. It has been
              already used to clear the background of area being painted. It
              can be modified.
            - Pen is guaranteed to be 1-wide 'black' (or whatever is the proper
              colour) pen for drawing framing rectangle. It can be changed as
              well.

        @see ValueToString()
    */
    virtual void OnCustomPaint( wxDC& dc,
                                const wxRect& rect,
                                wxPGPaintData& paintdata );

    /**
        Returns used wxPGCellRenderer instance for given property column
        (label=0, value=1).

        Default implementation returns editor's renderer for all columns.
    */
    virtual wxPGCellRenderer* GetCellRenderer( int column ) const;

    /** Returns which choice is currently selected. Only applies to properties
        which have choices.

        Needs to reimplemented in derived class if property value does not
        map directly to a choice. Integer as index, bool, and string usually do.
    */
    virtual int GetChoiceSelection() const;

    /**
        Refresh values of child properties.

        Automatically called after value is set.
    */
    virtual void RefreshChildren();

    /**
        Reimplement this member function to add special handling for
        attributes of this property.

        @return Return @false to have the attribute automatically stored in
                m_attributes. Default implementation simply does that and
                nothing else.

        @remarks To actually set property attribute values from the
                 application, use wxPGProperty::SetAttribute() instead.
    */
    virtual bool DoSetAttribute( const wxString& name, wxVariant& value );

    /** Returns value of an attribute.

        Override if custom handling of attributes is needed.

        Default implementation simply return NULL variant.
    */
    virtual wxVariant DoGetAttribute( const wxString& name ) const;

    /** Returns instance of a new wxPGEditorDialogAdapter instance, which is
        used when user presses the (optional) button next to the editor control;

        Default implementation returns NULL (ie. no action is generated when
        button is pressed).
    */
    virtual wxPGEditorDialogAdapter* GetEditorDialog() const;

    /**
        Called whenever validation has failed with given pending value.

        @remarks If you implement this in your custom property class, please
                 remember to call the baser implementation as well, since they
                 may use it to revert property into pre-change state.
    */
    virtual void OnValidationFailure( wxVariant& pendingValue );

    /** Append a new choice to property's list of choices.
    */
    int AddChoice( const wxString& label, int value = wxPG_INVALID_VALUE )
    {
        return InsertChoice(label, wxNOT_FOUND, value);
    }

    /**
        Returns true if children of this property are component values (for
        instance, points size, face name, and is_underlined are component
        values of a font).
    */
    bool AreChildrenComponents() const
    {
        if ( m_flags & (wxPG_PROP_COMPOSED_VALUE|wxPG_PROP_AGGREGATE) )
            return true;

        return false;
    }

    /**
        Deletes children of the property.
    */
    void DeleteChildren();

    /**
        Removes entry from property's wxPGChoices and editor control (if it is
        active).

        If selected item is deleted, then the value is set to unspecified.
    */
    void DeleteChoice( int index );

    /**
        Enables or disables the property. Disabled property usually appears
        as having grey text.

        @param enable
            If @false, property is disabled instead.

        @see wxPropertyGridInterface::EnableProperty()
    */
    void Enable( bool enable = true );

    /**
        Call to enable or disable usage of common value (integer value that can
        be selected for properties instead of their normal values) for this
        property.

        Common values are disabled by the default for all properties.
    */
    void EnableCommonValue( bool enable = true )
    {
        if ( enable ) SetFlag( wxPG_PROP_USES_COMMON_VALUE );
        else ClearFlag( wxPG_PROP_USES_COMMON_VALUE );
    }

    /**
        Composes text from values of child properties.
    */
    wxString GenerateComposedValue() const
    {
        wxString s;
        DoGenerateComposedValue(s);
        return s;
    }

    /** Returns property's label. */
    const wxString& GetLabel() const { return m_label; }

    /** Returns property's name with all (non-category, non-root) parents. */
    wxString GetName() const;

    /**
        Returns property's base name (ie parent's name is not added in any
        case)
     */
    const wxString& GetBaseName() const { return m_name; }

    /** Returns read-only reference to property's list of choices.
    */
    const wxPGChoices& GetChoices() const
    {
        return m_choices;
    }

    /** Returns coordinate to the top y of the property. Note that the
        position of scrollbars is not taken into account.
    */
    int GetY() const;

    wxVariant GetValue() const
    {
        return DoGetValue();
    }

    /** Returns reference to the internal stored value. GetValue is preferred
        way to get the actual value, since GetValueRef ignores DoGetValue,
        which may override stored value.
    */
    wxVariant& GetValueRef()
    {
        return m_value;
    }

    const wxVariant& GetValueRef() const
    {
        return m_value;
    }

    // Helper function (for wxPython bindings and such) for settings protected
    // m_value.
    wxVariant GetValuePlain() const
    {
        return m_value;
    }

    /** Returns text representation of property's value.

        @param argFlags
            If 0 (default value), then displayed string is returned.
            If wxPG_FULL_VALUE is set, returns complete, storable string value
            instead of displayable. If wxPG_EDITABLE_VALUE is set, returns
            string value that must be editable in textctrl. If
            wxPG_COMPOSITE_FRAGMENT is set, returns text that is appropriate to
            display as a part of string property's composite text
            representation.

        @remarks In older versions, this function used to be overridden to convert
                 property's value into a string representation. This function is
                 now handled by ValueToString(), and overriding this function now
                 will result in run-time assertion failure.
    */
    virtual wxString GetValueAsString( int argFlags = 0 ) const;

    /** Synonymous to GetValueAsString().

        @deprecated Use GetValueAsString() instead.

        @see GetValueAsString()
    */
    wxDEPRECATED( wxString GetValueString( int argFlags = 0 ) const );

    /**
        Returns wxPGCell of given column.

        @remarks const version of this member function returns 'default'
                 wxPGCell object if the property itself didn't hold
                 cell data.
    */
    const wxPGCell& GetCell( unsigned int column ) const;

    /**
        Returns wxPGCell of given column, creating one if necessary.
    */
    wxPGCell& GetCell( unsigned int column )
    {
        return GetOrCreateCell(column);
    }

    /**
        Returns wxPGCell of given column, creating one if necessary.
    */
    wxPGCell& GetOrCreateCell( unsigned int column );

    /** Return number of displayed common values for this property.
    */
    int GetDisplayedCommonValueCount() const;

    wxString GetDisplayedString() const
    {
        return GetValueAsString(0);
    }

    /**
        Returns property's hint text (shown in empty value cell).
    */
    inline wxString GetHintText() const;

    /** Returns property grid where property lies. */
    wxPropertyGrid* GetGrid() const;

    /** Returns owner wxPropertyGrid, but only if one is currently on a page
        displaying this property. */
    wxPropertyGrid* GetGridIfDisplayed() const;

    /** Returns highest level non-category, non-root parent. Useful when you
        have nested wxCustomProperties/wxParentProperties.
        @remarks
        Thus, if immediate parent is root or category, this will return the
        property itself.
    */
    wxPGProperty* GetMainParent() const;

    /** Return parent of property */
    wxPGProperty* GetParent() const { return m_parent; }

    /** Returns true if property has editable wxTextCtrl when selected.

        @remarks
        Although disabled properties do not displayed editor, they still
        return True here as being disabled is considered a temporary
        condition (unlike being read-only or having limited editing enabled).
    */
    bool IsTextEditable() const;

    bool IsValueUnspecified() const
    {
        return m_value.IsNull();
    }

    /**
        Returns non-zero if property has given flag set.

        @see propgrid_propflags
    */
    FlagType HasFlag( wxPGPropertyFlags flag ) const
    {
        return ( m_flags & flag );
    }

    /** Returns comma-delimited string of property attributes.
    */
    const wxPGAttributeStorage& GetAttributes() const
    {
        return m_attributes;
    }

    /** Returns m_attributes as list wxVariant.
    */
    wxVariant GetAttributesAsList() const;

    /**
        Returns property flags.
    */
    FlagType GetFlags() const
    {
        return m_flags;
    }

    const wxPGEditor* GetEditorClass() const;

    wxString GetValueType() const
    {
        return m_value.GetType();
    }

    /** Returns editor used for given column. NULL for no editor.
    */
    const wxPGEditor* GetColumnEditor( int column ) const
    {
        if ( column == 1 )
            return GetEditorClass();

        return NULL;
    }

    /** Returns common value selected for this property. -1 for none.
    */
    int GetCommonValue() const
    {
        return m_commonValue;
    }

    /** Returns true if property has even one visible child.
    */
    bool HasVisibleChildren() const;

    /**
        Use this member function to add independent (ie. regular) children to
        a property.

        @return Inserted childProperty.

        @remarks wxPropertyGrid is not automatically refreshed by this
                 function.

        @see AddPrivateChild()
    */
    wxPGProperty* InsertChild( int index, wxPGProperty* childProperty );

    /** Inserts a new choice to property's list of choices.
    */
    int InsertChoice( const wxString& label, int index, int value = wxPG_INVALID_VALUE );

    /**
        Returns true if this property is actually a wxPropertyCategory.
    */
    bool IsCategory() const { return HasFlag(wxPG_PROP_CATEGORY)?true:false; }

    /** Returns true if this property is actually a wxRootProperty.
    */
    bool IsRoot() const { return (m_parent == NULL); }

    /** Returns true if this is a sub-property. */
    bool IsSubProperty() const
    {
        wxPGProperty* parent = (wxPGProperty*)m_parent;
        if ( parent && !parent->IsCategory() )
            return true;
        return false;
    }

    /** Returns last visible sub-property, recursively.
    */
    const wxPGProperty* GetLastVisibleSubItem() const;

    wxVariant GetDefaultValue() const;

    int GetMaxLength() const
    {
        return (int) m_maxLen;
    }

    /**
        Determines, recursively, if all children are not unspecified.

        @param pendingList
            Assumes members in this wxVariant list as pending
            replacement values.
    */
    bool AreAllChildrenSpecified( wxVariant* pendingList = NULL ) const;

    /** Updates composed values of parent non-category properties, recursively.
        Returns topmost property updated.

        @remarks
        - Must not call SetValue() (as can be called in it).
    */
    wxPGProperty* UpdateParentValues();

    /** Returns true if containing grid uses wxPG_EX_AUTO_UNSPECIFIED_VALUES.
    */
    bool UsesAutoUnspecified() const
    {
        return HasFlag(wxPG_PROP_AUTO_UNSPECIFIED)?true:false;
    }

    wxBitmap* GetValueImage() const
    {
        return m_valueBitmap;
    }

    wxVariant GetAttribute( const wxString& name ) const;

    /**
        Returns named attribute, as string, if found.

        Otherwise defVal is returned.
    */
    wxString GetAttribute( const wxString& name, const wxString& defVal ) const;

    /**
        Returns named attribute, as long, if found.

        Otherwise defVal is returned.
    */
    long GetAttributeAsLong( const wxString& name, long defVal ) const;

    /**
        Returns named attribute, as double, if found.

        Otherwise defVal is returned.
    */
    double GetAttributeAsDouble( const wxString& name, double defVal ) const;

    unsigned int GetDepth() const { return (unsigned int)m_depth; }

    /** Gets flags as a'|' delimited string. Note that flag names are not
        prepended with 'wxPG_PROP_'.
        @param flagsMask
        String will only be made to include flags combined by this parameter.
    */
    wxString GetFlagsAsString( FlagType flagsMask ) const;

    /** Returns position in parent's array. */
    unsigned int GetIndexInParent() const
    {
        return (unsigned int)m_arrIndex;
    }

    /** Hides or reveals the property.
        @param hide
            true for hide, false for reveal.
        @param flags
            By default changes are applied recursively. Set this paramter
            wxPG_DONT_RECURSE to prevent this.
    */
    bool Hide( bool hide, int flags = wxPG_RECURSE );

    bool IsExpanded() const
        { return (!(m_flags & wxPG_PROP_COLLAPSED) && GetChildCount()); }

    /** Returns true if all parents expanded.
    */
    bool IsVisible() const;

    bool IsEnabled() const { return !(m_flags & wxPG_PROP_DISABLED); }

    /** If property's editor is created this forces its recreation.
        Useful in SetAttribute etc. Returns true if actually did anything.
    */
    bool RecreateEditor();

    /** If property's editor is active, then update it's value.
    */
    void RefreshEditor();

    /** Sets an attribute for this property.
        @param name
        Text identifier of attribute. See @ref propgrid_property_attributes.
        @param value
        Value of attribute.
    */
    void SetAttribute( const wxString& name, wxVariant value );

    void SetAttributes( const wxPGAttributeStorage& attributes );

    /**
        Set if user can change the property's value to unspecified by
        modifying the value of the editor control (usually by clearing
        it).  Currently, this can work with following properties:
        wxIntProperty, wxUIntProperty, wxFloatProperty, wxEditEnumProperty.

        @param enable
            Whether to enable or disable this behaviour (it is disabled by
            default).
    */
    void SetAutoUnspecified( bool enable = true )
    {
        ChangeFlag(wxPG_PROP_AUTO_UNSPECIFIED, enable);
    }

    /**
        Sets property's background colour.

        @param colour
            Background colour to use.

        @param flags
            Default is wxPG_RECURSE which causes colour to be set recursively.
            Omit this flag to only set colour for the property in question
            and not any of its children.
    */
    void SetBackgroundColour( const wxColour& colour,
                              int flags = wxPG_RECURSE );

    /**
        Sets property's text colour.

        @param colour
            Text colour to use.

        @param flags
            Default is wxPG_RECURSE which causes colour to be set recursively.
            Omit this flag to only set colour for the property in question
            and not any of its children.
    */
    void SetTextColour( const wxColour& colour,
                        int flags = wxPG_RECURSE );

    /** Set default value of a property. Synonymous to

        @code
            SetAttribute("DefaultValue", value);
        @endcode
    */
    void SetDefaultValue( wxVariant& value );

    /** Sets editor for a property.

        @param editor
            For builtin editors, use wxPGEditor_X, where X is builtin editor's
            name (TextCtrl, Choice, etc. see wxPGEditor documentation for full
            list).

        For custom editors, use pointer you received from
        wxPropertyGrid::RegisterEditorClass().
    */
    void SetEditor( const wxPGEditor* editor )
    {
        m_customEditor = editor;
    }

    /** Sets editor for a property.
    */
    inline void SetEditor( const wxString& editorName );

    /**
        Sets cell information for given column.
    */
    void SetCell( int column, const wxPGCell& cell );

    /** Sets common value selected for this property. -1 for none.
    */
    void SetCommonValue( int commonValue )
    {
        m_commonValue = commonValue;
    }

    /** Sets flags from a '|' delimited string. Note that flag names are not
        prepended with 'wxPG_PROP_'.
    */
    void SetFlagsFromString( const wxString& str );

    /** Sets property's "is it modified?" flag. Affects children recursively.
    */
    void SetModifiedStatus( bool modified )
    {
        SetFlagRecursively(wxPG_PROP_MODIFIED, modified);
    }

    /** Call in OnEvent(), OnButtonClick() etc. to change the property value
        based on user input.

        @remarks
        This method is const since it doesn't actually modify value, but posts
        given variant as pending value, stored in wxPropertyGrid.
    */
    void SetValueInEvent( wxVariant value ) const;

    /**
        Call this to set value of the property.

        Unlike methods in wxPropertyGrid, this does not automatically update
        the display.

        @remarks
        Use wxPropertyGrid::ChangePropertyValue() instead if you need to run
        through validation process and send property change event.

        If you need to change property value in event, based on user input, use
        SetValueInEvent() instead.

        @param pList
            Pointer to list variant that contains child values. Used to
            indicate which children should be marked as modified.

        @param flags
            Various flags (for instance, wxPG_SETVAL_REFRESH_EDITOR, which is
            enabled by default).
    */
    void SetValue( wxVariant value, wxVariant* pList = NULL,
                   int flags = wxPG_SETVAL_REFRESH_EDITOR );

    /** Set wxBitmap in front of the value. This bitmap may be ignored
        by custom cell renderers.
    */
    void SetValueImage( wxBitmap& bmp );

    /** Sets selected choice and changes property value.

        Tries to retain value type, although currently if it is not string,
        then it is forced to integer.
    */
    void SetChoiceSelection( int newValue );

    void SetExpanded( bool expanded )
    {
        if ( !expanded ) m_flags |= wxPG_PROP_COLLAPSED;
        else m_flags &= ~wxPG_PROP_COLLAPSED;
    }

    /**
        Sets or clears given property flag. Mainly for internal use.

        @remarks Setting a property flag never has any side-effect, and is
                 intended almost exclusively for internal use. So, for
                 example, if you want to disable a property, call
                 Enable(false) instead of setting wxPG_PROP_DISABLED flag.

        @see HasFlag(), GetFlags()
    */
    void ChangeFlag( wxPGPropertyFlags flag, bool set )
    {
        if ( set )
            m_flags |= flag;
        else
            m_flags &= ~flag;
    }

    /**
        Sets or clears given property flag, recursively. This function is
        primarily intended for internal use.

        @see ChangeFlag()
    */
    void SetFlagRecursively( wxPGPropertyFlags flag, bool set );

    void SetHelpString( const wxString& helpString )
    {
        m_helpString = helpString;
    }

    void SetLabel( const wxString& label ) { m_label = label; }

    void SetName( const wxString& newName );

    /**
        Changes what sort of parent this property is for its children.

        @param flag
            Use one of the following values: wxPG_PROP_MISC_PARENT (for
            generic parents), wxPG_PROP_CATEGORY (for categories), or
            wxPG_PROP_AGGREGATE (for derived property classes with private
            children).

        @remarks You generally do not need to call this function.
    */
    void SetParentalType( int flag )
    {
        m_flags &= ~(wxPG_PROP_PROPERTY|wxPG_PROP_PARENTAL_FLAGS);
        m_flags |= flag;
    }

    void SetValueToUnspecified()
    {
        wxVariant val;  // Create NULL variant
        SetValue(val, NULL, wxPG_SETVAL_REFRESH_EDITOR);
    }

    // Helper function (for wxPython bindings and such) for settings protected
    // m_value.
    void SetValuePlain( wxVariant value )
    {
        m_value = value;
    }

#if wxUSE_VALIDATORS
    /** Sets wxValidator for a property*/
    void SetValidator( const wxValidator& validator )
    {
        m_validator = wxDynamicCast(validator.Clone(),wxValidator);
    }

    /** Gets assignable version of property's validator. */
    wxValidator* GetValidator() const
    {
        if ( m_validator )
            return m_validator;
        return DoGetValidator();
    }
#endif // wxUSE_VALIDATORS

    /** Returns client data (void*) of a property.
    */
    void* GetClientData() const
    {
        return m_clientData;
    }

    /** Sets client data (void*) of a property.
        @remarks
        This untyped client data has to be deleted manually.
    */
    void SetClientData( void* clientData )
    {
        m_clientData = clientData;
    }

    /** Returns client object of a property.
    */
    void SetClientObject(wxClientData* clientObject)
    {
        delete m_clientObject;
        m_clientObject = clientObject;
    }

    /** Sets managed client object of a property.
    */
    wxClientData *GetClientObject() const { return m_clientObject; }

    /**
        Sets new set of choices for the property.

        @remarks This operation deselects the property and clears its
                 value.
    */
    bool SetChoices( const wxPGChoices& choices );

    /** Set max length of text in text editor.
    */
    inline bool SetMaxLength( int maxLen );

    /** Call with 'false' in OnSetValue to cancel value changes after all
        (ie. cancel 'true' returned by StringToValue() or IntToValue()).
    */
    void SetWasModified( bool set = true )
    {
        if ( set ) m_flags |= wxPG_PROP_WAS_MODIFIED;
        else m_flags &= ~wxPG_PROP_WAS_MODIFIED;
    }

    const wxString& GetHelpString() const
    {
        return m_helpString;
    }

    // Use, for example, to detect if item is inside collapsed section.
    bool IsSomeParent( wxPGProperty* candidate_parent ) const;

    /**
        Adapts list variant into proper value using consecutive
        ChildChanged-calls.
    */
    void AdaptListToValue( wxVariant& list, wxVariant* value ) const;

#if wxPG_COMPATIBILITY_1_4
    /**
        Adds a private child property.

        @deprecated Use AddPrivateChild() instead.

        @see AddPrivateChild()
    */
    wxDEPRECATED( void AddChild( wxPGProperty* prop ) );
#endif

    /**
        Adds a private child property. If you use this instead of
        wxPropertyGridInterface::Insert() or
        wxPropertyGridInterface::AppendIn(), then property's parental
        type will automatically be set up to wxPG_PROP_AGGREGATE. In other
        words, all properties of this property will become private.
    */
    void AddPrivateChild( wxPGProperty* prop );

    /**
        Appends a new child property.
    */
    wxPGProperty* AppendChild( wxPGProperty* prop )
    {
        return InsertChild(-1, prop);
    }

    /** Returns height of children, recursively, and
        by taking expanded/collapsed status into account.

        iMax is used when finding property y-positions.
    */
    int GetChildrenHeight( int lh, int iMax = -1 ) const;

    /** Returns number of child properties */
    unsigned int GetChildCount() const
    {
        return (unsigned int) m_children.size();
    }

    /** Returns sub-property at index i. */
    wxPGProperty* Item( unsigned int i ) const
        { return m_children[i]; }

    /** Returns last sub-property.
    */
    wxPGProperty* Last() const { return m_children.back(); }

    /** Returns index of given child property. */
    int Index( const wxPGProperty* p ) const;

    // Puts correct indexes to children
    void FixIndicesOfChildren( unsigned int starthere = 0 );

    /**
        Converts image width into full image offset, with margins.
    */
    int GetImageOffset( int imageWidth ) const;

    // Returns wxPropertyGridPageState in which this property resides.
    wxPropertyGridPageState* GetParentState() const { return m_parentState; }

    wxPGProperty* GetItemAtY( unsigned int y,
                              unsigned int lh,
                              unsigned int* nextItemY ) const;

    /** Returns property at given virtual y coordinate.
    */
    wxPGProperty* GetItemAtY( unsigned int y ) const;

    /** Returns (direct) child property with given name (or NULL if not found).
    */
    wxPGProperty* GetPropertyByName( const wxString& name ) const;

    // Returns various display-related information for given column
    void GetDisplayInfo( unsigned int column,
                         int choiceIndex,
                         int flags,
                         wxString* pString,
                         const wxPGCell** pCell );

    static wxString*            sm_wxPG_LABEL;

    /** This member is public so scripting language bindings
        wrapper code can access it freely.
    */
    void*                       m_clientData;

protected:

    /**
        Sets property cell in fashion that reduces number of exclusive
        copies of cell data. Used when setting, for instance, same
        background colour for a number of properties.

        @param firstCol
            First column to affect.

        @param lastCol
            Last column to affect.

        @param preparedCell
            Pre-prepared cell that is used for those which cell data
            before this matched unmodCellData.

        @param srcData
            If unmodCellData did not match, valid cell data from this
            is merged into cell (usually generating new exclusive copy
            of cell's data).

        @param unmodCellData
            If cell's cell data matches this, its cell is now set to
            preparedCell.

        @param ignoreWithFlags
            Properties with any one of these flags are skipped.

        @param recursively
            If @true, apply this operation recursively in child properties.
    */
    void AdaptiveSetCell( unsigned int firstCol,
                          unsigned int lastCol,
                          const wxPGCell& preparedCell,
                          const wxPGCell& srcData,
                          wxPGCellData* unmodCellData,
                          FlagType ignoreWithFlags,
                          bool recursively );

    /**
        Makes sure m_cells has size of column+1 (or more).
    */
    void EnsureCells( unsigned int column );

    /** Returns (direct) child property with given name (or NULL if not found),
        with hint index.

        @param hintIndex
        Start looking for the child at this index.

        @remarks
        Does not support scope (ie. Parent.Child notation).
    */
    wxPGProperty* GetPropertyByNameWH( const wxString& name,
                                       unsigned int hintIndex ) const;

    /** This is used by Insert etc. */
    void DoAddChild( wxPGProperty* prop,
                     int index = -1,
                     bool correct_mode = true );

    void DoGenerateComposedValue( wxString& text,
                                  int argFlags = wxPG_VALUE_IS_CURRENT,
                                  const wxVariantList* valueOverrides = NULL,
                                  wxPGHashMapS2S* childResults = NULL ) const;

    bool DoHide( bool hide, int flags );

    void DoSetName(const wxString& str) { m_name = str; }

    /** Deletes all sub-properties. */
    void Empty();

    bool HasCell( unsigned int column ) const
    {
        if ( m_cells.size() > column )
            return true;
        return false;
    }

    void InitAfterAdded( wxPropertyGridPageState* pageState,
                         wxPropertyGrid* propgrid );

    /**
        Returns true if child property is selected.
    */
    bool IsChildSelected( bool recursive = false ) const;

    // Removes child property with given pointer. Does not delete it.
    void RemoveChild( wxPGProperty* p );

    void DoEnable( bool enable );

    void DoPreAddChild( int index, wxPGProperty* prop );

    void SetParentState( wxPropertyGridPageState* pstate )
        { m_parentState = pstate; }

    void SetFlag( wxPGPropertyFlags flag )
    {
        //
        // NB: While using wxPGPropertyFlags here makes it difficult to
        //     combine different flags, it usefully prevents user from
        //     using incorrect flags (say, wxWindow styles).
        m_flags |= flag;
    }

    void ClearFlag( FlagType flag ) { m_flags &= ~(flag); }

    // Called when the property is being removed from the grid and/or
    // page state (but *not* when it is also deleted).
    void OnDetached(wxPropertyGridPageState* state,
                    wxPropertyGrid* propgrid);

    // Call after fixed sub-properties added/removed after creation.
    // if oldSelInd >= 0 and < new max items, then selection is
    // moved to it.
    void SubPropsChanged( int oldSelInd = -1 );

    int GetY2( int lh ) const;

    wxString                    m_label;
    wxString                    m_name;
    wxPGProperty*               m_parent;
    wxPropertyGridPageState*    m_parentState;

    wxClientData*               m_clientObject;

    // Overrides editor returned by property class
    const wxPGEditor*           m_customEditor;
#if wxUSE_VALIDATORS
    // Editor is going to get this validator
    wxValidator*                m_validator;
#endif
    // Show this in front of the value
    //
    // TODO: Can bitmap be implemented with wxPGCell?
    wxBitmap*                   m_valueBitmap;

    wxVariant                   m_value;
    wxPGAttributeStorage        m_attributes;
    wxArrayPGProperty           m_children;

    // Extended cell information
    wxVector<wxPGCell>          m_cells;

    // Choices shown in drop-down list of editor control.
    wxPGChoices                 m_choices;

    // Help shown in statusbar or help box.
    wxString                    m_helpString;

    // Index in parent's property array.
    unsigned int                m_arrIndex;

    // If not -1, then overrides m_value
    int                         m_commonValue;

    FlagType                    m_flags;

    // Maximum length (mainly for string properties). Could be in some sort of
    // wxBaseStringProperty, but currently, for maximum flexibility and
    // compatibility, we'll stick it here. Anyway, we had 3 excess bytes to use
    // so short int will fit in just fine.
    short                       m_maxLen;

    // Root has 0, categories etc. at that level 1, etc.
    unsigned char               m_depth;

    // m_depthBgCol indicates width of background colour between margin and item
    // (essentially this is category's depth, if none then equals m_depth).
    unsigned char               m_depthBgCol;

private:
    // Called in constructors.
    void Init();
    void Init( const wxString& label, const wxString& name );
};

// -----------------------------------------------------------------------

//
// Property class declaration helper macros
// (wxPGRootPropertyClass and wxPropertyCategory require this).
//

#define WX_PG_DECLARE_DOGETEDITORCLASS \
    virtual const wxPGEditor* DoGetEditorClass() const;

#ifndef WX_PG_DECLARE_PROPERTY_CLASS
    #define WX_PG_DECLARE_PROPERTY_CLASS(CLASSNAME) \
        public: \
            DECLARE_DYNAMIC_CLASS(CLASSNAME) \
            WX_PG_DECLARE_DOGETEDITORCLASS \
        private:
#endif

// Implements sans constructor function. Also, first arg is class name, not
// property name.
#define WX_PG_IMPLEMENT_PROPERTY_CLASS_PLAIN(PROPNAME,T,EDITOR) \
const wxPGEditor* PROPNAME::DoGetEditorClass() const \
{ \
    return wxPGEditor_##EDITOR; \
}

// -----------------------------------------------------------------------

/** @class wxPGRootProperty
    @ingroup classes
    Root parent property.
*/
class WXDLLIMPEXP_PROPGRID wxPGRootProperty : public wxPGProperty
{
public:
    WX_PG_DECLARE_PROPERTY_CLASS(wxPGRootProperty)
public:

    /** Constructor. */
    wxPGRootProperty( const wxString& name = wxS("<Root>") );
    virtual ~wxPGRootProperty();

    virtual bool StringToValue( wxVariant&, const wxString&, int ) const
    {
        return false;
    }

protected:
};

// -----------------------------------------------------------------------

/** @class wxPropertyCategory
    @ingroup classes
    Category (caption) property.
*/
class WXDLLIMPEXP_PROPGRID wxPropertyCategory : public wxPGProperty
{
    friend class wxPropertyGrid;
    friend class wxPropertyGridPageState;
    WX_PG_DECLARE_PROPERTY_CLASS(wxPropertyCategory)
public:

    /** Default constructor is only used in special cases. */
    wxPropertyCategory();

    wxPropertyCategory( const wxString& label,
                        const wxString& name = wxPG_LABEL );
    ~wxPropertyCategory();

    int GetTextExtent( const wxWindow* wnd, const wxFont& font ) const;

    virtual wxString ValueToString( wxVariant& value, int argFlags ) const;
    virtual wxString GetValueAsString( int argFlags = 0 ) const;

protected:
    void SetTextColIndex( unsigned int colInd )
        { m_capFgColIndex = (wxByte) colInd; }
    unsigned int GetTextColIndex() const
        { return (unsigned int) m_capFgColIndex; }

    void CalculateTextExtent( wxWindow* wnd, const wxFont& font );

    int     m_textExtent;  // pre-calculated length of text
    wxByte  m_capFgColIndex;  // caption text colour index

private:
    void Init();
};

// -----------------------------------------------------------------------

#endif // wxUSE_PROPGRID

#endif // _WX_PROPGRID_PROPERTY_H_
