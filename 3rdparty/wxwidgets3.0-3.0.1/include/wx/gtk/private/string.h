///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/private/string.h
// Purpose:     wxGtkString class declaration
// Author:      Vadim Zeitlin
// Created:     2006-10-19
// Copyright:   (c) 2006 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_PRIVATE_STRING_H_
#define _WX_GTK_PRIVATE_STRING_H_

// ----------------------------------------------------------------------------
// Convenience class for g_freeing a gchar* on scope exit automatically
// ----------------------------------------------------------------------------

class wxGtkString
{
public:
    explicit wxGtkString(gchar *s) : m_str(s) { }
    ~wxGtkString() { g_free(m_str); }

    const gchar *c_str() const { return m_str; }

    operator gchar *() const { return m_str; }

private:
    gchar *m_str;

    wxDECLARE_NO_COPY_CLASS(wxGtkString);
};


// ----------------------------------------------------------------------------
// list for sorting collated strings
// ----------------------------------------------------------------------------

#include "wx/string.h"
#include "wx/vector.h"
#include "wx/sharedptr.h"

class wxGtkCollatableString
{
public:
    wxGtkCollatableString( const wxString &label, gchar *key )
    {
        m_label = label;
        m_key = key;
    }

    ~wxGtkCollatableString()
    {
        if (m_key)
            g_free( m_key );
    }

    wxString     m_label;
    gchar       *m_key;
};

class wxGtkCollatedArrayString
{
public:
    wxGtkCollatedArrayString() { }

    int Add( const wxString &new_label )
    {
        int index = 0;

        gchar *new_key_lower = g_utf8_casefold( new_label.utf8_str(), -1);
        gchar *new_key = g_utf8_collate_key( new_key_lower, -1);
        g_free( new_key_lower );

        wxSharedPtr<wxGtkCollatableString> new_ptr( new wxGtkCollatableString( new_label, new_key ) );

        wxVector< wxSharedPtr<wxGtkCollatableString> >::iterator iter;
        for (iter = m_list.begin(); iter != m_list.end(); ++iter)
        {
            wxSharedPtr<wxGtkCollatableString> ptr = *iter;

            gchar *key = ptr->m_key;
            if (strcmp(key,new_key) >= 0)
            {
                m_list.insert( iter, new_ptr );
                return index;
            }
            index ++;
        }

        m_list.push_back( new_ptr );
        return index;
    }

    size_t GetCount()
    {
        return m_list.size();
    }

    wxString At( size_t index )
    {
        return m_list[index]->m_label;
    }

    void Clear()
    {
        m_list.clear();
    }

    void RemoveAt( size_t index )
    {
        m_list.erase( m_list.begin() + index );
    }

private:
    wxVector< wxSharedPtr<wxGtkCollatableString> > m_list;
};


#endif // _WX_GTK_PRIVATE_STRING_H_

