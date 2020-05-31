/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/sound.cpp
// Purpose:     wxSound
// Author:      Julian Smart
// Modified by: 2005-07-29: Vadim Zeitlin: redesign
// Created:     04/01/98
// Copyright:   (c) Julian Smart
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

#if defined(__BORLANDC__)
    #pragma hdrstop
#endif

#if wxUSE_SOUND

#include "wx/sound.h"
#include "wx/msw/private.h"

#include <mmsystem.h>

// ----------------------------------------------------------------------------
// wxSoundData
// ----------------------------------------------------------------------------

// ABC for different sound data representations
class wxSoundData
{
public:
    wxSoundData() { }

    // return true if we had been successfully initialized
    virtual bool IsOk() const = 0;

    // get the flag corresponding to our content for PlaySound()
    virtual DWORD GetSoundFlag() const = 0;

    // get the data to be passed to PlaySound()
    virtual LPCTSTR GetSoundData() const = 0;

    virtual ~wxSoundData() { }
};

// class for in-memory sound data
class wxSoundDataMemory : public wxSoundData
{
public:
    // we copy the data
    wxSoundDataMemory(size_t size, const void* buf);

    void *GetPtr() const { return m_waveDataPtr; }

    virtual bool IsOk() const { return GetPtr() != NULL; }
    virtual DWORD GetSoundFlag() const { return SND_MEMORY; }
    virtual LPCTSTR GetSoundData() const { return (LPCTSTR)GetPtr(); }

private:
    GlobalPtr m_waveData;
    GlobalPtrLock m_waveDataPtr;

    wxDECLARE_NO_COPY_CLASS(wxSoundDataMemory);
};

// class for sound files and resources
class wxSoundDataFile : public wxSoundData
{
public:
    wxSoundDataFile(const wxString& filename, bool isResource);

    virtual bool IsOk() const { return !m_name.empty(); }
    virtual DWORD GetSoundFlag() const
    {
        return m_isResource ? SND_RESOURCE : SND_FILENAME;
    }
    virtual LPCTSTR GetSoundData() const { return m_name.c_str(); }

private:
    const wxString m_name;
    const bool m_isResource;

    wxDECLARE_NO_COPY_CLASS(wxSoundDataFile);
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxSoundData-derived classes
// ----------------------------------------------------------------------------

wxSoundDataMemory::wxSoundDataMemory(size_t size, const void* buf)
                 : m_waveData(size),
                   m_waveDataPtr(m_waveData)
{
    if ( IsOk() )
        ::CopyMemory(m_waveDataPtr, buf, size);
}

wxSoundDataFile::wxSoundDataFile(const wxString& filename, bool isResource)
               : m_name(filename),
                 m_isResource(isResource)
{
    // check for file/resource existence?
}

// ----------------------------------------------------------------------------
// wxSound
// ----------------------------------------------------------------------------

wxSound::wxSound()
{
    Init();
}

wxSound::wxSound(const wxString& filename, bool isResource)
{
    Init();
    Create(filename, isResource);
}

wxSound::wxSound(size_t size, const void* data)
{
    Init();
    Create(size, data);
}

wxSound::~wxSound()
{
    Free();
}

void wxSound::Free()
{
    wxDELETE(m_data);
}

bool wxSound::CheckCreatedOk()
{
    if ( m_data && !m_data->IsOk() )
        Free();

    return m_data != NULL;
}

bool wxSound::Create(const wxString& filename, bool isResource)
{
    Free();

    m_data = new wxSoundDataFile(filename, isResource);

    return CheckCreatedOk();
}

bool wxSound::Create(size_t size, const void* data)
{
    Free();

    m_data = new wxSoundDataMemory(size, data);

    return CheckCreatedOk();
}

bool wxSound::DoPlay(unsigned flags) const
{
    if ( !IsOk() || !m_data->IsOk() )
        return false;

    DWORD flagsMSW = m_data->GetSoundFlag();
    HMODULE hmod = flagsMSW == SND_RESOURCE ? wxGetInstance() : NULL;

    // we don't want replacement default sound
    flagsMSW |= SND_NODEFAULT;

    // NB: wxSOUND_SYNC is 0, don't test for it
    flagsMSW |= (flags & wxSOUND_ASYNC) ? SND_ASYNC : SND_SYNC;
    if ( flags & wxSOUND_LOOP )
    {
        // looping only works with async flag
        flagsMSW |= SND_LOOP | SND_ASYNC;
    }

    return ::PlaySound(m_data->GetSoundData(), hmod, flagsMSW) != FALSE;
}

/* static */
void wxSound::Stop()
{
    ::PlaySound(NULL, NULL, 0);
}

#endif // wxUSE_SOUND

