/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/textctrl.h
// Purpose:     wxTextCtrl class
// Author:      David Elliott
// Modified by:
// Created:     2003/03/16
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_TEXTCTRL_H__
#define __WX_COCOA_TEXTCTRL_H__

#include "wx/cocoa/NSTextField.h"

// ========================================================================
// wxTextCtrl
// ========================================================================
class WXDLLIMPEXP_CORE wxTextCtrl : public wxTextCtrlBase, protected wxCocoaNSTextField
{
    DECLARE_DYNAMIC_CLASS(wxTextCtrl)
    DECLARE_EVENT_TABLE()
    WX_DECLARE_COCOA_OWNER(NSTextField,NSControl,NSView)
// ------------------------------------------------------------------------
// initialization
// ------------------------------------------------------------------------
public:
    wxTextCtrl() {}
    wxTextCtrl(wxWindow *parent, wxWindowID winid,
            const wxString& value = wxEmptyString,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize, long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxTextCtrlNameStr)
    {
        Create(parent, winid, value, pos, size, style, validator, name);
    }

    bool Create(wxWindow *parent, wxWindowID winid,
            const wxString& value = wxEmptyString,
            const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize, long style = 0,
            const wxValidator& validator = wxDefaultValidator,
            const wxString& name = wxTextCtrlNameStr);
    virtual ~wxTextCtrl();

// ------------------------------------------------------------------------
// Cocoa specifics
// ------------------------------------------------------------------------
protected:
    virtual void Cocoa_didChangeText(void);
    virtual void CocoaTarget_action(void);
// ------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------
public:
    virtual wxString GetValue() const;

    virtual int GetLineLength(long lineNo) const;
    virtual wxString GetLineText(long lineNo) const;
    virtual int GetNumberOfLines() const;

    virtual bool IsModified() const;
    virtual bool IsEditable() const;

    // If the return values from and to are the same, there is no selection.
    virtual void GetSelection(long* from, long* to) const;

    // operations
    // ----------

    // editing
    virtual void Clear();
    virtual void Replace(long from, long to, const wxString& value);
    virtual void Remove(long from, long to);

    // clears the dirty flag
    virtual void MarkDirty();
    virtual void DiscardEdits();

    // writing text inserts it at the current position, appending always
    // inserts it at the end
    virtual void WriteText(const wxString& text);
    virtual void AppendText(const wxString& text);

    // translate between the position (which is just an index in the text ctrl
    // considering all its contents as a single strings) and (x, y) coordinates
    // which represent column and line.
    virtual long XYToPosition(long x, long y) const;
    virtual bool PositionToXY(long pos, long *x, long *y) const;

    virtual void ShowPosition(long pos);

    // Clipboard operations
    virtual void Copy();
    virtual void Cut();
    virtual void Paste();

    // Undo/redo
    virtual void Undo();
    virtual void Redo();

    virtual bool CanUndo() const;
    virtual bool CanRedo() const;

    // Insertion point
    virtual void SetInsertionPoint(long pos);
    virtual void SetInsertionPointEnd();
    virtual long GetInsertionPoint() const;
    virtual wxTextPos GetLastPosition() const;

    virtual void SetSelection(long from, long to);
//    virtual void SelectAll();
    virtual void SetEditable(bool editable);

protected:
    virtual wxSize DoGetBestSize() const;

    virtual void DoSetValue(const wxString& value, int flags = 0);
};

#endif // __WX_COCOA_TEXTCTRL_H__
