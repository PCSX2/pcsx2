///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/private/list.h
// Purpose:     wxGtkList class.
// Author:      Vadim Zeitlin
// Created:     2011-08-21
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_PRIVATE_LIST_H_
#define _WX_GTK_PRIVATE_LIST_H_

// ----------------------------------------------------------------------------
// Convenience class for calling g_list_free() automatically
// ----------------------------------------------------------------------------

class wxGtkList
{
public:
    explicit wxGtkList(GList* list) : m_list(list) { }
    ~wxGtkList() { g_list_free(m_list); }

    operator GList *() const { return m_list; }
    GList * operator->() const { return m_list; }

protected:
    GList* const m_list;

    wxDECLARE_NO_COPY_CLASS(wxGtkList);
};

#endif // _WX_GTK_PRIVATE_LIST_H_
