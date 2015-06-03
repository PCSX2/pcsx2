///////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/dvrenderers.h
// Purpose:     All generic wxDataViewCtrl renderer classes
// Author:      Robert Roebling, Vadim Zeitlin
// Created:     2009-11-07 (extracted from wx/generic/dataview.h)
// Copyright:   (c) 2006 Robert Roebling
//              (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_DVRENDERERS_H_
#define _WX_GENERIC_DVRENDERERS_H_

// ---------------------------------------------------------
// wxDataViewCustomRenderer
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewCustomRenderer: public wxDataViewRenderer
{
public:
    static wxString GetDefaultType() { return wxS("string"); }

    wxDataViewCustomRenderer( const wxString &varianttype = GetDefaultType(),
                              wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                              int align = wxDVR_DEFAULT_ALIGNMENT );


    // see the explanation of the following WXOnXXX() methods in wx/generic/dvrenderer.h

    virtual bool WXActivateCell(const wxRect& cell,
                                wxDataViewModel *model,
                                const wxDataViewItem& item,
                                unsigned int col,
                                const wxMouseEvent *mouseEvent)
    {
        return ActivateCell(cell, model, item, col, mouseEvent);
    }

private:
    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewCustomRenderer);
};


// ---------------------------------------------------------
// wxDataViewTextRenderer
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewTextRenderer: public wxDataViewRenderer
{
public:
    static wxString GetDefaultType() { return wxS("string"); }

    wxDataViewTextRenderer( const wxString &varianttype = GetDefaultType(),
                            wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                            int align = wxDVR_DEFAULT_ALIGNMENT );

    virtual bool SetValue( const wxVariant &value );
    virtual bool GetValue( wxVariant &value ) const;

    virtual bool Render(wxRect cell, wxDC *dc, int state);
    virtual wxSize GetSize() const;

    // in-place editing
    virtual bool HasEditorCtrl() const;
    virtual wxWindow* CreateEditorCtrl( wxWindow *parent, wxRect labelRect,
                                        const wxVariant &value );
    virtual bool GetValueFromEditorCtrl( wxWindow* editor, wxVariant &value );

protected:
    wxString   m_text;

protected:
    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewTextRenderer);
};

// ---------------------------------------------------------
// wxDataViewBitmapRenderer
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewBitmapRenderer: public wxDataViewRenderer
{
public:
    static wxString GetDefaultType() { return wxS("wxBitmap"); }

    wxDataViewBitmapRenderer( const wxString &varianttype = GetDefaultType(),
                              wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                              int align = wxDVR_DEFAULT_ALIGNMENT );

    virtual bool SetValue( const wxVariant &value );
    virtual bool GetValue( wxVariant &value ) const;

    virtual bool Render( wxRect cell, wxDC *dc, int state );
    virtual wxSize GetSize() const;

private:
    wxIcon m_icon;
    wxBitmap m_bitmap;

protected:
    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewBitmapRenderer);
};

// ---------------------------------------------------------
// wxDataViewToggleRenderer
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewToggleRenderer: public wxDataViewRenderer
{
public:
    static wxString GetDefaultType() { return wxS("bool"); }

    wxDataViewToggleRenderer( const wxString &varianttype = GetDefaultType(),
                              wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                              int align = wxDVR_DEFAULT_ALIGNMENT );

    virtual bool SetValue( const wxVariant &value );
    virtual bool GetValue( wxVariant &value ) const;

    virtual bool Render( wxRect cell, wxDC *dc, int state );
    virtual wxSize GetSize() const;

    // Implementation only, don't use nor override
    virtual bool WXActivateCell(const wxRect& cell,
                                wxDataViewModel *model,
                                const wxDataViewItem& item,
                                unsigned int col,
                                const wxMouseEvent *mouseEvent);
private:
    bool    m_toggle;

protected:
    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewToggleRenderer);
};

// ---------------------------------------------------------
// wxDataViewProgressRenderer
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewProgressRenderer: public wxDataViewRenderer
{
public:
    static wxString GetDefaultType() { return wxS("long"); }

    wxDataViewProgressRenderer( const wxString &label = wxEmptyString,
                                const wxString &varianttype = GetDefaultType(),
                                wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                                int align = wxDVR_DEFAULT_ALIGNMENT );

    virtual bool SetValue( const wxVariant &value );
    virtual bool GetValue( wxVariant& value ) const;

    virtual bool Render(wxRect cell, wxDC *dc, int state);
    virtual wxSize GetSize() const;

private:
    wxString    m_label;
    int         m_value;

protected:
    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewProgressRenderer);
};

// ---------------------------------------------------------
// wxDataViewIconTextRenderer
// ---------------------------------------------------------

class WXDLLIMPEXP_ADV wxDataViewIconTextRenderer: public wxDataViewRenderer
{
public:
    static wxString GetDefaultType() { return wxS("wxDataViewIconText"); }

    wxDataViewIconTextRenderer( const wxString &varianttype = GetDefaultType(),
                                wxDataViewCellMode mode = wxDATAVIEW_CELL_INERT,
                                int align = wxDVR_DEFAULT_ALIGNMENT );

    virtual bool SetValue( const wxVariant &value );
    virtual bool GetValue( wxVariant &value ) const;

    virtual bool Render(wxRect cell, wxDC *dc, int state);
    virtual wxSize GetSize() const;

    virtual bool HasEditorCtrl() const { return true; }
    virtual wxWindow* CreateEditorCtrl( wxWindow *parent, wxRect labelRect,
                                        const wxVariant &value );
    virtual bool GetValueFromEditorCtrl( wxWindow* editor, wxVariant &value );

private:
    wxDataViewIconText   m_value;

protected:
    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxDataViewIconTextRenderer);
};

#endif // _WX_GENERIC_DVRENDERERS_H_

