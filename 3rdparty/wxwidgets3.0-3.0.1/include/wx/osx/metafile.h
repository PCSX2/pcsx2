/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/metafile.h
// Purpose:     wxMetaFile, wxMetaFileDC classes.
//              This probably should be restricted to Windows platforms,
//              but if there is an equivalent on your platform, great.
// Author:      Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////


#ifndef _WX_METAFIILE_H_
#define _WX_METAFIILE_H_

#include "wx/dc.h"
#include "wx/gdiobj.h"

#if wxUSE_DATAOBJ
#include "wx/dataobj.h"
#endif

#include "wx/osx/dcclient.h"

/*
 * Metafile and metafile device context classes
 *
 */

#define wxMetaFile wxMetafile
#define wxMetaFileDC wxMetafileDC

class WXDLLIMPEXP_FWD_CORE wxMetafile;
class wxMetafileRefData ;

#define M_METAFILEDATA ((wxMetafileRefData *)m_refData)

class WXDLLIMPEXP_CORE wxMetafile : public wxGDIObject
{
public:
    wxMetafile(const wxString& file = wxEmptyString);
    virtual ~wxMetafile(void);

    // After this is called, the metafile cannot be used for anything
    // since it is now owned by the clipboard.
    virtual bool SetClipboard(int width = 0, int height = 0);

    virtual bool Play(wxDC *dc);

    wxSize GetSize() const;
    int GetWidth() const { return GetSize().x; }
    int GetHeight() const { return GetSize().y; }

    // Implementation
    WXHMETAFILE GetHMETAFILE() const ;
    void SetHMETAFILE(WXHMETAFILE mf) ;
protected:
    virtual wxGDIRefData *CreateGDIRefData() const;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const;

    DECLARE_DYNAMIC_CLASS(wxMetafile)
};


class WXDLLIMPEXP_CORE wxMetafileDCImpl: public wxGCDCImpl
{
public:
    wxMetafileDCImpl( wxDC *owner,
                      const wxString& filename,
                      int width, int height,
                      const wxString& description );

    virtual ~wxMetafileDCImpl();

    // Should be called at end of drawing
    virtual wxMetafile *Close();

    // Implementation
    wxMetafile *GetMetaFile(void) const { return m_metaFile; }
    void SetMetaFile(wxMetafile *mf) { m_metaFile = mf; }

protected:
    virtual void DoGetSize(int *width, int *height) const;

    wxMetafile*   m_metaFile;

private:
    DECLARE_CLASS(wxMetafileDCImpl)
    wxDECLARE_NO_COPY_CLASS(wxMetafileDCImpl);
};

class WXDLLIMPEXP_CORE wxMetafileDC: public wxDC
{
 public:
    // the ctor parameters specify the filename (empty for memory metafiles),
    // the metafile picture size and the optional description/comment
    wxMetafileDC(  const wxString& filename = wxEmptyString,
                    int width = 0, int height = 0,
                    const wxString& description = wxEmptyString ) :
      wxDC( new wxMetafileDCImpl( this, filename, width, height, description) )
    { }

    wxMetafile *GetMetafile() const
       { return ((wxMetafileDCImpl*)m_pimpl)->GetMetaFile(); }

    wxMetafile *Close()
       { return ((wxMetafileDCImpl*)m_pimpl)->Close(); }

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
bool WXDLLIMPEXP_CORE wxMakeMetaFilePlaceable(const wxString& filename, int x1, int y1, int x2, int y2, float scale = 1.0, bool useOriginAndExtent = true);

// ----------------------------------------------------------------------------
// wxMetafileDataObject is a specialization of wxDataObject for metafile data
// ----------------------------------------------------------------------------

#if wxUSE_DATAOBJ
class WXDLLIMPEXP_CORE wxMetafileDataObject : public wxDataObjectSimple
{
public:
  // ctors
  wxMetafileDataObject()
    : wxDataObjectSimple(wxDF_METAFILE) {  }
  wxMetafileDataObject(const wxMetafile& metafile)
    : wxDataObjectSimple(wxDF_METAFILE), m_metafile(metafile) { }

    // virtual functions which you may override if you want to provide data on
    // demand only - otherwise, the trivial default versions will be used
    virtual void SetMetafile(const wxMetafile& metafile)
        { m_metafile = metafile; }
    virtual wxMetafile GetMetafile() const
        { return m_metafile; }

    // implement base class pure virtuals
    virtual size_t GetDataSize() const;
    virtual bool GetDataHere(void *buf) const;
    virtual bool SetData(size_t len, const void *buf);

    virtual size_t GetDataSize(const wxDataFormat& WXUNUSED(format)) const
        { return GetDataSize(); }
    virtual bool GetDataHere(const wxDataFormat& WXUNUSED(format),
                             void *buf) const
        { return GetDataHere(buf); }
    virtual bool SetData(const wxDataFormat& WXUNUSED(format),
                         size_t len, const void *buf)
        { return SetData(len, buf); }
protected:
  wxMetafile   m_metafile;
};
#endif

#endif
    // _WX_METAFIILE_H_
