///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/nonownedwnd.h
// Purpose:     wxGTK-specific wxNonOwnedWindow declaration.
// Author:      Vadim Zeitlin
// Created:     2011-10-12
// Copyright:   (c) 2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_NONOWNEDWND_H_
#define _WX_GTK_NONOWNEDWND_H_

class wxNonOwnedWindowShapeImpl;

// ----------------------------------------------------------------------------
// wxNonOwnedWindow contains code common to wx{Popup,TopLevel}Window in wxGTK.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxNonOwnedWindow : public wxNonOwnedWindowBase
{
public:
    wxNonOwnedWindow() { m_shapeImpl = NULL; }
    virtual ~wxNonOwnedWindow();

    // Overridden to actually set the shape when the window becomes realized.
    virtual void GTKHandleRealized();

protected:
    virtual bool DoClearShape();
    virtual bool DoSetRegionShape(const wxRegion& region);
#if wxUSE_GRAPHICS_CONTEXT
    virtual bool DoSetPathShape(const wxGraphicsPath& path);
#endif // wxUSE_GRAPHICS_CONTEXT


private:
    // If non-NULL, contains information about custom window shape.
    wxNonOwnedWindowShapeImpl* m_shapeImpl;

    wxDECLARE_NO_COPY_CLASS(wxNonOwnedWindow);
};

#endif // _WX_GTK_NONOWNEDWND_H_
