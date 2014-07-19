/////////////////////////////////////////////////////////////////////////////
// Name:        wx/propgrid/propgrid.h
// Purpose:     wxPropertyGrid
// Author:      Jaakko Salli
// Modified by:
// Created:     2004-09-25
// Copyright:   (c) Jaakko Salli
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PROPGRID_PROPGRID_H_
#define _WX_PROPGRID_PROPGRID_H_

#include "wx/defs.h"

#if wxUSE_PROPGRID

#include "wx/thread.h"
#include "wx/dcclient.h"
#include "wx/control.h"
#include "wx/scrolwin.h"
#include "wx/tooltip.h"
#include "wx/datetime.h"
#include "wx/recguard.h"

#include "wx/propgrid/property.h"
#include "wx/propgrid/propgridiface.h"


#ifndef SWIG
extern WXDLLIMPEXP_DATA_PROPGRID(const char) wxPropertyGridNameStr[];
#endif

class wxPGComboBox;

#if wxUSE_STATUSBAR
class WXDLLIMPEXP_FWD_CORE wxStatusBar;
#endif

// -----------------------------------------------------------------------
// Global variables
// -----------------------------------------------------------------------

// This is required for sharing common global variables.
class WXDLLIMPEXP_PROPGRID wxPGGlobalVarsClass
{
public:

    wxPGGlobalVarsClass();
    ~wxPGGlobalVarsClass();

#if wxUSE_THREADS
    // Critical section for handling the globals. Generally it is not needed
    // since GUI code is supposed to be in single thread. However,
    // we do want the user to be able to convey wxPropertyGridEvents to other
    // threads.
    wxCriticalSection   m_critSect;
#endif

    // Used by advprops, but here to make things easier.
    wxString            m_pDefaultImageWildcard;

    // Map of editor class instances (keys are name string).
    wxPGHashMapS2P      m_mapEditorClasses;

#if wxUSE_VALIDATORS
    wxVector<wxValidator*>  m_arrValidators; // These wxValidators need to be freed
#endif

    wxPGHashMapS2P      m_dictPropertyClassInfo; // PropertyName -> ClassInfo

    wxPGChoices*        m_fontFamilyChoices;

    // Replace with your own to affect all properties using default renderer.
    wxPGCellRenderer*   m_defaultRenderer;

    wxPGChoices         m_boolChoices;

    wxVariant           m_vEmptyString;
    wxVariant           m_vZero;
    wxVariant           m_vMinusOne;
    wxVariant           m_vTrue;
    wxVariant           m_vFalse;

    // Cached constant strings
    wxPGCachedString    m_strstring;
    wxPGCachedString    m_strlong;
    wxPGCachedString    m_strbool;
    wxPGCachedString    m_strlist;

    wxPGCachedString    m_strDefaultValue;
    wxPGCachedString    m_strMin;
    wxPGCachedString    m_strMax;
    wxPGCachedString    m_strUnits;
    wxPGCachedString    m_strHint;
#if wxPG_COMPATIBILITY_1_4
    wxPGCachedString    m_strInlineHelp;
#endif

    // If true then some things are automatically translated
    bool                m_autoGetTranslation;

    // > 0 if errors cannot or should not be shown in statusbar etc.
    int                 m_offline;

    int                 m_extraStyle;  // global extra style

    int                 m_warnings;

    int HasExtraStyle( int style ) const { return (m_extraStyle & style); }
};

extern WXDLLIMPEXP_DATA_PROPGRID(wxPGGlobalVarsClass*) wxPGGlobalVars;

#define wxPGVariant_EmptyString     (wxPGGlobalVars->m_vEmptyString)
#define wxPGVariant_Zero            (wxPGGlobalVars->m_vZero)
#define wxPGVariant_MinusOne        (wxPGGlobalVars->m_vMinusOne)
#define wxPGVariant_True            (wxPGGlobalVars->m_vTrue)
#define wxPGVariant_False           (wxPGGlobalVars->m_vFalse)

#define wxPGVariant_Bool(A)     (A?wxPGVariant_True:wxPGVariant_False)

// When wxPG is loaded dynamically after the application is already running
// then the built-in module system won't pick this one up.  Add it manually.
WXDLLIMPEXP_PROPGRID void wxPGInitResourceModule();

// -----------------------------------------------------------------------

/** @section propgrid_window_styles wxPropertyGrid Window Styles

    SetWindowStyleFlag method can be used to modify some of these at run-time.
    @{
*/
enum wxPG_WINDOW_STYLES
{

/** This will cause Sort() automatically after an item is added.
    When inserting a lot of items in this mode, it may make sense to
    use Freeze() before operations and Thaw() afterwards to increase
    performance.
*/
wxPG_AUTO_SORT                      = 0x00000010,

/** Categories are not initially shown (even if added).
    IMPORTANT NOTE: If you do not plan to use categories, then this
    style will waste resources.
    This flag can also be changed using wxPropertyGrid::EnableCategories method.
*/
wxPG_HIDE_CATEGORIES                = 0x00000020,

/* This style combines non-categoric mode and automatic sorting.
*/
wxPG_ALPHABETIC_MODE                = (wxPG_HIDE_CATEGORIES|wxPG_AUTO_SORT),

/** Modified values are shown in bold font. Changing this requires Refresh()
    to show changes.
*/
wxPG_BOLD_MODIFIED                  = 0x00000040,

/** Using this style, the column splitters move automatically based on column
    proportions (default is equal proportion for every column). This behaviour
    stops once the user manually moves a splitter, and returns when a
    splitter is double-clicked.

    @see wxPropertyGridInterface::SetColumnProportion().
*/
wxPG_SPLITTER_AUTO_CENTER           = 0x00000080,

/** Display tooltips for cell text that cannot be shown completely. If
    wxUSE_TOOLTIPS is 0, then this doesn't have any effect.
*/
wxPG_TOOLTIPS                       = 0x00000100,

/** Disables margin and hides all expand/collapse buttons that would appear
    outside the margin (for sub-properties). Toggling this style automatically
    expands all collapsed items.
*/
wxPG_HIDE_MARGIN                    = 0x00000200,

/** This style prevents user from moving the splitter.
*/
wxPG_STATIC_SPLITTER                = 0x00000400,

/** Combination of other styles that make it impossible for user to modify
    the layout.
*/
wxPG_STATIC_LAYOUT                  = (wxPG_HIDE_MARGIN|wxPG_STATIC_SPLITTER),

/** Disables wxTextCtrl based editors for properties which
    can be edited in another way.

    Equals calling wxPropertyGrid::LimitPropertyEditing for all added
    properties.
*/
wxPG_LIMITED_EDITING                = 0x00000800,

/** wxPropertyGridManager only: Show toolbar for mode and page selection. */
wxPG_TOOLBAR                        = 0x00001000,

/** wxPropertyGridManager only: Show adjustable text box showing description
    or help text, if available, for currently selected property.
*/
wxPG_DESCRIPTION                    = 0x00002000,

/** wxPropertyGridManager only: don't show an internal border around the
    property grid. Recommended if you use a header.
*/
wxPG_NO_INTERNAL_BORDER             = 0x00004000
};

#if wxPG_COMPATIBILITY_1_4
    // In wxPG 1.4 this was used to enable now-default theme border support
    // in wxPropertyGridManager.
    #define wxPG_THEME_BORDER           0x00000000
#endif


enum wxPG_EX_WINDOW_STYLES
{

/**
    NOTE: wxPG_EX_xxx are extra window styles and must be set using
    SetExtraStyle() member function.

    Speeds up switching to wxPG_HIDE_CATEGORIES mode. Initially, if
    wxPG_HIDE_CATEGORIES is not defined, the non-categorized data storage is
    not activated, and switching the mode first time becomes somewhat slower.
    wxPG_EX_INIT_NOCAT activates the non-categorized data storage right away.
    IMPORTANT NOTE: If you do plan not switching to non-categoric mode, or if
    you don't plan to use categories at all, then using this style will result
    in waste of resources.

*/
wxPG_EX_INIT_NOCAT                  = 0x00001000,

/** Extended window style that sets wxPropertyGridManager toolbar to not
    use flat style.
*/
wxPG_EX_NO_FLAT_TOOLBAR             = 0x00002000,

/** Shows alphabetic/categoric mode buttons from toolbar.
*/
wxPG_EX_MODE_BUTTONS                = 0x00008000,

/** Show property help strings as tool tips instead as text on the status bar.
    You can set the help strings using SetPropertyHelpString member function.
*/
wxPG_EX_HELP_AS_TOOLTIPS            = 0x00010000,

/** Prevent TAB from focusing to wxButtons. This behaviour was default
    in version 1.2.0 and earlier.
    NOTE! Tabbing to button doesn't work yet. Problem seems to be that on wxMSW
      atleast the button doesn't properly propagate key events (yes, I'm using
      wxWANTS_CHARS).
*/
//wxPG_EX_NO_TAB_TO_BUTTON            = 0x00020000,

/** Allows relying on native double-buffering.
*/
wxPG_EX_NATIVE_DOUBLE_BUFFERING         = 0x00080000,

/** Set this style to let user have ability to set values of properties to
    unspecified state. Same as setting wxPG_PROP_AUTO_UNSPECIFIED for
    all properties.
*/
wxPG_EX_AUTO_UNSPECIFIED_VALUES         = 0x00200000,

/**
    If this style is used, built-in attributes (such as wxPG_FLOAT_PRECISION
    and wxPG_STRING_PASSWORD) are not stored into property's attribute storage
    (thus they are not readable).

    Note that this option is global, and applies to all wxPG property
    containers.
*/
wxPG_EX_WRITEONLY_BUILTIN_ATTRIBUTES    = 0x00400000,

/** Hides page selection buttons from toolbar.
*/
wxPG_EX_HIDE_PAGE_BUTTONS               = 0x01000000,

/** Allows multiple properties to be selected by user (by pressing SHIFT
    when clicking on a property, or by dragging with left mouse button
    down).

    You can get array of selected properties with
    wxPropertyGridInterface::GetSelectedProperties(). In multiple selection
    mode wxPropertyGridInterface::GetSelection() returns
    property which has editor active (usually the first one
    selected). Other useful member functions are ClearSelection(),
    AddToSelection() and RemoveFromSelection().
*/
wxPG_EX_MULTIPLE_SELECTION              = 0x02000000,

/**
    This enables top-level window tracking which allows wxPropertyGrid to
    notify the application of last-minute property value changes by user.

    This style is not enabled by default because it may cause crashes when
    wxPropertyGrid is used in with wxAUI or similar system.

    @remarks If you are not in fact using any system that may change
             wxPropertyGrid's top-level parent window on its own, then you
             are recommended to enable this style.
*/
wxPG_EX_ENABLE_TLP_TRACKING             = 0x04000000,

/** Don't show divider above toolbar, on Windows.
*/
wxPG_EX_NO_TOOLBAR_DIVIDER              = 0x08000000,

/** Show a separator below the toolbar.
*/
wxPG_EX_TOOLBAR_SEPARATOR               = 0x10000000

};

