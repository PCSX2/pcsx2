/////////////////////////////////////////////////////////////////////////////
// Name:        wx/gdiobj.h
// Purpose:     wxGDIObject base header
// Author:      Julian Smart
// Modified by:
// Created:
// Copyright:   (c) Julian Smart
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GDIOBJ_H_BASE_
#define _WX_GDIOBJ_H_BASE_

#include "wx/object.h"

// ----------------------------------------------------------------------------
// wxGDIRefData is the base class for wxXXXData structures which contain the
// real data for the GDI object and are shared among all wxWin objects sharing
// the same native GDI object
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGDIRefData : public wxObjectRefData
{
public:
    // Default ctor which needs to be defined just because we use
    // wxDECLARE_NO_COPY_CLASS() below.
    wxGDIRefData() { }

    // override this in the derived classes to check if this data object is
    // really fully initialized
    virtual bool IsOk() const { return true; }

private:
    wxDECLARE_NO_COPY_CLASS(wxGDIRefData);
};

// ----------------------------------------------------------------------------
// wxGDIObject: base class for bitmaps, pens, brushes, ...
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxGDIObject : public wxObject
{
public:
    // checks if the object can be used
    virtual bool IsOk() const
    {
        // the cast here is safe because the derived classes always create
        // wxGDIRefData objects
        return m_refData && static_cast<wxGDIRefData *>(m_refData)->IsOk();
    }

    // don't use in the new code, use IsOk() instead
    bool IsNull() const { return m_refData == NULL; }

    // older version, for backwards compatibility only (but not deprecated
    // because it's still widely used)
    bool Ok() const { return IsOk(); }

#if defined(__WXMSW__)
    // Creates the resource
    virtual bool RealizeResource() { return false; }

    // Frees the resource
    virtual bool FreeResource(bool WXUNUSED(force) = false) { return false; }

    virtual bool IsFree() const { return false; }

    // Returns handle.
    virtual WXHANDLE GetResourceHandle() const { return 0; }
#endif // defined(__WXMSW__)

protected:
    // replace base class functions using wxObjectRefData with our own which
    // use wxGDIRefData to ensure that we always work with data objects of the
    // correct type (i.e. derived from wxGDIRefData)
    virtual wxObjectRefData *CreateRefData() const wxOVERRIDE
    {
        return CreateGDIRefData();
    }

    virtual wxObjectRefData *CloneRefData(const wxObjectRefData *data) const wxOVERRIDE
    {
        return CloneGDIRefData(static_cast<const wxGDIRefData *>(data));
    }

    virtual wxGDIRefData *CreateGDIRefData() const = 0;
    virtual wxGDIRefData *CloneGDIRefData(const wxGDIRefData *data) const = 0;

    wxDECLARE_DYNAMIC_CLASS(wxGDIObject);
};

#endif // _WX_GDIOBJ_H_BASE_
