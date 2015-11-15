/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/clipbrd.h
// Purpose:     wxClipboad class and clipboard functions for MSW
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CLIPBRD_H_
#define _WX_CLIPBRD_H_

#if wxUSE_CLIPBOARD

// These functions superceded by wxClipboard, but retained in order to
// implement wxClipboard, and for compatibility.

// open/close the clipboard
WXDLLIMPEXP_CORE bool wxOpenClipboard();
WXDLLIMPEXP_CORE bool wxIsClipboardOpened();
#define wxClipboardOpen wxIsClipboardOpened
WXDLLIMPEXP_CORE bool wxCloseClipboard();

// get/set data
WXDLLIMPEXP_CORE bool wxEmptyClipboard();
WXDLLIMPEXP_CORE bool wxSetClipboardData(wxDataFormat dataFormat,
                                    const void *data,
                                    int width = 0, int height = 0);
WXDLLIMPEXP_CORE void* wxGetClipboardData(wxDataFormat dataFormat,
                                     long *len = NULL);

// clipboard formats
WXDLLIMPEXP_CORE bool wxIsClipboardFormatAvailable(wxDataFormat dataFormat);
WXDLLIMPEXP_CORE wxDataFormat wxEnumClipboardFormats(wxDataFormat dataFormat);
WXDLLIMPEXP_CORE int  wxRegisterClipboardFormat(wxChar *formatName);
WXDLLIMPEXP_CORE bool wxGetClipboardFormatName(wxDataFormat dataFormat,
                                          wxChar *formatName,
                                          int maxCount);

//-----------------------------------------------------------------------------
// wxClipboard
//-----------------------------------------------------------------------------

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

    // flushes the clipboard: this means that the data which is currently on
    // clipboard will stay available even after the application exits (possibly
    // eating memory), otherwise the clipboard will be emptied on exit
    virtual bool Flush();

private:
    IDataObject *m_lastDataObject;
    bool m_isOpened;

    DECLARE_DYNAMIC_CLASS(wxClipboard)
};

#endif // wxUSE_CLIPBOARD

#endif // _WX_CLIPBRD_H_