#if wxPG_COMPATIBILITY_1_4
    #define wxPG_EX_DISABLE_TLP_TRACKING    0x00000000
#endif

/** Combines various styles.
*/
#define wxPG_DEFAULT_STYLE          (0)

/** Combines various styles.
*/
#define wxPGMAN_DEFAULT_STYLE       (0)

/** @}
*/

// -----------------------------------------------------------------------

//
// Ids for sub-controls
// NB: It should not matter what these are.
#define wxPG_SUBID1                     2
#define wxPG_SUBID2                     3
#define wxPG_SUBID_TEMP1                4

// -----------------------------------------------------------------------

/** @class wxPGCommonValue

    wxPropertyGrid stores information about common values in these
    records.

    NB: Common value feature is not complete, and thus not mentioned in
        documentation.
*/
class WXDLLIMPEXP_PROPGRID wxPGCommonValue
{
public:

    wxPGCommonValue( const wxString& label, wxPGCellRenderer* renderer )
    {
        m_label = label;
        m_renderer = renderer;
        renderer->IncRef();
    }
    virtual ~wxPGCommonValue()
    {
        m_renderer->DecRef();
    }

    virtual wxString GetEditableText() const { return m_label; }
    const wxString& GetLabel() const { return m_label; }
    wxPGCellRenderer* GetRenderer() const { return m_renderer; }

protected:
    wxString            m_label;
    wxPGCellRenderer*   m_renderer;
};

// -----------------------------------------------------------------------

/** @section propgrid_vfbflags wxPropertyGrid Validation Failure behaviour Flags
    @{
*/

enum wxPG_VALIDATION_FAILURE_BEHAVIOR_FLAGS
{

/** Prevents user from leaving property unless value is valid. If this
    behaviour flag is not used, then value change is instead cancelled.
*/
wxPG_VFB_STAY_IN_PROPERTY           = 0x01,

/** Calls wxBell() on validation failure.
*/
wxPG_VFB_BEEP                       = 0x02,

/** Cell with invalid value will be marked (with red colour).
*/
wxPG_VFB_MARK_CELL                  = 0x04,

/**
    Display a text message explaining the situation.

    To customize the way the message is displayed, you need to
    reimplement wxPropertyGrid::DoShowPropertyError() in a
    derived class. Default behaviour is to display the text on
    the top-level frame's status bar, if present, and otherwise
    using wxMessageBox.
*/
wxPG_VFB_SHOW_MESSAGE               = 0x08,

/**
    Similar to wxPG_VFB_SHOW_MESSAGE, except always displays the
    message using wxMessageBox.
*/
wxPG_VFB_SHOW_MESSAGEBOX            = 0x10,

/**
    Similar to wxPG_VFB_SHOW_MESSAGE, except always displays the
    message on the status bar (when present - you can reimplement
    wxPropertyGrid::GetStatusBar() in a derived class to specify
    this yourself).
*/
wxPG_VFB_SHOW_MESSAGE_ON_STATUSBAR  = 0x20,

/** Defaults. */
wxPG_VFB_DEFAULT                    = wxPG_VFB_MARK_CELL |
                                      wxPG_VFB_SHOW_MESSAGEBOX,

/** Only used internally. */
wxPG_VFB_UNDEFINED                  = 0x80

};

/** @}
*/

// Having this as define instead of wxByte typedef makes things easier for
// wxPython bindings (ignoring and redefining it in SWIG interface file
// seemed rather tricky)
#define wxPGVFBFlags unsigned char

/**
    wxPGValidationInfo

    Used to convey validation information to and from functions that
    actually perform validation. Mostly used in custom property
    classes.
*/
class WXDLLIMPEXP_PROPGRID wxPGValidationInfo
{
    friend class wxPropertyGrid;
public:
    wxPGValidationInfo()
    {
        m_failureBehavior = 0;
        m_isFailing = false;
    }

    ~wxPGValidationInfo()
    {
    }

    /**
        @return Returns failure behaviour which is a combination of
               @ref propgrid_vfbflags.
    */
    wxPGVFBFlags GetFailureBehavior() const
        { return m_failureBehavior; }

    /**
        Returns current failure message.
    */
    const wxString& GetFailureMessage() const
        { return m_failureMessage; }

    /**
        Returns reference to pending value.
    */
    wxVariant& GetValue()
    {
        wxASSERT(m_pValue);
        return *m_pValue;
    }

    /** Set validation failure behaviour

        @param failureBehavior
            Mixture of @ref propgrid_vfbflags.
    */
    void SetFailureBehavior(wxPGVFBFlags failureBehavior)
        { m_failureBehavior = failureBehavior; }

    /**
        Set current failure message.
    */
    void SetFailureMessage(const wxString& message)
        { m_failureMessage = message; }

private:
    /** Value to be validated.
    */
    wxVariant*      m_pValue;

    /** Message displayed on validation failure.
    */
    wxString        m_failureMessage;

    /** Validation failure behaviour. Use wxPG_VFB_XXX flags.
    */
    wxPGVFBFlags    m_failureBehavior;

    // True when validation is currently failing.
    bool            m_isFailing;
};

// -----------------------------------------------------------------------

/** @section propgrid_pgactions wxPropertyGrid Action Identifiers

    These are used with wxPropertyGrid::AddActionTrigger() and
    wxPropertyGrid::ClearActionTriggers().
    @{
*/

enum wxPG_KEYBOARD_ACTIONS
{
    wxPG_ACTION_INVALID = 0,

    /** Select the next property. */
    wxPG_ACTION_NEXT_PROPERTY,

    /** Select the previous property. */
    wxPG_ACTION_PREV_PROPERTY,

    /** Expand the selected property, if it has child items. */
    wxPG_ACTION_EXPAND_PROPERTY,

    /** Collapse the selected property, if it has child items. */
    wxPG_ACTION_COLLAPSE_PROPERTY,

    /** Cancel and undo any editing done in the currently active property
        editor.
    */
    wxPG_ACTION_CANCEL_EDIT,

    /** Move focus to the editor control of the currently selected
        property.
    */
    wxPG_ACTION_EDIT,

    /** Causes editor's button (if any) to be pressed. */
    wxPG_ACTION_PRESS_BUTTON,

    wxPG_ACTION_MAX
};

/** @}
*/

// -----------------------------------------------------------------------


// wxPropertyGrid::DoSelectProperty flags (selFlags)

// Focuses to created editor
#define wxPG_SEL_FOCUS                  0x0001
// Forces deletion and recreation of editor
#define wxPG_SEL_FORCE                  0x0002
// For example, doesn't cause EnsureVisible
#define wxPG_SEL_NONVISIBLE             0x0004
// Do not validate editor's value before selecting
#define wxPG_SEL_NOVALIDATE             0x0008
// Property being deselected is about to be deleted
#define wxPG_SEL_DELETING               0x0010
// Property's values was set to unspecified by the user
#define wxPG_SEL_SETUNSPEC              0x0020
// Property's event handler changed the value
#define wxPG_SEL_DIALOGVAL              0x0040
// Set to disable sending of wxEVT_PG_SELECTED event
#define wxPG_SEL_DONT_SEND_EVENT        0x0080
// Don't make any graphics updates
#define wxPG_SEL_NO_REFRESH             0x0100

// -----------------------------------------------------------------------

// DoSetSplitterPosition() flags

enum wxPG_SET_SPLITTER_POSITION_SPLITTER_FLAGS
{
    wxPG_SPLITTER_REFRESH           = 0x0001,
    wxPG_SPLITTER_ALL_PAGES         = 0x0002,
    wxPG_SPLITTER_FROM_EVENT        = 0x0004,
    wxPG_SPLITTER_FROM_AUTO_CENTER  = 0x0008
};


// -----------------------------------------------------------------------

// Internal flags
#define wxPG_FL_INITIALIZED                 0x0001
// Set when creating editor controls if it was clicked on.
#define wxPG_FL_ACTIVATION_BY_CLICK         0x0002
#define wxPG_FL_DONT_CENTER_SPLITTER        0x0004
#define wxPG_FL_FOCUSED                     0x0008
#define wxPG_FL_MOUSE_CAPTURED              0x0010
#define wxPG_FL_MOUSE_INSIDE                0x0020
#define wxPG_FL_VALUE_MODIFIED              0x0040
// don't clear background of m_wndEditor
#define wxPG_FL_PRIMARY_FILLS_ENTIRE        0x0080
// currently active editor uses custom image
#define wxPG_FL_CUR_USES_CUSTOM_IMAGE       0x0100
// cell colours override selection colours for selected cell
#define wxPG_FL_CELL_OVERRIDES_SEL          0x0200
#define wxPG_FL_SCROLLED                    0x0400
// set when all added/inserted properties get hideable flag
#define wxPG_FL_ADDING_HIDEABLES            0x0800
// Disables showing help strings on statusbar.
#define wxPG_FL_NOSTATUSBARHELP             0x1000
// Marks that we created the state, so we have to destroy it too.
#define wxPG_FL_CREATEDSTATE                0x2000
// Set if scrollbar's existence was detected in last onresize.
#define wxPG_FL_SCROLLBAR_DETECTED          0x4000
// Set if wxPGMan requires redrawing of description text box.
#define wxPG_FL_DESC_REFRESH_REQUIRED       0x8000
// Set if contained in wxPropertyGridManager
#define wxPG_FL_IN_MANAGER                  0x00020000
// Set after wxPropertyGrid is shown in its initial good size
#define wxPG_FL_GOOD_SIZE_SET               0x00040000
// Set when in SelectProperty.
#define wxPG_FL_IN_SELECT_PROPERTY          0x00100000
// Set when help string is shown in status bar
#define wxPG_FL_STRING_IN_STATUSBAR         0x00200000
// Auto sort is enabled (for categorized mode)
#define wxPG_FL_CATMODE_AUTO_SORT           0x01000000
// Set after page has been inserted to manager
#define wxPG_MAN_FL_PAGE_INSERTED           0x02000000
// Active editor control is abnormally large
#define wxPG_FL_ABNORMAL_EDITOR             0x04000000
// Recursion guard for HandleCustomEditorEvent
#define wxPG_FL_IN_HANDLECUSTOMEDITOREVENT  0x08000000
#define wxPG_FL_VALUE_CHANGE_IN_EVENT       0x10000000
// Editor control width should not change on resize
#define wxPG_FL_FIXED_WIDTH_EDITOR          0x20000000
// Width of panel can be different than width of grid
#define wxPG_FL_HAS_VIRTUAL_WIDTH           0x40000000
// Prevents RecalculateVirtualSize re-entrancy
#define wxPG_FL_RECALCULATING_VIRTUAL_SIZE  0x80000000

#if !defined(__wxPG_SOURCE_FILE__)
    // Reduce compile time, but still include in user app
    #include "wx/propgrid/props.h"
