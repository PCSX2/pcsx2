///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/dvrenderer.h
// Purpose:     wxDataViewRenderer for GTK wxDataViewCtrl implementation
// Author:      Robert Roebling, Vadim Zeitlin
// Created:     2009-11-07 (extracted from wx/gtk/dataview.h)
// Copyright:   (c) 2006 Robert Roebling
//              (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_DVRENDERER_H_
#define _WX_GTK_DVRENDERER_H_

typedef struct _GtkCellRendererText GtkCellRendererText;
typedef struct _GtkTreeViewColumn GtkTreeViewColumn;

// ----------------------------------------------------------------------------
// wxDataViewRenderer
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewRenderer: public wxDataViewRendererBase
{
public:
    wxDataViewRenderer( const wxString &varianttype,
                        wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                        int align = wxDVR_DEFAULT_ALIGNMENT );

    virtual void SetMode( wxDataViewCellMode mode );
    virtual wxDataViewCellMode GetMode() const;

    virtual void SetAlignment( int align );
    virtual int GetAlignment() const;

    virtual void EnableEllipsize(wxEllipsizeMode mode = wxELLIPSIZE_MIDDLE);
    virtual wxEllipsizeMode GetEllipsizeMode() const;

    // GTK-specific implementation
    // ---------------------------

    // pack the GTK cell renderers used by this renderer to the given column
    //
    // by default only a single m_renderer is used but some renderers use more
    // than one GTK cell renderer
    virtual void GtkPackIntoColumn(GtkTreeViewColumn *column);

    // called when the cell value was edited by user with the new value
    //
    // it validates the new value and notifies the model about the change by
    // calling GtkOnCellChanged() if it was accepted
    virtual void GtkOnTextEdited(const char *itempath, const wxString& value);

    GtkCellRenderer* GetGtkHandle() { return m_renderer; }
    void GtkInitHandlers();
    void GtkUpdateAlignment() { GtkApplyAlignment(m_renderer); }

    // should be overridden to return true if the renderer supports properties
    // corresponding to wxDataViewItemAttr field, see wxGtkTreeCellDataFunc()
    // for details
    virtual bool GtkSupportsAttrs() const { return false; }

    // if GtkSupportsAttrs() returns true, this function will be called to
    // effectively set the attribute to use for rendering the next item
    //
    // it should return true if the attribute had any non-default properties
    virtual bool GtkSetAttr(const wxDataViewItemAttr& WXUNUSED(attr))
        { return false; }


    // these functions are only called if GtkSupportsAttrs() returns true and
    // are used to remember whether the renderer currently uses the default
    // attributes or if we changed (and not reset them)
    bool GtkIsUsingDefaultAttrs() const { return m_usingDefaultAttrs; }
    void GtkSetUsingDefaultAttrs(bool def) { m_usingDefaultAttrs = def; }

    // return the text renderer used by this renderer for setting text cell
    // specific attributes: can return NULL if this renderer doesn't render any
    // text
    virtual GtkCellRendererText *GtkGetTextRenderer() const { return NULL; }
    
    wxDataViewCellMode GtkGetMode() { return m_mode; }

protected:
    virtual void GtkOnCellChanged(const wxVariant& value,
                                  const wxDataViewItem& item,
                                  unsigned col);

    // Apply our effective alignment (i.e. m_alignment if specified or the
    // associated column alignment by default) to the given renderer.
    void GtkApplyAlignment(GtkCellRenderer *renderer);

    GtkCellRenderer    *m_renderer;
    int                 m_alignment;
    wxDataViewCellMode  m_mode;

    // true if we hadn't changed any visual attributes or restored them since
    // doing this
    bool m_usingDefaultAttrs;

protected:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewRenderer)
};

#endif // _WX_GTK_DVRENDERER_H_

