/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/colour.h
// Purpose:     wxColour class
// Author:      David Webster
// Modified by:
// Created:     10/13/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COLOUR_H_
#define _WX_COLOUR_H_

#include "wx/object.h"

// Colour
class WXDLLIMPEXP_CORE wxColour: public wxColourBase
{
public:
    // constructors
    // ------------
    DEFINE_STD_WXCOLOUR_CONSTRUCTORS

    // Copy ctors and assignment operators
    wxColour(const wxColour& rCol);
    wxColour(const wxColour* pCol);
    wxColour&operator = (const wxColour& rCol);

    // Dtor
    virtual ~wxColour();

    // Accessors
    virtual bool IsOk(void) const { return m_bIsInit; }

    unsigned char Red(void) const { return m_cRed; }
    unsigned char Green(void) const { return m_cGreen; }
    unsigned char Blue(void) const { return m_cBlue; }

    // Comparison
    bool operator == (const wxColour& rColour) const
    {
        return (m_bIsInit == rColour.m_bIsInit
                && m_cRed   == rColour.m_cRed
                && m_cGreen == rColour.m_cGreen
                && m_cBlue  == rColour.m_cBlue
               );
    }

    bool operator != (const wxColour& rColour) const { return !(*this == rColour); }

    WXCOLORREF GetPixel(void) const { return m_vPixel; }


private:

    // Helper function
    void Init();

    bool           m_bIsInit;
    unsigned char  m_cRed;
    unsigned char  m_cBlue;
    unsigned char  m_cGreen;

    virtual void
    InitRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

public:
    WXCOLORREF                      m_vPixel ;
private:
    DECLARE_DYNAMIC_CLASS(wxColour)
}; // end of class wxColour

#endif
  // _WX_COLOUR_H_
