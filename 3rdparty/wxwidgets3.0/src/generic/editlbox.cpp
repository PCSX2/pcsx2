/////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/editlbox.cpp
// Purpose:     ListBox with editable items
// Author:      Vaclav Slavik
// Copyright:   (c) Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_EDITABLELISTBOX

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWidgets headers)
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "wx/editlbox.h"
#include "wx/sizer.h"
#include "wx/listctrl.h"

// ============================================================================
// implementation
// ============================================================================

const char wxEditableListBoxNameStr[] = "editableListBox";

static const char* const eledit_xpm[] = {
"16 16 3 1",
"   c None",
".  c #000000",
"+  c #00007F",
"                ",
"                ",
"      .. ..     ",
"        .       ",
"        .       ",
"  ++++  .  ++++ ",
"     ++ . ++  ++",
"  +++++ . ++++++",
" ++  ++ . ++    ",
" ++  ++ . ++  ++",
"  +++++ .  ++++ ",
"        .       ",
"        .       ",
"      .. ..     ",
"                ",
"                "};

static const char* const elnew_xpm[] = {
"16 16 5 1",
"   c None",
".  c #7F7F7F",
"+  c #FFFFFF",
"@  c #FFFF00",
"#  c #000000",
"                ",
"                ",
" .  .+ .@       ",
"  . .@.@# # #   ",
"  @.@+....   #  ",
" ... @@         ",
"  @ . @.     #  ",
" .# .@          ",
"    .        #  ",
"  #             ",
"             #  ",
"  #             ",
"             #  ",
"  # # # # # #   ",
"                ",
"                "};

static const char* const eldel_xpm[] = {
"16 16 3 1",
"   c None",
".  c #7F0000",
"+  c #FFFFFF",
"                ",
"                ",
"                ",
" ..+        ..+ ",
" ....+     ..+  ",
"  ....+   ..+   ",
"    ...+ .+     ",
"     .....+     ",
"      ...+      ",
"     .....+     ",
"    ...+ ..+    ",
"   ...+   ..+   ",
"  ...+     .+   ",
"  ...+      .+  ",
"   .         .  ",
"                "};

static const char* const eldown_xpm[] = {
"16 16 2 1",
"   c None",
".  c #000000",
"                ",
"                ",
"         ...    ",
"        ...     ",
"       ...      ",
"       ...      ",
"       ...      ",
"       ...      ",
"   ...........  ",
"    .........   ",
"     .......    ",
"      .....     ",
"       ...      ",
"        .       ",
"                ",
"                "};

static const char* const elup_xpm[] = {
"16 16 2 1",
"   c None",
".  c #000000",
"                ",
"        .       ",
"       ...      ",
"      .....     ",
"     .......    ",
"    .........   ",
"   ...........  ",
"       ...      ",
"       ...      ",
"       ...      ",
"       ...      ",
"      ...       ",
"     ...        ",
"                ",
"                ",
"                "};

// list control with auto-resizable column:
class CleverListCtrl : public wxListCtrl
{
public:
   CleverListCtrl(wxWindow *parent,
                  wxWindowID id = wxID_ANY,
                  const wxPoint &pos = wxDefaultPosition,
                  const wxSize &size = wxDefaultSize,
                  long style = wxLC_ICON,
                  const wxValidator& validator = wxDefaultValidator,
                  const wxString &name = wxListCtrlNameStr)
         : wxListCtrl(parent, id, pos, size, style, validator, name)
    {
        CreateColumns();
    }

    void CreateColumns()
    {
        InsertColumn(0, wxT("item"));
        SizeColumns();
    }

    void SizeColumns()
    {
         int w = GetSize().x;
#ifdef __WXMSW__
         w -= wxSystemSettings::GetMetric(wxSYS_VSCROLL_X) + 6;
#else
         w -= 2*wxSystemSettings::GetMetric(wxSYS_VSCROLL_X);
#endif
         if (w < 0) w = 0;
         SetColumnWidth(0, w);
    }

private:
    DECLARE_EVENT_TABLE()
    void OnSize(wxSizeEvent& event)
    {
        SizeColumns();
        event.Skip();
    }
};

BEGIN_EVENT_TABLE(CleverListCtrl, wxListCtrl)
   EVT_SIZE(CleverListCtrl::OnSize)
END_EVENT_TABLE()


// ----------------------------------------------------------------------------
// wxEditableListBox
// ----------------------------------------------------------------------------

IMPLEMENT_CLASS(wxEditableListBox, wxPanel)

