///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/textentry.h
// Purpose:     wxGTK-specific wxTextEntry implementation
// Author:      Vadim Zeitlin
// Created:     2007-09-24
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_TEXTENTRY_H_
#define _WX_GTK_TEXTENTRY_H_

typedef struct _GdkEventKey GdkEventKey;
typedef struct _GtkEditable GtkEditable;
typedef struct _GtkEntry GtkEntry;

// ----------------------------------------------------------------------------
// wxTextEntry: roughly corresponds to GtkEditable
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxTextEntry : public wxTextEntryBase
{
public:
    wxTextEntry() { }

    // implement wxTextEntryBase pure virtual methods
    virtual void WriteText(const wxString& text);
    virtual void Remove(long from, long to);

    virtual void Copy();
    virtual void Cut();
    virtual void Paste();

    virtual void Undo();
    virtual void Redo();
    virtual bool CanUndo() const;
    virtual bool CanRedo() const;

    virtual void SetInsertionPoint(long pos);
    virtual long GetInsertionPoint() const;
    virtual long GetLastPosition() const;

    virtual void SetSelection(long from, long to);
    virtual void GetSelection(long *from, long *to) const;

    virtual bool IsEditable() const;
    virtual void SetEditable(bool editable);

    virtual void SetMaxLength(unsigned long len);

    // implementation only from now on
    void SendMaxLenEvent();
    bool GTKEntryOnInsertText(const char* text);

protected:
    // This method must be called from the derived class Create() to connect
    // the handlers for the clipboard (cut/copy/paste) events.
    void GTKConnectClipboardSignals(GtkWidget* entry);

    // And this one to connect "insert-text" signal.
    void GTKConnectInsertTextSignal(GtkEntry* entry);


    virtual void DoSetValue(const wxString& value, int flags);
    virtual wxString DoGetValue() const;

    // margins functions
    virtual bool DoSetMargins(const wxPoint& pt);
    virtual wxPoint DoGetMargins() const;

    virtual bool DoAutoCompleteStrings(const wxArrayString& choices);

    // Override the base class method to use GtkEntry IM context.
    virtual int GTKIMFilterKeypress(GdkEventKey* event) const;

private:
    // implement this to return the associated GtkEntry or another widget
    // implementing GtkEditable
    virtual GtkEditable *GetEditable() const = 0;

    // implement this to return the associated GtkEntry
    virtual GtkEntry *GetEntry() const = 0;
};

#endif // _WX_GTK_TEXTENTRY_H_

