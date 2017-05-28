/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/filehistorycmn.cpp
// Purpose:     wxFileHistory class
// Author:      Julian Smart, Vaclav Slavik, Vadim Zeitlin
// Created:     2010-05-03
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

#include "wx/filehistory.h"

#if wxUSE_FILE_HISTORY

#include "wx/menu.h"
#include "wx/confbase.h"
#include "wx/filename.h"

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// private helpers
// ----------------------------------------------------------------------------

namespace
{

// return the string used for the MRU list items in the menu
//
// NB: the index n is 0-based, as usual, but the strings start from 1
wxString GetMRUEntryLabel(int n, const wxString& path)
{
    // we need to quote '&' characters which are used for mnemonics
    wxString pathInMenu(path);
    pathInMenu.Replace("&", "&&");

    return wxString::Format("&%d %s", n + 1, pathInMenu);
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// File history (a.k.a. MRU, most recently used, files list)
// ----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxFileHistory, wxObject);

wxFileHistoryBase::wxFileHistoryBase(size_t maxFiles, wxWindowID idBase)
{
    m_fileMaxFiles = maxFiles;
    m_idBase = idBase;
}

/* static */
wxString wxFileHistoryBase::NormalizeFileName(const wxFileName& fn)
{
    // We specifically exclude wxPATH_NORM_LONG here as it can take a long time
    // (several seconds) for network file paths under MSW, resulting in huge
    // delays when opening a program using wxFileHistory. We also exclude
    // wxPATH_NORM_ENV_VARS as the file names here are supposed to be "real"
    // file names and not have any environment variables in them.
    wxFileName fnNorm(fn);
    fnNorm.Normalize(wxPATH_NORM_DOTS |
                     wxPATH_NORM_TILDE |
                     wxPATH_NORM_CASE |
                     wxPATH_NORM_ABSOLUTE);
    return fnNorm.GetFullPath();
}

void wxFileHistoryBase::AddFileToHistory(const wxString& file)
{
    // Check if we don't already have this file. Notice that we avoid
    // wxFileName::operator==(wxString) here as it converts the string to
    // wxFileName and then normalizes it using all normalizations which is too
    // slow (see the comment above), so we use our own quick normalization
    // functions and a string comparison.
    const wxFileName fnNew(file);
    const wxString newFile = NormalizeFileName(fnNew);
    size_t i,
           numFiles = m_fileHistory.size();
    for ( i = 0; i < numFiles; i++ )
    {
        if ( newFile == NormalizeFileName(m_fileHistory[i]) )
        {
            // we do have it, move it to the top of the history
            RemoveFileFromHistory(i);
            numFiles--;
            break;
        }
    }

    // if we already have a full history, delete the one at the end
    if ( numFiles == m_fileMaxFiles )
    {
        RemoveFileFromHistory(--numFiles);
    }

    // add a new menu item to all file menus (they will be updated below)
    for ( wxList::compatibility_iterator node = m_fileMenus.GetFirst();
        node;
        node = node->GetNext() )
    {
        wxMenu * const menu = (wxMenu *)node->GetData();

        if ( !numFiles && menu->GetMenuItemCount() )
            menu->AppendSeparator();

        // label doesn't matter, it will be set below anyhow, but it can't
        // be empty (this is supposed to indicate a stock item)
        menu->Append(m_idBase + numFiles, " ");
    }

    // insert the new file in the beginning of the file history
    m_fileHistory.insert(m_fileHistory.begin(), file);
    numFiles++;

    // update the labels in all menus
    for ( i = 0; i < numFiles; i++ )
    {
        // if in same directory just show the filename; otherwise the full path
        const wxFileName fnOld(m_fileHistory[i]);

        wxString pathInMenu;
        if ( (fnOld.GetPath() == fnNew.GetPath()) && fnOld.HasName() )
        {
            pathInMenu = fnOld.GetFullName();
        }
        else // file in different directory or it's not a file but a directory
        {
            // absolute path; could also set relative path
            pathInMenu = m_fileHistory[i];
        }

        for ( wxList::compatibility_iterator node = m_fileMenus.GetFirst();
              node;
              node = node->GetNext() )
        {
            wxMenu * const menu = (wxMenu *)node->GetData();

            menu->SetLabel(m_idBase + i, GetMRUEntryLabel(i, pathInMenu));
        }
    }
}

void wxFileHistoryBase::RemoveFileFromHistory(size_t i)
{
    size_t numFiles = m_fileHistory.size();
    wxCHECK_RET( i < numFiles,
                 wxT("invalid index in wxFileHistoryBase::RemoveFileFromHistory") );

    // delete the element from the array
    m_fileHistory.RemoveAt(i);
    numFiles--;

    for ( wxList::compatibility_iterator node = m_fileMenus.GetFirst();
          node;
          node = node->GetNext() )
    {
        wxMenu * const menu = (wxMenu *) node->GetData();

        // shift filenames up
        for ( size_t j = i; j < numFiles; j++ )
        {
            menu->SetLabel(m_idBase + j, GetMRUEntryLabel(j, m_fileHistory[j]));
        }

        // delete the last menu item which is unused now
        const wxWindowID lastItemId = m_idBase + numFiles;
        if ( menu->FindItem(lastItemId) )
            menu->Delete(lastItemId);

        // delete the last separator too if no more files are left
        if ( m_fileHistory.empty() )
        {
            const wxMenuItemList::compatibility_iterator
                nodeLast = menu->GetMenuItems().GetLast();
            if ( nodeLast )
            {
                wxMenuItem * const lastMenuItem = nodeLast->GetData();
                if ( lastMenuItem->IsSeparator() )
                    menu->Delete(lastMenuItem);
            }
            //else: menu is empty somehow
        }
    }
}

void wxFileHistoryBase::UseMenu(wxMenu *menu)
{
    if ( !m_fileMenus.Member(menu) )
        m_fileMenus.Append(menu);
}

void wxFileHistoryBase::RemoveMenu(wxMenu *menu)
{
    m_fileMenus.DeleteObject(menu);
}

#if wxUSE_CONFIG
void wxFileHistoryBase::Load(const wxConfigBase& config)
{
    RemoveExistingHistory();

    m_fileHistory.Clear();

    wxString buf;
    buf.Printf(wxT("file%d"), 1);

    wxString historyFile;
    while ((m_fileHistory.GetCount() < m_fileMaxFiles) &&
           config.Read(buf, &historyFile) && !historyFile.empty())
    {
        m_fileHistory.Add(historyFile);

        buf.Printf(wxT("file%d"), (int)m_fileHistory.GetCount()+1);
        historyFile = wxEmptyString;
    }

    AddFilesToMenu();
}

void wxFileHistoryBase::Save(wxConfigBase& config)
{
    size_t i;
    for (i = 0; i < m_fileMaxFiles; i++)
    {
        wxString buf;
        buf.Printf(wxT("file%d"), (int)i+1);
        if (i < m_fileHistory.GetCount())
            config.Write(buf, wxString(m_fileHistory[i]));
        else
            config.Write(buf, wxEmptyString);
    }
}
#endif // wxUSE_CONFIG

void wxFileHistoryBase::AddFilesToMenu()
{
    if ( m_fileHistory.empty() )
        return;

    for ( wxList::compatibility_iterator node = m_fileMenus.GetFirst();
          node;
          node = node->GetNext() )
    {
        AddFilesToMenu((wxMenu *) node->GetData());
    }
}

void wxFileHistoryBase::AddFilesToMenu(wxMenu* menu)
{
    if ( m_fileHistory.empty() )
        return;

    if ( menu->GetMenuItemCount() )
        menu->AppendSeparator();

    for ( size_t i = 0; i < m_fileHistory.GetCount(); i++ )
    {
        menu->Append(m_idBase + i, GetMRUEntryLabel(i, m_fileHistory[i]));
    }
}

void wxFileHistoryBase::RemoveExistingHistory()
{
    size_t count = m_fileHistory.GetCount();
    if ( !count )
        return;

    for ( wxList::compatibility_iterator node = m_fileMenus.GetFirst();
          node;
          node = node->GetNext() )
    {
        wxMenu * const menu = static_cast<wxMenu *>(node->GetData());

        // Notice that we remove count+1 items from the menu as we also remove
        // the separator preceding them.
        for ( size_t n = 0; n <= count; n++ )
        {
            const wxMenuItemList::compatibility_iterator
                nodeLast = menu->GetMenuItems().GetLast();
            if ( nodeLast )
            {
                wxMenuItem * const lastMenuItem = nodeLast->GetData();
                menu->Delete(lastMenuItem);
            }
        }
    }
}

#endif // wxUSE_FILE_HISTORY
