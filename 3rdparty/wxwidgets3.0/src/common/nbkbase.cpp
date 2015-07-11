///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/nbkbase.cpp
// Purpose:     common wxNotebook methods
// Author:      Vadim Zeitlin
// Modified by:
// Created:     02.07.01
// Copyright:   (c) 2001 Vadim Zeitlin
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

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

#if wxUSE_NOTEBOOK

#ifndef WX_PRECOMP
#endif //WX_PRECOMP

#include "wx/notebook.h"

// ============================================================================
// implementation
// ============================================================================

extern WXDLLEXPORT_DATA(const char) wxNotebookNameStr[] = "notebook";

wxDEFINE_EVENT( wxEVT_NOTEBOOK_PAGE_CHANGED, wxBookCtrlEvent );
wxDEFINE_EVENT( wxEVT_NOTEBOOK_PAGE_CHANGING, wxBookCtrlEvent );

// ----------------------------------------------------------------------------
// XTI
// ----------------------------------------------------------------------------

#include "wx/listimpl.cpp"
wxDEFINE_FLAGS( wxNotebookStyle )
wxBEGIN_FLAGS( wxNotebookStyle )
// new style border flags, we put them first to
// use them for streaming out
wxFLAGS_MEMBER(wxBORDER_SIMPLE)
wxFLAGS_MEMBER(wxBORDER_SUNKEN)
wxFLAGS_MEMBER(wxBORDER_DOUBLE)
wxFLAGS_MEMBER(wxBORDER_RAISED)
wxFLAGS_MEMBER(wxBORDER_STATIC)
wxFLAGS_MEMBER(wxBORDER_NONE)

// old style border flags
wxFLAGS_MEMBER(wxSIMPLE_BORDER)
wxFLAGS_MEMBER(wxSUNKEN_BORDER)
wxFLAGS_MEMBER(wxDOUBLE_BORDER)
wxFLAGS_MEMBER(wxRAISED_BORDER)
wxFLAGS_MEMBER(wxSTATIC_BORDER)
wxFLAGS_MEMBER(wxBORDER)

// standard window styles
wxFLAGS_MEMBER(wxTAB_TRAVERSAL)
wxFLAGS_MEMBER(wxCLIP_CHILDREN)
wxFLAGS_MEMBER(wxTRANSPARENT_WINDOW)
wxFLAGS_MEMBER(wxWANTS_CHARS)
wxFLAGS_MEMBER(wxFULL_REPAINT_ON_RESIZE)
wxFLAGS_MEMBER(wxALWAYS_SHOW_SB )
wxFLAGS_MEMBER(wxVSCROLL)
wxFLAGS_MEMBER(wxHSCROLL)

wxFLAGS_MEMBER(wxNB_FIXEDWIDTH)
wxFLAGS_MEMBER(wxBK_DEFAULT)
wxFLAGS_MEMBER(wxBK_TOP)
wxFLAGS_MEMBER(wxBK_LEFT)
wxFLAGS_MEMBER(wxBK_RIGHT)
wxFLAGS_MEMBER(wxBK_BOTTOM)
wxFLAGS_MEMBER(wxNB_NOPAGETHEME)
wxFLAGS_MEMBER(wxNB_FLAT)
wxEND_FLAGS( wxNotebookStyle )

#if wxUSE_EXTENDED_RTTI

WX_DEFINE_LIST( wxNotebookPageInfoList )

wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxNotebookPageInfo, wxObject, "wx/notebook.h" )

wxCOLLECTION_TYPE_INFO( wxNotebookPageInfo *, wxNotebookPageInfoList );

template<> void wxCollectionToVariantArray( wxNotebookPageInfoList const &theList,
                                           wxAnyList &value)
{
    wxListCollectionToAnyList<wxNotebookPageInfoList::compatibility_iterator>( theList, value );
}

wxBEGIN_PROPERTIES_TABLE(wxNotebookPageInfo)
wxREADONLY_PROPERTY( Page, wxNotebookPage*, GetPage, wxEMPTY_PARAMETER_VALUE, \
                    0 /*flags*/, wxT("Helpstring"), wxT("group"))
wxREADONLY_PROPERTY( Text, wxString, GetText, wxString(), 0 /*flags*/, \
                    wxT("Helpstring"), wxT("group"))
