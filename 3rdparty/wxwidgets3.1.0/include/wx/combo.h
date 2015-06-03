///////////////////////////////////////////////////////////////////////////////
// Name:        wx/combo.h
// Purpose:     wxComboCtrl declaration
// Author:      Jaakko Salli
// Modified by:
// Created:     Apr-30-2006
// Copyright:   (c) Jaakko Salli
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COMBOCONTROL_H_BASE_
#define _WX_COMBOCONTROL_H_BASE_


/*
   A few words about all the classes defined in this file are probably in
   order: why do we need extra wxComboCtrl and wxComboPopup classes?

   This is because a traditional combobox is a combination of a text control
   (with a button allowing to open the pop down list) with a listbox and
   wxComboBox class is exactly such control, however we want to also have other
   combinations - in fact, we want to allow anything at all to be used as pop
   down list, not just a wxListBox.

   So we define a base wxComboCtrl which can use any control as pop down
   list and wxComboBox deriving from it which implements the standard wxWidgets
   combobox API. wxComboCtrl needs to be told somehow which control to use
   and this is done by SetPopupControl(). However, we need something more than
   just a wxControl in this method as, for example, we need to call
   SetSelection("initial text value") and wxControl doesn't have such method.
   So we also need a wxComboPopup which is just a very simple interface which
   must be implemented by a control to be usable as a popup.

   We couldn't derive wxComboPopup from wxControl as this would make it
   impossible to have a class deriving from both wxListBx and from it, so
   instead it is just a mix-in.
 */


#include "wx/defs.h"

#if wxUSE_COMBOCTRL

#include "wx/control.h"
#include "wx/renderer.h" // this is needed for wxCONTROL_XXX flags
#include "wx/bitmap.h" // wxBitmap used by-value
#include "wx/textentry.h"
#include "wx/time.h" // needed for wxMilliClock_t

class WXDLLIMPEXP_FWD_CORE wxTextCtrl;
class WXDLLIMPEXP_FWD_CORE wxComboPopup;

//
// New window styles for wxComboCtrlBase
//
enum
{
    // Double-clicking a read-only combo triggers call to popup's OnComboPopup.
    // In wxOwnerDrawnComboBox, for instance, it cycles item.
    wxCC_SPECIAL_DCLICK             = 0x0100,

    // Dropbutton acts like standard push button.
    wxCC_STD_BUTTON                 = 0x0200
};


// wxComboCtrl internal flags
enum
{
    // First those that can be passed to Customize.
    // It is Windows style for all flags to be clear.

    // Button is preferred outside the border (GTK style)
    wxCC_BUTTON_OUTSIDE_BORDER      = 0x0001,
    // Show popup on mouse up instead of mouse down (which is the Windows style)
    wxCC_POPUP_ON_MOUSE_UP          = 0x0002,
    // All text is not automatically selected on click
    wxCC_NO_TEXT_AUTO_SELECT        = 0x0004,
    // Drop-button stays down as long as popup is displayed.
    wxCC_BUTTON_STAYS_DOWN          = 0x0008,
    // Drop-button covers the entire control.
    wxCC_FULL_BUTTON                = 0x0010,
    // Drop-button goes over the custom-border (used under WinVista).
    wxCC_BUTTON_COVERS_BORDER       = 0x0020,

    // Internal use: signals creation is complete
    wxCC_IFLAG_CREATED              = 0x0100,
    // Internal use: really put button outside
    wxCC_IFLAG_BUTTON_OUTSIDE       = 0x0200,
    // Internal use: SetMargins has been successfully called
    wxCC_IFLAG_LEFT_MARGIN_SET      = 0x0400,
    // Internal use: Set wxTAB_TRAVERSAL to parent when popup is dismissed
    wxCC_IFLAG_PARENT_TAB_TRAVERSAL = 0x0800,
    // Internal use: Secondary popup window type should be used (if available).
    wxCC_IFLAG_USE_ALT_POPUP        = 0x1000,
    // Internal use: Skip popup animation.
    wxCC_IFLAG_DISABLE_POPUP_ANIM   = 0x2000,
    // Internal use: Drop-button is a bitmap button or has non-default size
    // (but can still be on either side of the control), regardless whether
    // specified by the platform or the application.
    wxCC_IFLAG_HAS_NONSTANDARD_BUTTON   = 0x4000
};


