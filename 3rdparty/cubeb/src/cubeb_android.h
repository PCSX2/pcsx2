#ifndef CUBEB_ANDROID_H
#define CUBEB_ANDROID_H

#ifdef __cplusplus
extern "C" {
#endif
// If the latency requested is above this threshold, this stream is considered
// intended for playback (vs. real-time). Tell Android it should favor saving
// power over performance or latency.
// This is around 100ms at 44100 or 48000
const uint16_t POWERSAVE_LATENCY_FRAMES_THRESHOLD = 4000;

#ifdef __cplusplus
};
#endif

#endif // CUBEB_ANDROID_H
