///////////////////////////////////////////////////////////////////////////////
// Name:        affinematrix2d.cpp
// Purpose:     implementation of wxAffineMatrix2D
// Author:      Based on wxTransformMatrix by Chris Breeze, Julian Smart
// Created:     2011-04-05
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_GEOMETRY

#include "wx/affinematrix2d.h"
#include "wx/math.h"

// sets the matrix to the respective values
void wxAffineMatrix2D::Set(const wxMatrix2D &mat2D, const wxPoint2DDouble &tr)
{
    m_11 = mat2D.m_11;
    m_12 = mat2D.m_12;
    m_21 = mat2D.m_21;
    m_22 = mat2D.m_22;
    m_tx = tr.m_x;
    m_ty = tr.m_y;
}

// gets the component valuess of the matrix
void wxAffineMatrix2D::Get(wxMatrix2D *mat2D, wxPoint2DDouble *tr) const
{
    mat2D->m_11 = m_11;
    mat2D->m_12 = m_12;
    mat2D->m_21 = m_21;
    mat2D->m_22 = m_22;

    if ( tr )
    {
        tr->m_x = m_tx;
        tr->m_y = m_ty;
    }
}

// concatenates the matrix
// | t.m_11  t.m_12  0 |   | m_11  m_12   0 |
// | t.m_21  t.m_22  0 | x | m_21  m_22   0 |
// | t.m_tx  t.m_ty  1 |   | m_tx  m_ty   1 |
void wxAffineMatrix2D::Concat(const wxAffineMatrix2DBase &t)
{
    wxMatrix2D mat;
    wxPoint2DDouble tr;
    t.Get(&mat, &tr);

    m_tx += tr.m_x*m_11 + tr.m_y*m_21;
    m_ty += tr.m_x*m_12 + tr.m_y*m_22;
    wxDouble e11 = mat.m_11*m_11 + mat.m_12*m_21;
    wxDouble e12 = mat.m_11*m_12 + mat.m_12*m_22;
    wxDouble e21 = mat.m_21*m_11 + mat.m_22*m_21;
    m_22 = mat.m_21*m_12 + mat.m_22*m_22;
    m_11 = e11;
    m_12 = e12;
    m_21 = e21;
}

// makes this its inverse matrix.
// Invert
// | m_11  m_12   0 |
// | m_21  m_22   0 |
// | m_tx  m_ty   1 |
bool wxAffineMatrix2D::Invert()
{
    const wxDouble det = m_11*m_22 - m_12*m_21;

    if ( !det )
        return false;

    wxDouble ex = (m_21*m_ty - m_22*m_tx) / det;
    m_ty = (-m_11*m_ty + m_12*m_tx) / det;
    m_tx = ex;
    wxDouble e11 = m_22 / det;
    m_12 = -m_12 / det;
    m_21 = -m_21 / det;
    m_22 = m_11 / det;
    m_11 = e11;

    return true;
}

// returns true if the elements of the transformation matrix are equal
bool wxAffineMatrix2D::IsEqual(const wxAffineMatrix2DBase& t) const
{
    wxMatrix2D mat;
    wxPoint2DDouble tr;
    t.Get(&mat, &tr);

    return m_11 == mat.m_11 && m_12 == mat.m_12 &&
           m_21 == mat.m_21 && m_22 == mat.m_22 &&
           m_tx == tr.m_x && m_ty == tr.m_y;
}

//
// transformations
//

// add the translation to this matrix
// |  1   0   0 |   | m_11  m_12   0 |
// |  0   1   0 | x | m_21  m_22   0 |
// | dx  dy   1 |   | m_tx  m_ty   1 |
void wxAffineMatrix2D::Translate(wxDouble dx, wxDouble dy)
{
    m_tx += m_11 * dx + m_21 * dy;
    m_ty += m_12 * dx + m_22 * dy;
}

// add the scale to this matrix
// | xScale   0      0 |   | m_11  m_12   0 |
// |   0    yScale   0 | x | m_21  m_22   0 |
// |   0      0      1 |   | m_tx  m_ty   1 |
void wxAffineMatrix2D::Scale(wxDouble xScale, wxDouble yScale)
{
    m_11 *= xScale;
    m_12 *= xScale;
    m_21 *= yScale;
    m_22 *= yScale;
}

// add the rotation to this matrix (clockwise, radians)
// | cos    sin   0 |   | m_11  m_12   0 |
// | -sin   cos   0 | x | m_21  m_22   0 |
// |  0      0    1 |   | m_tx  m_ty   1 |
void wxAffineMatrix2D::Rotate(wxDouble cRadians)
{
    wxDouble c = cos(cRadians);
    wxDouble s = sin(cRadians);

    wxDouble e11 = c*m_11 + s*m_21;
    wxDouble e12 = c*m_12 + s*m_22;
    m_21 = c*m_21 - s*m_11;
    m_22 = c*m_22 - s*m_12;
    m_11 = e11;
    m_12 = e12;
}

//
// apply the transforms
//

// applies that matrix to the point
//                           | m_11  m_12   0 |
// | src.m_x  src._my  1 | x | m_21  m_22   0 |
//                           | m_tx  m_ty   1 |
wxPoint2DDouble
wxAffineMatrix2D::DoTransformPoint(const wxPoint2DDouble& src) const
{
    if ( IsIdentity() )
        return src;

    return wxPoint2DDouble(src.m_x * m_11 + src.m_y * m_21 + m_tx,
                           src.m_x * m_12 + src.m_y * m_22 + m_ty);
}

// applies the matrix except for translations
//                           | m_11  m_12   0 |
// | src.m_x  src._my  0 | x | m_21  m_22   0 |
//                           | m_tx  m_ty   1 |
wxPoint2DDouble
wxAffineMatrix2D::DoTransformDistance(const wxPoint2DDouble& src) const
{
    if ( IsIdentity() )
        return src;

    return wxPoint2DDouble(src.m_x * m_11 + src.m_y * m_21,
                           src.m_x * m_12 + src.m_y * m_22);
}

bool wxAffineMatrix2D::IsIdentity() const
{
    return m_11 == 1 && m_12 == 0 &&
           m_21 == 0 && m_22 == 1 &&
           m_tx == 0 && m_ty == 0;
}

#endif // wxUSE_GEOMETRY