// Flags used by PreprocessMouseEvent and HandleButtonMouseEvent
enum
{
    wxCC_MF_ON_BUTTON               =   0x0001, // cursor is on dropbutton area
    wxCC_MF_ON_CLICK_AREA           =   0x0002  // cursor is on dropbutton or other area
                                                // that can be clicked to show the popup.
};


// Namespace for wxComboCtrl feature flags
struct wxComboCtrlFeatures
{
    enum
    {
        MovableButton       = 0x0001, // Button can be on either side of control
        BitmapButton        = 0x0002, // Button may be replaced with bitmap
        ButtonSpacing       = 0x0004, // Button can have spacing from the edge
                                      // of the control
        TextIndent          = 0x0008, // SetMargins can be used to control
                                      // left margin.
        PaintControl        = 0x0010, // Combo control itself can be custom painted
        PaintWritable       = 0x0020, // A variable-width area in front of writable
                                      // combo control's textctrl can be custom
                                      // painted
        Borderless          = 0x0040, // wxNO_BORDER window style works

        // There are no feature flags for...
        // PushButtonBitmapBackground - if its in wxRendererNative, then it should be
        //   not an issue to have it automatically under the bitmap.

        All                 = MovableButton|BitmapButton|
                              ButtonSpacing|TextIndent|
                              PaintControl|PaintWritable|
                              Borderless
    };
};


