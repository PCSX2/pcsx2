/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/textentry.h
// Purpose:     wxTextEntry class
// Author:      Stefan Csomor
// Modified by: Kevin Ollivier
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_TEXTENTRY_H_
#define _WX_OSX_TEXTENTRY_H_

#if wxUSE_SYSTEM_OPTIONS
    // set this to 'true' if you want to use the 'classic' MLTE-based implementation
    // instead of the HIView-based implementation in 10.3 and upwards, the former
    // has more features (backgrounds etc.), but may show redraw artefacts and other
    // problems depending on your usage; hence, the default is 'false'.
    #define wxMAC_TEXTCONTROL_USE_MLTE wxT("mac.textcontrol-use-mlte")
    // set this to 'true' if you want editable text controls to have spell checking turned
    // on by default, you can change this setting individually on a control using MacCheckSpelling
    #define wxMAC_TEXTCONTROL_USE_SPELL_CHECKER wxT("mac.textcontrol-use-spell-checker")
#endif

#include "wx/control.h"

// forward decl for wxListWidgetImpl implementation type.
class WXDLLIMPEXP_FWD_CORE wxTextWidgetImpl;

class WXDLLIMPEXP_CORE wxTextEntry: public wxTextEntryBase
{

public:
    wxTextEntry();
    virtual ~wxTextEntry();

    virtual bool IsEditable() const;

    // If the return values from and to are the same, there is no selection.
    virtual void GetSelection(long* from, long* to) const;

    // operations
    // ----------

    // editing
    virtual void Clear();
    virtual void Remove(long from, long to);

    // set the max number of characters which may be entered
    // in a single line text control
    virtual void SetMaxLength(unsigned long len);

    // writing text inserts it at the current position;
    // appending always inserts it at the end
    virtual void WriteText(const wxString& text);

    // Clipboard operations
    virtual void Copy();
    virtual void Cut();
    virtual void Paste();

    virtual bool CanCopy() const;
    virtual bool CanCut() const;
    virtual bool CanPaste() const;

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
    virtual void SetEditable(bool editable);

    virtual bool SendMaxLenEvent();

    // set the grayed out hint text
    virtual bool SetHint(const wxString& hint);
    virtual wxString GetHint() const;

    // Implementation
    // --------------

    virtual wxTextWidgetImpl * GetTextPeer() const;
    wxTextCompleter *OSXGetCompleter() const { return m_completer; }

protected:

    virtual wxString DoGetValue() const;

    virtual bool DoAutoCompleteStrings(const wxArrayString& choices);
    virtual bool DoAutoCompleteCustom(wxTextCompleter *completer);

    // The object providing auto-completions or NULL if none.
    wxTextCompleter *m_completer;

    bool  m_editable;

  // need to make this public because of the current implementation via callbacks
    unsigned long  m_maxLength;

private:
    wxString m_hintString;
};

#endif // _WX_OSX_TEXTENTRY_H_
