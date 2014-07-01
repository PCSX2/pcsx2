/////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/sound.h
// Purpose:     wxSound class (loads and plays short Windows .wav files).
//              Optional on non-Windows platforms.
// Author:      David Webster
// Modified by:
// Created:     10/17/99
// Copyright:   (c) David Webster
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SOUND_H_
#define _WX_SOUND_H_

#include "wx/object.h"

class wxSound : public wxSoundBase
{
public:
  wxSound();
  wxSound(const wxString& fileName, bool isResource = FALSE);
  wxSound(size_t size, const void* data);
  virtual ~wxSound();

public:
  // Create from resource or file
  bool  Create(const wxString& fileName, bool isResource = FALSE);
  // Create from data
  bool Create(size_t size, const void* data);

  bool  IsOk() const { return (m_waveData ? TRUE : FALSE); }

protected:
  bool  Free();

  bool  DoPlay(unsigned flags) const;

private:
  wxByte* m_waveData;
  int   m_waveLength;
  bool  m_isResource;
};

#endif
    // _WX_SOUND_H_