class WXDLLIMPEXP_CORE wxComboCtrlBase : public wxControl,
                                         public wxTextEntry
{
    friend class wxComboPopup;
    friend class wxComboPopupEvtHandler;
public:
    // ctors and such
    wxComboCtrlBase() : wxControl(), wxTextEntry() { Init(); }

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& value,
                const wxPoint& pos,
                const wxSize& size,
                long style,
                const wxValidator& validator,
                const wxString& name);

    virtual ~wxComboCtrlBase();

    // Show/hide popup window (wxComboBox-compatible methods)
    virtual void Popup();
    virtual void Dismiss()
    {
        HidePopup(true);
    }

    // Show/hide popup window.
    // TODO: Maybe deprecate in favor of Popup()/Dismiss().
    //       However, these functions are still called internally
    //       so it is not straightforward.
    virtual void ShowPopup();
    virtual void HidePopup(bool generateEvent=false);

    // Override for totally custom combo action
    virtual void OnButtonClick();

    // return true if the popup is currently shown
    bool IsPopupShown() const { return m_popupWinState == Visible; }

    // set interface class instance derived from wxComboPopup
    // NULL popup can be used to indicate default in a derived class
    void SetPopupControl( wxComboPopup* popup )
    {
        DoSetPopupControl(popup);
    }

    // get interface class instance derived from wxComboPopup
    wxComboPopup* GetPopupControl()
    {
        EnsurePopupControl();
        return m_popupInterface;
    }

    // get the popup window containing the popup control
    wxWindow *GetPopupWindow() const { return m_winPopup; }

    // Get the text control which is part of the combobox.
    wxTextCtrl *GetTextCtrl() const { return m_text; }

    // get the dropdown button which is part of the combobox
    // note: its not necessarily a wxButton or wxBitmapButton
    wxWindow *GetButton() const { return m_btn; }

    // forward these methods to all subcontrols
    virtual bool Enable(bool enable = true) wxOVERRIDE;
    virtual bool Show(bool show = true) wxOVERRIDE;
    virtual bool SetFont(const wxFont& font) wxOVERRIDE;

    //
    // wxTextEntry methods
    //
    // NB: We basically need to override all of them because there is
    //     no guarantee how platform-specific wxTextEntry is implemented.
    //
    virtual void SetValue(const wxString& value) wxOVERRIDE
        { wxTextEntryBase::SetValue(value); }
    virtual void ChangeValue(const wxString& value) wxOVERRIDE
        { wxTextEntryBase::ChangeValue(value); }

    virtual void WriteText(const wxString& text) wxOVERRIDE;
    virtual void AppendText(const wxString& text) wxOVERRIDE
        { wxTextEntryBase::AppendText(text); }

    virtual wxString GetValue() const wxOVERRIDE
        { return wxTextEntryBase::GetValue(); }

    virtual wxString GetRange(long from, long to) const wxOVERRIDE
        { return wxTextEntryBase::GetRange(from, to); }

    // Replace() and DoSetValue() need to be fully re-implemented since
    // EventSuppressor utility class does not work with the way
    // wxComboCtrl is implemented.
    virtual void Replace(long from, long to, const wxString& value) wxOVERRIDE;

    virtual void Remove(long from, long to) wxOVERRIDE;

    virtual void Copy() wxOVERRIDE;
    virtual void Cut() wxOVERRIDE;
    virtual void Paste() wxOVERRIDE;

    virtual void Undo() wxOVERRIDE;
    virtual void Redo() wxOVERRIDE;
    virtual bool CanUndo() const wxOVERRIDE;
    virtual bool CanRedo() const wxOVERRIDE;

    virtual void SetInsertionPoint(long pos) wxOVERRIDE;
    virtual long GetInsertionPoint() const wxOVERRIDE;
    virtual long GetLastPosition() const wxOVERRIDE;

    virtual void SetSelection(long from, long to) wxOVERRIDE;
    virtual void GetSelection(long *from, long *to) const wxOVERRIDE;

    virtual bool IsEditable() const wxOVERRIDE;
    virtual void SetEditable(bool editable) wxOVERRIDE;

    virtual bool SetHint(const wxString& hint) wxOVERRIDE;
    virtual wxString GetHint() const wxOVERRIDE;

    // This method sets the text without affecting list selection
    // (ie. wxComboPopup::SetStringValue doesn't get called).
    void SetText(const wxString& value);

    // This method sets value and also optionally sends EVT_TEXT
    // (needed by combo popups)
    wxDEPRECATED( void SetValueWithEvent(const wxString& value,
                                         bool withEvent = true) );

    // Changes value of the control as if user had done it by selecting an
    // item from a combo box drop-down list. Needs to be public so that
    // derived popup classes can call it.
    void SetValueByUser(const wxString& value);

    //
    // Popup customization methods
    //

    // Sets minimum width of the popup. If wider than combo control, it will extend to the left.
    // Remarks:
    // * Value -1 indicates the default.
    // * Custom popup may choose to ignore this (wxOwnerDrawnComboBox does not).
    void SetPopupMinWidth( int width )
    {
        m_widthMinPopup = width;
    }

    // Sets preferred maximum height of the popup.
    // Remarks:
    // * Value -1 indicates the default.
    // * Custom popup may choose to ignore this (wxOwnerDrawnComboBox does not).
    void SetPopupMaxHeight( int height )
    {
        m_heightPopup = height;
    }

    // Extends popup size horizontally, relative to the edges of the combo control.
    // Remarks:
    // * Popup minimum width may override extLeft (ie. it has higher precedence).
    // * Values 0 indicate default.
    // * Custom popup may not take this fully into account (wxOwnerDrawnComboBox takes).
    void SetPopupExtents( int extLeft, int extRight )
    {
        m_extLeft = extLeft;
        m_extRight = extRight;
    }

    // Set width, in pixels, of custom paint area in writable combo.
    // In read-only, used to indicate area that is not covered by the
    // focus rectangle (which may or may not be drawn, depending on the
    // popup type).
    void SetCustomPaintWidth( int width );
    int GetCustomPaintWidth() const { return m_widthCustomPaint; }

    // Set side of the control to which the popup will align itself.
    // Valid values are wxLEFT, wxRIGHT and 0. The default value 0 wmeans
    // that the side of the button will be used.
    void SetPopupAnchor( int anchorSide )
    {
        m_anchorSide = anchorSide;
    }

    // Set position of dropdown button.
    //   width: button width. <= 0 for default.
    //   height: button height. <= 0 for default.
    //   side: wxLEFT or wxRIGHT, indicates on which side the button will be placed.
    //   spacingX: empty space on sides of the button. Default is 0.
    // Remarks:
    //   There is no spacingY - the button will be centered vertically.
    void SetButtonPosition( int width = -1,
                            int height = -1,
                            int side = wxRIGHT,
                            int spacingX = 0 );

    // Returns current size of the dropdown button.
    wxSize GetButtonSize();

    //
    // Sets dropbutton to be drawn with custom bitmaps.
    //
    //  bmpNormal: drawn when cursor is not on button
    //  pushButtonBg: Draw push button background below the image.
    //                NOTE! This is usually only properly supported on platforms with appropriate
    //                      method in wxRendererNative.
    //  bmpPressed: drawn when button is depressed
    //  bmpHover: drawn when cursor hovers on button. This is ignored on platforms
    //            that do not generally display hover differently.
    //  bmpDisabled: drawn when combobox is disabled.
    void SetButtonBitmaps( const wxBitmap& bmpNormal,
                           bool pushButtonBg = false,
                           const wxBitmap& bmpPressed = wxNullBitmap,
                           const wxBitmap& bmpHover = wxNullBitmap,
                           const wxBitmap& bmpDisabled = wxNullBitmap );