#endif

// -----------------------------------------------------------------------

/** @class wxPropertyGrid

    wxPropertyGrid is a specialized grid for editing properties
    such as strings, numbers, flagsets, fonts, and colours. wxPropertySheet
    used to do the very same thing, but it hasn't been updated for a while
    and it is currently deprecated.

    Please note that most member functions are inherited and as such not
    documented on this page. This means you will probably also want to read
    wxPropertyGridInterface class reference.

    See also @ref overview_propgrid.

    @section propgrid_window_styles_ Window Styles

    See @ref propgrid_window_styles.

    @section propgrid_event_handling Event Handling

    To process input from a propertygrid control, use these event handler
    macros to direct input to member functions that take a wxPropertyGridEvent
    argument.

    @beginEventTable{wxPropertyGridEvent}
    @event{EVT_PG_SELECTED (id, func)}
        Respond to wxEVT_PG_SELECTED event, generated when a property selection
        has been changed, either by user action or by indirect program
        function. For instance, collapsing a parent property programmatically
        causes any selected child property to become unselected, and may
        therefore cause this event to be generated.
    @event{EVT_PG_CHANGING(id, func)}
        Respond to wxEVT_PG_CHANGING event, generated when property value
        is about to be changed by user. Use wxPropertyGridEvent::GetValue()
        to take a peek at the pending value, and wxPropertyGridEvent::Veto()
        to prevent change from taking place, if necessary.
    @event{EVT_PG_HIGHLIGHTED(id, func)}
        Respond to wxEVT_PG_HIGHLIGHTED event, which occurs when mouse
        moves over a property. Event's property is NULL if hovered area does
        not belong to any property.
    @event{EVT_PG_RIGHT_CLICK(id, func)}
        Respond to wxEVT_PG_RIGHT_CLICK event, which occurs when property is
        clicked on with right mouse button.
    @event{EVT_PG_DOUBLE_CLICK(id, func)}
        Respond to wxEVT_PG_DOUBLE_CLICK event, which occurs when property is
        double-clicked onwith left mouse button.
    @event{EVT_PG_ITEM_COLLAPSED(id, func)}
        Respond to wxEVT_PG_ITEM_COLLAPSED event, generated when user collapses
        a property or category..
    @event{EVT_PG_ITEM_EXPANDED(id, func)}
        Respond to wxEVT_PG_ITEM_EXPANDED event, generated when user expands
        a property or category..
    @event{EVT_PG_LABEL_EDIT_BEGIN(id, func)}
        Respond to wxEVT_PG_LABEL_EDIT_BEGIN event, generated when is about to
        begin editing a property label. You can veto this event to prevent the
        action.
    @event{EVT_PG_LABEL_EDIT_ENDING(id, func)}
        Respond to wxEVT_PG_LABEL_EDIT_ENDING event, generated when is about to
        end editing of a property label. You can veto this event to prevent the
        action.
    @event{EVT_PG_COL_BEGIN_DRAG(id, func)}
        Respond to wxEVT_PG_COL_BEGIN_DRAG event, generated when user
        starts resizing a column - can be vetoed.
    @event{EVT_PG_COL_DRAGGING,(id, func)}
        Respond to wxEVT_PG_COL_DRAGGING, event, generated when a
        column resize by user is in progress. This event is also generated
        when user double-clicks the splitter in order to recenter
        it.
    @event{EVT_PG_COL_END_DRAG(id, func)}
        Respond to wxEVT_PG_COL_END_DRAG event, generated after column
        resize by user has finished.
    @endEventTable

    @remarks

    - Use Freeze() and Thaw() respectively to disable and enable drawing. This
      will also delay sorting etc. miscellaneous calculations to the last
      possible moment.

    @library{wxpropgrid}
    @category{propgrid}
*/
class WXDLLIMPEXP_PROPGRID wxPropertyGrid : public wxControl,
                                            public wxScrollHelper,
                                            public wxPropertyGridInterface
{
    friend class wxPropertyGridEvent;
    friend class wxPropertyGridPageState;
    friend class wxPropertyGridInterface;
    friend class wxPropertyGridManager;
    friend class wxPGHeaderCtrl;

    DECLARE_DYNAMIC_CLASS(wxPropertyGrid)
public:

#ifndef SWIG
    /**
        Two step constructor.

        Call Create when this constructor is called to build up the
        wxPropertyGrid
    */
    wxPropertyGrid();
#endif

    /** The default constructor. The styles to be used are styles valid for
        the wxWindow.

        @see @link wndflags Additional Window Styles @endlink
    */
    wxPropertyGrid( wxWindow *parent, wxWindowID id = wxID_ANY,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxDefaultSize,
                    long style = wxPG_DEFAULT_STYLE,
                    const wxString& name = wxPropertyGridNameStr );

    /** Destructor */
    virtual ~wxPropertyGrid();

    /** Adds given key combination to trigger given action.

        Here is a sample code to make Enter key press move focus to
        the next property.

        @code
            propGrid->AddActionTrigger(wxPG_ACTION_NEXT_PROPERTY,
                                       WXK_RETURN);
            propGrid->DedicateKey(WXK_RETURN);
        @endcode

        @param action
            Which action to trigger. See @ref propgrid_keyboard_actions.
        @param keycode
            Which keycode triggers the action.
        @param modifiers
            Which key event modifiers, in addition to keycode, are needed to
            trigger the action.
    */
    void AddActionTrigger( int action, int keycode, int modifiers = 0 );

    /**
        Dedicates a specific keycode to wxPropertyGrid. This means that such
        key presses will not be redirected to editor controls.

        Using this function allows, for example, navigation between
        properties using arrow keys even when the focus is in the editor
        control.
    */
    void DedicateKey( int keycode )
    {
        m_dedicatedKeys.push_back(keycode);
    }

    /**
        This static function enables or disables automatic use of
        wxGetTranslation for following strings: wxEnumProperty list labels,
        wxFlagsProperty sub-property labels.

        Default is false.
    */
    static void AutoGetTranslation( bool enable );

    /**
        Changes value of a property, as if from an editor.

        Use this instead of SetPropertyValue() if you need the value to run
        through validation process, and also send the property change event.

        @return
        Returns true if value was successfully changed.
    */
    bool ChangePropertyValue( wxPGPropArg id, wxVariant newValue );

    /**
        Centers the splitter.

        @param enableAutoResizing
            If @true, automatic column resizing is enabled (only applicapple
            if window style wxPG_SPLITTER_AUTO_CENTER is used).
    */
    void CenterSplitter( bool enableAutoResizing = false );

    /** Deletes all properties.
    */
    virtual void Clear();

    /** Clears action triggers for given action.
        @param action
            Which action to trigger. See @link pgactions List of list of
            wxPropertyGrid actions@endlink.
    */
    void ClearActionTriggers( int action );

    /** Forces updating the value of property from the editor control.

        Note that wxEVT_PG_CHANGING and wxEVT_PG_CHANGED are dispatched using
        ProcessEvent, meaning your event handlers will be called immediately.

        @return
        Returns true if anything was changed.
    */
    virtual bool CommitChangesFromEditor( wxUint32 flags = 0 );

    /**
        Two step creation.

        Whenever the control is created without any parameters, use Create to
        actually create it. Don't access the control's public methods before
        this is called @see @link wndflags Additional Window Styles@endlink
    */
    bool Create( wxWindow *parent, wxWindowID id = wxID_ANY,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = wxPG_DEFAULT_STYLE,
                 const wxString& name = wxPropertyGridNameStr );

    /**
        Call when editor widget's contents is modified.

        For example, this is called when changes text in wxTextCtrl (used in
        wxStringProperty and wxIntProperty).

        @remarks
        This function should only be called by custom properties.

        @see wxPGProperty::OnEvent()
    */
    void EditorsValueWasModified() { m_iFlags |= wxPG_FL_VALUE_MODIFIED; }

    /** Reverse of EditorsValueWasModified().

        @remarks
        This function should only be called by custom properties.
    */
    void EditorsValueWasNotModified()
    {
        m_iFlags &= ~(wxPG_FL_VALUE_MODIFIED);
    }

    /**
        Enables or disables (shows/hides) categories according to parameter
        enable.
     */
    bool EnableCategories( bool enable );

    /** Scrolls and/or expands items to ensure that the given item is visible.
        Returns true if something was actually done.
    */
    bool EnsureVisible( wxPGPropArg id );

    /**
        Reduces column sizes to minimum possible that contents are still
        visibly (naturally some margin space will be applied as well).

        @return
        Minimum size for the grid to still display everything.

        @remarks
        Does not work well with wxPG_SPLITTER_AUTO_CENTER window style.

        This function only works properly if grid size prior to call was already
        fairly large.

        Note that you can also get calculated column widths by calling
        GetState->GetColumnWidth() immediately after this function returns.
    */
    wxSize FitColumns()
    {
        wxSize sz = m_pState->DoFitColumns();
        return sz;
    }

    /**
        Returns wxWindow that the properties are painted on, and which should
        be used as the parent for editor controls.
    */
    wxWindow* GetPanel()
    {
        return this;
    }

    /** Returns current category caption background colour. */
    wxColour GetCaptionBackgroundColour() const { return m_colCapBack; }

    wxFont& GetCaptionFont() { return m_captionFont; }

    const wxFont& GetCaptionFont() const { return m_captionFont; }

    /** Returns current category caption text colour. */
    wxColour GetCaptionForegroundColour() const { return m_colCapFore; }

    /** Returns current cell background colour. */
    wxColour GetCellBackgroundColour() const { return m_colPropBack; }

    /** Returns current cell text colour when disabled. */
    wxColour GetCellDisabledTextColour() const { return m_colDisPropFore; }

    /** Returns current cell text colour. */
    wxColour GetCellTextColour() const { return m_colPropFore; }

    /**
        Returns number of columns currently on grid.
    */
    unsigned int GetColumnCount() const
    {
        return (unsigned int) m_pState->m_colWidths.size();
    }

    /** Returns colour of empty space below properties. */
    wxColour GetEmptySpaceColour() const { return m_colEmptySpace; }

    /** Returns height of highest characters of used font. */
    int GetFontHeight() const { return m_fontHeight; }

    /** Returns pointer to itself. Dummy function that enables same kind
        of code to use wxPropertyGrid and wxPropertyGridManager.
    */
    wxPropertyGrid* GetGrid() { return this; }

    /** Returns rectangle of custom paint image.
    */
    wxRect GetImageRect( wxPGProperty* p, int item ) const;

    /** Returns size of the custom paint image in front of property.
        If no argument is given, returns preferred size.
    */
    wxSize GetImageSize( wxPGProperty* p = NULL, int item = -1 ) const;

    //@{
    /** Returns last item which could be iterated using given flags.
        @param flags
        See @ref propgrid_iterator_flags.
    */
    wxPGProperty* GetLastItem( int flags = wxPG_ITERATE_DEFAULT )
    {
        return m_pState->GetLastItem(flags);
    }

    const wxPGProperty* GetLastItem( int flags = wxPG_ITERATE_DEFAULT ) const
    {
        return m_pState->GetLastItem(flags);
    }
    //@}

    /** Returns colour of lines between cells. */
    wxColour GetLineColour() const { return m_colLine; }

    /** Returns background colour of margin. */
    wxColour GetMarginColour() const { return m_colMargin; }

    /** Returns margin width. */
    int GetMarginWidth() const { return m_marginWidth; }

    /**
        Returns most up-to-date value of selected property. This will return
        value different from GetSelectedProperty()->GetValue() only when text
        editor is activate and string edited by user represents valid,
        uncommitted property value.
    */
    wxVariant GetUncommittedPropertyValue();

    /** Returns "root property". It does not have name, etc. and it is not
        visible. It is only useful for accessing its children.
    */
    wxPGProperty* GetRoot() const { return m_pState->m_properties; }

    /** Returns height of a single grid row (in pixels). */
    int GetRowHeight() const { return m_lineHeight; }

    /** Returns currently selected property. */
    wxPGProperty* GetSelectedProperty() const { return GetSelection(); }

    /** Returns current selection background colour. */
    wxColour GetSelectionBackgroundColour() const { return m_colSelBack; }

    /** Returns current selection text colour. */
    wxColour GetSelectionForegroundColour() const { return m_colSelFore; }

    /**
        Returns current splitter x position.
    */
    int GetSplitterPosition( unsigned int splitterIndex = 0 ) const
    {
        return m_pState->DoGetSplitterPosition(splitterIndex);
    }

    /** Returns wxTextCtrl active in currently selected property, if any. Takes
        into account wxOwnerDrawnComboBox.
    */
    wxTextCtrl* GetEditorTextCtrl() const;

    wxPGValidationInfo& GetValidationInfo()
    {
        return m_validationInfo;
    }

    /** Returns current vertical spacing. */
    int GetVerticalSpacing() const { return (int)m_vspacing; }

    /**
        Returns @true if a property editor control has focus.
    */
    bool IsEditorFocused() const;

    /** Returns true if editor's value was marked modified.
    */
    bool IsEditorsValueModified() const
        { return  ( m_iFlags & wxPG_FL_VALUE_MODIFIED ) ? true : false; }

    /**
        Returns information about arbitrary position in the grid.

        @param pt
            Coordinates in the virtual grid space. You may need to use
            wxScrolled<T>::CalcScrolledPosition() for translating
            wxPropertyGrid client coordinates into something this member
            function can use.
    */
    wxPropertyGridHitTestResult HitTest( const wxPoint& pt ) const;

    /** Returns true if any property has been modified by the user. */
    bool IsAnyModified() const { return (m_pState->m_anyModified>0); }

    /**
        Returns true if updating is frozen (ie Freeze() called but not yet
        Thaw() ).
     */
    bool IsFrozen() const { return (m_frozen>0)?true:false; }

    /**
        It is recommended that you call this function any time your code causes
        wxPropertyGrid's top-level parent to change. wxPropertyGrid's OnIdle()
        handler should be able to detect most changes, but it is not perfect.

        @param newTLP
            New top-level parent that is about to be set. Old top-level parent
            window should still exist as the current one.

        @remarks This function is automatically called from wxPropertyGrid::
                 Reparent() and wxPropertyGridManager::Reparent(). You only
                 need to use it if you reparent wxPropertyGrid indirectly.
    */
    void OnTLPChanging( wxWindow* newTLP );

    /** Redraws given property.
    */
    virtual void RefreshProperty( wxPGProperty* p );

    /** Registers a new editor class.
        @return
        Pointer to the editor class instance that should be used.
    */
    static wxPGEditor* RegisterEditorClass( wxPGEditor* editor,
                                            bool noDefCheck = false )
    {
        return DoRegisterEditorClass(editor, wxEmptyString, noDefCheck);
    }

    static wxPGEditor* DoRegisterEditorClass( wxPGEditor* editorClass,
                                              const wxString& editorName,
                                              bool noDefCheck = false );

    /** Resets all colours to the original system values.
    */
    void ResetColours();

    /**
        Resets column sizes and splitter positions, based on proportions.

        @param enableAutoResizing
            If @true, automatic column resizing is enabled (only applicapple
            if window style wxPG_SPLITTER_AUTO_CENTER is used).

        @see wxPropertyGridInterface::SetColumnProportion()
    */
    void ResetColumnSizes( bool enableAutoResizing = false );

    /**
        Selects a property.
        Editor widget is automatically created, but not focused unless focus is
        true.

        @param id
            Property to select.

        @return
            True if selection finished successfully. Usually only fails if
            current value in editor is not valid.

        @remarks In wxPropertyGrid 1.4, this member function used to generate
                 wxEVT_PG_SELECTED. In wxWidgets 2.9 and later, it no longer
                 does that.

        @remarks This clears any previous selection.
    */
    bool SelectProperty( wxPGPropArg id, bool focus = false );

    /**
        Set entire new selection from given list of properties.
    */
    void SetSelection( const wxArrayPGProperty& newSelection )
    {
        DoSetSelection( newSelection, wxPG_SEL_DONT_SEND_EVENT );
    }

    /**
        Adds given property into selection. If wxPG_EX_MULTIPLE_SELECTION
        extra style is not used, then this has same effect as
        calling SelectProperty().

        @remarks Multiple selection is not supported for categories. This
                 means that if you have properties selected, you cannot
                 add category to selection, and also if you have category
                 selected, you cannot add other properties to selection.
                 This member function will fail silently in these cases,
                 even returning true.
    */
    bool AddToSelection( wxPGPropArg id )
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(false)
        return DoAddToSelection(p, wxPG_SEL_DONT_SEND_EVENT);
    }

    /**
        Removes given property from selection. If property is not selected,
        an assertion failure will occur.
    */
    bool RemoveFromSelection( wxPGPropArg id )
    {
        wxPG_PROP_ARG_CALL_PROLOG_RETVAL(false)
        return DoRemoveFromSelection(p, wxPG_SEL_DONT_SEND_EVENT);
    }

    /**
        Makes given column editable by user.

        @param editable
            Using @false here will disable column from being editable.
    */
    void MakeColumnEditable( unsigned int column, bool editable = true );

    /**
        Creates label editor wxTextCtrl for given column, for property
        that is currently selected. When multiple selection is
        enabled, this applies to whatever property GetSelection()
        returns.

        @param colIndex
            Which column's label to edit. Note that you should not
            use value 1, which is reserved for property value
            column.

        @see EndLabelEdit(), MakeColumnEditable()
    */
    void BeginLabelEdit( unsigned int column = 0 )
    {
        DoBeginLabelEdit(column, wxPG_SEL_DONT_SEND_EVENT);
    }

    /**
        Destroys label editor wxTextCtrl, if any.

        @param commit
            Use @true (default) to store edited label text in
            property cell data.

        @see BeginLabelEdit(), MakeColumnEditable()
    */
    void EndLabelEdit( bool commit = true )
    {
        DoEndLabelEdit(commit, wxPG_SEL_DONT_SEND_EVENT);
    }

    /**
        Returns currently active label editor, NULL if none.
    */
    wxTextCtrl* GetLabelEditor() const
    {
        return m_labelEditor;
    }

    /** Sets category caption background colour. */
    void SetCaptionBackgroundColour(const wxColour& col);

    /** Sets category caption text colour. */
    void SetCaptionTextColour(const wxColour& col);

    /** Sets default cell background colour - applies to property cells.
        Note that appearance of editor widgets may not be affected.
    */
    void SetCellBackgroundColour(const wxColour& col);

    /** Sets cell text colour for disabled properties.
    */
    void SetCellDisabledTextColour(const wxColour& col);

    /** Sets default cell text colour - applies to property name and value text.
        Note that appearance of editor widgets may not be affected.
    */
    void SetCellTextColour(const wxColour& col);

    /** Set number of columns (2 or more).
    */
    void SetColumnCount( int colCount )
    {
        m_pState->SetColumnCount(colCount);
        Refresh();
    }

    /**
        Sets the 'current' category - Append will add non-category properties
        under it.
    */
    void SetCurrentCategory( wxPGPropArg id )
    {
        wxPG_PROP_ARG_CALL_PROLOG()
        wxPropertyCategory* pc = wxDynamicCast(p, wxPropertyCategory);
        wxASSERT(pc);
        m_pState->m_currentCategory = pc;
    }

    /** Sets colour of empty space below properties. */
    void SetEmptySpaceColour(const wxColour& col);

    /** Sets colour of lines between cells. */
    void SetLineColour(const wxColour& col);

    /** Sets background colour of margin. */
    void SetMarginColour(const wxColour& col);

    /**
        Sets selection background colour - applies to selected property name
        background.
    */
    void SetSelectionBackgroundColour(const wxColour& col);

    /**
        Sets selection foreground colour - applies to selected property name
        text.
     */
    void SetSelectionTextColour(const wxColour& col);

    /** Sets x coordinate of the splitter.
        @remarks
        Splitter position cannot exceed grid size, and therefore setting it
        during form creation may fail as initial grid size is often smaller
        than desired splitter position, especially when sizers are being used.
    */
    void SetSplitterPosition( int newXPos, int col = 0 )
    {
        DoSetSplitterPosition(newXPos, col, wxPG_SPLITTER_REFRESH);
    }

    /**
        Sets the property sorting function.

        @param sortFunction
            The sorting function to be used. It should return a value greater
            than 0 if position of p1 is after p2. So, for instance, when
            comparing property names, you can use following implementation:

            @code
                int MyPropertySortFunction(wxPropertyGrid* propGrid,
                                           wxPGProperty* p1,
                                           wxPGProperty* p2)
                {
                    return p1->GetBaseName().compare( p2->GetBaseName() );
                }
            @endcode

        @remarks
            Default property sort function sorts properties by their labels
            (case-insensitively).

        @see GetSortFunction, wxPropertyGridInterface::Sort,
             wxPropertyGridInterface::SortChildren
    */
    void SetSortFunction( wxPGSortCallback sortFunction )
    {
        m_sortFunction = sortFunction;
    }

    /**
        Returns the property sort function (default is @NULL).

        @see SetSortFunction
    */
    wxPGSortCallback GetSortFunction() const
    {
        return m_sortFunction;
    }

    /**
        Sets appearance of value cells representing an unspecified property
        value. Default appearance is blank.

        @remarks If you set the unspecified value to have any
                 textual representation, then that will override
                 "InlineHelp" attribute.

        @see wxPGProperty::SetValueToUnspecified(),
             wxPGProperty::IsValueUnspecified()
    */
    void SetUnspecifiedValueAppearance( const wxPGCell& cell )
    {
        m_unspecifiedAppearance = m_propertyDefaultCell;
        m_unspecifiedAppearance.MergeFrom(cell);
    }

    /**
        Returns current appearance of unspecified value cells.

        @see SetUnspecifiedValueAppearance()
    */
    const wxPGCell& GetUnspecifiedValueAppearance() const
    {
        return m_unspecifiedAppearance;
    }

    /**
        Returns (visual) text representation of the unspecified
        property value.

        @param argFlags For internal use only.
    */
    wxString GetUnspecifiedValueText( int argFlags = 0 ) const;

    /** Set virtual width for this particular page. Width -1 indicates that the
        virtual width should be disabled. */
    void SetVirtualWidth( int width );

    /**
        Moves splitter as left as possible, while still allowing all
        labels to be shown in full.

        @param privateChildrenToo
            If @false, will still allow private children to be cropped.
    */
    void SetSplitterLeft( bool privateChildrenToo = false )
    {
        m_pState->SetSplitterLeft(privateChildrenToo);
    }

    /** Sets vertical spacing. Can be 1, 2, or 3 - a value relative to font
        height. Value of 2 should be default on most platforms.
    */
    void SetVerticalSpacing( int vspacing )
    {
        m_vspacing = (unsigned char)vspacing;
        CalculateFontAndBitmapStuff( vspacing );
        if ( !m_pState->m_itemsAdded ) Refresh();
    }

    /** Shows an brief error message that is related to a property. */
    void ShowPropertyError( wxPGPropArg id, const wxString& msg )
    {
        wxPG_PROP_ARG_CALL_PROLOG()
        DoShowPropertyError(p, msg);
    }

    /////////////////////////////////////////////////////////////////
    //
    // Following methods do not need to be (currently) documented
    //
    /////////////////////////////////////////////////////////////////

    bool HasVirtualWidth() const
        { return (m_iFlags & wxPG_FL_HAS_VIRTUAL_WIDTH) ? true : false; }

    const wxPGCommonValue* GetCommonValue( unsigned int i ) const
    {
        return (wxPGCommonValue*) m_commonValues[i];
    }

    /** Returns number of common values.
    */
    unsigned int GetCommonValueCount() const
    {
        return (unsigned int) m_commonValues.size();
    }

    /** Returns label of given common value.
    */
    wxString GetCommonValueLabel( unsigned int i ) const
    {
        wxASSERT( GetCommonValue(i) );
        return GetCommonValue(i)->GetLabel();
    }

    /**
        Returns index of common value that will truly change value to
        unspecified.
    */
    int GetUnspecifiedCommonValue() const { return m_cvUnspecified; }

    /** Set index of common value that will truly change value to unspecified.
        Using -1 will set none to have such effect.
        Default is 0.
    */
    void SetUnspecifiedCommonValue( int index ) { m_cvUnspecified = index; }

    /**
        Shortcut for creating dialog-caller button. Used, for example, by
        wxFontProperty.
        @remarks
        This should only be called by properties.
    */
    wxWindow* GenerateEditorButton( const wxPoint& pos, const wxSize& sz );

    /** Fixes position of wxTextCtrl-like control (wxSpinCtrl usually
        fits as one). Call after control has been created (but before
        shown).
    */
    void FixPosForTextCtrl( wxWindow* ctrl,
                            unsigned int forColumn = 1,
                            const wxPoint& offset = wxPoint(0, 0) );

    /** Shortcut for creating text editor widget.
        @param pos
            Same as pos given for CreateEditor.
        @param sz
            Same as sz given for CreateEditor.
        @param value
            Initial text for wxTextCtrl.
        @param secondary
            If right-side control, such as button, also created, then create it
            first and pass it as this parameter.
        @param extraStyle
            Extra style flags to pass for wxTextCtrl.
        @remarks
        Note that this should generally be called only by new classes derived
        from wxPGProperty.
    */
    wxWindow* GenerateEditorTextCtrl( const wxPoint& pos,
                                      const wxSize& sz,
                                      const wxString& value,
                                      wxWindow* secondary,
                                      int extraStyle = 0,
                                      int maxLen = 0,
                                      unsigned int forColumn = 1 );

    /* Generates both textctrl and button.
    */
    wxWindow* GenerateEditorTextCtrlAndButton( const wxPoint& pos,
        const wxSize& sz, wxWindow** psecondary, int limited_editing,
        wxPGProperty* property );

    /** Generates position for a widget editor dialog box.
        @param p
        Property for which dialog is positioned.
        @param sz
        Known or over-approximated size of the dialog.
        @return
        Position for dialog.
    */
    wxPoint GetGoodEditorDialogPosition( wxPGProperty* p,
                                         const wxSize& sz );

    // Converts escape sequences in src_str to newlines,
    // tabs, etc. and copies result to dst_str.
    static wxString& ExpandEscapeSequences( wxString& dst_str,
                                            wxString& src_str );

    // Converts newlines, tabs, etc. in src_str to escape
    // sequences, and copies result to dst_str.
    static wxString& CreateEscapeSequences( wxString& dst_str,
                                            wxString& src_str );

    /**
        Returns rectangle that fully contains properties between and including
        p1 and p2. Rectangle is in virtual scrolled window coordinates.
    */
    wxRect GetPropertyRect( const wxPGProperty* p1,
                            const wxPGProperty* p2 ) const;

    /** Returns pointer to current active primary editor control (NULL if none).
    */
    wxWindow* GetEditorControl() const;

    wxWindow* GetPrimaryEditor() const
    {
        return GetEditorControl();
    }

    /**
        Returns pointer to current active secondary editor control (NULL if
        none).
    */
    wxWindow* GetEditorControlSecondary() const
    {
        return m_wndEditor2;
    }

    /**
        Refreshes any active editor control.
    */
    void RefreshEditor();

    // Events from editor controls are forward to this function
    bool HandleCustomEditorEvent( wxEvent &event );

    // Mostly useful for page switching.
    void SwitchState( wxPropertyGridPageState* pNewState );

    long GetInternalFlags() const { return m_iFlags; }
    bool HasInternalFlag( long flag ) const
        { return (m_iFlags & flag) ? true : false; }
    void SetInternalFlag( long flag ) { m_iFlags |= flag; }
    void ClearInternalFlag( long flag ) { m_iFlags &= ~(flag); }
    void IncFrozen() { m_frozen++; }
    void DecFrozen() { m_frozen--; }

    void OnComboItemPaint( const wxPGComboBox* pCb,
                           int item,
                           wxDC* pDc,
                           wxRect& rect,
                           int flags );

    /** Standardized double-to-string conversion.
    */
    static const wxString& DoubleToString( wxString& target,
                                           double value,
                                           int precision,
                                           bool removeZeroes,
                                           wxString* precTemplate = NULL );

    /**
        Call this from wxPGProperty::OnEvent() to cause property value to be
        changed after the function returns (with true as return value).
        ValueChangeInEvent() must be used if you wish the application to be
        able to use wxEVT_PG_CHANGING to potentially veto the given value.
    */
    void ValueChangeInEvent( wxVariant variant )
    {
        m_changeInEventValue = variant;
        m_iFlags |= wxPG_FL_VALUE_CHANGE_IN_EVENT;
    }

    /**
        You can use this member function, for instance, to detect in
        wxPGProperty::OnEvent() if wxPGProperty::SetValueInEvent() was
        already called in wxPGEditor::OnEvent(). It really only detects
        if was value was changed using wxPGProperty::SetValueInEvent(), which
        is usually used when a 'picker' dialog is displayed. If value was
        written by "normal means" in wxPGProperty::StringToValue() or
        IntToValue(), then this function will return false (on the other hand,
        wxPGProperty::OnEvent() is not even called in those cases).
    */
    bool WasValueChangedInEvent() const
    {
        return (m_iFlags & wxPG_FL_VALUE_CHANGE_IN_EVENT) ? true : false;
    }

    /** Returns true if given event is from first of an array of buttons
        (as can be in case when wxPGMultiButton is used).
    */
    bool IsMainButtonEvent( const wxEvent& event )
    {
        return (event.GetEventType() == wxEVT_BUTTON)
                    && (m_wndSecId == event.GetId());
    }

    /** Pending value is expected to be passed in PerformValidation().
    */
    virtual bool DoPropertyChanged( wxPGProperty* p,
                                    unsigned int selFlags = 0 );

    /** Called when validation for given property fails.
        @param invalidValue
            Value which failed in validation.
        @return
            Return true if user is allowed to change to another property even
            if current has invalid value.
        @remarks
        To add your own validation failure behaviour, override
        wxPropertyGrid::DoOnValidationFailure().
    */
    bool OnValidationFailure( wxPGProperty* property,
                              wxVariant& invalidValue );

    /** Called to indicate property and editor has valid value now.
    */
    void OnValidationFailureReset( wxPGProperty* property )
    {
        if ( property && property->HasFlag(wxPG_PROP_INVALID_VALUE) )
        {
            DoOnValidationFailureReset(property);
            property->ClearFlag(wxPG_PROP_INVALID_VALUE);
        }
        m_validationInfo.m_failureMessage.clear();
    }

    /**
        Override in derived class to display error messages in custom manner
        (these message usually only result from validation failure).

        @remarks If you implement this, then you also need to implement
                 DoHidePropertyError() - possibly to do nothing, if error
                 does not need hiding (e.g. it was logged or shown in a
                 message box).

        @see DoHidePropertyError()
    */
    virtual void DoShowPropertyError( wxPGProperty* property,
                                      const wxString& msg );

    /**
        Override in derived class to hide an error displayed by
        DoShowPropertyError().

        @see DoShowPropertyError()
    */
    virtual void DoHidePropertyError( wxPGProperty* property );

