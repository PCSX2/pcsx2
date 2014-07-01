/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/bmpcbox.h
// Purpose:     wxBitmapComboBox
// Author:      Jaakko Salli
// Created:     2008-05-19
// Copyright:   (c) 2008 Jaakko Salli
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_BMPCBOX_H_
#define _WX_GTK_BMPCBOX_H_


#include "wx/combobox.h"

// ----------------------------------------------------------------------------
// wxBitmapComboBox: a wxComboBox that allows images to be shown
// in front of string items.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxBitmapComboBox : public wxComboBox,
                                         public wxBitmapComboBoxBase
{
public:
    // ctors and such
    wxBitmapComboBox() : wxComboBox(), wxBitmapComboBoxBase()
    {
        Init();
    }

    wxBitmapComboBox(wxWindow *parent,
                     wxWindowID id = wxID_ANY,
                     const wxString& value = wxEmptyString,
                     const wxPoint& pos = wxDefaultPosition,
                     const wxSize& size = wxDefaultSize,
                     int n = 0,
                     const wxString choices[] = NULL,
                     long style = 0,
                     const wxValidator& validator = wxDefaultValidator,
                     const wxString& name = wxBitmapComboBoxNameStr)
        : wxComboBox(),
          wxBitmapComboBoxBase()
    {
        Init();

        (void)Create(parent, id, value, pos, size, n,
                     choices, style, validator, name);
    }

    wxBitmapComboBox(wxWindow *parent,
                     wxWindowID id,
                     const wxString& value,
                     const wxPoint& pos,
                     const wxSize& size,
                     const wxArrayString& choices,
                     long style,
                     const wxValidator& validator = wxDefaultValidator,
                     const wxString& name = wxBitmapComboBoxNameStr);

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& value,
                const wxPoint& pos,
                const wxSize& size,
                int n,
                const wxString choices[],
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxBitmapComboBoxNameStr);

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxString& value,
                const wxPoint& pos,
                const wxSize& size,
                const wxArrayString& choices,
                long style = 0,
                const wxValidator& validator = wxDefaultValidator,
                const wxString& name = wxBitmapComboBoxNameStr);

    virtual ~wxBitmapComboBox();

    // Sets the image for the given item.
    virtual void SetItemBitmap(unsigned int n, const wxBitmap& bitmap);

    // Returns the image of the item with the given index.
    virtual wxBitmap GetItemBitmap(unsigned int n) const;

    // Returns size of the image used in list
    virtual wxSize GetBitmapSize() const
    {
        return m_bitmapSize;
    }

    // Adds item with image to the end of the combo box.
    int Append(const wxString& item, const wxBitmap& bitmap = wxNullBitmap);
    int Append(const wxString& item, const wxBitmap& bitmap, void *clientData);
    int Append(const wxString& item, const wxBitmap& bitmap, wxClientData *clientData);

    // Inserts item with image into the list before pos. Not valid for wxCB_SORT
    // styles, use Append instead.
    int Insert(const wxString& item, const wxBitmap& bitmap, unsigned int pos);
    int Insert(const wxString& item, const wxBitmap& bitmap,
               unsigned int pos, void *clientData);
    int Insert(const wxString& item, const wxBitmap& bitmap,
               unsigned int pos, wxClientData *clientData);

    // Override some wxTextEntry interface.
    virtual void WriteText(const wxString& value);

    virtual wxString GetValue() const;
    virtual void Remove(long from, long to);

    virtual void SetInsertionPoint(long pos);
    virtual long GetInsertionPoint() const;
    virtual long GetLastPosition() const;

    virtual void SetSelection(long from, long to);
    virtual void GetSelection(long *from, long *to) const;

    virtual void SetSelection(int n) { wxComboBox::SetSelection(n); }
    virtual int GetSelection() const { return wxComboBox::GetSelection(); }

    virtual bool IsEditable() const;
    virtual void SetEditable(bool editable);

    virtual GtkWidget* GetConnectWidget();

protected:
    virtual GdkWindow *GTKGetWindow(wxArrayGdkWindows& windows) const;

    virtual void GTKCreateComboBoxWidget();
    virtual void GTKInsertComboBoxTextItem( unsigned int n, const wxString& text );

    virtual wxSize DoGetBestSize() const;

    wxSize                  m_bitmapSize;
    int                     m_bitmapCellIndex;

private:
    void Init();

    DECLARE_DYNAMIC_CLASS(wxBitmapComboBox)
};

#endif // _WX_GTK_BMPCBOX_H_
