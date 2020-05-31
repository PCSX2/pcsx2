/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/dcbufcmn.cpp
// Purpose:     Buffered DC implementation
// Author:      Ron Lee, Jaakko Salli
// Modified by:
// Created:     Sep-20-2006
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

#include "wx/dcbuffer.h"

#ifndef WX_PRECOMP
    #include "wx/module.h"
#endif

// ============================================================================
// implementation
// ============================================================================

IMPLEMENT_DYNAMIC_CLASS(wxBufferedDC,wxMemoryDC)
IMPLEMENT_ABSTRACT_CLASS(wxBufferedPaintDC,wxBufferedDC)

// ----------------------------------------------------------------------------
// wxSharedDCBufferManager: helper class maintaining backing store bitmap
// ----------------------------------------------------------------------------

class wxSharedDCBufferManager : public wxModule
{
public:
    wxSharedDCBufferManager() { }

    virtual bool OnInit() { return true; }
    virtual void OnExit() { wxDELETE(ms_buffer); }

    static wxBitmap* GetBuffer(int w, int h)
    {
        if ( ms_usingSharedBuffer )
            return new wxBitmap(w, h);

        if ( !ms_buffer ||
                w > ms_buffer->GetWidth() ||
                    h > ms_buffer->GetHeight() )
        {
            delete ms_buffer;

            // we must always return a valid bitmap but creating a bitmap of
            // size 0 would fail, so create a 1*1 bitmap in this case
            if ( !w )
                w = 1;
            if ( !h )
                h = 1;

            ms_buffer = new wxBitmap(w, h);
        }

        ms_usingSharedBuffer = true;
        return ms_buffer;
    }

    static void ReleaseBuffer(wxBitmap* buffer)
    {
        if ( buffer == ms_buffer )
        {
            wxASSERT_MSG( ms_usingSharedBuffer, wxT("shared buffer already released") );
            ms_usingSharedBuffer = false;
        }
        else
        {
            delete buffer;
        }
    }

private:
    static wxBitmap *ms_buffer;
    static bool ms_usingSharedBuffer;

    DECLARE_DYNAMIC_CLASS(wxSharedDCBufferManager)
};

wxBitmap* wxSharedDCBufferManager::ms_buffer = NULL;
bool wxSharedDCBufferManager::ms_usingSharedBuffer = false;

IMPLEMENT_DYNAMIC_CLASS(wxSharedDCBufferManager, wxModule)

// ============================================================================
// wxBufferedDC
// ============================================================================

void wxBufferedDC::UseBuffer(wxCoord w, wxCoord h)
{
    wxCHECK_RET( w >= -1 && h >= -1, "Invalid buffer size" );

    if ( !m_buffer || !m_buffer->IsOk() )
    {
        if ( w == -1 || h == -1 )
            m_dc->GetSize(&w, &h);

        m_buffer = wxSharedDCBufferManager::GetBuffer(w, h);
        m_style |= wxBUFFER_USES_SHARED_BUFFER;
        m_area.Set(w,h);
    }
    else
        m_area = m_buffer->GetSize();

    SelectObject(*m_buffer);

    // now that the DC is valid we can inherit the attributes (fonts, colours,
    // layout direction, ...) from the original DC
    if ( m_dc && m_dc->IsOk() )
        CopyAttributes(*m_dc);
}

void wxBufferedDC::UnMask()
{
    wxCHECK_RET( m_dc, wxT("no underlying wxDC?") );
    wxASSERT_MSG( m_buffer && m_buffer->IsOk(), wxT("invalid backing store") );

    wxCoord x = 0,
            y = 0;

    // Ensure the scale matches the device
    SetUserScale(1.0, 1.0);

    if ( m_style & wxBUFFER_CLIENT_AREA )
        GetDeviceOrigin(&x, &y);

    // It's possible that the buffer may be bigger than the area that needs to
    // be drawn (the client size of the window is smaller than the bitmap, or
    // a shared bitmap has been reused for a smaller area, etc.) so avoid
    // blitting too much if possible, but only use the real DC size if the
    // wxBUFFER_VIRTUAL_AREA style is not set.
    int width = m_area.GetWidth(),
        height = m_area.GetHeight();

    if (!(m_style & wxBUFFER_VIRTUAL_AREA))
    {
        int widthDC,
            heightDC;
        m_dc->GetSize(&widthDC, &heightDC);
        width = wxMin(width, widthDC);
        height = wxMin(height, heightDC);
    }

    const wxPoint origin = GetLogicalOrigin();
    m_dc->Blit(-origin.x, -origin.y, width, height, this, -x, -y);
    m_dc = NULL;

    if ( m_style & wxBUFFER_USES_SHARED_BUFFER )
        wxSharedDCBufferManager::ReleaseBuffer(m_buffer);
}
