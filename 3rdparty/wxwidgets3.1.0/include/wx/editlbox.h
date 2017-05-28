/////////////////////////////////////////////////////////////////////////////
// Name:        wx/editlbox.h
// Purpose:     ListBox with editable items
// Author:      Vaclav Slavik
// Copyright:   (c) Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////


#ifndef __WX_EDITLBOX_H__
#define __WX_EDITLBOX_H__

#include "wx/defs.h"

#if wxUSE_EDITABLELISTBOX

#include "wx/panel.h"

class WXDLLIMPEXP_FWD_CORE wxBitmapButton;
class WXDLLIMPEXP_FWD_CORE wxListCtrl;
class WXDLLIMPEXP_FWD_CORE wxListEvent;

#define wxEL_ALLOW_NEW          0x0100
#define wxEL_ALLOW_EDIT         0x0200
#define wxEL_ALLOW_DELETE       0x0400
#define wxEL_NO_REORDER         0x0800
#define wxEL_DEFAULT_STYLE      (wxEL_ALLOW_NEW | wxEL_ALLOW_EDIT | wxEL_ALLOW_DELETE)

extern WXDLLIMPEXP_DATA_ADV(const char) wxEditableListBoxNameStr[];

// This class provides a composite control that lets the
// user easily enter list of strings

class WXDLLIMPEXP_ADV wxEditableListBox : public wxPanel
{
public:
    wxEditableListBox() { Init(); }

    wxEditableListBox(wxWindow *parent, wxWindowID id,
                      const wxString& label,
                      const wxPoint& pos = wxDefaultPosition,
                      const wxSize& size = wxDefaultSize,
                      long style = wxEL_DEFAULT_STYLE,
                      const wxString& name = wxEditableListBoxNameStr)
    {
        Init();
        Create(parent, id, label, pos, size, style, name);
    }

    bool Create(wxWindow *parent, wxWindowID id,
                const wxString& label,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxEL_DEFAULT_STYLE,
                const wxString& name = wxEditableListBoxNameStr);

    void SetStrings(const wxArrayString& strings);
    void GetStrings(wxArrayString& strings) const;

    wxListCtrl* GetListCtrl()       { return m_listCtrl; }
    wxBitmapButton* GetDelButton()  { return m_bDel; }
    wxBitmapButton* GetNewButton()  { return m_bNew; }
    wxBitmapButton* GetUpButton()   { return m_bUp; }
    wxBitmapButton* GetDownButton() { return m_bDown; }
    wxBitmapButton* GetEditButton() { return m_bEdit; }

protected:
    wxBitmapButton *m_bDel, *m_bNew, *m_bUp, *m_bDown, *m_bEdit;
    wxListCtrl *m_listCtrl;
    int m_selection;
    long m_style;

    void Init()
    {
        m_style = 0;
        m_selection = 0;
        m_bEdit = m_bNew = m_bDel = m_bUp = m_bDown = NULL;
        m_listCtrl = NULL;
    }

    void OnItemSelected(wxListEvent& event);
    void OnEndLabelEdit(wxListEvent& event);
    void OnNewItem(wxCommandEvent& event);
    void OnDelItem(wxCommandEvent& event);
    void OnEditItem(wxCommandEvent& event);
    void OnUpItem(wxCommandEvent& event);
    void OnDownItem(wxCommandEvent& event);

    wxDECLARE_CLASS(wxEditableListBox);
    wxDECLARE_EVENT_TABLE();

private:
    void SwapItems(long i1, long i2);

};

#endif  // wxUSE_EDITABLELISTBOX

#endif // __WX_EDITLBOX_H__
