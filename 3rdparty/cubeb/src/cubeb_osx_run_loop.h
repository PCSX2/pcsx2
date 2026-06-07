/*
 * Copyright © 2014 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

/* On OSX 10.6 and after, the notification callbacks from the audio hardware are
 * called on the main thread. Setting the kAudioHardwarePropertyRunLoop property
 * to null tells the OSX to use a separate thread for that.
 *
 * This has to be called only once per process, so it is in a separate header
 * for easy integration in other code bases.  */
#if defined(__cplusplus)
extern "C" {
#endif

void
cubeb_set_coreaudio_notification_runloop();

#if defined(__cplusplus)
}
#endif
