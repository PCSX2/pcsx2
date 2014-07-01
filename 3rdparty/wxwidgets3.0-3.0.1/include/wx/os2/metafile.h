/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/metafile.h
// Purpose:     wxMetaFile, wxMetaFileDC classes.
//              This probably should be restricted to Windows platforms,
//              but if there is an equivalent on your platform, great.
// Author:      David Webster
// Modified by:
// Created:     10/10/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////


#ifndef _WX_METAFIILE_H_
#define _WX_METAFIILE_H_

#include "wx/dc.h"
#include "wx/gdiobj.h"
#include "wx/os2/dc.h"

#if wxUSE_DRAG_AND_DROP
#include "wx/dataobj.h"
#endif

/*
 * Metafile and metafile device context classes
 *
 */

#define wxMetaFile wxMetafile
#define wxMetaFileDC wxMetafileDC

class WXDLLIMPEXP_FWD_CORE wxMetafile;

class WXDLLIMPEXP_CORE wxMetafileRefData: public wxGDIRefData
{
    friend class WXDLLIMPEXP_FWD_CORE wxMetafile;
public:
    wxMetafileRefData(void);
    virtual ~wxMetafileRefData(void);

    virtual bool IsOk() const { return m_metafile != 0; }

public:
    WXHANDLE m_metafile;
    int m_windowsMappingMode;
};

#define M_METAFILEDATA ((wxMetafileRefData *)m_refData)

class WXDLLIMPEXP_CORE wxMetafile: public wxGDIObject
{
    DECLARE_DYNAMIC_CLASS(wxMetafile)
public:
    wxMetafile(const wxString& file = wxEmptyString);
    virtual ~wxMetafile(void);

    // After this is called, the metafile cannot be used for anything
    // since it is now owned by the clipboard.
    virtual bool SetClipboard(int width = 0, int height = 0);

    virtual bool Play(wxDC *dc);

    // Implementation
    inline WXHANDLE GetHMETAFILE(void) { return M_METAFILEDATA->m_metafile; }
    void SetHMETAFILE(WXHANDLE mf) ;
    inline int GetWindowsMappingMode(void) { return M_METAFILEDATA->m_windowsMappingMode; }
    void SetWindowsMappingMode(int mm);

protected:
    virtual wxGDIRefData *CreateGDIRefData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;
};

class WXDLLIMPEXP_CORE wxMetafileDCImpl: public wxPMDCImpl
{
public:
    wxMetafileDCImpl(wxDC *owner, const wxString& file = wxEmptyString);
    wxMetafileDCImpl(wxDC *owner, const wxString& file,
                     int xext, int yext, int xorg, int yorg);
    virtual ~wxMetafileDCImpl();

    virtual wxMetafile *Close();
    virtual void SetMapMode(wxMappingMode mode);
    virtual void DoGetTextExtent(const wxString& string,
                                 wxCoord *x, wxCoord *y,
                                 wxCoord *descent = NULL,
                                 wxCoord *externalLeading = NULL,
                                 const wxFont *theFont = NULL) const;

    // Implementation
    wxMetafile *GetMetaFile() const { return m_metaFile; }
    void SetMetaFile(wxMetafile *mf) { m_metaFile = mf; }
    int GetWindowsMappingMode() const { return m_windowsMappingMode; }
    void SetWindowsMappingMode(int mm) { m_windowsMappingMode = mm; }

protected:
    virtual void DoGetSize(int *width, int *height) const;

    int           m_windowsMappingMode;
    wxMetafile*   m_metaFile;

private:
    DECLARE_CLASS(wxMetafileDCImpl)
    wxDECLARE_NO_COPY_CLASS(wxMetafileDCImpl);
};

class WXDLLIMPEXP_CORE wxMetafileDC: public wxDC
{
public:
    // Don't supply origin and extent
    // Supply them to wxMakeMetaFilePlaceable instead.
    wxMetafileDC(const wxString& file = wxEmptyString)
         :wxDC(new wxMetafileDCImpl( this, file ))
         { }

    // Supply origin and extent (recommended).
    // Then don't need to supply them to wxMakeMetaFilePlaceable.
    wxMetafileDC(const wxString& file, int xext, int yext, int xorg, int yorg)
        : wxDC(new wxMetafileDCImpl( this, file, xext, yext, xorg, yorg ))
         { }

    wxMetafile *GetMetafile() const
        { return ((wxMetafileDCImpl*)m_pimpl)->GetMetaFile(); }

    virtual ~wxMetafileDC(void)
        { delete m_pimpl; }

    // Should be called at end of drawing
    virtual wxMetafile *Close(void)
        { return ((wxMetafileDCImpl*)m_pimpl)->Close(); }

    inline void SetMetaFile(wxMetafile *mf)
        { ((wxMetafileDCImpl*)m_pimpl)->SetMetaFile(mf); }

private:
    DECLARE_CLASS(wxMetafileDC)
    wxDECLARE_NO_COPY_CLASS(wxMetafileDC);
};

/*
 * Pass filename of existing non-placeable metafile, and bounding box.
 * Adds a placeable metafile header, sets the mapping mode to anisotropic,
 * and sets the window origin and extent to mimic the wxMM_TEXT mapping mode.
 *
 */

// No origin or extent
#define wxMakeMetaFilePlaceable wxMakeMetafilePlaceable
bool WXDLLIMPEXP_CORE wxMakeMetafilePlaceable(const wxString& filename, float scale = 1.0);

// Optional origin and extent
bool WXDLLIMPEXP_CORE wxMakeMetaFilePlaceable( const wxString& filename
                                         ,int x1
                                         ,int y1
                                         ,int x2
                                         ,int y2
                                         ,float scale = 1.0
                                         ,bool useOriginAndExtent = true
                                        );

// ----------------------------------------------------------------------------
// wxMetafileDataObject is a specialization of wxDataObject for metafile data
// ----------------------------------------------------------------------------

// TODO: implement OLE side of things. At present, it's just for clipboard
// use.

#if wxUSE_DRAG_AND_DROP
class WXDLLIMPEXP_CORE wxMetafileDataObject : public wxDataObject
{
public:
    // ctors
    wxMetafileDataObject() { m_width = 0; m_height = 0; }
    wxMetafileDataObject(const wxMetafile& metafile, int width = 0,int height = 0)
                        :m_metafile(metafile)
                        ,m_width(width)
                        ,m_height(height) { }

    void SetMetafile(const wxMetafile& metafile, int w = 0, int h = 0)
        { m_metafile = metafile; m_width = w; m_height = h; }
    wxMetafile GetMetafile() const { return m_metafile; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    virtual wxDataFormat GetFormat() const { return wxDF_METAFILE; }

/* ??
    // implement base class pure virtuals
    virtual wxDataFormat GetPreferredFormat() const
        { return (wxDataFormat) wxDataObject::Text; }
    virtual bool IsSupportedFormat(wxDataFormat format) const
        { return format == wxDataObject::Text || format == wxDataObject::Locale; }
    virtual size_t GetDataSize() const
        { return m_strText.Len() + 1; } // +1 for trailing '\0'of course
    virtual void GetDataHere(void *pBuf) const
        { memcpy(pBuf, m_strText.c_str(), GetDataSize()); }
*/

private:
    wxMetafile   m_metafile;
    int          m_width;
    int          m_height;
};
#endif

#endif
    // _WX_METAFIILE_H_
