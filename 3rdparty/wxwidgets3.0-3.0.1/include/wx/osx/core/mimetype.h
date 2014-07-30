/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/core/mimetype.h
// Purpose:     Mac implementation for wx mime-related classes
// Author:      Neil Perkins
// Modified by:
// Created:     2010-05-15
// Copyright:   (C) 2010 Neil Perkins
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _MIMETYPE_IMPL_H
#define _MIMETYPE_IMPL_H

#include "wx/defs.h"

#if wxUSE_MIMETYPE

#include "wx/mimetype.h"
#include "wx/hashmap.h"
#include "wx/iconloc.h"


// This class implements mime type functionality for Mac OS X using UTIs and Launch Services
// Currently only the GetFileTypeFromXXXX public functions have been implemented
class WXDLLIMPEXP_BASE wxMimeTypesManagerImpl
{
public:

    wxMimeTypesManagerImpl();
    virtual ~wxMimeTypesManagerImpl();

    // These functions are not needed on Mac OS X and have no-op implementations
    void Initialize(int mailcapStyles = wxMAILCAP_STANDARD, const wxString& extraDir = wxEmptyString);
    void ClearData();

    // Functions to look up types by ext, mime or UTI
    wxFileType *GetFileTypeFromExtension(const wxString& ext);
    wxFileType *GetFileTypeFromMimeType(const wxString& mimeType);
    wxFileType *GetFileTypeFromUti(const wxString& uti);

    // These functions are only stubs on Mac OS X
    size_t EnumAllFileTypes(wxArrayString& mimetypes);
    wxFileType *Associate(const wxFileTypeInfo& ftInfo);
    bool Unassociate(wxFileType *ft);

private:

    // The work of querying the OS for type data is done in these two functions
    void LoadTypeDataForUti(const wxString& uti);
    void LoadDisplayDataForUti(const wxString& uti);

    // These functions are pass-throughs from wxFileTypeImpl
    bool GetExtensions(const wxString& uti, wxArrayString& extensions);
    bool GetMimeType(const wxString& uti, wxString *mimeType);
    bool GetMimeTypes(const wxString& uti, wxArrayString& mimeTypes);
    bool GetIcon(const wxString& uti, wxIconLocation *iconLoc);
    bool GetDescription(const wxString& uti, wxString *desc);
    bool GetApplication(const wxString& uti, wxString *command);

    // Structure to represent file types
    typedef struct FileTypeData
    {
        wxArrayString extensions;
        wxArrayString mimeTypes;
        wxIconLocation iconLoc;
        wxString application;
        wxString description;
    }
    FileTypeInfo;

    // Map types
    WX_DECLARE_STRING_HASH_MAP( wxString, TagMap );
    WX_DECLARE_STRING_HASH_MAP( FileTypeData, UtiMap );

    // Data store
    TagMap m_extMap;
    TagMap m_mimeMap;
    UtiMap m_utiMap;

    friend class wxFileTypeImpl;
};


// This class provides the interface between wxFileType and wxMimeTypesManagerImple for Mac OS X
// Currently only extension, mimetype, description and icon information is available
// All other methods have no-op implementation
class WXDLLIMPEXP_BASE wxFileTypeImpl
{
public:

    wxFileTypeImpl();
    virtual ~wxFileTypeImpl();

    bool GetExtensions(wxArrayString& extensions) const ;
    bool GetMimeType(wxString *mimeType) const ;
    bool GetMimeTypes(wxArrayString& mimeTypes) const ;
    bool GetIcon(wxIconLocation *iconLoc) const ;
    bool GetDescription(wxString *desc) const ;
    bool GetOpenCommand(wxString *openCmd, const wxFileType::MessageParameters& params) const;

    // These functions are only stubs on Mac OS X
    bool GetPrintCommand(wxString *printCmd, const wxFileType::MessageParameters& params) const;
    size_t GetAllCommands(wxArrayString *verbs, wxArrayString *commands, const wxFileType::MessageParameters& params) const;
    bool SetCommand(const wxString& cmd, const wxString& verb, bool overwriteprompt = TRUE);
    bool SetDefaultIcon(const wxString& strIcon = wxEmptyString, int index = 0);
    bool Unassociate(wxFileType *ft);

private:

    // All that is needed to query type info - UTI and pointer to the manager
    wxString m_uti;
    wxMimeTypesManagerImpl* m_manager;

    friend class wxMimeTypesManagerImpl;
};

#endif // wxUSE_MIMETYPE
#endif //_MIMETYPE_IMPL_H


