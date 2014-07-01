/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/cocoa/private/textimpl.h
// Purpose:     textcontrol implementation classes that have to be exposed
// Author:      Stefan Csomor
// Modified by:
// Created:     03/02/99
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_COCOA_PRIVATE_TEXTIMPL_H_
#define _WX_OSX_COCOA_PRIVATE_TEXTIMPL_H_

#include "wx/combobox.h"
#include "wx/osx/private.h"

// implementation exposed, so that search control can pull it

class wxNSTextFieldControl : public wxWidgetCocoaImpl, public wxTextWidgetImpl
{
public :
    // wxNSTextFieldControl must always be associated with a wxTextEntry. If
    // it's associated with a wxTextCtrl then we can get the associated entry
    // from it but otherwise the second ctor should be used to explicitly pass
    // us the entry.
    wxNSTextFieldControl( wxTextCtrl *text, WXWidget w );
    wxNSTextFieldControl( wxWindow *wxPeer, wxTextEntry *entry, WXWidget w );
    virtual ~wxNSTextFieldControl();

    virtual bool CanClipMaxLength() const { return true; }
    virtual void SetMaxLength(unsigned long len);
        
    virtual wxString GetStringValue() const ;
    virtual void SetStringValue( const wxString &str) ;
    virtual void Copy() ;
    virtual void Cut() ;
    virtual void Paste() ;
    virtual bool CanPaste() const ;
    virtual void SetEditable(bool editable) ;
    virtual void GetSelection( long* from, long* to) const ;
    virtual void SetSelection( long from , long to );
    virtual void WriteText(const wxString& str) ;
    virtual bool HasOwnContextMenu() const { return true; }
    virtual bool SetHint(const wxString& hint);

    virtual void controlAction(WXWidget slf, void* _cmd, void *sender);
    virtual bool becomeFirstResponder(WXWidget slf, void *_cmd);
    virtual bool resignFirstResponder(WXWidget slf, void *_cmd);

    virtual void SetInternalSelection( long from , long to );

protected :
    NSTextField* m_textField;
    long m_selStart;
    long m_selEnd;

private:
    // Common part of both ctors.
    void Init(WXWidget w);
};

class wxNSTextViewControl : public wxWidgetCocoaImpl, public wxTextWidgetImpl
{
public:
    wxNSTextViewControl( wxTextCtrl *wxPeer, WXWidget w );
    virtual ~wxNSTextViewControl();

    virtual wxString GetStringValue() const ;
    virtual void SetStringValue( const wxString &str) ;
    virtual void Copy() ;
    virtual void Cut() ;
    virtual void Paste() ;
    virtual bool CanPaste() const ;
    virtual void SetEditable(bool editable) ;
    virtual void GetSelection( long* from, long* to) const ;
    virtual void SetSelection( long from , long to );
    virtual void WriteText(const wxString& str) ;
    virtual void SetFont( const wxFont & font , const wxColour& foreground , long windowStyle, bool ignoreBlack = true );

    virtual bool GetStyle(long position, wxTextAttr& style);
    virtual void SetStyle(long start, long end, const wxTextAttr& style);

    virtual bool CanFocus() const;

    virtual bool HasOwnContextMenu() const { return true; }

    virtual void CheckSpelling(bool check);
    virtual wxSize GetBestSize() const;

protected:
    NSScrollView* m_scrollView;
    NSTextView* m_textView;
};

class wxNSComboBoxControl : public wxNSTextFieldControl, public wxComboWidgetImpl
{
public :
    wxNSComboBoxControl( wxComboBox *wxPeer, WXWidget w );
    virtual ~wxNSComboBoxControl();

    virtual int GetSelectedItem() const;
    virtual void SetSelectedItem(int item);

    virtual int GetNumberOfItems() const;

    virtual void InsertItem(int pos, const wxString& item);
    virtual void RemoveItem(int pos);

    virtual void Clear();

    virtual wxString GetStringAtIndex(int pos) const;

    virtual int FindString(const wxString& text) const;
    virtual void Popup();
    virtual void Dismiss();

    virtual void SetEditable(bool editable);

    virtual void mouseEvent(WX_NSEvent event, WXWidget slf, void *_cmd);

private:
    NSComboBox* m_comboBox;
};

#endif // _WX_OSX_COCOA_PRIVATE_TEXTIMPL_H_
