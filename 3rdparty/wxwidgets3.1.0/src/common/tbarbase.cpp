/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/tbarbase.cpp
// Purpose:     wxToolBarBase implementation
// Author:      Julian Smart
// Modified by: VZ at 11.12.99 (wxScrollableToolBar split off)
// Created:     04/01/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_TOOLBAR

#include "wx/toolbar.h"

#ifndef WX_PRECOMP
    #include "wx/control.h"
    #include "wx/frame.h"
    #include "wx/settings.h"
    #if WXWIN_COMPATIBILITY_2_8
        #include "wx/image.h"
    #endif // WXWIN_COMPATIBILITY_2_8
    #include "wx/menu.h"
#endif

extern WXDLLEXPORT_DATA(const char) wxToolBarNameStr[] = "toolbar";

// ----------------------------------------------------------------------------
// wxWidgets macros
// ----------------------------------------------------------------------------

wxBEGIN_EVENT_TABLE(wxToolBarBase, wxControl)
wxEND_EVENT_TABLE()

#include "wx/listimpl.cpp"

WX_DEFINE_LIST(wxToolBarToolsList)

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxToolBarToolBase
// ----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxToolBarToolBase, wxObject);

wxToolBarToolBase::~wxToolBarToolBase()
{
#if wxUSE_MENUS
    delete m_dropdownMenu;
#endif

    if ( IsControl() )
        GetControl()->Destroy();
}


bool wxToolBarToolBase::Enable(bool enable)
{
    if ( m_enabled == enable )
        return false;

    m_enabled = enable;

    return true;
}

bool wxToolBarToolBase::Toggle(bool toggle)
{
    wxASSERT_MSG( CanBeToggled(), wxT("can't toggle this tool") );

    if ( m_toggled == toggle )
        return false;

    m_toggled = toggle;

    return true;
}

bool wxToolBarToolBase::SetToggle(bool toggle)
{
    wxItemKind kind = toggle ? wxITEM_CHECK : wxITEM_NORMAL;
    if ( m_kind == kind )
        return false;

    m_kind = kind;

    return true;
}

bool wxToolBarToolBase::SetShortHelp(const wxString& help)
{
    if ( m_shortHelpString == help )
        return false;

    m_shortHelpString = help;

    return true;
}

bool wxToolBarToolBase::SetLongHelp(const wxString& help)
{
    if ( m_longHelpString == help )
        return false;

    m_longHelpString = help;

    return true;
}


#if wxUSE_MENUS
void wxToolBarToolBase::SetDropdownMenu(wxMenu* menu)
{
    delete m_dropdownMenu;
    m_dropdownMenu = menu;
}
#endif


// ----------------------------------------------------------------------------
// wxToolBarBase adding/deleting items
// ----------------------------------------------------------------------------

wxToolBarBase::wxToolBarBase()
{
    // the list owns the pointers
    m_xMargin = m_yMargin = 0;
    m_maxRows = m_maxCols = 0;
    m_toolPacking = m_toolSeparation = 0;
    m_defaultWidth = 16;
    m_defaultHeight = 15;
}

void wxToolBarBase::FixupStyle()
{
    if ( !HasFlag(wxTB_TOP | wxTB_LEFT | wxTB_RIGHT | wxTB_BOTTOM) )
    {
        // this is the default
        m_windowStyle |= wxTB_TOP;
    }
}

wxToolBarToolBase *wxToolBarBase::DoAddTool(int toolid,
                                            const wxString& label,
                                            const wxBitmap& bitmap,
                                            const wxBitmap& bmpDisabled,
                                            wxItemKind kind,
                                            const wxString& shortHelp,
                                            const wxString& longHelp,
                                            wxObject *clientData,
                                            wxCoord WXUNUSED(xPos),
                                            wxCoord WXUNUSED(yPos))
{
    InvalidateBestSize();
    return InsertTool(GetToolsCount(), toolid, label, bitmap, bmpDisabled,
                      kind, shortHelp, longHelp, clientData);
}