// NB: generate the IDs at runtime to avoid conflict with XRCID values,
//     they could cause XRCCTRL() failures in XRC-based dialogs
const wxWindowIDRef wxID_ELB_DELETE = wxWindow::NewControlId();
const wxWindowIDRef wxID_ELB_EDIT = wxWindow::NewControlId();
const wxWindowIDRef wxID_ELB_NEW = wxWindow::NewControlId();
const wxWindowIDRef wxID_ELB_UP = wxWindow::NewControlId();
const wxWindowIDRef wxID_ELB_DOWN = wxWindow::NewControlId();
const wxWindowIDRef wxID_ELB_LISTCTRL = wxWindow::NewControlId();

BEGIN_EVENT_TABLE(wxEditableListBox, wxPanel)
    EVT_LIST_ITEM_SELECTED(wxID_ELB_LISTCTRL, wxEditableListBox::OnItemSelected)
    EVT_LIST_END_LABEL_EDIT(wxID_ELB_LISTCTRL, wxEditableListBox::OnEndLabelEdit)
    EVT_BUTTON(wxID_ELB_NEW, wxEditableListBox::OnNewItem)
    EVT_BUTTON(wxID_ELB_UP, wxEditableListBox::OnUpItem)
    EVT_BUTTON(wxID_ELB_DOWN, wxEditableListBox::OnDownItem)
    EVT_BUTTON(wxID_ELB_EDIT, wxEditableListBox::OnEditItem)
    EVT_BUTTON(wxID_ELB_DELETE, wxEditableListBox::OnDelItem)
END_EVENT_TABLE()

bool wxEditableListBox::Create(wxWindow *parent, wxWindowID id,
                          const wxString& label,
                          const wxPoint& pos, const wxSize& size,
                          long style,
                          const wxString& name)
{
    if (!wxPanel::Create(parent, id, pos, size, wxTAB_TRAVERSAL, name))
        return false;

    m_style = style;

    wxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    wxPanel *subp = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                wxSUNKEN_BORDER | wxTAB_TRAVERSAL);
    wxSizer *subsizer = new wxBoxSizer(wxHORIZONTAL);
    subsizer->Add(new wxStaticText(subp, wxID_ANY, label), 1, wxALIGN_CENTRE_VERTICAL | wxLEFT, 4);

#ifdef __WXMSW__
    #define BTN_BORDER 4
    // FIXME - why is this needed? There's some reason why sunken border is
    //         ignored by sizers in wxMSW but not in wxGTK that I can't
    //         figure out...
#else
    #define BTN_BORDER 0
#endif

    if ( m_style & wxEL_ALLOW_EDIT )
    {
        m_bEdit = new wxBitmapButton(subp, wxID_ELB_EDIT, wxBitmap(eledit_xpm));
        subsizer->Add(m_bEdit, 0, wxALIGN_CENTRE_VERTICAL | wxTOP | wxBOTTOM, BTN_BORDER);
    }

    if ( m_style & wxEL_ALLOW_NEW )
    {
        m_bNew = new wxBitmapButton(subp, wxID_ELB_NEW, wxBitmap(elnew_xpm));
        subsizer->Add(m_bNew, 0, wxALIGN_CENTRE_VERTICAL | wxTOP | wxBOTTOM, BTN_BORDER);
    }

    if ( m_style & wxEL_ALLOW_DELETE )
    {
        m_bDel = new wxBitmapButton(subp, wxID_ELB_DELETE, wxBitmap(eldel_xpm));
        subsizer->Add(m_bDel, 0, wxALIGN_CENTRE_VERTICAL | wxTOP | wxBOTTOM, BTN_BORDER);
    }

    if (!(m_style & wxEL_NO_REORDER))
    {
        m_bUp = new wxBitmapButton(subp, wxID_ELB_UP, wxBitmap(elup_xpm));
        subsizer->Add(m_bUp, 0, wxALIGN_CENTRE_VERTICAL | wxTOP | wxBOTTOM, BTN_BORDER);

        m_bDown = new wxBitmapButton(subp, wxID_ELB_DOWN, wxBitmap(eldown_xpm));
        subsizer->Add(m_bDown, 0, wxALIGN_CENTRE_VERTICAL | wxTOP | wxBOTTOM, BTN_BORDER);
    }

#if wxUSE_TOOLTIPS
    if ( m_bEdit ) m_bEdit->SetToolTip(_("Edit item"));
    if ( m_bNew ) m_bNew->SetToolTip(_("New item"));
    if ( m_bDel ) m_bDel->SetToolTip(_("Delete item"));
    if ( m_bUp ) m_bUp->SetToolTip(_("Move up"));
    if ( m_bDown ) m_bDown->SetToolTip(_("Move down"));