#if WXWIN_COMPATIBILITY_2_8
    //
    // This will set the space in pixels between left edge of the control and the
    // text, regardless whether control is read-only (ie. no wxTextCtrl) or not.
    // Platform-specific default can be set with value-1.
    // Remarks
    // * This method may do nothing on some native implementations.
    wxDEPRECATED( void SetTextIndent( int indent ) );

    // Returns actual indentation in pixels.
    wxDEPRECATED( wxCoord GetTextIndent() const );
#endif

    // Returns area covered by the text field.
    const wxRect& GetTextRect() const
    {
        return m_tcArea;
    }

    // Call with enable as true to use a type of popup window that guarantees ability
    // to focus the popup control, and normal function of common native controls.
    // This alternative popup window is usually a wxDialog, and as such it's parent
    // frame will appear as if the focus has been lost from it.
    void UseAltPopupWindow( bool enable = true )
    {
        wxASSERT_MSG( !m_winPopup,
                      wxT("call this only before SetPopupControl") );

        if ( enable )
            m_iFlags |= wxCC_IFLAG_USE_ALT_POPUP;
        else
            m_iFlags &= ~wxCC_IFLAG_USE_ALT_POPUP;
    }

    // Call with false to disable popup animation, if any.
    void EnablePopupAnimation( bool enable = true )
    {
        if ( enable )
            m_iFlags &= ~wxCC_IFLAG_DISABLE_POPUP_ANIM;
        else
            m_iFlags |= wxCC_IFLAG_DISABLE_POPUP_ANIM;
    }

    //
    // Utilies needed by the popups or native implementations
    //

    // Returns true if given key combination should toggle the popup.
    // NB: This is a separate from other keyboard handling because:
    //     1) Replaceability.
    //     2) Centralized code (otherwise it'd be split up between
    //        wxComboCtrl key handler and wxVListBoxComboPopup's
    //        key handler).
    virtual bool IsKeyPopupToggle(const wxKeyEvent& event) const = 0;

    // Prepare background of combo control or an item in a dropdown list
    // in a way typical on platform. This includes painting the focus/disabled
    // background and setting the clipping region.
    // Unless you plan to paint your own focus indicator, you should always call this
    // in your wxComboPopup::PaintComboControl implementation.
    // In addition, it sets pen and text colour to what looks good and proper
    // against the background.
    // flags: wxRendererNative flags: wxCONTROL_ISSUBMENU: is drawing a list item instead of combo control
    //                                wxCONTROL_SELECTED: list item is selected
    //                                wxCONTROL_DISABLED: control/item is disabled
    virtual void PrepareBackground( wxDC& dc, const wxRect& rect, int flags ) const;

    // Returns true if focus indicator should be drawn in the control.
    bool ShouldDrawFocus() const
    {
        const wxWindow* curFocus = FindFocus();
        return ( IsPopupWindowState(Hidden) &&
                 (curFocus == m_mainCtrlWnd || (m_btn && curFocus == m_btn)) &&
                 (m_windowStyle & wxCB_READONLY) );
    }

    // These methods return references to appropriate dropbutton bitmaps
    const wxBitmap& GetBitmapNormal() const { return m_bmpNormal; }
    const wxBitmap& GetBitmapPressed() const { return m_bmpPressed; }
    const wxBitmap& GetBitmapHover() const { return m_bmpHover; }
    const wxBitmap& GetBitmapDisabled() const { return m_bmpDisabled; }

    // Set custom style flags for embedded wxTextCtrl. Usually must be used
    // with two-step creation, before Create() call.
    void SetTextCtrlStyle( int style );

    // Return internal flags
    wxUint32 GetInternalFlags() const { return m_iFlags; }

    // Return true if Create has finished
    bool IsCreated() const { return m_iFlags & wxCC_IFLAG_CREATED ? true : false; }

    // Need to override to return text area background colour
    wxColour GetBackgroundColour() const;

    // common code to be called on popup hide/dismiss
    void OnPopupDismiss(bool generateEvent);

    // PopupShown states
    enum
    {
        Hidden       = 0,
        //Closing      = 1,
        Animating    = 2,
        Visible      = 3
    };

    bool IsPopupWindowState( int state ) const { return (state == m_popupWinState) ? true : false; }

    wxByte GetPopupWindowState() const { return m_popupWinState; }

    // Set value returned by GetMainWindowOfCompositeControl
    void SetCtrlMainWnd( wxWindow* wnd ) { m_mainCtrlWnd = wnd; }

    // This is public so we can access it from wxComboCtrlTextCtrl
    virtual wxWindow *GetMainWindowOfCompositeControl() wxOVERRIDE
        { return m_mainCtrlWnd; }

    // also set the embedded wxTextCtrl colours
    virtual bool SetForegroundColour(const wxColour& colour) wxOVERRIDE;
    virtual bool SetBackgroundColour(const wxColour& colour) wxOVERRIDE;

