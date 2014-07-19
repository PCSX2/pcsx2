///////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/dvrenderers.h
// Purpose:     All OS X wxDataViewCtrl renderer classes
// Author:      Vadim Zeitlin
// Created:     2009-11-07 (extracted from wx/osx/dataview.h)
// Copyright:   (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OSX_DVRENDERERS_H_
#define _WX_OSX_DVRENDERERS_H_

// ---------------------------------------------------------
// wxDataViewCustomRenderer
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewCustomRenderer : public wxDataViewCustomRendererBase
{
public:
    wxDataViewCustomRenderer(const wxString& varianttype = "string",
                             wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                             int align = wxDVR_DEFAULT_ALIGNMENT);

    virtual ~wxDataViewCustomRenderer();


    // implementation only
    // -------------------

    virtual bool MacRender();

#if wxOSX_USE_COCOA
    virtual void OSXApplyAttr(const wxDataViewItemAttr& attr);
#endif // Cocoa

    virtual wxDC* GetDC(); // creates a device context and keeps it
    void SetDC(wxDC* newDCPtr); // this method takes ownership of the pointer

private:
    wxControl* m_editorCtrlPtr; // pointer to an in-place editor control

    wxDC* m_DCPtr;

    DECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewCustomRenderer)
};

// ---------------------------------------------------------
// wxDataViewTextRenderer
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewTextRenderer: public wxDataViewRenderer
{
public:
    wxDataViewTextRenderer(const wxString& varianttype = "string",
                           wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                           int align = wxDVR_DEFAULT_ALIGNMENT);

    virtual bool MacRender();

#if wxOSX_USE_COCOA
    virtual void OSXOnCellChanged(NSObject *value,
                                  const wxDataViewItem& item,
                                  unsigned col);
#endif // Cocoa

private:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewTextRenderer)
};

// ---------------------------------------------------------
// wxDataViewBitmapRenderer
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewBitmapRenderer: public wxDataViewRenderer
{
public:
    wxDataViewBitmapRenderer(const wxString& varianttype = "wxBitmap",
                             wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                             int align = wxDVR_DEFAULT_ALIGNMENT);

    virtual bool MacRender();

private:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewBitmapRenderer)
};

#if wxOSX_USE_COCOA

// -------------------------------------
// wxDataViewChoiceRenderer
// -------------------------------------

class WXDLLIMPEXP_ADV wxDataViewChoiceRenderer: public wxDataViewRenderer
{
public:
    wxDataViewChoiceRenderer(const wxArrayString& choices,
                             wxDataViewCellMode mode = wxDATAVIEW_CELL_EDITABLE,
                             int alignment = wxDVR_DEFAULT_ALIGNMENT );

    virtual bool MacRender();

    wxString GetChoice(size_t index) const { return m_choices[index]; }
    const wxArrayString& GetChoices() const { return m_choices; }

#if wxOSX_USE_COCOA
    virtual void OSXOnCellChanged(NSObject *value,
                                  const wxDataViewItem& item,
                                  unsigned col);
#endif // Cocoa

private:
    wxArrayString m_choices;

    DECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewChoiceRenderer)
};

#endif // wxOSX_USE_COCOA

// ---------------------------------------------------------
// wxDataViewIconTextRenderer
// ---------------------------------------------------------
class WXDLLIMPEXP_ADV wxDataViewIconTextRenderer: public wxDataViewRenderer
{
public:
    wxDataViewIconTextRenderer(const wxString& varianttype = "wxDataViewIconText",
                               wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                               int align = wxDVR_DEFAULT_ALIGNMENT);

    virtual bool MacRender();

#if wxOSX_USE_COCOA
    virtual void OSXOnCellChanged(NSObject *value,
                                  const wxDataViewItem& item,
                                  unsigned col);
#endif // Cocoa

private:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewIconTextRenderer)
};

// ---------------------------------------------------------
// wxDataViewToggleRenderer
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewToggleRenderer: public wxDataViewRenderer
{
public:
    wxDataViewToggleRenderer(const wxString& varianttype = "bool",
                             wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                             int align = wxDVR_DEFAULT_ALIGNMENT);

    virtual bool MacRender();

#if wxOSX_USE_COCOA
    virtual void OSXOnCellChanged(NSObject *value,
                                  const wxDataViewItem& item,
                                  unsigned col);
#endif // Cocoa

private:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewToggleRenderer)
};

// ---------------------------------------------------------
// wxDataViewProgressRenderer
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewProgressRenderer: public wxDataViewRenderer
{
public:
    wxDataViewProgressRenderer(const wxString& label = wxEmptyString,
                               const wxString& varianttype = "long",
                               wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                               int align = wxDVR_DEFAULT_ALIGNMENT);

    virtual bool MacRender();

#if wxOSX_USE_COCOA
    virtual void OSXOnCellChanged(NSObject *value,
                                  const wxDataViewItem& item,
                                  unsigned col);
#endif // Cocoa

private:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewProgressRenderer)
};

// ---------------------------------------------------------
// wxDataViewDateRenderer
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewDateRenderer: public wxDataViewRenderer
{
public:
    wxDataViewDateRenderer(const wxString& varianttype = "datetime",
                           wxDataViewCellMode mode = wxDATAVIEW_CELL_ACTIVATABLE,
                           int align = wxDVR_DEFAULT_ALIGNMENT);

    virtual bool MacRender();

#if wxOSX_USE_COCOA
    virtual void OSXOnCellChanged(NSObject *value,
                                  const wxDataViewItem& item,
                                  unsigned col);
#endif // Cocoa

private:
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewDateRenderer)
};

#endif // _WX_OSX_DVRENDERERS_H_

