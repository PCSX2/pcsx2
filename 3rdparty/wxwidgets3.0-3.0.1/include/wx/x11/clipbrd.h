/////////////////////////////////////////////////////////////////////////////
// Name:        wx/x11/clipbrd.h
// Purpose:     Clipboard functionality.
// Author:      Robert Roebling
// Created:     17/09/98
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_X11_CLIPBRD_H_
#define _WX_X11_CLIPBRD_H_

#if wxUSE_CLIPBOARD

#include "wx/object.h"
#include "wx/list.h"
#include "wx/dataobj.h"
#include "wx/control.h"
#include "wx/module.h"

// ----------------------------------------------------------------------------
// wxClipboard
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxClipboard : public wxClipboardBase
{
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

    // implementation from now on
    bool              m_open;
    bool              m_ownsClipboard;
    bool              m_ownsPrimarySelection;
    wxDataObject     *m_data;

    WXWindow          m_clipboardWidget;  /* for getting and offering data */
    WXWindow          m_targetsWidget;    /* for getting list of supported formats */
    bool              m_waiting;          /* querying data or formats is asynchronous */

    bool              m_formatSupported;
    Atom              m_targetRequested;
    wxDataObject     *m_receivedData;

private:
    DECLARE_DYNAMIC_CLASS(wxClipboard)
};

#endif // wxUSE_CLIPBOARD

#endif // _WX_X11_CLIPBRD_H_