protected:

    // Returns true if hint text should be drawn in the control
    bool ShouldUseHintText(int flags = 0) const
    {
        return ( !m_text &&
                 !(flags & wxCONTROL_ISSUBMENU) &&
                 !m_valueString.length() &&
                 m_hintText.length() &&
                 !ShouldDrawFocus() );
    }

    //
    // Override these for customization purposes
    //

    // called from wxSizeEvent handler
    virtual void OnResize() = 0;

    // Return native text identation
    // (i.e. text margin, for pure text, not textctrl)
    virtual wxCoord GetNativeTextIndent() const;

    // Called in syscolourchanged handler and base create
    virtual void OnThemeChange();

    // Creates wxTextCtrl.
    //   extraStyle: Extra style parameters
    void CreateTextCtrl( int extraStyle );

    // Called when text was changed programmatically
    // (e.g. from WriteText())
    void OnSetValue(const wxString& value);

    // Installs standard input handler to combo (and optionally to the textctrl)
    void InstallInputHandlers();

    // Flags for DrawButton
    enum
    {
        Button_PaintBackground             = 0x0001, // Paints control background below the button
        Button_BitmapOnly                  = 0x0002  // Only paints the bitmap
    };

    // Draws dropbutton. Using wxRenderer or bitmaps, as appropriate.
    // Flags are defined above.
    virtual void DrawButton( wxDC& dc, const wxRect& rect, int flags = Button_PaintBackground );

    // Call if cursor is on button area or mouse is captured for the button.
    //bool HandleButtonMouseEvent( wxMouseEvent& event, bool isInside );
    bool HandleButtonMouseEvent( wxMouseEvent& event, int flags );

    // returns true if event was consumed or filtered (event type is also set to 0 in this case)
    bool PreprocessMouseEvent( wxMouseEvent& event, int flags );

    //
    // This will handle left_down and left_dclick events outside button in a Windows-like manner.
    // If you need alternate behaviour, it is recommended you manipulate and filter events to it
    // instead of building your own handling routine (for reference, on wxEVT_LEFT_DOWN it will
    // toggle popup and on wxEVT_LEFT_DCLICK it will do the same or run the popup's dclick method,
    // if defined - you should pass events of other types of it for common processing).
    void HandleNormalMouseEvent( wxMouseEvent& event );

    // Creates popup window, calls interface->Create(), etc
    void CreatePopup();

    // Destroy popup window and all related constructs
    void DestroyPopup();

    // override the base class virtuals involved in geometry calculations
    // The common version only sets a default width, so the derived classes
    // should override it and set the height and change the width as needed.
    virtual wxSize DoGetBestSize() const wxOVERRIDE;
    virtual wxSize DoGetSizeFromTextSize(int xlen, int ylen = -1) const wxOVERRIDE;

    // NULL popup can be used to indicate default in a derived class
    virtual void DoSetPopupControl(wxComboPopup* popup);

    // ensures there is atleast the default popup
    void EnsurePopupControl();

    // Recalculates button and textctrl areas. Called when size or button setup change.
    //   btnWidth: default/calculated width of the dropbutton. 0 means unchanged,
    //             just recalculate.
    void CalculateAreas( int btnWidth = 0 );

    // Standard textctrl positioning routine. Just give it platform-dependent
    // textctrl coordinate adjustment.
    virtual void PositionTextCtrl( int textCtrlXAdjust = 0,
                                   int textCtrlYAdjust = 0);

    // event handlers
    void OnSizeEvent( wxSizeEvent& event );
    void OnFocusEvent(wxFocusEvent& event);
    void OnIdleEvent(wxIdleEvent& event);
    void OnTextCtrlEvent(wxCommandEvent& event);
    void OnSysColourChanged(wxSysColourChangedEvent& event);
    void OnKeyEvent(wxKeyEvent& event);
    void OnCharEvent(wxKeyEvent& event);

    // Set customization flags (directs how wxComboCtrlBase helpers behave)
    void Customize( wxUint32 flags ) { m_iFlags |= flags; }

    // Dispatches size event and refreshes
    void RecalcAndRefresh();

    // Flags for DoShowPopup and AnimateShow
    enum
    {
        ShowBelow       = 0x0000,  // Showing popup below the control
        ShowAbove       = 0x0001,  // Showing popup above the control
        CanDeferShow    = 0x0002  // Can only return true from AnimateShow if this is set
    };

    // Shows and positions the popup.
    virtual void DoShowPopup( const wxRect& rect, int flags );

    // Implement in derived class to create a drop-down animation.
    // Return true if finished immediately. Otherwise popup is only
    // shown when the derived class call DoShowPopup.
    // Flags are same as for DoShowPopup.
    virtual bool AnimateShow( const wxRect& rect, int flags );