#if wxUSE_STATUSBAR
    /**
        Return wxStatusBar that is used by this wxPropertyGrid. You can
        reimplement this member function in derived class to override
        the default behaviour of using the top-level wxFrame's status
        bar, if any.
    */
    virtual wxStatusBar* GetStatusBar();
#endif

    /** Override to customize property validation failure behaviour.
        @param invalidValue
            Value which failed in validation.
        @return
            Return true if user is allowed to change to another property even
            if current has invalid value.
    */
    virtual bool DoOnValidationFailure( wxPGProperty* property,
                                        wxVariant& invalidValue );

    /** Override to customize resetting of property validation failure status.
        @remarks
        Property is guaranteed to have flag wxPG_PROP_INVALID_VALUE set.
    */
    virtual void DoOnValidationFailureReset( wxPGProperty* property );

    int GetSpacingY() const { return m_spacingy; }

    /**
        Must be called in wxPGEditor::CreateControls() if primary editor window
        is wxTextCtrl, just before textctrl is created.
        @param text
            Initial text value of created wxTextCtrl.
    */
    void SetupTextCtrlValue( const wxString text ) { m_prevTcValue = text; }

    /**
        Unfocuses or closes editor if one was open, but does not deselect
        property.
    */
    bool UnfocusEditor();

    virtual void SetWindowStyleFlag( long style );

    void DrawItems( const wxPGProperty* p1, const wxPGProperty* p2 );

    void DrawItem( wxPGProperty* p )
    {
        DrawItems(p,p);
    }

    virtual void DrawItemAndChildren( wxPGProperty* p );

    /**
        Draws item, children, and consequtive parents as long as category is
        not met.
     */
    void DrawItemAndValueRelated( wxPGProperty* p );

