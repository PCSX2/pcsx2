/////////////////////////////////////////////////////////////////////////////
// Name:        wx/cocoa/sound.h
// Purpose:     wxSound class (loads and plays short Windows .wav files).
//              Optional on non-Windows platforms.
// Authors:     David Elliott, Ryan Norton
// Modified by:
// Created:     2004-10-02
// Copyright:   (c) 2004 David Elliott, Ryan Norton
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_COCOA_SOUND_H_
#define _WX_COCOA_SOUND_H_

#include "wx/object.h"
#include "wx/cocoa/ObjcRef.h"

class WXDLLIMPEXP_ADV wxSound : public wxSoundBase
{
public:
    wxSound()
    :   m_cocoaNSSound(NULL)
    {}
    wxSound(const wxString& fileName, bool isResource = false)
    :   m_cocoaNSSound(NULL)
    {   Create(fileName, isResource); }
    wxSound(size_t size, const void* data)
    :   m_cocoaNSSound(NULL)
    {   LoadWAV(data,size,true); }
    wxSound(const wxSound& sound); // why not?
    virtual ~wxSound();

public:
    bool Create(const wxString& fileName, bool isResource = false);
    bool IsOk() const
    {   return m_cocoaNSSound; }
    static void Stop();
    static bool IsPlaying();

    void SetNSSound(WX_NSSound cocoaNSSound);
    inline WX_NSSound GetNSSound()
    {   return m_cocoaNSSound; }
protected:
    bool DoPlay(unsigned flags) const;
    bool LoadWAV(const void* data, size_t length, bool copyData);
private:
    WX_NSSound m_cocoaNSSound;
    static const wxObjcAutoRefFromAlloc<struct objc_object *> sm_cocoaDelegate;
};

#endif //ndef _WX_COCOA_SOUND_H_
