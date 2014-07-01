/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/clipbrd.h
// Purpose:     wxClipboard
// Author:      David Elliott <dfe@cox.net>
// Modified by:
// Created:     2003/07/23
// Copyright:   (c) 2003 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef __WX_COCOA_CLIPBRD_H__
#define __WX_COCOA_CLIPBRD_H__

#include "wx/dataobj.h"

//=========================================================================
// wxClipboard
//=========================================================================
class wxClipboard : public wxClipboardBase
{
    DECLARE_DYNAMIC_CLASS(wxClipboard)
public:
    wxClipboard();
    virtual ~wxClipboard();

    // open the clipboard before SetData() and GetData()
    virtual bool Open();

    // close the clipboard after SetData() and GetData()
    virtual void Close();

    // query whether the clipboard is opened
    virtual bool IsOpened() const;

    // set the clipboard data. all other formats will be deleted.
    virtual bool SetData( wxDataObject *data );

    // add to the clipboard data.
    virtual bool AddData( wxDataObject *data );

    // ask if data in correct format is available
    virtual bool IsSupported( const wxDataFormat& format );

    // fill data with data on the clipboard (if available)
    virtual bool GetData( wxDataObject& data );

    // clears wxTheClipboard and the system's clipboard if possible
    virtual void Clear();
};

#endif //__WX_COCOA_CLIPBRD_H__
