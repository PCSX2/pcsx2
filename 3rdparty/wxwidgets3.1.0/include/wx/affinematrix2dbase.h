/////////////////////////////////////////////////////////////////////////////
// Name:         wx/affinematrix2dbase.h
// Purpose:      Common interface for 2D transformation matrices.
// Author:       Catalin Raceanu
// Created:      2011-04-06
// Copyright:    (c) wxWidgets team
// Licence:      wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_AFFINEMATRIX2DBASE_H_
#define _WX_AFFINEMATRIX2DBASE_H_

#include "wx/defs.h"

#if wxUSE_GEOMETRY

#include "wx/geometry.h"

struct wxMatrix2D
{
    wxMatrix2D(wxDouble v11 = 1,
               wxDouble v12 = 0,
               wxDouble v21 = 0,
               wxDouble v22 = 1)
    {
        m_11 = v11; m_12 = v12;
        m_21 = v21; m_22 = v22;
    }

    wxDouble m_11, m_12, m_21, m_22;
};

// A 2x3 matrix representing an affine 2D transformation.
//
// This is an abstract base class implemented by wxAffineMatrix2D only so far,
// but in the future we also plan to derive wxGraphicsMatrix from it (it should
// also be documented then as currently only wxAffineMatrix2D itself is).
class WXDLLIMPEXP_CORE wxAffineMatrix2DBase
{
public:
    wxAffineMatrix2DBase() {}
    virtual ~wxAffineMatrix2DBase() {}

    // sets the matrix to the respective values
    virtual void Set(const wxMatrix2D& mat2D, const wxPoint2DDouble& tr) = 0;

    // gets the component valuess of the matrix
    virtual void Get(wxMatrix2D* mat2D, wxPoint2DDouble* tr) const = 0;

    // concatenates the matrix
    virtual void Concat(const wxAffineMatrix2DBase& t) = 0;

    // makes this the inverse matrix
    virtual bool Invert() = 0;

    // return true if this is the identity matrix
    virtual bool IsIdentity() const = 0;

    // returns true if the elements of the transformation matrix are equal ?
    virtual bool IsEqual(const wxAffineMatrix2DBase& t) const = 0;
    bool operator==(const wxAffineMatrix2DBase& t) const { return IsEqual(t); }
    bool operator!=(const wxAffineMatrix2DBase& t) const { return !IsEqual(t); }


    //
    // transformations
    //

    // add the translation to this matrix
    virtual void Translate(wxDouble dx, wxDouble dy) = 0;

    // add the scale to this matrix
    virtual void Scale(wxDouble xScale, wxDouble yScale) = 0;

    // add the rotation to this matrix (counter clockwise, radians)
    virtual void Rotate(wxDouble ccRadians) = 0;

    // add mirroring to this matrix
    void Mirror(int direction = wxHORIZONTAL)
    {
        wxDouble x = (direction & wxHORIZONTAL) ? -1 : 1;
        wxDouble y = (direction & wxVERTICAL) ? -1 : 1;
        Scale(x, y);
    }


    // applies that matrix to the point
    wxPoint2DDouble TransformPoint(const wxPoint2DDouble& src) const
    {
        return DoTransformPoint(src);
    }

    void TransformPoint(wxDouble* x, wxDouble* y) const
    {
        wxCHECK_RET( x && y, "Can't be NULL" );

        const wxPoint2DDouble dst = DoTransformPoint(wxPoint2DDouble(*x, *y));
        *x = dst.m_x;
        *y = dst.m_y;
    }

    // applies the matrix except for translations
    wxPoint2DDouble TransformDistance(const wxPoint2DDouble& src) const
    {
        return DoTransformDistance(src);
    }

    void TransformDistance(wxDouble* dx, wxDouble* dy) const
    {
        wxCHECK_RET( dx && dy, "Can't be NULL" );

        const wxPoint2DDouble
            dst = DoTransformDistance(wxPoint2DDouble(*dx, *dy));
        *dx = dst.m_x;
        *dy = dst.m_y;
    }

protected:
    virtual
        wxPoint2DDouble DoTransformPoint(const wxPoint2DDouble& p) const = 0;
    virtual
        wxPoint2DDouble DoTransformDistance(const wxPoint2DDouble& p) const = 0;
};

#endif // wxUSE_GEOMETRY

#endif // _WX_AFFINEMATRIX2DBASE_H_
