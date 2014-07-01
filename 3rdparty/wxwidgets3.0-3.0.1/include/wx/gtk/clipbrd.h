/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/clipbrd.h
// Purpose:     wxClipboard for wxGTK
// Author:      Robert Roebling, Vadim Zeitlin
// Copyright:   (c) 1998 Robert Roebling
//              (c) 2007 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_CLIPBOARD_H_
#define _WX_GTK_CLIPBOARD_H_

// ----------------------------------------------------------------------------
// wxClipboard
// ----------------------------------------------------------------------------

#include "wx/weakref.h"

class WXDLLIMPEXP_CORE wxClipboard : public wxClipboardBase
{
public:
    // there are several clipboards in X11 (and in GDK)
    enum Kind
    {
        Primary,
        Clipboard
    };

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

    // ask if data in correct format is available
    virtual bool IsSupportedAsync( wxEvtHandler *sink );

    // fill data with data on the clipboard (if available)
    virtual bool GetData( wxDataObject& data );

    // clears wxTheClipboard and the system's clipboard if possible
    virtual void Clear();



    // implementation from now on
    // --------------------------

    // get our clipboard item (depending on m_usePrimary value)
    GdkAtom GTKGetClipboardAtom() const;

    // get the data object currently being requested
    wxDataObject *GTKGetDataObject( GdkAtom atom );

    // clear the data for the given clipboard kind
    void GTKClearData(Kind kind);

    // called when selection data is received
    void GTKOnSelectionReceived(const GtkSelectionData& sel);

    // called when available target information is received
    bool GTKOnTargetReceived(const wxDataFormat& format);

private:
    // the data object for the specific selection
    wxDataObject *& Data(Kind kind)
    {
        return kind == Primary ? m_dataPrimary : m_dataClipboard;
    }

    // the data object we're currently using
    wxDataObject *& Data()
    {
        return Data(m_usePrimary ? Primary : Clipboard);
    }


    // set or unset selection ownership
    bool SetSelectionOwner(bool set = true);

    // add atom to the list of supported targets
    void AddSupportedTarget(GdkAtom atom);

    // check if the given format is supported
    bool DoIsSupported(const wxDataFormat& format);


    // both of these pointers can be non-NULL simultaneously but we only use
    // one of them at any moment depending on m_usePrimary value, use Data()
    // (from inside) or GTKGetDataObject() (from outside) accessors
    wxDataObject *m_dataPrimary,
                 *m_dataClipboard;

    // this is used to temporarily hold the object passed to our GetData() so
    // that GTK callbacks could access it
    wxDataObject *m_receivedData;

    // used to pass information about the format we need from DoIsSupported()
    // to GTKOnTargetReceived()
    GdkAtom m_targetRequested;

    GtkWidget *m_clipboardWidget;  // for getting and offering data
    GtkWidget *m_targetsWidget;    // for getting list of supported formats

    // ID of the connection to "selection_get" signal, initially 0.
    unsigned long m_idSelectionGetHandler;

    bool m_open;
    bool m_formatSupported;

public:
    // async stuff
    wxEvtHandlerRef    m_sink;
private:
    GtkWidget         *m_targetsWidgetAsync;  // for getting list of supported formats

    DECLARE_DYNAMIC_CLASS(wxClipboard)
};

#endif // _WX_GTK_CLIPBOARD_H_
