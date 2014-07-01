///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/dvrenderer.h
// Purpose:     wxDataViewRenderer for OS X wxDataViewCtrl implementations
// Author:      Vadim Zeitlin
// Created:     2009-11-07 (extracted from wx/osx/dataview.h)
// Copyright:   (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_DVRENDERER_H_
#define _WX_OSX_DVRENDERER_H_

class wxDataViewRendererNativeData;

// ----------------------------------------------------------------------------
// wxDataViewRenderer
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewRenderer : public wxDataViewRendererBase
{
public:
    // constructors / destructor
    // -------------------------

    wxDataViewRenderer(const wxString& varianttype,
                       wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                       int align = wxDVR_DEFAULT_ALIGNMENT);

    virtual ~wxDataViewRenderer();

    // inherited methods from wxDataViewRendererBase
    // ---------------------------------------------

    virtual int GetAlignment() const
    {
        return m_alignment;
    }
    virtual wxDataViewCellMode GetMode() const
    {
        return m_mode;
    }
    virtual bool GetValue(wxVariant& value) const
    {
        value = m_value;
        return true;
    }

    // NB: in Carbon this is always identical to the header alignment
    virtual void SetAlignment(int align);
    virtual void SetMode(wxDataViewCellMode mode);
    virtual bool SetValue(const wxVariant& newValue)
    {
        m_value = newValue;
        return true;
    }

    virtual void EnableEllipsize(wxEllipsizeMode mode = wxELLIPSIZE_MIDDLE);
    virtual wxEllipsizeMode GetEllipsizeMode() const;

    // implementation
    // --------------

    const wxVariant& GetValue() const
    {
        return m_value;
    }

    wxDataViewRendererNativeData* GetNativeData() const
    {
        return m_NativeDataPtr;
    }

    // a call to the native data browser function to render the data;
    // returns true if the data value could be rendered, false otherwise
    virtual bool MacRender() = 0;

    void SetNativeData(wxDataViewRendererNativeData* newNativeDataPtr);


#if wxOSX_USE_COCOA
    // called when a value was edited by user
    virtual void OSXOnCellChanged(NSObject *value,
                                  const wxDataViewItem& item,
                                  unsigned col);

    // called to ensure that the given attribute will be used for rendering the
    // next cell (which had been already associated with this renderer before)
    virtual void OSXApplyAttr(const wxDataViewItemAttr& attr);

    // called to set the state of the next cell to be rendered
    virtual void OSXApplyEnabled(bool enabled);
#endif // Cocoa

private:
    // contains the alignment flags
    int m_alignment;

    // storing the mode that determines how the cell is going to be shown
    wxDataViewCellMode m_mode;

    // data used by implementation of the native renderer
    wxDataViewRendererNativeData* m_NativeDataPtr;

    // value that is going to be rendered
    wxVariant m_value;

    DECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewRenderer)
};

#endif // _WX_OSX_DVRENDERER_H_

