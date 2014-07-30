/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/carbon/mimetype.h
// Purpose:     Mac Carbon implementation for wx mime-related classes
// Author:      Ryan Norton
// Modified by:
// Created:     04/16/2005
// Copyright:   (c) 2005 Ryan Norton (<wxprojects@comcast.net>)
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _MIMETYPE_IMPL_H
#define _MIMETYPE_IMPL_H

#include "wx/defs.h"
#include "wx/mimetype.h"


class wxMimeTypesManagerImpl
{
public :
    //kinda kooky but in wxMimeTypesManager::EnsureImpl it doesn't call
    //intialize, so we do it ourselves
    wxMimeTypesManagerImpl() : m_hIC(NULL) { Initialize(); }
    ~wxMimeTypesManagerImpl() { ClearData(); }

    // load all data into memory - done when it is needed for the first time
    void Initialize(int mailcapStyles = wxMAILCAP_STANDARD,
                    const wxString& extraDir = wxEmptyString);

    // and delete the data here
    void ClearData();

    // implement containing class functions
    wxFileType *GetFileTypeFromExtension(const wxString& ext);
    wxFileType *GetOrAllocateFileTypeFromExtension(const wxString& ext) ;
    wxFileType *GetFileTypeFromMimeType(const wxString& mimeType);

    size_t EnumAllFileTypes(wxArrayString& mimetypes);

    void AddFallback(const wxFileTypeInfo& ft) { m_fallbacks.Add(ft); }

    // create a new filetype association
    wxFileType *Associate(const wxFileTypeInfo& ftInfo);
    // remove association
    bool Unassociate(wxFileType *ft);

private:
    wxArrayFileTypeInfo m_fallbacks;
    void*  m_hIC;
    void** m_hDatabase;
    long   m_lCount;

    void* pReserved1;
    void* pReserved2;
    void* pReserved3;
    void* pReserved4;
    void* pReserved5;
    void* pReserved6;

    friend class wxFileTypeImpl;
};

class wxFileTypeImpl
{
public:
    //kind of nutty, but mimecmn.cpp creates one with an empty new
    wxFileTypeImpl() : m_manager(NULL) {}
    ~wxFileTypeImpl() {} //for those broken compilers

    // implement accessor functions
    bool GetExtensions(wxArrayString& extensions);
    bool GetMimeType(wxString *mimeType) const;
    bool GetMimeTypes(wxArrayString& mimeTypes) const;
    bool GetIcon(wxIconLocation *iconLoc) const;
    bool GetDescription(wxString *desc) const;
    bool GetOpenCommand(wxString *openCmd,
                        const wxFileType::MessageParameters&) const;
    bool GetPrintCommand(wxString *printCmd,
                         const wxFileType::MessageParameters&) const;

    size_t GetAllCommands(wxArrayString * verbs, wxArrayString * commands,
                          const wxFileType::MessageParameters& params) const;

    // remove the record for this file type
    // probably a mistake to come here, use wxMimeTypesManager.Unassociate (ft) instead
    bool Unassociate(wxFileType *ft)
    {
        return m_manager->Unassociate(ft);
    }

    // set an arbitrary command, ask confirmation if it already exists and
    // overwriteprompt is TRUE
    bool SetCommand(const wxString& cmd, const wxString& verb, bool overwriteprompt = true);
    bool SetDefaultIcon(const wxString& strIcon = wxEmptyString, int index = 0);

 private:
    void Init(wxMimeTypesManagerImpl *manager, long lIndex)
    { m_manager=(manager); m_lIndex=(lIndex); }

    // helper function
    wxString GetCommand(const wxString& verb) const;

    wxMimeTypesManagerImpl *m_manager;
    long                    m_lIndex;

    void* pReserved1;
    void* pReserved2;
    void* pReserved3;
    void* pReserved4;
    void* pReserved5;
    void* pReserved6;

    friend class wxMimeTypesManagerImpl;
};

#endif
    //_MIMETYPE_H