protected:

    /**
        wxPropertyGridPageState used by the grid is created here.

        If grid is used in wxPropertyGridManager, there is no point overriding
        this - instead, set custom wxPropertyGridPage classes.
    */
    virtual wxPropertyGridPageState* CreateState() const;

    enum PerformValidationFlags
    {
        SendEvtChanging         = 0x0001,
        IsStandaloneValidation  = 0x0002   // Not called in response to event
    };

    /**
        Runs all validation functionality (includes sending wxEVT_PG_CHANGING).
        Returns true if all tests passed. Implement in derived class to
        add additional validation behaviour.
    */
    virtual bool PerformValidation( wxPGProperty* p,
                                    wxVariant& pendingValue,
                                    int flags = SendEvtChanging );

public:

    // Control font changer helper.
    void SetCurControlBoldFont();

    wxPGCell& GetPropertyDefaultCell()
    {
        return m_propertyDefaultCell;
    }

    wxPGCell& GetCategoryDefaultCell()
    {
        return m_categoryDefaultCell;
    }

    //
    // Public methods for semi-public use
    //
    bool DoSelectProperty( wxPGProperty* p, unsigned int flags = 0 );

    // Overridden functions.
    virtual bool Destroy();
    // Returns property at given y coordinate (relative to grid's top left).
    wxPGProperty* GetItemAtY( int y ) const { return DoGetItemAtY(y); }

    virtual void Refresh( bool eraseBackground = true,
                          const wxRect *rect = (const wxRect *) NULL );
    virtual bool SetFont( const wxFont& font );
    virtual void Freeze();
    virtual void SetExtraStyle( long exStyle );
    virtual void Thaw();
    virtual bool Reparent( wxWindowBase *newParent );