#endif

    subp->SetSizer(subsizer);
    subsizer->Fit(subp);

    sizer->Add(subp, 0, wxEXPAND);

    long st = wxLC_REPORT | wxLC_NO_HEADER | wxLC_SINGLE_SEL | wxSUNKEN_BORDER;
    if ( style & wxEL_ALLOW_EDIT )
         st |= wxLC_EDIT_LABELS;
    m_listCtrl = new CleverListCtrl(this, wxID_ELB_LISTCTRL,
                                    wxDefaultPosition, wxDefaultSize, st);
    wxArrayString empty_ar;
    SetStrings(empty_ar);

    sizer->Add(m_listCtrl, 1, wxEXPAND);

    SetSizer(sizer);
    Layout();

    return true;
}

void wxEditableListBox::SetStrings(const wxArrayString& strings)
{
    m_listCtrl->DeleteAllItems();
    size_t i;

    for (i = 0; i < strings.GetCount(); i++)
        m_listCtrl->InsertItem(i, strings[i]);

    m_listCtrl->InsertItem(strings.GetCount(), wxEmptyString);
    m_listCtrl->SetItemState(0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
}

void wxEditableListBox::GetStrings(wxArrayString& strings) const
{
    strings.Clear();

    for (int i = 0; i < m_listCtrl->GetItemCount()-1; i++)
        strings.Add(m_listCtrl->GetItemText(i));
}

void wxEditableListBox::OnItemSelected(wxListEvent& event)
{
    m_selection = event.GetIndex();
    if (!(m_style & wxEL_NO_REORDER))
    {
        m_bUp->Enable(m_selection != 0 && m_selection < m_listCtrl->GetItemCount()-1);
        m_bDown->Enable(m_selection < m_listCtrl->GetItemCount()-2);
    }

    if (m_style & wxEL_ALLOW_EDIT)
        m_bEdit->Enable(m_selection < m_listCtrl->GetItemCount()-1);
    if (m_style & wxEL_ALLOW_DELETE)
        m_bDel->Enable(m_selection < m_listCtrl->GetItemCount()-1);
}

void wxEditableListBox::OnNewItem(wxCommandEvent& WXUNUSED(event))
{
    m_listCtrl->SetItemState(m_listCtrl->GetItemCount()-1,
                             wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    m_listCtrl->EditLabel(m_selection);
}

void wxEditableListBox::OnEndLabelEdit(wxListEvent& event)
{
    if ( event.GetIndex() == m_listCtrl->GetItemCount()-1 &&
         !event.GetText().empty() )
    {
        // The user edited last (empty) line, i.e. added new entry. We have to
        // add new empty line here so that adding one more line is still
        // possible:
        m_listCtrl->InsertItem(m_listCtrl->GetItemCount(), wxEmptyString);

        // Simulate a wxEVT_LIST_ITEM_SELECTED event for the new item,
        // so that the buttons are enabled/disabled properly
        wxListEvent selectionEvent(wxEVT_LIST_ITEM_SELECTED, m_listCtrl->GetId());
        selectionEvent.m_itemIndex = event.GetIndex();
        m_listCtrl->GetEventHandler()->ProcessEvent(selectionEvent);
    }
}

void wxEditableListBox::OnDelItem(wxCommandEvent& WXUNUSED(event))
{
    m_listCtrl->DeleteItem(m_selection);
    m_listCtrl->SetItemState(m_selection,
                             wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
}

void wxEditableListBox::OnEditItem(wxCommandEvent& WXUNUSED(event))
{
    m_listCtrl->EditLabel(m_selection);
}

void wxEditableListBox::SwapItems(long i1, long i2)
{
    // swap the text
    wxString t1 = m_listCtrl->GetItemText(i1);
    wxString t2 = m_listCtrl->GetItemText(i2);
    m_listCtrl->SetItemText(i1, t2);
    m_listCtrl->SetItemText(i2, t1);

    // swap the item data
    wxUIntPtr d1 = m_listCtrl->GetItemData(i1);
    wxUIntPtr d2 = m_listCtrl->GetItemData(i2);
    m_listCtrl->SetItemPtrData(i1, d2);
    m_listCtrl->SetItemPtrData(i2, d1);
}


void wxEditableListBox::OnUpItem(wxCommandEvent& WXUNUSED(event))
{
    SwapItems(m_selection - 1, m_selection);
    m_listCtrl->SetItemState(m_selection - 1,
                             wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
}

void wxEditableListBox::OnDownItem(wxCommandEvent& WXUNUSED(event))
{
    SwapItems(m_selection + 1, m_selection);
    m_listCtrl->SetItemState(m_selection + 1,
                             wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
}

#endif // wxUSE_EDITABLELISTBOX
