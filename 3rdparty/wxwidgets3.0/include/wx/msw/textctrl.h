/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/textctrl.h
// Purpose:     wxTextCtrl class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TEXTCTRL_H_
#define _WX_TEXTCTRL_H_

class WXDLLIMPEXP_CORE wxTextCtrl : public wxTextCtrlBase
{
public:
    // creation
    // --------

    wxTextCtrl() { Init(); }
    wxTextCtrl(wxWindow *parent, wxWindowID id,
               const wxString& value = wxEmptyString,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = 0,
               const wxValidator& validator = wxDefaultValidator,
               const wxString& name = wxTextCtrlNameStr)
    {
        Init();

        Create(parent, id, value, pos, size, style, validator, name);
    }
    virtual ~wxTextCtrl();

    bool Create(wxWindow *parent, wxWindowID id,
                const wxString& value = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxTextCtrlNameStr);

    // overridden wxTextEntry methods
    // ------------------------------

    virtual wxString GetValue() const;
    virtual wxString GetRange(long from, long to) const;

    virtual bool IsEmpty() const;

    virtual void WriteText(const wxString& text);
    virtual void AppendText(const wxString& text);
    virtual void Clear();

    virtual int GetLineLength(long lineNo) const;
    virtual wxString GetLineText(long lineNo) const;
    virtual int GetNumberOfLines() const;

    virtual void SetMaxLength(unsigned long len);

    virtual void GetSelection(long *from, long *to) const;

    virtual void Redo();
    virtual bool CanRedo() const;

    virtual void SetInsertionPointEnd();
    virtual long GetInsertionPoint() const;
    virtual wxTextPos GetLastPosition() const;

    // implement base class pure virtuals
    // ----------------------------------

    virtual bool IsModified() const;
    virtual void MarkDirty();
    virtual void DiscardEdits();

#ifdef __WIN32__
    virtual bool EmulateKeyPress(const wxKeyEvent& event);
#endif // __WIN32__

#if wxUSE_RICHEDIT
    // apply text attribute to the range of text (only works with richedit
    // controls)
    virtual bool SetStyle(long start, long end, const wxTextAttr& style);
    virtual bool SetDefaultStyle(const wxTextAttr& style);
    virtual bool GetStyle(long position, wxTextAttr& style);
#endif // wxUSE_RICHEDIT

    // translate between the position (which is just an index in the text ctrl
    // considering all its contents as a single strings) and (x, y) coordinates
    // which represent column and line.
    virtual long XYToPosition(long x, long y) const;
    virtual bool PositionToXY(long pos, long *x, long *y) const;

    virtual void ShowPosition(long pos);
    virtual wxTextCtrlHitTestResult HitTest(const wxPoint& pt, long *pos) const;
    virtual wxTextCtrlHitTestResult HitTest(const wxPoint& pt,
                                            wxTextCoord *col,
                                            wxTextCoord *row) const
    {
        return wxTextCtrlBase::HitTest(pt, col, row);
    }

    // Caret handling (Windows only)
    bool ShowNativeCaret(bool show = true);
    bool HideNativeCaret() { return ShowNativeCaret(false); }

    // Implementation from now on
    // --------------------------

#if wxUSE_DRAG_AND_DROP && wxUSE_RICHEDIT
    virtual void SetDropTarget(wxDropTarget *dropTarget);
#endif // wxUSE_DRAG_AND_DROP && wxUSE_RICHEDIT

    virtual void SetWindowStyleFlag(long style);

    virtual void Command(wxCommandEvent& event);
    virtual bool MSWCommand(WXUINT param, WXWORD id);
    virtual WXHBRUSH MSWControlColor(WXHDC hDC, WXHWND hWnd);

#if wxUSE_RICHEDIT
    virtual bool MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM *result);

    int GetRichVersion() const { return m_verRichEdit; }
    bool IsRich() const { return m_verRichEdit != 0; }

    // rich edit controls are not compatible with normal ones and we must set
    // the colours and font for them otherwise
    virtual bool SetBackgroundColour(const wxColour& colour);
    virtual bool SetForegroundColour(const wxColour& colour);
    virtual bool SetFont(const wxFont& font);
#else
    bool IsRich() const { return false; }
#endif // wxUSE_RICHEDIT

#if wxUSE_INKEDIT && wxUSE_RICHEDIT
    bool IsInkEdit() const { return m_isInkEdit != 0; }
#else
    bool IsInkEdit() const { return false; }
#endif

    virtual void AdoptAttributesFromHWND();

    virtual bool AcceptsFocusFromKeyboard() const;

    // returns true if the platform should explicitly apply a theme border
    virtual bool CanApplyThemeBorder() const;

    // callbacks
    void OnDropFiles(wxDropFilesEvent& event);
    void OnChar(wxKeyEvent& event); // Process 'enter' if required

    void OnCut(wxCommandEvent& event);
    void OnCopy(wxCommandEvent& event);
    void OnPaste(wxCommandEvent& event);
    void OnUndo(wxCommandEvent& event);
    void OnRedo(wxCommandEvent& event);
    void OnDelete(wxCommandEvent& event);
    void OnSelectAll(wxCommandEvent& event);

    void OnUpdateCut(wxUpdateUIEvent& event);
    void OnUpdateCopy(wxUpdateUIEvent& event);
    void OnUpdatePaste(wxUpdateUIEvent& event);
    void OnUpdateUndo(wxUpdateUIEvent& event);
    void OnUpdateRedo(wxUpdateUIEvent& event);
    void OnUpdateDelete(wxUpdateUIEvent& event);
    void OnUpdateSelectAll(wxUpdateUIEvent& event);

    // Show a context menu for Rich Edit controls (the standard
    // EDIT control has one already)
    void OnContextMenu(wxContextMenuEvent& event);

