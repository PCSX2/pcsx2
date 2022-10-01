/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#include "cubeb_osx_run_loop.h"
#include "cubeb_log.h"
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/AudioHardware.h>
#include <CoreAudio/HostTime.h>
#include <CoreFoundation/CoreFoundation.h>
#include <cubeb/cubeb.h>

void
cubeb_set_coreaudio_notification_runloop()
{
  /* This is needed so that AudioUnit listeners get called on this thread, and
   * not the main thread. If we don't do that, they are not called, or a crash
   * occur, depending on the OSX version. */
  AudioObjectPropertyAddress runloop_address = {
      kAudioHardwarePropertyRunLoop, kAudioObjectPropertyScopeGlobal,
      kAudioObjectPropertyElementMaster};

  CFRunLoopRef run_loop = nullptr;

  OSStatus r;
  r = AudioObjectSetPropertyData(kAudioObjectSystemObject, &runloop_address, 0,
                                 NULL, sizeof(CFRunLoopRef), &run_loop);
  if (r != noErr) {
    LOG("Could not make global CoreAudio notifications use their own thread.");
  }
}