#if wxUSE_TOOLTIPS
    virtual void DoSetToolTip( wxToolTip *tip ) wxOVERRIDE;
#endif

    // protected wxTextEntry methods
    virtual void DoSetValue(const wxString& value, int flags) wxOVERRIDE;
    virtual wxString DoGetValue() const wxOVERRIDE;
    virtual wxWindow *GetEditableWindow() wxOVERRIDE { return this; }

    // margins functions
    virtual bool DoSetMargins(const wxPoint& pt) wxOVERRIDE;
    virtual wxPoint DoGetMargins() const wxOVERRIDE;

    // This is used when m_text is hidden (readonly).
    wxString                m_valueString;

    // This is used when control is unfocused and m_valueString is empty
    wxString                m_hintText;

    // the text control and button we show all the time
    wxTextCtrl*             m_text;
    wxWindow*               m_btn;

    // wxPopupWindow or similar containing the window managed by the interface.
    wxWindow*               m_winPopup;

    // the popup control/panel
    wxWindow*               m_popup;

    // popup interface
    wxComboPopup*           m_popupInterface;

    // this is input etc. handler for the text control
    wxEvtHandler*           m_textEvtHandler;

    // this is for the top level window
    wxEvtHandler*           m_toplevEvtHandler;

    // this is for the control in popup
    wxEvtHandler*           m_popupEvtHandler;

    // this is for the popup window
    wxEvtHandler*           m_popupWinEvtHandler;

    // main (ie. topmost) window of a composite control (default = this)
    wxWindow*               m_mainCtrlWnd;

    // used to prevent immediate re-popupping in case closed popup
    // by clicking on the combo control (needed because of inconsistent
    // transient implementation across platforms).
    wxMilliClock_t          m_timeCanAcceptClick;

    // how much popup should expand to the left/right of the control
    wxCoord                 m_extLeft;
    wxCoord                 m_extRight;

    // minimum popup width
    wxCoord                 m_widthMinPopup;

    // preferred popup height
    wxCoord                 m_heightPopup;

    // how much of writable combo is custom-paint by callback?
    // also used to indicate area that is not covered by "blue"
    // selection indicator.
    wxCoord                 m_widthCustomPaint;

    // left margin, in pixels
    wxCoord                 m_marginLeft;

    // side on which the popup is aligned
    int                     m_anchorSide;

    // Width of the "fake" border
    wxCoord                 m_widthCustomBorder;

    // The button and textctrl click/paint areas
    wxRect                  m_tcArea;
    wxRect                  m_btnArea;

    // Colour of the text area, in case m_text is NULL
    wxColour                m_tcBgCol;

    // current button state (uses renderer flags)
    int                     m_btnState;

    // button position
    int                     m_btnWid;
    int                     m_btnHei;
    int                     m_btnSide;
    int                     m_btnSpacingX;

    // last default button width
    int                     m_btnWidDefault;

    // custom dropbutton bitmaps
    wxBitmap                m_bmpNormal;
    wxBitmap                m_bmpPressed;
    wxBitmap                m_bmpHover;
    wxBitmap                m_bmpDisabled;

    // area used by the button
    wxSize                  m_btnSize;

    // platform-dependent customization and other flags
    wxUint32                m_iFlags;

    // custom style for m_text
    int                     m_textCtrlStyle;

    // draw blank button background under bitmap?
    bool                    m_blankButtonBg;

    // is the popup window currenty shown?
    wxByte                  m_popupWinState;

    // should the focus be reset to the textctrl in idle time?
    bool                    m_resetFocus;

    // is the text-area background colour overridden?
    bool                    m_hasTcBgCol;