#if wxABI_VERSION >= 30002
    // Create context menu for RICHEDIT controls. This may be called once during
    // the control's lifetime or every time the menu is shown, depending on
    // implementation.
    wxMenu *MSWCreateContextMenu();
#endif

    // be sure the caret remains invisible if the user
    // called HideNativeCaret() before
    void OnSetFocus(wxFocusEvent& event);

    // intercept WM_GETDLGCODE
    virtual WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam);

    virtual bool MSWShouldPreProcessMessage(WXMSG* pMsg);
    virtual WXDWORD MSWGetStyle(long style, WXDWORD *exstyle) const;

protected:
    // common part of all ctors
    void Init();

    virtual bool DoLoadFile(const wxString& file, int fileType);

    // creates the control of appropriate class (plain or rich edit) with the
    // styles corresponding to m_windowStyle
    //
    // this is used by ctor/Create() and when we need to recreate the control
    // later
    bool MSWCreateText(const wxString& value,
                       const wxPoint& pos,
                       const wxSize& size);

    virtual void DoSetValue(const wxString &value, int flags = 0);

    virtual wxPoint DoPositionToCoords(long pos) const;

    // return true if this control has a user-set limit on amount of text (i.e.
    // the limit is due to a previous call to SetMaxLength() and not built in)
    bool HasSpaceLimit(unsigned int *len) const;

    // Used by EN_MAXTEXT handler to increase the size limit (will do nothing
    // if the current limit is big enough). Should never be called directly.
    //
    // Returns true if we increased the limit to allow entering more text,
    // false if we hit the limit set by SetMaxLength() and so didn't change it.
    bool AdjustSpaceLimit();

#if wxUSE_RICHEDIT && (!wxUSE_UNICODE || wxUSE_UNICODE_MSLU)
    // replace the selection or the entire control contents with the given text
    // in the specified encoding
    bool StreamIn(const wxString& value, wxFontEncoding encoding, bool selOnly);

    // get the contents of the control out as text in the given encoding
    wxString StreamOut(wxFontEncoding encoding, bool selOnly = false) const;
#endif // wxUSE_RICHEDIT

    // replace the contents of the selection or of the entire control with the
    // given text
    void DoWriteText(const wxString& text,
                     int flags = SetValue_SendEvent | SetValue_SelectionOnly);

    // set the selection (possibly without scrolling the caret into view)
    void DoSetSelection(long from, long to, int flags);

    // get the length of the line containing the character at the given
    // position
    long GetLengthOfLineContainingPos(long pos) const;

    // send TEXT_UPDATED event, return true if it was handled, false otherwise
    bool SendUpdateEvent();

    virtual wxSize DoGetBestSize() const;
    virtual wxSize DoGetSizeFromTextSize(int xlen, int ylen = -1) const;

#if wxUSE_RICHEDIT
    // Apply the character-related parts of wxTextAttr to the given selection
    // or the entire control if start == end == -1.
    //
    // This function is private and should only be called for rich edit
    // controls and with (from, to) already in correct order, i.e. from <= to.
    bool MSWSetCharFormat(const wxTextAttr& attr, long from = -1, long to = -1);

    // Same as above for paragraph-related parts of wxTextAttr. Note that this
    // can only be applied to the selection as RichEdit doesn't support setting
    // the paragraph styles globally.
    bool MSWSetParaFormat(const wxTextAttr& attr, long from, long to);


    // we're using RICHEDIT (and not simple EDIT) control if this field is not
    // 0, it also gives the version of the RICHEDIT control being used
    // (although not directly: 1 is for 1.0, 2 is for either 2.0 or 3.0 as we
    // can't nor really need to distinguish between them and 4 is for 4.1)
    int m_verRichEdit;
#endif // wxUSE_RICHEDIT

    // number of EN_UPDATE events sent by Windows when we change the controls
    // text ourselves: we want this to be exactly 1
    int m_updatesCount;

private:
    virtual void EnableTextChangedEvents(bool enable)
    {
        m_updatesCount = enable ? -1 : -2;
    }

    // implement wxTextEntry pure virtual: it implements all the operations for
    // the simple EDIT controls
    virtual WXHWND GetEditHWND() const { return m_hWnd; }

    void OnKeyDown(wxKeyEvent& event);

    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxTextCtrl)

    wxMenu* m_privateContextMenu;

    bool m_isNativeCaretShown;

#if wxUSE_INKEDIT && wxUSE_RICHEDIT
    int  m_isInkEdit;
#endif

};

#endif // _WX_TEXTCTRL_H_
