/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/sound.h
// Purpose:     wxSound class (loads and plays short Windows .wav files).
//              Optional on non-Windows platforms.
// Author:      Ryan Norton, Stefan Csomor
// Modified by:
// Created:     1998-01-01
// Copyright:   (c) Ryan Norton, Stefan Csomor
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SOUND_H_
#define _WX_SOUND_H_

#if wxUSE_SOUND

#include "wx/object.h"

class WXDLLIMPEXP_FWD_ADV wxSoundTimer;

class WXDLLIMPEXP_ADV wxSoundData
{
public :
    wxSoundData();
    virtual ~wxSoundData();

    virtual bool Play(unsigned int flags) = 0;
    // stops the sound and deletes the optional timer
    virtual void Stop();
    // can be called by a timer for repeated tasks during playback
    virtual void SoundTask();
    // mark this to be deleted
    virtual void MarkForDeletion();
    virtual bool IsMarkedForDeletion() const { return m_markedForDeletion; }

    // does the true work of stopping and cleaning up
    virtual void DoStop() = 0;
protected :
    void CreateAndStartTimer();

    unsigned int m_flags;
    wxSoundTimer* m_pTimer;
    bool m_markedForDeletion;
} ;

class WXDLLIMPEXP_ADV wxSound : public wxSoundBase
{
public:
    wxSound();
    wxSound(const wxString& fileName, bool isResource = false);
    wxSound(size_t size, const void* data);
    virtual ~wxSound();

    // Create from resource or file
    bool  Create(const wxString& fileName, bool isResource = false);
    // Create from data
    bool Create(size_t size, const void* data);

    bool IsOk() const { return m_data != NULL; }

    // Stop playing any sound
    static void Stop();

    // Returns true if a sound is being played
    static bool IsPlaying();

    // Notification when a sound has stopped
    static void SoundStopped(const wxSoundData* data);

protected:
    bool    DoPlay(unsigned flags) const;
    void    Init();

private:
    // data of this object
    class wxSoundData *m_data;

    wxDECLARE_NO_COPY_CLASS(wxSound);
};

#endif
#endif
    // _WX_SOUND_H_