private:
    void Init();

    wxByte                  m_ignoreEvtText;  // Number of next EVT_TEXTs to ignore

    // Is popup window wxPopupTransientWindow, wxPopupWindow or wxDialog?
    wxByte                  m_popupWinType;

    wxDECLARE_EVENT_TABLE();

    wxDECLARE_ABSTRACT_CLASS(wxComboCtrlBase);
};


// ----------------------------------------------------------------------------
// wxComboPopup is the interface which must be implemented by a control to be
// used as a popup by wxComboCtrl
// ----------------------------------------------------------------------------


// wxComboPopup internal flags
enum
{
    wxCP_IFLAG_CREATED      = 0x0001 // Set by wxComboCtrlBase after Create is called
};

class WXDLLIMPEXP_FWD_CORE wxComboCtrl;


class WXDLLIMPEXP_CORE wxComboPopup
{
    friend class wxComboCtrlBase;
public:
    wxComboPopup()
    {
        m_combo = NULL;
        m_iFlags = 0;
    }

    // This is called immediately after construction finishes. m_combo member
    // variable has been initialized before the call.
    // NOTE: It is not in constructor so the derived class doesn't need to redefine
    //       a default constructor of its own.
    virtual void Init() { }

    virtual ~wxComboPopup();

    // Create the popup child control.
    // Return true for success.
    virtual bool Create(wxWindow* parent) = 0;

