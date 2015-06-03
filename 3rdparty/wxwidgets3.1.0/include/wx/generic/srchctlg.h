/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/srchctlg.h
// Purpose:     generic wxSearchCtrl class
// Author:      Vince Harron
// Created:     2006-02-19
// Copyright:   Vince Harron
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_SEARCHCTRL_H_
#define _WX_GENERIC_SEARCHCTRL_H_

#if wxUSE_SEARCHCTRL

#include "wx/bitmap.h"

class WXDLLIMPEXP_FWD_CORE wxSearchButton;
class WXDLLIMPEXP_FWD_CORE wxSearchTextCtrl;

// ----------------------------------------------------------------------------
// wxSearchCtrl is a combination of wxTextCtrl and wxSearchButton
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxSearchCtrl : public wxSearchCtrlBase
{
public:
    // creation
    // --------

    wxSearchCtrl();
    wxSearchCtrl(wxWindow *parent, wxWindowID id,
               const wxString& value = wxEmptyString,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = 0,
               const wxValidator& validator = wxDefaultValidator,
               const wxString& name = wxSearchCtrlNameStr);

    virtual ~wxSearchCtrl();

    bool Create(wxWindow *parent, wxWindowID id,
                const wxString& value = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxSearchCtrlNameStr);

#if wxUSE_MENUS
    // get/set search button menu
    // --------------------------
    virtual void SetMenu( wxMenu* menu ) wxOVERRIDE;
    virtual wxMenu* GetMenu() wxOVERRIDE;
#endif // wxUSE_MENUS

    // get/set search options
    // ----------------------
    virtual void ShowSearchButton( bool show ) wxOVERRIDE;
    virtual bool IsSearchButtonVisible() const wxOVERRIDE;

    virtual void ShowCancelButton( bool show ) wxOVERRIDE;
    virtual bool IsCancelButtonVisible() const wxOVERRIDE;

    // TODO: In 2.9 these should probably be virtual, and declared in the base class...
    void SetDescriptiveText(const wxString& text);
    wxString GetDescriptiveText() const;

    // accessors
    // ---------

    virtual wxString GetRange(long from, long to) const wxOVERRIDE;

    virtual int GetLineLength(long lineNo) const wxOVERRIDE;
    virtual wxString GetLineText(long lineNo) const wxOVERRIDE;
    virtual int GetNumberOfLines() const wxOVERRIDE;

    virtual bool IsModified() const wxOVERRIDE;
    virtual bool IsEditable() const wxOVERRIDE;

    // more readable flag testing methods
    virtual bool IsSingleLine() const;
    virtual bool IsMultiLine() const;

    // If the return values from and to are the same, there is no selection.
    virtual void GetSelection(long* from, long* to) const wxOVERRIDE;

    virtual wxString GetStringSelection() const wxOVERRIDE;

    // operations
    // ----------

    // editing
    virtual void Clear() wxOVERRIDE;
    virtual void Replace(long from, long to, const wxString& value) wxOVERRIDE;
    virtual void Remove(long from, long to) wxOVERRIDE;

    // load/save the controls contents from/to the file
    virtual bool LoadFile(const wxString& file);
    virtual bool SaveFile(const wxString& file = wxEmptyString);

    // sets/clears the dirty flag
    virtual void MarkDirty() wxOVERRIDE;
    virtual void DiscardEdits() wxOVERRIDE;

    // set the max number of characters which may be entered in a single line
    // text control
    virtual void SetMaxLength(unsigned long WXUNUSED(len)) wxOVERRIDE;

    // writing text inserts it at the current position, appending always
    // inserts it at the end
    virtual void WriteText(const wxString& text) wxOVERRIDE;
    virtual void AppendText(const wxString& text) wxOVERRIDE;

    // insert the character which would have resulted from this key event,
    // return true if anything has been inserted
    virtual bool EmulateKeyPress(const wxKeyEvent& event);

    // text control under some platforms supports the text styles: these
    // methods allow to apply the given text style to the given selection or to
    // set/get the style which will be used for all appended text
    virtual bool SetStyle(long start, long end, const wxTextAttr& style) wxOVERRIDE;
    virtual bool GetStyle(long position, wxTextAttr& style) wxOVERRIDE;
    virtual bool SetDefaultStyle(const wxTextAttr& style) wxOVERRIDE;
    virtual const wxTextAttr& GetDefaultStyle() const wxOVERRIDE;

    // translate between the position (which is just an index in the text ctrl
    // considering all its contents as a single strings) and (x, y) coordinates
    // which represent column and line.
    virtual long XYToPosition(long x, long y) const wxOVERRIDE;
    virtual bool PositionToXY(long pos, long *x, long *y) const wxOVERRIDE;

    virtual void ShowPosition(long pos) wxOVERRIDE;

    // find the character at position given in pixels
    //
    // NB: pt is in device coords (not adjusted for the client area origin nor
    //     scrolling)
    virtual wxTextCtrlHitTestResult HitTest(const wxPoint& pt, long *pos) const wxOVERRIDE;
    virtual wxTextCtrlHitTestResult HitTest(const wxPoint& pt,
                                            wxTextCoord *col,
                                            wxTextCoord *row) const wxOVERRIDE;

    // Clipboard operations
    virtual void Copy() wxOVERRIDE;
    virtual void Cut() wxOVERRIDE;
    virtual void Paste() wxOVERRIDE;

    virtual bool CanCopy() const wxOVERRIDE;
    virtual bool CanCut() const wxOVERRIDE;
    virtual bool CanPaste() const wxOVERRIDE;

    // Undo/redo
    virtual void Undo() wxOVERRIDE;
    virtual void Redo() wxOVERRIDE;

    virtual bool CanUndo() const wxOVERRIDE;
    virtual bool CanRedo() const wxOVERRIDE;

    // Insertion point
    virtual void SetInsertionPoint(long pos) wxOVERRIDE;
    virtual void SetInsertionPointEnd() wxOVERRIDE;
    virtual long GetInsertionPoint() const wxOVERRIDE;
    virtual wxTextPos GetLastPosition() const wxOVERRIDE;

    virtual void SetSelection(long from, long to) wxOVERRIDE;
    virtual void SelectAll() wxOVERRIDE;
    virtual void SetEditable(bool editable) wxOVERRIDE;

#if 0

    // override streambuf method
#if wxHAS_TEXT_WINDOW_STREAM
    int overflow(int i);
#endif // wxHAS_TEXT_WINDOW_STREAM

    // stream-like insertion operators: these are always available, whether we
    // were, or not, compiled with streambuf support
    wxTextCtrl& operator<<(const wxString& s);
    wxTextCtrl& operator<<(int i);
    wxTextCtrl& operator<<(long i);
    wxTextCtrl& operator<<(float f);
    wxTextCtrl& operator<<(double d);
    wxTextCtrl& operator<<(const wxChar c);
#endif

    // do the window-specific processing after processing the update event
    virtual void DoUpdateWindowUI(wxUpdateUIEvent& event) wxOVERRIDE;

    virtual bool ShouldInheritColours() const wxOVERRIDE;

    // wxWindow overrides
    virtual bool SetFont(const wxFont& font) wxOVERRIDE;
    virtual bool SetBackgroundColour(const wxColour& colour) wxOVERRIDE;

    // search control generic only
    void SetSearchBitmap( const wxBitmap& bitmap );
    void SetCancelBitmap( const wxBitmap& bitmap );
#if wxUSE_MENUS
    void SetSearchMenuBitmap( const wxBitmap& bitmap );
#endif // wxUSE_MENUS

protected:
    virtual void DoSetValue(const wxString& value, int flags) wxOVERRIDE;
    virtual wxString DoGetValue() const wxOVERRIDE;

    virtual bool DoLoadFile(const wxString& file, int fileType) wxOVERRIDE;
    virtual bool DoSaveFile(const wxString& file, int fileType) wxOVERRIDE;

    // override the base class virtuals involved into geometry calculations
    virtual wxSize DoGetBestClientSize() const wxOVERRIDE;
    virtual void DoMoveWindow(int x, int y, int width, int height) wxOVERRIDE;

    virtual void RecalcBitmaps();

    void Init();

    virtual wxBitmap RenderSearchBitmap( int x, int y, bool renderDrop );
    virtual wxBitmap RenderCancelBitmap( int x, int y );

    void OnCancelButton( wxCommandEvent& event );

    void OnSetFocus( wxFocusEvent& event );
    void OnSize( wxSizeEvent& event );

    bool HasMenu() const
    {
#if wxUSE_MENUS
        return m_menu != NULL;
#else // !wxUSE_MENUS
        return false;
#endif // wxUSE_MENUS/!wxUSE_MENUS
    }

private:
    friend class wxSearchButton;

    // Implement pure virtual function inherited from wxCompositeWindow.
    virtual wxWindowList GetCompositeWindowParts() const wxOVERRIDE;

    // Position the child controls using the current window size.
    void LayoutControls();

#if wxUSE_MENUS
    void PopupSearchMenu();
#endif // wxUSE_MENUS

    // the subcontrols
    wxSearchTextCtrl *m_text;
    wxSearchButton *m_searchButton;
    wxSearchButton *m_cancelButton;
#if wxUSE_MENUS
    wxMenu *m_menu;
#endif // wxUSE_MENUS

    bool m_searchButtonVisible;
    bool m_cancelButtonVisible;

    bool m_searchBitmapUser;
    bool m_cancelBitmapUser;
#if wxUSE_MENUS
    bool m_searchMenuBitmapUser;
#endif // wxUSE_MENUS

    wxBitmap m_searchBitmap;
    wxBitmap m_cancelBitmap;
#if wxUSE_MENUS
    wxBitmap m_searchMenuBitmap;
#endif // wxUSE_MENUS

private:
    wxDECLARE_DYNAMIC_CLASS(wxSearchCtrl);

    wxDECLARE_EVENT_TABLE();
};

#endif // wxUSE_SEARCHCTRL

#endif // _WX_GENERIC_SEARCHCTRL_H_

