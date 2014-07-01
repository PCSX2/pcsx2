/////////////////////////////////////////////////////////////////////////////
// Name:        wx/dfb/private/fontmgr.h
// Purpose:     font management for wxDFB
// Author:      Vaclav Slavik
// Created:     2006-11-18
// Copyright:   (c) 2001-2002 SciTech Software, Inc. (www.scitechsoft.com)
//              (c) 2006 REA Elektronik GmbH
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DFB_PRIVATE_FONTMGR_H_
#define _WX_DFB_PRIVATE_FONTMGR_H_

#include "wx/dfb/wrapdfb.h"

class wxFileConfig;

class wxFontInstance : public wxFontInstanceBase
{
public:
    wxFontInstance(float ptSize, bool aa, const wxString& filename);

    wxIDirectFBFontPtr GetDirectFBFont() const { return m_font; }

private:
    wxIDirectFBFontPtr m_font;
};

class wxFontFace : public wxFontFaceBase
{
public:
    wxFontFace(const wxString& filename) : m_fileName(filename) {}

protected:
    wxFontInstance *CreateFontInstance(float ptSize, bool aa);

private:
    wxString m_fileName;
};

class wxFontBundle : public wxFontBundleBase
{
public:
    wxFontBundle(const wxString& name,
                 const wxString& fileRegular,
                 const wxString& fileBold,
                 const wxString& fileItalic,
                 const wxString& fileBoldItalic,
                 bool isFixed);

    /// Returns name of the family
    virtual wxString GetName() const { return m_name; }

    virtual bool IsFixed() const { return m_isFixed; }

private:
    wxString m_name;
    bool     m_isFixed;
};

class wxFontsManager : public wxFontsManagerBase
{
public:
    wxFontsManager() { AddAllFonts(); }

    virtual wxString GetDefaultFacename(wxFontFamily family) const
    {
        return m_defaultFacenames[family];
    }

private:
    // adds all fonts using AddBundle()
    void AddAllFonts();
    void AddFontsFromDir(const wxString& indexFile);
    void AddFont(const wxString& dir, const wxString& name, wxFileConfig& cfg);
    void SetDefaultFonts(wxFileConfig& cfg);

private:
    // default facenames
    wxString m_defaultFacenames[wxFONTFAMILY_MAX];
};

#endif // _WX_DFB_PRIVATE_FONTMGR_H_
