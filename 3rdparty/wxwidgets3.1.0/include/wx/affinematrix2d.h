/////////////////////////////////////////////////////////////////////////////
// Name:         wx/affinematrix2d.h
// Purpose:      wxAffineMatrix2D class.
// Author:       Based on wxTransformMatrix by Chris Breeze, Julian Smart
// Created:      2011-04-05
// Copyright:    (c) wxWidgets team
// Licence:      wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_AFFINEMATRIX2D_H_
#define _WX_AFFINEMATRIX2D_H_

#include "wx/defs.h"

#if wxUSE_GEOMETRY

#include "wx/affinematrix2dbase.h"

// A simple implementation of wxAffineMatrix2DBase interface done entirely in
// wxWidgets.
class WXDLLIMPEXP_CORE wxAffineMatrix2D : public wxAffineMatrix2DBase
{
public:
    wxAffineMatrix2D() : m_11(1), m_12(0),
                         m_21(0), m_22(1),
                         m_tx(0), m_ty(0)
    {
    }

    // Implement base class pure virtual methods.
    virtual void Set(const wxMatrix2D& mat2D, const wxPoint2DDouble& tr) wxOVERRIDE;
    virtual void Get(wxMatrix2D* mat2D, wxPoint2DDouble* tr) const wxOVERRIDE;
    virtual void Concat(const wxAffineMatrix2DBase& t) wxOVERRIDE;
    virtual bool Invert() wxOVERRIDE;
    virtual bool IsIdentity() const wxOVERRIDE;
    virtual bool IsEqual(const wxAffineMatrix2DBase& t) const wxOVERRIDE;
    virtual void Translate(wxDouble dx, wxDouble dy) wxOVERRIDE;
    virtual void Scale(wxDouble xScale, wxDouble yScale) wxOVERRIDE;
    virtual void Rotate(wxDouble cRadians) wxOVERRIDE;

protected:
    virtual wxPoint2DDouble DoTransformPoint(const wxPoint2DDouble& p) const wxOVERRIDE;
    virtual wxPoint2DDouble DoTransformDistance(const wxPoint2DDouble& p) const wxOVERRIDE;

private:
    wxDouble m_11, m_12, m_21, m_22, m_tx, m_ty;
};

#endif // wxUSE_GEOMETRY

#endif // _WX_AFFINEMATRIX2D_H_
