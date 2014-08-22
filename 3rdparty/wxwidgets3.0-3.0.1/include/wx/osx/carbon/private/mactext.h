/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/carbon/private/mactext.h
// Purpose:     private wxMacTextControl base class
// Author:      Stefan Csomor
// Modified by:
// Created:     03/02/99
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MAC_PRIVATE_MACTEXT_H_
#define _WX_MAC_PRIVATE_MACTEXT_H_

#include "wx/osx/private.h"

// implementation exposed, so that search control can pull it

class wxMacUnicodeTextControl : public wxMacControl, public wxTextWidgetImpl
{
public :
    wxMacUnicodeTextControl( wxTextCtrl *wxPeer ) ;
    wxMacUnicodeTextControl( wxTextCtrl *wxPeer,
                             const wxString& str,
                             const wxPoint& pos,
                             const wxSize& size, long style ) ;
    virtual ~wxMacUnicodeTextControl();

    virtual bool CanFocus() const
                  { return true; }
    virtual void VisibilityChanged(bool shown);
    virtual wxString GetStringValue() const ;
    virtual void SetStringValue( const wxString &str) ;
    virtual void Copy();
    virtual void Cut();
    virtual void Paste();
    virtual bool CanPaste() const;
    virtual void SetEditable(bool editable) ;
    virtual void GetSelection( long* from, long* to) const ;
    virtual void SetSelection( long from , long to ) ;
    virtual void WriteText(const wxString& str) ;

protected :
    void    InstallEventHandlers();

    // contains the tag for the content (is different for password and non-password controls)
    OSType m_valueTag ;
    WXEVENTHANDLERREF    m_macTextCtrlEventHandler ;
public :
    ControlEditTextSelectionRec m_selection ;
};

#endif // _WX_MAC_PRIVATE_MACTEXT_H_
