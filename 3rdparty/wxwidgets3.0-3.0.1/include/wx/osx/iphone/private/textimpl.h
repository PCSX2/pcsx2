/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/iphone/private/textimpl.h
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

class wxUITextFieldControl : public wxWidgetIPhoneImpl, public wxTextWidgetImpl
{
public :
    wxUITextFieldControl( wxTextCtrl *wxPeer, UITextField* w );
    virtual ~wxUITextFieldControl();

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

    virtual wxSize GetBestSize() const;    
    
    virtual bool SetHint(const wxString& hint);
    
    virtual void controlAction(WXWidget slf, void* _cmd, void *sender);
protected :
    UITextField* m_textField;
    NSObject<UITextFieldDelegate>* m_delegate;
    long m_selStart;
    long m_selEnd;
};

class wxUITextViewControl : public wxWidgetIPhoneImpl, public wxTextWidgetImpl
{
public:
    wxUITextViewControl( wxTextCtrl *wxPeer, UITextView* w );
    virtual ~wxUITextViewControl();

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
    NSObject<UITextViewDelegate>* m_delegate;
    UITextView* m_textView;
};

#if 0
class wxNSComboBoxControl : public wxUITextFieldControl, public wxComboWidgetImpl
{
public :
    wxNSComboBoxControl( wxWindow *wxPeer, WXWidget w );
    virtual ~wxNSComboBoxControl();

    virtual int GetSelectedItem() const;
    virtual void SetSelectedItem(int item);

    virtual int GetNumberOfItems() const;

    virtual void InsertItem(int pos, const wxString& item);
    virtual void RemoveItem(int pos);

    virtual void Clear();

    virtual wxString GetStringAtIndex(int pos) const;

    virtual int FindString(const wxString& text) const;
private:
    NSComboBox* m_comboBox;
};
#endif

#endif // _WX_OSX_COCOA_PRIVATE_TEXTIMPL_H_