    // Calls Destroy() for the popup control (i.e. one returned by
    // GetControl()) and makes sure that 'this' is deleted at the end.
    // Default implementation works for both cases where popup control
    // class is multiple inherited or created on heap as a separate
    // object.
    virtual void DestroyPopup();

    // We must have an associated control which is subclassed by the combobox.
    virtual wxWindow *GetControl() = 0;

    // Called immediately after the popup is shown
    virtual void OnPopup();

    // Called when popup is dismissed
    virtual void OnDismiss();

    // Called just prior to displaying popup.
    // Default implementation does nothing.
    virtual void SetStringValue( const wxString& value );

    // Gets displayed string representation of the value.
    virtual wxString GetStringValue() const = 0;

    // Called to check if the popup - when an item container - actually
    // has matching item. Case-sensitivity checking etc. is up to the
    // implementation. If the found item matched the string, but is
    // different, it should be written back to pItem. Default implementation
    // always return true and does not alter trueItem.
    virtual bool FindItem(const wxString& item, wxString* trueItem=NULL);

    // This is called to custom paint in the combo control itself (ie. not the popup).
    // Default implementation draws value as string.
    virtual void PaintComboControl( wxDC& dc, const wxRect& rect );

    // Receives wxEVT_KEY_DOWN key events from the parent wxComboCtrl.
    // Events not handled should be skipped, as usual.
    virtual void OnComboKeyEvent( wxKeyEvent& event );

    // Receives wxEVT_CHAR key events from the parent wxComboCtrl.
    // Events not handled should be skipped, as usual.
    virtual void OnComboCharEvent( wxKeyEvent& event );

    // Implement if you need to support special action when user
    // double-clicks on the parent wxComboCtrl.
    virtual void OnComboDoubleClick();

    // Return final size of popup. Called on every popup, just prior to OnShow.
    // minWidth = preferred minimum width for window
    // prefHeight = preferred height. Only applies if > 0,
    // maxHeight = max height for window, as limited by screen size
    //   and should only be rounded down, if necessary.
    virtual wxSize GetAdjustedSize( int minWidth, int prefHeight, int maxHeight );

    // Return true if you want delay call to Create until the popup is shown
    // for the first time. It is more efficient, but note that it is often
    // more convenient to have the control created immediately.
    // Default returns false.
    virtual bool LazyCreate();

    //
    // Utilies
    //

    // Hides the popup
    void Dismiss();

    // Returns true if Create has been called.
    bool IsCreated() const
    {
        return (m_iFlags & wxCP_IFLAG_CREATED) ? true : false;
    }

    // Returns pointer to the associated parent wxComboCtrl.
    wxComboCtrl* GetComboCtrl() const;

    // Default PaintComboControl behaviour
    static void DefaultPaintComboControl( wxComboCtrlBase* combo,
                                          wxDC& dc,
                                          const wxRect& rect );

protected:
    wxComboCtrlBase* m_combo;
    wxUint32            m_iFlags;

private:
    // Called in wxComboCtrlBase::SetPopupControl
    void InitBase(wxComboCtrlBase *combo)
    {
        m_combo = combo;
    }
};


// ----------------------------------------------------------------------------
// include the platform-dependent header defining the real class
// ----------------------------------------------------------------------------

#if defined(__WXUNIVERSAL__)
    // No native universal (but it must still be first in the list)
#elif defined(__WXMSW__)
    #include "wx/msw/combo.h"
#endif

// Any ports may need generic as an alternative
#include "wx/generic/combo.h"

#endif // wxUSE_COMBOCTRL

#endif
    // _WX_COMBOCONTROL_H_BASE_
