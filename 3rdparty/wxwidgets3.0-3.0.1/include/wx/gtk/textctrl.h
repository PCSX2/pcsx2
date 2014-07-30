/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/textctrl.h
// Purpose:
// Author:      Robert Roebling
// Created:     01/02/97
// Copyright:   (c) 1998 Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_TEXTCTRL_H_
#define _WX_GTK_TEXTCTRL_H_

typedef struct _GtkTextMark GtkTextMark;

//-----------------------------------------------------------------------------
// wxTextCtrl
//-----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxTextCtrl: public wxTextCtrlBase
{
public:
    wxTextCtrl() { Init(); }
    wxTextCtrl(wxWindow *parent,
               wxWindowID id,
               const wxString &value = wxEmptyString,
               const wxPoint &pos = wxDefaultPosition,
               const wxSize &size = wxDefaultSize,
               long style = 0,
               const wxValidator& validator = wxDefaultValidator,
               const wxString &name = wxTextCtrlNameStr);

    virtual ~wxTextCtrl();

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString &value = wxEmptyString,
                const wxPoint &pos = wxDefaultPosition,
                const wxSize &size = wxDefaultSize,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString &name = wxTextCtrlNameStr);

    // implement base class pure virtuals
    // ----------------------------------

    virtual void WriteText(const wxString& text);
    virtual wxString GetValue() const;
    virtual bool IsEmpty() const;

    virtual int GetLineLength(long lineNo) const;
    virtual wxString GetLineText(long lineNo) const;
    virtual int GetNumberOfLines() const;

    virtual bool IsModified() const;
    virtual bool IsEditable() const;

    virtual void GetSelection(long* from, long* to) const;

    virtual void Remove(long from, long to);

    virtual void MarkDirty();
    virtual void DiscardEdits();

    virtual bool SetStyle(long start, long end, const wxTextAttr& style);
    virtual bool GetStyle(long position, wxTextAttr& style);

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

    // Clipboard operations
    virtual void Copy();
    virtual void Cut();
    virtual void Paste();

    // Insertion point
    virtual void SetInsertionPoint(long pos);
    virtual long GetInsertionPoint() const;
    virtual wxTextPos GetLastPosition() const;

    virtual void SetSelection(long from, long to);
    virtual void SetEditable(bool editable);

    // Overridden wxWindow methods
    virtual void SetWindowStyleFlag( long style );
    virtual bool Enable( bool enable = true );

    // Implementation from now on
    void OnDropFiles( wxDropFilesEvent &event );
    void OnChar( wxKeyEvent &event );

    void OnCut(wxCommandEvent& event);
    void OnCopy(wxCommandEvent& event);
    void OnPaste(wxCommandEvent& event);
    void OnUndo(wxCommandEvent& event);
    void OnRedo(wxCommandEvent& event);

    void OnUpdateCut(wxUpdateUIEvent& event);
    void OnUpdateCopy(wxUpdateUIEvent& event);
    void OnUpdatePaste(wxUpdateUIEvent& event);
    void OnUpdateUndo(wxUpdateUIEvent& event);
    void OnUpdateRedo(wxUpdateUIEvent& event);

    bool SetFont(const wxFont& font);
    bool SetForegroundColour(const wxColour& colour);
    bool SetBackgroundColour(const wxColour& colour);

    GtkWidget* GetConnectWidget();

    void SetUpdateFont(bool WXUNUSED(update)) { }

    // implementation only from now on

    // tell the control to ignore next text changed signal
    void IgnoreNextTextUpdate(int n = 1) { m_countUpdatesToIgnore = n; }

    // should we ignore the changed signal? always resets the flag
    bool IgnoreTextUpdate();

    // call this to indicate that the control is about to be changed
    // programmatically and so m_modified flag shouldn't be set
    void DontMarkDirtyOnNextChange() { m_dontMarkDirty = true; }

    // should we mark the control as dirty? always resets the flag
    bool MarkDirtyOnChange();

    // always let GTK have mouse release events for multiline controls
    virtual bool GTKProcessEvent(wxEvent& event) const;


    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

protected:
    // overridden wxWindow virtual methods
    virtual wxSize DoGetBestSize() const;
    virtual void DoApplyWidgetStyle(GtkRcStyle *style);
    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const;

    virtual wxSize DoGetSizeFromTextSize(int xlen, int ylen = -1) const;

    virtual void DoFreeze();
    virtual void DoThaw();

    // Widgets that use the style->base colour for the BG colour should
    // override this and return true.
    virtual bool UseGTKStyleBase() const { return true; }

    virtual void DoSetValue(const wxString &value, int flags = 0);

    // Override this to use either GtkEntry or GtkTextView IME depending on the
    // kind of control we are.
    virtual int GTKIMFilterKeypress(GdkEventKey* event) const;

    virtual wxPoint DoPositionToCoords(long pos) const;

    // wrappers hiding the differences between functions doing the same thing
    // for GtkTextView and GtkEntry (all of them use current window style to
    // set the given characteristic)
    void GTKSetEditable();
    void GTKSetVisibility();
    void GTKSetActivatesDefault();
    void GTKSetWrapMode();
    void GTKSetJustification();

private:
    void Init();

    // overridden wxTextEntry virtual methods
    virtual GtkEditable *GetEditable() const;
    virtual GtkEntry *GetEntry() const;
    virtual void EnableTextChangedEvents(bool enable);

    // change the font for everything in this control
    void ChangeFontGlobally();

    // get the encoding which is used in this control: this looks at our font
    // and default style but not the current style (i.e. the style for the
    // current position); returns wxFONTENCODING_SYSTEM if we have no specific
    // encoding
    wxFontEncoding GetTextEncoding() const;

    // returns either m_text or m_buffer depending on whether the control is
    // single- or multi-line; convenient for the GTK+ functions which work with
    // both
    void *GetTextObject() const
    {
        return IsMultiLine() ? static_cast<void *>(m_buffer)
                             : static_cast<void *>(m_text);
    }


    // the widget used for single line controls
    GtkWidget  *m_text;

    bool        m_modified:1;
    bool        m_dontMarkDirty:1;

    int         m_countUpdatesToIgnore;

    // Our text buffer. Convenient, and holds the buffer while using
    // a dummy one when frozen
    GtkTextBuffer *m_buffer;

    GtkTextMark* m_showPositionOnThaw;
    GSList* m_anonymousMarkList;

    // For wxTE_AUTO_URL
    void OnUrlMouseEvent(wxMouseEvent&);

    DECLARE_EVENT_TABLE()
    DECLARE_DYNAMIC_CLASS(wxTextCtrl)
};

#endif // _WX_GTK_TEXTCTRL_H_