protected:
    virtual wxSize DoGetBestSize() const;

#ifndef wxPG_ICON_WIDTH
    wxBitmap            *m_expandbmp, *m_collbmp;
#endif

    wxCursor            *m_cursorSizeWE;

    /** wxWindow pointers to editor control(s). */
    wxWindow            *m_wndEditor;
    wxWindow            *m_wndEditor2;

    wxBitmap            *m_doubleBuffer;

    /** Local time ms when control was created. */
    wxLongLong          m_timeCreated;

    /** wxPGProperty::OnEvent can change value by setting this. */
    wxVariant           m_changeInEventValue;

    /** Id of m_wndEditor2, or its first child, if any. */
    int                 m_wndSecId;

    /** Extra Y spacing between the items. */
    int                 m_spacingy;

    /** Control client area width; updated on resize. */
    int                 m_width;

    /** Control client area height; updated on resize. */
    int                 m_height;

    /** Current non-client width (needed when auto-centering). */
    int                 m_ncWidth;

    /** Non-client width (auto-centering helper). */
    //int                 m_fWidth;

    /** Previously recorded scroll start position. */
    int                 m_prevVY;

    /**
        The gutter spacing in front and back of the image.
        This determines the amount of spacing in front of each item
     */
    int                 m_gutterWidth;

    /** Includes separator line. */
    int                 m_lineHeight;

    /** Gutter*2 + image width. */
    int                 m_marginWidth;

    // y spacing for expand/collapse button.
    int                 m_buttonSpacingY;

    /** Extra margin for expanded sub-group items. */
    int                 m_subgroup_extramargin;

    /**
        The image width of the [+] icon.

        This is also calculated in the gutter
     */
    int                 m_iconWidth;

#ifndef wxPG_ICON_WIDTH

    /**
        The image height of the [+] icon.

        This is calculated as minimal size and to align
     */
    int                 m_iconHeight;
#endif

    /** Current cursor id. */
    int                 m_curcursor;

    // Caption font. Same as normal font plus bold style.
    wxFont              m_captionFont;

    int                 m_fontHeight;  // Height of the font.

    /** m_splitterx when drag began. */
    int                 m_startingSplitterX;

    /**
        Index to splitter currently being dragged (0=one after the first
        column).
     */
    int                 m_draggedSplitter;

    /** Changed property, calculated in PerformValidation(). */
    wxPGProperty*       m_chgInfo_changedProperty;

    /**
        Lowest property for which editing happened, but which does not have
        aggregate parent
     */
    wxPGProperty*       m_chgInfo_baseChangedProperty;

    /** Changed property value, calculated in PerformValidation(). */
    wxVariant           m_chgInfo_pendingValue;

    /** Passed to SetValue. */
    wxVariant           m_chgInfo_valueList;

    /** Validation information. */
    wxPGValidationInfo  m_validationInfo;

    /** Actions and keys that trigger them. */
    wxPGHashMapI2I      m_actionTriggers;

    /** Appearance of currently active editor. */
    wxPGCell            m_editorAppearance;

    /** Appearance of a unspecified value cell. */
    wxPGCell            m_unspecifiedAppearance;

    /** List of properties to be deleted/removed in idle event handler. */
    wxArrayPGProperty   m_deletedProperties;
    wxArrayPGProperty   m_removedProperties;

    /** List of key codes that will not be handed over to editor controls. */
    // FIXME: Make this a hash set once there is template-based wxHashSet.
    wxVector<int>       m_dedicatedKeys;

    //
    // Temporary values
    //

    /** Bits are used to indicate which colours are customized. */
    unsigned short      m_coloursCustomized;

    /** x - m_splitterx. */
    signed char                 m_dragOffset;

    /** 0 = not dragging, 1 = drag just started, 2 = drag in progress */
    unsigned char       m_dragStatus;

    /** 0 = margin, 1 = label, 2 = value. */
    unsigned char       m_mouseSide;

    /** True when editor control is focused. */
    unsigned char       m_editorFocused;

    /** 1 if m_latsCaption is also the bottommost caption. */
    //unsigned char       m_lastCaptionBottomnest;

    /** Set to 1 when graphics frozen. */
    unsigned char       m_frozen;

    unsigned char       m_vspacing;

    // Used to track when Alt/Ctrl+Key was consumed.
    unsigned char       m_keyComboConsumed;

    /** 1 if in DoPropertyChanged() */
    bool                m_inDoPropertyChanged;

    /** 1 if in CommitChangesFromEditor() */
    bool                m_inCommitChangesFromEditor;

    /** 1 if in DoSelectProperty() */
    bool                m_inDoSelectProperty;

    bool                m_inOnValidationFailure;

    wxPGVFBFlags        m_permanentValidationFailureBehavior;  // Set by app

    // DoEditorValidate() recursion guard
    wxRecursionGuardFlag    m_validatingEditor;

    /** Internal flags - see wxPG_FL_XXX constants. */
    wxUint32            m_iFlags;

    /** When drawing next time, clear this many item slots at the end. */
    int                 m_clearThisMany;

    // Mouse is hovering over this column (index)
    unsigned int        m_colHover;

    // pointer to property that has mouse hovering
    wxPGProperty*       m_propHover;

    // Active label editor
    wxTextCtrl*         m_labelEditor;

    // For which property the label editor is active
    wxPGProperty*       m_labelEditorProperty;

    // EventObject for wxPropertyGridEvents
    wxWindow*           m_eventObject;

    // What (global) window is currently focused (needed to resolve event
    // handling mess).
    wxWindow*           m_curFocused;

    // Event currently being sent - NULL if none at the moment
    wxPropertyGridEvent*    m_processedEvent;

    // Last known top-level parent
    wxWindow*           m_tlp;

    // Last closed top-level parent
    wxWindow*           m_tlpClosed;

    // Local time ms when tlp was closed.
    wxLongLong          m_tlpClosedTime;

    // Sort function
    wxPGSortCallback    m_sortFunction;

    // y coordinate of property that mouse hovering
    int                 m_propHoverY;

    // Which column's editor is selected (usually 1)?
    unsigned int        m_selColumn;

    // x relative to splitter (needed for resize).
    int                 m_ctrlXAdjust;

    // lines between cells
    wxColour            m_colLine;
    // property labels and values are written in this colour
    wxColour            m_colPropFore;
    // or with this colour when disabled
    wxColour            m_colDisPropFore;
    // background for m_colPropFore
    wxColour            m_colPropBack;
    // text color for captions
    wxColour            m_colCapFore;
    // background color for captions
    wxColour            m_colCapBack;
    // foreground for selected property
    wxColour            m_colSelFore;
    // background for selected property (actually use background color when
    // control out-of-focus)
    wxColour            m_colSelBack;
    // background colour for margin
    wxColour            m_colMargin;
    // background colour for empty space below the grid
    wxColour            m_colEmptySpace;

    // Default property colours
    wxPGCell            m_propertyDefaultCell;

    // Default property category
    wxPGCell            m_categoryDefaultCell;

    // Backup of selected property's cells
    wxVector<wxPGCell>  m_propCellsBackup;

    // NB: These *cannot* be moved to globals.

    // labels when properties use common values
    wxVector<wxPGCommonValue*>  m_commonValues;

    // array of live events
    wxVector<wxPropertyGridEvent*>  m_liveEvents;

    // Which cv selection really sets value to unspecified?
    int                 m_cvUnspecified;

    // Used to skip excess text editor events
    wxString            m_prevTcValue;

protected:

    // Sets some members to defaults (called constructors).
    void Init1();

    // Initializes some members (called by Create and complex constructor).
    void Init2();

    void OnPaint(wxPaintEvent &event );

    // main event receivers
    void OnMouseMove( wxMouseEvent &event );
    void OnMouseClick( wxMouseEvent &event );
    void OnMouseRightClick( wxMouseEvent &event );
    void OnMouseDoubleClick( wxMouseEvent &event );
    void OnMouseUp( wxMouseEvent &event );
    void OnKey( wxKeyEvent &event );
    void OnResize( wxSizeEvent &event );

    // event handlers
    bool HandleMouseMove( int x, unsigned int y, wxMouseEvent &event );
    bool HandleMouseClick( int x, unsigned int y, wxMouseEvent &event );
    bool HandleMouseRightClick( int x, unsigned int y, wxMouseEvent &event );
    bool HandleMouseDoubleClick( int x, unsigned int y, wxMouseEvent &event );
    bool HandleMouseUp( int x, unsigned int y, wxMouseEvent &event );
    void HandleKeyEvent( wxKeyEvent &event, bool fromChild );

    void OnMouseEntry( wxMouseEvent &event );

    void OnIdle( wxIdleEvent &event );
    void OnFocusEvent( wxFocusEvent &event );
    void OnChildFocusEvent( wxChildFocusEvent& event );

    bool OnMouseCommon( wxMouseEvent &event, int* px, int *py );
    bool OnMouseChildCommon( wxMouseEvent &event, int* px, int *py );

    // sub-control event handlers
    void OnMouseClickChild( wxMouseEvent &event );
    void OnMouseRightClickChild( wxMouseEvent &event );
    void OnMouseMoveChild( wxMouseEvent &event );
    void OnMouseUpChild( wxMouseEvent &event );
    void OnChildKeyDown( wxKeyEvent &event );

    void OnCaptureChange( wxMouseCaptureChangedEvent &event );

    void OnScrollEvent( wxScrollWinEvent &event );

    void OnSysColourChanged( wxSysColourChangedEvent &event );

    void OnTLPClose( wxCloseEvent& event );