wxToolBarToolBase *wxToolBarBase::InsertTool(size_t pos,
                                             int toolid,
                                             const wxString& label,
                                             const wxBitmap& bitmap,
                                             const wxBitmap& bmpDisabled,
                                             wxItemKind kind,
                                             const wxString& shortHelp,
                                             const wxString& longHelp,
                                             wxObject *clientData)
{
    wxCHECK_MSG( pos <= GetToolsCount(), NULL,
                 wxT("invalid position in wxToolBar::InsertTool()") );

    return DoInsertNewTool(pos, CreateTool(toolid, label, bitmap, bmpDisabled, kind,
                                           clientData, shortHelp, longHelp));
}

wxToolBarToolBase *wxToolBarBase::AddTool(wxToolBarToolBase *tool)
{
    return InsertTool(GetToolsCount(), tool);
}

wxToolBarToolBase *
wxToolBarBase::InsertTool(size_t pos, wxToolBarToolBase *tool)
{
    wxCHECK_MSG( pos <= GetToolsCount(), NULL,
                 wxT("invalid position in wxToolBar::InsertTool()") );

    if ( !tool || !DoInsertTool(pos, tool) )
    {
        return NULL;
    }

    m_tools.Insert(pos, tool);
    tool->Attach(this);

    return tool;
}

wxToolBarToolBase *
wxToolBarBase::AddControl(wxControl *control, const wxString& label)
{
    return InsertControl(GetToolsCount(), control, label);
}

wxToolBarToolBase *
wxToolBarBase::InsertControl(size_t pos,
                             wxControl *control,
                             const wxString& label)
{
    wxCHECK_MSG( control, NULL,
                 wxT("toolbar: can't insert NULL control") );

    wxCHECK_MSG( control->GetParent() == this, NULL,
                 wxT("control must have toolbar as parent") );

    return DoInsertNewTool(pos, CreateTool(control, label));
}

wxControl *wxToolBarBase::FindControl( int toolid )
{
    for ( wxToolBarToolsList::compatibility_iterator node = m_tools.GetFirst();
          node;
          node = node->GetNext() )
    {
        const wxToolBarToolBase * const tool = node->GetData();
        if ( tool->IsControl() )
        {
            wxControl * const control = tool->GetControl();

            if ( !control )
            {
                wxFAIL_MSG( wxT("NULL control in toolbar?") );
            }
            else if ( control->GetId() == toolid )
            {
                // found
                return control;
            }
        }
    }

   return NULL;
}

wxToolBarToolBase *wxToolBarBase::AddSeparator()
{
    return InsertSeparator(GetToolsCount());
}

wxToolBarToolBase *wxToolBarBase::InsertSeparator(size_t pos)
{
    return DoInsertNewTool(pos, CreateSeparator());
}

wxToolBarToolBase *wxToolBarBase::AddStretchableSpace()
{
    return InsertStretchableSpace(GetToolsCount());
}

wxToolBarToolBase *wxToolBarBase::InsertStretchableSpace(size_t pos)
{
    wxToolBarToolBase * const tool = CreateSeparator();
    if ( tool )
    {
        // this is a hack but we know that all the current implementations
        // don't really use the tool when it's created, they will do it
        // InsertTool() at earliest and maybe even in Realize() much later
        //
        // so we can create the tool as a plain separator and mark it as being
        // a stretchable space later
        tool->MakeStretchable();
    }

    return DoInsertNewTool(pos, tool);
}

wxToolBarToolBase *wxToolBarBase::RemoveTool(int toolid)
{
    size_t pos = 0;
    wxToolBarToolsList::compatibility_iterator node;
    for ( node = m_tools.GetFirst(); node; node = node->GetNext() )
    {
        if ( node->GetData()->GetId() == toolid )
            break;

        pos++;
    }

    if ( !node )
    {
        // don't give any error messages - sometimes we might call RemoveTool()
        // without knowing whether the tool is or not in the toolbar
        return NULL;
    }

    wxToolBarToolBase *tool = node->GetData();
    wxCHECK_MSG( tool, NULL, "NULL tool in the tools list?" );

    if ( !DoDeleteTool(pos, tool) )
        return NULL;

    m_tools.Erase(node);

    tool->Detach();

    return tool;
}