wxREADONLY_PROPERTY( Selected, bool, GetSelected, false, 0 /*flags*/, \
                    wxT("Helpstring"), wxT("group") )
wxREADONLY_PROPERTY( ImageId, int, GetImageId, -1, 0 /*flags*/, \
                    wxT("Helpstring"), wxT("group"))
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxNotebookPageInfo)

wxCONSTRUCTOR_4( wxNotebookPageInfo, wxNotebookPage*, Page, \
                wxString, Text, bool, Selected, int, ImageId )

// WX_IMPLEMENT_ANY_VALUE_TYPE(wxAnyValueTypeImpl<wxNotebookPageInfo**>)
// XTI accessors:

void wxNotebookBase::AddPageInfo( wxNotebookPageInfo* info )
{
    AddPage( info->GetPage(), info->GetText(), info->GetSelected(), info->GetImageId() );
}

const wxNotebookPageInfoList& wxNotebookBase::GetPageInfos() const
{
    wxNotebookPageInfoList* list = const_cast< wxNotebookPageInfoList* >( &m_pageInfos );
    WX_CLEAR_LIST( wxNotebookPageInfoList, *list );
    for( size_t i = 0; i < GetPageCount(); ++i )
    {
        wxNotebookPageInfo *info = new wxNotebookPageInfo();
        info->Create( const_cast<wxNotebookBase*>(this)->GetPage(i), GetPageText(i),
                     GetSelection() == int(i), GetPageImage(i) );
        list->Append( info );
    }
    return m_pageInfos;
}

#endif

wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxNotebook, wxBookCtrlBase, "wx/notebook.h")
wxBEGIN_PROPERTIES_TABLE(wxNotebook)
wxEVENT_PROPERTY( PageChanging, wxEVT_NOTEBOOK_PAGE_CHANGING, wxNotebookEvent )
wxEVENT_PROPERTY( PageChanged, wxEVT_NOTEBOOK_PAGE_CHANGED, wxNotebookEvent )

wxPROPERTY_COLLECTION( PageInfos, wxNotebookPageInfoList, wxNotebookPageInfo*, \
                      AddPageInfo, GetPageInfos, 0 /*flags*/, wxT("Helpstring"), \
                      wxT("group"))
wxPROPERTY_FLAGS( WindowStyle, wxNotebookStyle, long, SetWindowStyleFlag, \
                 GetWindowStyleFlag, wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, \
                 wxT("Helpstring"), wxT("group")) // style
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxNotebook)

wxCONSTRUCTOR_5( wxNotebook, wxWindow*, Parent, wxWindowID, Id, \
                wxPoint, Position, wxSize, Size, long, WindowStyle)

// ----------------------------------------------------------------------------
// geometry
// ----------------------------------------------------------------------------

wxSize wxNotebookBase::CalcSizeFromPage(const wxSize& sizePage) const
{
    // this is, of course, totally bogus -- but we must do something by
    // default because not all ports implement this
    wxSize sizeTotal = sizePage;

    if ( HasFlag(wxBK_LEFT) || HasFlag(wxBK_RIGHT) )
    {
        sizeTotal.x += 90;
        sizeTotal.y += 10;
    }
    else // tabs on top/bottom side
    {
        sizeTotal.x += 10;
        sizeTotal.y += 40;
    }

    return sizeTotal;
}

// ----------------------------------------------------------------------------
// events
// ----------------------------------------------------------------------------

bool wxNotebookBase::SendPageChangingEvent(int nPage)
{
    wxBookCtrlEvent event(wxEVT_NOTEBOOK_PAGE_CHANGING, GetId());
    event.SetSelection(nPage);
    event.SetOldSelection(GetSelection());
    event.SetEventObject(this);
    return !GetEventHandler()->ProcessEvent(event) || event.IsAllowed();
}

void wxNotebookBase::SendPageChangedEvent(int nPageOld, int nPageNew)
{
    wxBookCtrlEvent event(wxEVT_NOTEBOOK_PAGE_CHANGED, GetId());
    event.SetSelection(nPageNew == -1 ? GetSelection() : nPageNew);
    event.SetOldSelection(nPageOld);
    event.SetEventObject(this);
    GetEventHandler()->ProcessEvent(event);
}

#endif // wxUSE_NOTEBOOK
