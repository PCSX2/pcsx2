/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/overlaycmn.cpp
// Purpose:     common wxOverlay code
// Author:      Stefan Csomor
// Modified by:
// Created:     2006-10-20
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/overlay.h"
#include "wx/private/overlay.h"
#include "wx/dcclient.h"
#include "wx/dcmemory.h"

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxOverlay
// ----------------------------------------------------------------------------

wxOverlay::wxOverlay()
{
    m_impl = new wxOverlayImpl();
    m_inDrawing = false;
}

wxOverlay::~wxOverlay()
{
    delete m_impl;
}

bool wxOverlay::IsOk()
{
    return m_impl->IsOk();
}

void wxOverlay::Init( wxDC* dc, int x , int y , int width , int height )
{
    m_impl->Init(dc, x, y, width, height);
}

void wxOverlay::BeginDrawing( wxDC* dc)
{
    m_impl->BeginDrawing(dc);
    m_inDrawing = true ;
}

void wxOverlay::EndDrawing( wxDC* dc)
{
    m_impl->EndDrawing(dc);
    m_inDrawing = false ;
}

void wxOverlay::Clear( wxDC* dc)
{
    m_impl->Clear(dc);
}

void wxOverlay::Reset()
{
    wxASSERT_MSG(m_inDrawing==false,wxT("cannot reset overlay during drawing"));
    m_impl->Reset();
}


// ----------------------------------------------------------------------------
// wxDCOverlay
// ----------------------------------------------------------------------------

wxDCOverlay::wxDCOverlay(wxOverlay &overlay, wxDC *dc, int x , int y , int width , int height) :
    m_overlay(overlay)
{
    Init(dc, x, y, width, height);
}

wxDCOverlay::wxDCOverlay(wxOverlay &overlay, wxDC *dc) :
    m_overlay(overlay)
{
    int width;
    int height;
    dc->GetSize(&width,&height);
    Init(dc, 0, 0, width, height);
}

wxDCOverlay::~wxDCOverlay()
{
    m_overlay.EndDrawing(m_dc);
}

void wxDCOverlay::Init(wxDC *dc, int x , int y , int width , int height )
{
    m_dc = dc ;
    if ( !m_overlay.IsOk() )
    {
        m_overlay.Init(dc,x,y,width,height);
    }
    m_overlay.BeginDrawing(dc);
}

void wxDCOverlay::Clear()
{
    m_overlay.Clear(m_dc);
}

// ----------------------------------------------------------------------------
// generic implementation of wxOverlayImpl
// ----------------------------------------------------------------------------

#ifndef wxHAS_NATIVE_OVERLAY

wxOverlayImpl::wxOverlayImpl()
{
     m_window = NULL ;
     m_x = m_y = m_width = m_height = 0 ;
}

wxOverlayImpl::~wxOverlayImpl()
{
}

bool wxOverlayImpl::IsOk()
{
    return m_bmpSaved.IsOk() ;
}

void wxOverlayImpl::Init( wxDC* dc, int x , int y , int width , int height )
{
    m_window = dc->GetWindow();
    wxMemoryDC dcMem ;
    m_bmpSaved.Create( width, height );
    dcMem.SelectObject( m_bmpSaved );
    m_x = x ;
    m_y = y ;
    m_width = width ;
    m_height = height ;
    dcMem.Blit(0, 0, m_width, m_height,
        dc, x, y);
    dcMem.SelectObject( wxNullBitmap );
}

void wxOverlayImpl::Clear(wxDC* dc)
{
    wxMemoryDC dcMem ;
    dcMem.SelectObject( m_bmpSaved );
    dc->Blit( m_x, m_y, m_width, m_height , &dcMem , 0 , 0 );
    dcMem.SelectObject( wxNullBitmap );
}

void wxOverlayImpl::Reset()
{
    m_bmpSaved = wxBitmap();
}

void wxOverlayImpl::BeginDrawing(wxDC*  WXUNUSED(dc))
{
}

void wxOverlayImpl::EndDrawing(wxDC* WXUNUSED(dc))
{
}

#endif // !wxHAS_NATIVE_OVERLAY