protected:

    bool AddToSelectionFromInputEvent( wxPGProperty* prop,
                                       unsigned int colIndex,
                                       wxMouseEvent* event = NULL,
                                       int selFlags = 0 );

    /**
        Adjust the centering of the bitmap icons (collapse / expand) when the
        caption font changes.

        They need to be centered in the middle of the font, so a bit of deltaY
        adjustment is needed. On entry, m_captionFont must be set to window
        font. It will be modified properly.
      */
    void CalculateFontAndBitmapStuff( int vspacing );

    wxRect GetEditorWidgetRect( wxPGProperty* p, int column ) const;

    void CorrectEditorWidgetSizeX();

    /** Called in RecalculateVirtualSize() to reposition control
        on virtual height changes.
    */
    void CorrectEditorWidgetPosY();

    int DoDrawItems( wxDC& dc,
                     const wxRect* itemsRect,
                     bool isBuffered ) const;

    /** Draws an expand/collapse (ie. +/-) button.
    */
    virtual void DrawExpanderButton( wxDC& dc, const wxRect& rect,
                                     wxPGProperty* property ) const;

    /** Draws items from topitemy to bottomitemy */
    void DrawItems( wxDC& dc,
                    unsigned int topItemY,
                    unsigned int bottomItemY,
                    const wxRect* itemsRect = NULL );

    // Translate wxKeyEvent to wxPG_ACTION_XXX
    int KeyEventToActions(wxKeyEvent &event, int* pSecond) const;

    int KeyEventToAction(wxKeyEvent &event) const
    {
        return KeyEventToActions(event, NULL);
    }

    void ImprovedClientToScreen( int* px, int* py );

    // Called by focus event handlers. newFocused is the window that becomes
    // focused.
    void HandleFocusChange( wxWindow* newFocused );

    /** Reloads all non-customized colours from system settings. */
    void RegainColours();

    virtual bool DoEditorValidate();

    // Similar to DoSelectProperty() but also works on columns
    // other than 1. Does not active editor if column is not
    // editable.
    bool DoSelectAndEdit( wxPGProperty* prop,
                          unsigned int colIndex,
                          unsigned int selFlags );

    void DoSetSelection( const wxArrayPGProperty& newSelection,
                         int selFlags = 0 );

    void DoSetSplitterPosition( int newxpos,
                                int splitterIndex = 0,
                                int flags = wxPG_SPLITTER_REFRESH );

    bool DoAddToSelection( wxPGProperty* prop,
                           int selFlags = 0 );

    bool DoRemoveFromSelection( wxPGProperty* prop,
                                int selFlags = 0 );

    void DoBeginLabelEdit( unsigned int colIndex, int selFlags = 0 );
    void DoEndLabelEdit( bool commit, int selFlags = 0 );
    void OnLabelEditorEnterPress( wxCommandEvent& event );
    void OnLabelEditorKeyPress( wxKeyEvent& event );

    wxPGProperty* DoGetItemAtY( int y ) const;

    void DestroyEditorWnd( wxWindow* wnd );
    void FreeEditors();

    virtual bool DoExpand( wxPGProperty* p, bool sendEvent = false );

    virtual bool DoCollapse( wxPGProperty* p, bool sendEvent = false );

    // Returns nearest paint visible property (such that will be painted unless
    // window is scrolled or resized). If given property is paint visible, then
    // it itself will be returned.
    wxPGProperty* GetNearestPaintVisible( wxPGProperty* p ) const;

    static void RegisterDefaultEditors();

    // Sets up basic event handling for child control
    void SetupChildEventHandling( wxWindow* wnd );

    void CustomSetCursor( int type, bool override = false );

    /**
        Repositions scrollbar and underlying panel according to changed virtual
        size.
    */
    void RecalculateVirtualSize( int forceXPos = -1 );

    void SetEditorAppearance( const wxPGCell& cell,
                              bool unspecified = false );

    void ResetEditorAppearance()
    {
        wxPGCell cell;
        cell.SetEmptyData();
        SetEditorAppearance(cell, false);
    }

    void PrepareAfterItemsAdded();

    /**
        Send event from the property grid.

        Omit the wxPG_SEL_NOVALIDATE flag to allow vetoing the event
    */
    bool SendEvent( int eventType, wxPGProperty* p,
                    wxVariant* pValue = NULL,
                    unsigned int selFlags = wxPG_SEL_NOVALIDATE,
                    unsigned int column = 1 );

    // This function only moves focus to the wxPropertyGrid if it already
    // was on one of its child controls.
    void SetFocusOnCanvas();

    bool DoHideProperty( wxPGProperty* p, bool hide, int flags );

private:

    bool ButtonTriggerKeyTest( int action, wxKeyEvent& event );

    DECLARE_EVENT_TABLE()
};

// -----------------------------------------------------------------------
//
// Bunch of inlines that need to resolved after all classes have been defined.
//

inline bool wxPropertyGridPageState::IsDisplayed() const
{
    return ( this == m_pPropGrid->GetState() );
}

inline unsigned int wxPropertyGridPageState::GetActualVirtualHeight() const
{
    return DoGetRoot()->GetChildrenHeight(GetGrid()->GetRowHeight());
}

inline wxString wxPGProperty::GetHintText() const
{
    wxVariant vHintText = GetAttribute(wxPGGlobalVars->m_strHint);

#if wxPG_COMPATIBILITY_1_4
    // Try the old, deprecated "InlineHelp"
    if ( vHintText.IsNull() )
        vHintText = GetAttribute(wxPGGlobalVars->m_strInlineHelp);
#endif

    if ( !vHintText.IsNull() )
        return vHintText.GetString();

    return wxEmptyString;
}

inline int wxPGProperty::GetDisplayedCommonValueCount() const
{
    if ( HasFlag(wxPG_PROP_USES_COMMON_VALUE) )
    {
        wxPropertyGrid* pg = GetGrid();
        if ( pg )
            return (int) pg->GetCommonValueCount();
    }
    return 0;
}

inline void wxPGProperty::SetDefaultValue( wxVariant& value )
{
    SetAttribute(wxPG_ATTR_DEFAULT_VALUE, value);
}

inline void wxPGProperty::SetEditor( const wxString& editorName )
{
    m_customEditor = wxPropertyGridInterface::GetEditorByName(editorName);
}

inline bool wxPGProperty::SetMaxLength( int maxLen )
{
    return GetGrid()->SetPropertyMaxLength(this,maxLen);
}

// -----------------------------------------------------------------------

#define wxPG_BASE_EVT_PRE_ID     1775

#ifndef SWIG

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_PROPGRID, wxEVT_PG_SELECTED, wxPropertyGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_PROPGRID, wxEVT_PG_CHANGING, wxPropertyGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_PROPGRID, wxEVT_PG_CHANGED, wxPropertyGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_PROPGRID, wxEVT_PG_HIGHLIGHTED, wxPropertyGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_PROPGRID, wxEVT_PG_RIGHT_CLICK, wxPropertyGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_PROPGRID, wxEVT_PG_PAGE_CHANGED, wxPropertyGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_PROPGRID, wxEVT_PG_ITEM_COLLAPSED, wxPropertyGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_PROPGRID, wxEVT_PG_ITEM_EXPANDED, wxPropertyGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_PROPGRID, wxEVT_PG_DOUBLE_CLICK, wxPropertyGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_PROPGRID,
                          wxEVT_PG_LABEL_EDIT_BEGIN, wxPropertyGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_PROPGRID,
                          wxEVT_PG_LABEL_EDIT_ENDING, wxPropertyGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_PROPGRID,
                          wxEVT_PG_COL_BEGIN_DRAG, wxPropertyGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_PROPGRID,
                          wxEVT_PG_COL_DRAGGING, wxPropertyGridEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_PROPGRID,
                          wxEVT_PG_COL_END_DRAG, wxPropertyGridEvent );

#else
    enum {
        wxEVT_PG_SELECTED = wxPG_BASE_EVT_PRE_ID,
        wxEVT_PG_CHANGING,
        wxEVT_PG_CHANGED,
        wxEVT_PG_HIGHLIGHTED,
        wxEVT_PG_RIGHT_CLICK,
        wxEVT_PG_PAGE_CHANGED,
        wxEVT_PG_ITEM_COLLAPSED,
        wxEVT_PG_ITEM_EXPANDED,
        wxEVT_PG_DOUBLE_CLICK,
        wxEVT_PG_LABEL_EDIT_BEGIN,
        wxEVT_PG_LABEL_EDIT_ENDING,
        wxEVT_PG_COL_BEGIN_DRAG,
        wxEVT_PG_COL_DRAGGING,
        wxEVT_PG_COL_END_DRAG
    };
#endif


#define wxPG_BASE_EVT_TYPE       wxEVT_PG_SELECTED
#define wxPG_MAX_EVT_TYPE        (wxPG_BASE_EVT_TYPE+30)


#ifndef SWIG
typedef void (wxEvtHandler::*wxPropertyGridEventFunction)(wxPropertyGridEvent&);