bool wxToolBarBase::DeleteToolByPos(size_t pos)
{
    wxCHECK_MSG( pos < GetToolsCount(), false,
                 wxT("invalid position in wxToolBar::DeleteToolByPos()") );

    wxToolBarToolsList::compatibility_iterator node = m_tools.Item(pos);

    if ( !DoDeleteTool(pos, node->GetData()) )
    {
        return false;
    }

    delete node->GetData();
    m_tools.Erase(node);

    return true;
}

bool wxToolBarBase::DeleteTool(int toolid)
{
    size_t pos = 0;
    wxToolBarToolsList::compatibility_iterator node;
    for ( node = m_tools.GetFirst(); node; node = node->GetNext() )
    {
        if ( node->GetData()->GetId() == toolid )
            break;

        pos++;
    }

    if ( !node || !DoDeleteTool(pos, node->GetData()) )
    {
        return false;
    }

    delete node->GetData();
    m_tools.Erase(node);

    return true;
}

wxToolBarToolBase *wxToolBarBase::FindById(int toolid) const
{
    wxToolBarToolBase *tool = NULL;

    for ( wxToolBarToolsList::compatibility_iterator node = m_tools.GetFirst();
          node;
          node = node->GetNext() )
    {
        tool = node->GetData();
        if ( tool->GetId() == toolid )
        {
            // found
            break;
        }

        tool = NULL;
    }

    return tool;
}

void wxToolBarBase::UnToggleRadioGroup(wxToolBarToolBase *tool)
{
    wxCHECK_RET( tool, wxT("NULL tool in wxToolBarTool::UnToggleRadioGroup") );

    if ( !tool->IsButton() || tool->GetKind() != wxITEM_RADIO )
        return;

    wxToolBarToolsList::compatibility_iterator node = m_tools.Find(tool);
    wxCHECK_RET( node, wxT("invalid tool in wxToolBarTool::UnToggleRadioGroup") );

    wxToolBarToolsList::compatibility_iterator nodeNext = node->GetNext();
    while ( nodeNext )
    {
        wxToolBarToolBase *toolNext = nodeNext->GetData();

        if ( !toolNext->IsButton() || toolNext->GetKind() != wxITEM_RADIO )
            break;

        if ( toolNext->Toggle(false) )
        {
            DoToggleTool(toolNext, false);
        }

        nodeNext = nodeNext->GetNext();
    }

    wxToolBarToolsList::compatibility_iterator nodePrev = node->GetPrevious();
    while ( nodePrev )
    {
        wxToolBarToolBase *toolNext = nodePrev->GetData();

        if ( !toolNext->IsButton() || toolNext->GetKind() != wxITEM_RADIO )
            break;

        if ( toolNext->Toggle(false) )
        {
            DoToggleTool(toolNext, false);
        }

        nodePrev = nodePrev->GetPrevious();
    }
}

void wxToolBarBase::ClearTools()
{
    while ( GetToolsCount() )
    {
        DeleteToolByPos(0);
    }
}

void wxToolBarBase::AdjustToolBitmapSize()
{
    if ( HasFlag(wxTB_NOICONS) )
    {
        SetToolBitmapSize(wxSize(0, 0));
        return;
    }

    const wxSize sizeOrig(m_defaultWidth, m_defaultHeight);

    wxSize sizeActual(sizeOrig);

    for ( wxToolBarToolsList::const_iterator i = m_tools.begin();
          i != m_tools.end();
          ++i )
    {
        const wxBitmap& bmp = (*i)->GetNormalBitmap();
        if ( bmp.IsOk() )
            sizeActual.IncTo(bmp.GetScaledSize());
    }

    if ( sizeActual != sizeOrig )
        SetToolBitmapSize(sizeActual);
}

bool wxToolBarBase::Realize()
{
    // check if we have anything to do
    if ( m_tools.empty() )
        return false;

    // make sure tool size is large enough for all bitmaps to fit in
    AdjustToolBitmapSize();

    return true;
}

