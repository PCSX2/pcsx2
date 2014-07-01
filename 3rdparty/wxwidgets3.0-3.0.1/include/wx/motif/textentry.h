///////////////////////////////////////////////////////////////////////////////
// Name:        wx/motif/textentry.h
// Purpose:     wxMotif-specific wxTextEntry implementation
// Author:      Vadim Zeitlin
// Created:     2007-11-05
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MOTIF_TEXTENTRY_H_
#define _WX_MOTIF_TEXTENTRY_H_

// ----------------------------------------------------------------------------
// wxTextEntry wraps XmTextXXX() methods suitable for single-line controls
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxTextEntry : public wxTextEntryBase
{
public:
    wxTextEntry() { }

    // implement wxTextEntryBase pure virtual methods
    virtual void WriteText(const wxString& text);
    virtual void Replace(long from, long to, const wxString& value);
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

protected:
    virtual wxString DoGetValue() const;

    // translate wx text position (which may be -1 meaning "last one") to a
    // valid Motif text position
    long GetMotifPos(long pos) const;

private:
    // implement this to return the associated xmTextWidgetClass widget
    virtual WXWidget GetTextWidget() const = 0;
};

#endif // _WX_MOTIF_TEXTENTRY_H_