#define EVT_PG_SELECTED(id, fn)              wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_PG_SELECTED, id, -1, wxEVENT_HANDLER_CAST( wxPropertyGridEventFunction, fn ), NULL ),
#define EVT_PG_CHANGING(id, fn)              wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_PG_CHANGING, id, -1, wxEVENT_HANDLER_CAST( wxPropertyGridEventFunction, fn ), NULL ),
#define EVT_PG_CHANGED(id, fn)               wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_PG_CHANGED, id, -1, wxEVENT_HANDLER_CAST( wxPropertyGridEventFunction, fn ), NULL ),
#define EVT_PG_HIGHLIGHTED(id, fn)           wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_PG_HIGHLIGHTED, id, -1, wxEVENT_HANDLER_CAST( wxPropertyGridEventFunction, fn ), NULL ),
#define EVT_PG_RIGHT_CLICK(id, fn)           wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_PG_RIGHT_CLICK, id, -1, wxEVENT_HANDLER_CAST( wxPropertyGridEventFunction, fn ), NULL ),
#define EVT_PG_DOUBLE_CLICK(id, fn)          wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_PG_DOUBLE_CLICK, id, -1, wxEVENT_HANDLER_CAST( wxPropertyGridEventFunction, fn ), NULL ),
#define EVT_PG_PAGE_CHANGED(id, fn)          wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_PG_PAGE_CHANGED, id, -1, wxEVENT_HANDLER_CAST( wxPropertyGridEventFunction, fn ), NULL ),
#define EVT_PG_ITEM_COLLAPSED(id, fn)        wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_PG_ITEM_COLLAPSED, id, -1, wxEVENT_HANDLER_CAST( wxPropertyGridEventFunction, fn ), NULL ),
#define EVT_PG_ITEM_EXPANDED(id, fn)         wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_PG_ITEM_EXPANDED, id, -1, wxEVENT_HANDLER_CAST( wxPropertyGridEventFunction, fn ), NULL ),
#define EVT_PG_LABEL_EDIT_BEGIN(id, fn)      wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_PG_LABEL_EDIT_BEGIN, id, -1, wxEVENT_HANDLER_CAST( wxPropertyGridEventFunction, fn ), NULL ),
#define EVT_PG_LABEL_EDIT_ENDING(id, fn)     wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_PG_LABEL_EDIT_ENDING, id, -1, wxEVENT_HANDLER_CAST( wxPropertyGridEventFunction, fn ), NULL ),
#define EVT_PG_COL_BEGIN_DRAG(id, fn)        wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_PG_COL_BEGIN_DRAG, id, -1, wxEVENT_HANDLER_CAST( wxPropertyGridEventFunction, fn ), NULL ),
#define EVT_PG_COL_DRAGGING(id, fn)          wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_PG_COL_DRAGGING, id, -1, wxEVENT_HANDLER_CAST( wxPropertyGridEventFunction, fn ), NULL ),
#define EVT_PG_COL_END_DRAG(id, fn)          wxDECLARE_EVENT_TABLE_ENTRY( wxEVT_PG_COL_END_DRAG, id, -1, wxEVENT_HANDLER_CAST( wxPropertyGridEventFunction, fn ), NULL ),

#define wxPropertyGridEventHandler(fn) \
    wxEVENT_HANDLER_CAST( wxPropertyGridEventFunction, fn )

#endif


/** @class wxPropertyGridEvent

    A propertygrid event holds information about events associated with
    wxPropertyGrid objects.

    @library{wxpropgrid}
    @category{propgrid}
*/
class WXDLLIMPEXP_PROPGRID wxPropertyGridEvent : public wxCommandEvent
{
public:

    /** Constructor. */
    wxPropertyGridEvent(wxEventType commandType=0, int id=0);

    /** Copy constructor. */
    wxPropertyGridEvent(const wxPropertyGridEvent& event);

    /** Destructor. */
    ~wxPropertyGridEvent();

    /** Copyer. */
    virtual wxEvent* Clone() const;

    /**
        Returns the column index associated with this event.
        For the column dragging events, it is the column to the left
        of the splitter being dragged
    */
    unsigned int GetColumn() const
    {
        return m_column;
    }

    wxPGProperty* GetMainParent() const
    {
        wxASSERT(m_property);
        return m_property->GetMainParent();
    }

    /** Returns id of associated property. */
    wxPGProperty* GetProperty() const
    {
        return m_property;
    }

    wxPGValidationInfo& GetValidationInfo()
    {
        wxASSERT(m_validationInfo);
        return *m_validationInfo;
    }

    /** Returns true if you can veto the action that the event is signaling.
    */
    bool CanVeto() const { return m_canVeto; }

    /**
        Call this from your event handler to veto action that the event is
        signaling.

        You can only veto a shutdown if wxPropertyGridEvent::CanVeto returns
        true.
        @remarks
        Currently only wxEVT_PG_CHANGING supports vetoing.
    */
    void Veto( bool veto = true ) { m_wasVetoed = veto; }

    /**
        Returns name of the associated property.

        @remarks Property name is stored in event, so it remains
                 accessible even after the associated property or
                 the property grid has been deleted.
    */
    wxString GetPropertyName() const
    {
        return m_propertyName;
    }

    /**
        Returns value of the associated property. Works for all event
        types, but for wxEVT_PG_CHANGING this member function returns
        the value that is pending, so you can call Veto() if the
        value is not satisfactory.

        @remarks Property value is stored in event, so it remains
                 accessible even after the associated property or
                 the property grid has been deleted.
    */
    wxVariant GetPropertyValue() const
    {
        if ( m_validationInfo )
            return m_validationInfo->GetValue();
        return m_value;
    }

    /**
        Returns value of the associated property.

        @see GetPropertyValue
    */
    wxVariant GetValue() const
    {
        return GetPropertyValue();
    }

    /**
        Set override validation failure behaviour.

        Only effective if Veto was also called, and only allowed if event type
        is wxEVT_PG_CHANGING.
    */
    void SetValidationFailureBehavior( wxPGVFBFlags flags )
    {
        wxASSERT( GetEventType() == wxEVT_PG_CHANGING );
        m_validationInfo->SetFailureBehavior( flags );
    }

    /** Sets custom failure message for this time only. Only applies if
        wxPG_VFB_SHOW_MESSAGE is set in validation failure flags.
    */
    void SetValidationFailureMessage( const wxString& message )
    {
        wxASSERT( GetEventType() == wxEVT_PG_CHANGING );
        m_validationInfo->SetFailureMessage( message );
    }

    wxPGVFBFlags GetValidationFailureBehavior() const
    {
        wxASSERT( GetEventType() == wxEVT_PG_CHANGING );
        return m_validationInfo->GetFailureBehavior();
    }

    void SetColumn( unsigned int column )
    {
        m_column = column;
    }

    void SetCanVeto( bool canVeto ) { m_canVeto = canVeto; }
    bool WasVetoed() const { return m_wasVetoed; }

    /** Changes the associated property. */
    void SetProperty( wxPGProperty* p )
    {
        m_property = p;
        if ( p )
            m_propertyName = p->GetName();
    }

    void SetPropertyValue( wxVariant value )
    {
        m_value = value;
    }

    void SetPropertyGrid( wxPropertyGrid* pg )
    {
        m_pg = pg;
        OnPropertyGridSet();
    }

    void SetupValidationInfo()
    {
        wxASSERT(m_pg);
        wxASSERT( GetEventType() == wxEVT_PG_CHANGING );
        m_validationInfo = &m_pg->GetValidationInfo();
        m_value = m_validationInfo->GetValue();
    }

private:
    void Init();
    void OnPropertyGridSet();
    DECLARE_DYNAMIC_CLASS(wxPropertyGridEvent)

    wxPGProperty*       m_property;
    wxPropertyGrid*     m_pg;
    wxPGValidationInfo* m_validationInfo;

    wxString            m_propertyName;
    wxVariant           m_value;

    unsigned int        m_column;

    bool                m_canVeto;
    bool                m_wasVetoed;
};


// -----------------------------------------------------------------------

/** @class wxPropertyGridPopulator
    @ingroup classes
    Allows populating wxPropertyGrid from arbitrary text source.
*/
class WXDLLIMPEXP_PROPGRID wxPropertyGridPopulator
{
public:
    /** Default constructor.
    */
    wxPropertyGridPopulator();

    /** Destructor. */
    virtual ~wxPropertyGridPopulator();

    void SetState( wxPropertyGridPageState* state );

    void SetGrid( wxPropertyGrid* pg );

    /** Appends a new property under bottommost parent.
        @param propClass
        Property class as string.
    */
    wxPGProperty* Add( const wxString& propClass,
                       const wxString& propLabel,
                       const wxString& propName,
                       const wxString* propValue,
                       wxPGChoices* pChoices = NULL );

    /**
        Pushes property to the back of parent array (ie it becomes bottommost
        parent), and starts scanning/adding children for it.

        When finished, parent array is returned to the original state.
    */
    void AddChildren( wxPGProperty* property );

    /** Adds attribute to the bottommost property.
        @param type
        Allowed values: "string", (same as string), "int", "bool". Empty string
          mean autodetect.
    */
    bool AddAttribute( const wxString& name,
                       const wxString& type,
                       const wxString& value );

    /** Called once in AddChildren.
    */
    virtual void DoScanForChildren() = 0;

    /**
        Returns id of parent property for which children can currently be
        added.
     */
    wxPGProperty* GetCurParent() const
    {
        return (wxPGProperty*) m_propHierarchy[m_propHierarchy.size()-1];
    }

    wxPropertyGridPageState* GetState() { return m_state; }
    const wxPropertyGridPageState* GetState() const { return m_state; }

    /** Like wxString::ToLong, except allows N% in addition of N.
    */
    static bool ToLongPCT( const wxString& s, long* pval, long max );

    /** Parses strings of format "choice1"[=value1] ... "choiceN"[=valueN] into
        wxPGChoices. Registers parsed result using idString (if not empty).
        Also, if choices with given id already registered, then don't parse but
        return those choices instead.
    */
    wxPGChoices ParseChoices( const wxString& choicesString,
                              const wxString& idString );

    /** Implement in derived class to do custom process when an error occurs.
        Default implementation uses wxLogError.
    */
    virtual void ProcessError( const wxString& msg );

protected:

    /** Used property grid. */
    wxPropertyGrid*         m_pg;

    /** Used property grid state. */
    wxPropertyGridPageState*    m_state;

    /** Tree-hierarchy of added properties (that can have children). */
    wxArrayPGProperty       m_propHierarchy;

    /** Hashmap for string-id to wxPGChoicesData mapping. */
    wxPGHashMapS2P          m_dictIdChoices;
};

// -----------------------------------------------------------------------

//
// Undefine macros that are not needed outside propertygrid sources
//
#ifndef __wxPG_SOURCE_FILE__
    #undef wxPG_FL_DESC_REFRESH_REQUIRED
    #undef wxPG_FL_SCROLLBAR_DETECTED
    #undef wxPG_FL_CREATEDSTATE
    #undef wxPG_FL_NOSTATUSBARHELP
    #undef wxPG_FL_SCROLLED
    #undef wxPG_FL_FOCUS_INSIDE_CHILD
    #undef wxPG_FL_FOCUS_INSIDE
    #undef wxPG_FL_MOUSE_INSIDE_CHILD
    #undef wxPG_FL_CUR_USES_CUSTOM_IMAGE
    #undef wxPG_FL_PRIMARY_FILLS_ENTIRE
    #undef wxPG_FL_VALUE_MODIFIED
    #undef wxPG_FL_MOUSE_INSIDE
    #undef wxPG_FL_FOCUSED
    #undef wxPG_FL_MOUSE_CAPTURED
    #undef wxPG_FL_INITIALIZED
    #undef wxPG_FL_ACTIVATION_BY_CLICK
    #undef wxPG_SUPPORT_TOOLTIPS
    #undef wxPG_ICON_WIDTH
    #undef wxPG_USE_RENDERER_NATIVE
// Following are needed by the manager headers
//    #undef const wxString&
#endif

// -----------------------------------------------------------------------

#endif

#endif // _WX_PROPGRID_PROPGRID_H_