wxToolBarBase::~wxToolBarBase()
{
    WX_CLEAR_LIST(wxToolBarToolsList, m_tools);

    // notify the frame that it doesn't have a tool bar any longer to avoid
    // dangling pointers
    wxFrame *frame = wxDynamicCast(GetParent(), wxFrame);
    if ( frame && frame->GetToolBar() == this )
    {
        frame->SetToolBar(NULL);
    }
}

// ----------------------------------------------------------------------------
// wxToolBarBase tools state
// ----------------------------------------------------------------------------

void wxToolBarBase::EnableTool(int toolid, bool enable)
{
    wxToolBarToolBase *tool = FindById(toolid);
    if ( tool )
    {
        if ( tool->Enable(enable) )
        {
            DoEnableTool(tool, enable);
        }
    }
}

void wxToolBarBase::ToggleTool(int toolid, bool toggle)
{
    wxToolBarToolBase *tool = FindById(toolid);
    if ( tool && tool->CanBeToggled() )
    {
        if ( tool->Toggle(toggle) )
        {
            UnToggleRadioGroup(tool);
            DoToggleTool(tool, toggle);
        }
    }
}

void wxToolBarBase::SetToggle(int toolid, bool toggle)
{
    wxToolBarToolBase *tool = FindById(toolid);
    if ( tool )
    {
        if ( tool->SetToggle(toggle) )
        {
            DoSetToggle(tool, toggle);
        }
    }
}

void wxToolBarBase::SetToolShortHelp(int toolid, const wxString& help)
{
    wxToolBarToolBase *tool = FindById(toolid);
    if ( tool )
    {
        (void)tool->SetShortHelp(help);
    }
}

void wxToolBarBase::SetToolLongHelp(int toolid, const wxString& help)
{
    wxToolBarToolBase *tool = FindById(toolid);
    if ( tool )
    {
        (void)tool->SetLongHelp(help);
    }
}

wxObject *wxToolBarBase::GetToolClientData(int toolid) const
{
    wxToolBarToolBase *tool = FindById(toolid);

    return tool ? tool->GetClientData() : NULL;
}

void wxToolBarBase::SetToolClientData(int toolid, wxObject *clientData)
{
    wxToolBarToolBase *tool = FindById(toolid);

    wxCHECK_RET( tool, wxT("no such tool in wxToolBar::SetToolClientData") );

    tool->SetClientData(clientData);
}

int wxToolBarBase::GetToolPos(int toolid) const
{
    size_t pos = 0;
    wxToolBarToolsList::compatibility_iterator node;

    for ( node = m_tools.GetFirst(); node; node = node->GetNext() )
    {
        if ( node->GetData()->GetId() == toolid )
            return pos;

        pos++;
    }

    return wxNOT_FOUND;
}

bool wxToolBarBase::GetToolState(int toolid) const
{
    wxToolBarToolBase *tool = FindById(toolid);
    wxCHECK_MSG( tool, false, wxT("no such tool") );

    return tool->IsToggled();
}

bool wxToolBarBase::GetToolEnabled(int toolid) const
{
    wxToolBarToolBase *tool = FindById(toolid);
    wxCHECK_MSG( tool, false, wxT("no such tool") );

    return tool->IsEnabled();
}

wxString wxToolBarBase::GetToolShortHelp(int toolid) const
{
    wxToolBarToolBase *tool = FindById(toolid);
    wxCHECK_MSG( tool, wxEmptyString, wxT("no such tool") );

    return tool->GetShortHelp();
}

wxString wxToolBarBase::GetToolLongHelp(int toolid) const
{
    wxToolBarToolBase *tool = FindById(toolid);
    wxCHECK_MSG( tool, wxEmptyString, wxT("no such tool") );

    return tool->GetLongHelp();
}

// ----------------------------------------------------------------------------
// wxToolBarBase geometry
// ----------------------------------------------------------------------------

void wxToolBarBase::SetMargins(int x, int y)
{
    m_xMargin = x;
    m_yMargin = y;
}

void wxToolBarBase::SetRows(int WXUNUSED(nRows))
{
    // nothing
}

bool wxToolBarBase::IsVertical() const
{
    return HasFlag(wxTB_LEFT | wxTB_RIGHT);
}


// ----------------------------------------------------------------------------
// event processing
// ----------------------------------------------------------------------------

// Only allow toggle if returns true
bool wxToolBarBase::OnLeftClick(int toolid, bool toggleDown)
{
    wxCommandEvent event(wxEVT_TOOL, toolid);
    event.SetEventObject(this);

    // we use SetInt() to make wxCommandEvent::IsChecked() return toggleDown
    event.SetInt((int)toggleDown);

    // and SetExtraLong() for backwards compatibility
    event.SetExtraLong((long)toggleDown);

    // Send events to this toolbar instead (and thence up the window hierarchy)
    HandleWindowEvent(event);

    return true;
}

// Call when right button down.
void wxToolBarBase::OnRightClick(int toolid,
                                 long WXUNUSED(x),
                                 long WXUNUSED(y))
{
    wxCommandEvent event(wxEVT_TOOL_RCLICKED, toolid);
    event.SetEventObject(this);
    event.SetInt(toolid);

    GetEventHandler()->ProcessEvent(event);
}

// Called when the mouse cursor enters a tool bitmap (no button pressed).
// Argument is wxID_ANY if mouse is exiting the toolbar.
// Note that for this event, the toolid of the window is used,
// and the integer parameter of wxCommandEvent is used to retrieve
// the tool toolid.
void wxToolBarBase::OnMouseEnter(int toolid)
{
    wxCommandEvent event(wxEVT_TOOL_ENTER, GetId());
    event.SetEventObject(this);
    event.SetInt(toolid);

    wxFrame *frame = wxDynamicCast(GetParent(), wxFrame);
    if ( frame )
    {
        wxString help;
        if ( toolid != wxID_ANY )
        {
           const wxToolBarToolBase * const tool = FindById(toolid);
           if ( tool )
               help = tool->GetLongHelp();
        }

        // call DoGiveHelp() even if help string is empty to avoid showing the
        // help for the previously selected tool when another one is selected
        frame->DoGiveHelp(help, toolid != wxID_ANY);
    }

    (void)GetEventHandler()->ProcessEvent(event);
}

// ----------------------------------------------------------------------------
// UI updates
// ----------------------------------------------------------------------------

// Do the toolbar button updates (check for EVT_UPDATE_UI handlers)
void wxToolBarBase::UpdateWindowUI(long flags)
{
    wxWindowBase::UpdateWindowUI(flags);

    // don't waste time updating state of tools in a hidden toolbar
    if ( !IsShown() )
        return;

    wxEvtHandler* evtHandler = GetEventHandler() ;

    for ( wxToolBarToolsList::compatibility_iterator node = m_tools.GetFirst();
          node;
          node = node->GetNext() )
    {
        wxToolBarToolBase * const tool = node->GetData();
        if ( tool->IsSeparator() )
            continue;

        int toolid = tool->GetId();

        wxUpdateUIEvent event(toolid);
        event.SetEventObject(this);

        if ( evtHandler->ProcessEvent(event) )
        {
            if ( event.GetSetEnabled() )
                EnableTool(toolid, event.GetEnabled());
            if ( event.GetSetChecked() )
                ToggleTool(toolid, event.GetChecked());
#if 0
            if ( event.GetSetText() )
                // Set tooltip?
#endif // 0
        }
    }
}

#if wxUSE_MENUS
bool wxToolBarBase::SetDropdownMenu(int toolid, wxMenu* menu)
{
    wxToolBarToolBase * const tool = FindById(toolid);
    wxCHECK_MSG( tool, false, wxT("invalid tool toolid") );

    wxCHECK_MSG( tool->GetKind() == wxITEM_DROPDOWN, false,
                    wxT("menu can be only associated with drop down tools") );

    tool->SetDropdownMenu(menu);

    return true;
}
#endif

#if WXWIN_COMPATIBILITY_2_8

bool wxCreateGreyedImage(const wxImage& in, wxImage& out)
{
#if wxUSE_IMAGE
    out = in.ConvertToGreyscale();
    if ( out.IsOk() )
        return true;
#endif // wxUSE_IMAGE
    return false;
}

#endif // WXWIN_COMPATIBILITY_2_8

#endif // wxUSE_TOOLBAR
