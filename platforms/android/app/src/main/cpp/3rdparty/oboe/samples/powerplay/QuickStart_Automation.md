# PowerPlay Automation Guide

This guide explains how to control the PowerPlay audio player via ADB commands for automated testing of power-efficient audio features (PCM Offload).

## Quick Start

```bash
# Build and install
cd samples/powerplay
../../gradlew installDebug

# Play with PCM Offload enabled
adb shell am start -n com.google.oboe.samples.powerplay/.MainActivity \
    --es command play \
    --es perf_mode offload

# Monitor status
adb logcat -s PowerPlay:V
```

## Supported ADB Parameters

| Parameter            | Type    | Description                                                | Example                        |
| -------------------- | ------- | ---------------------------------------------------------- | ------------------------------ |
| `command`            | string  | Action: `play`, `pause`, `stop`                            | `--es command play`            |
| `perf_mode`          | string  | Performance mode: `none`, `lowlat`, `powersave`, `offload` | `--es perf_mode offload`       |
| `song_index`         | int     | Track index (0-2)                                          | `--ei song_index 1`            |
| `volume`             | int     | Volume percentage (0-100)                                  | `--ei volume 50`               |
| `background`         | boolean | Move app to background after starting                      | `--ez background true`         |
| `duration_ms`        | int     | Auto-stop after N milliseconds                             | `--ei duration_ms 10000`       |
| `use_mmap`           | boolean | Enable/disable MMAP audio path                             | `--ez use_mmap false`          |
| `buffer_frames`      | int     | Buffer size in frames (offload only)                       | `--ei buffer_frames 4096`      |

## Common Test Scenarios

### Test PCM Offload Mode
```bash
adb shell am start -n com.google.oboe.samples.powerplay/.MainActivity \
    --es command play \
    --es perf_mode offload
```

### Background Playback Test
```bash
adb shell am start -n com.google.oboe.samples.powerplay/.MainActivity \
    --es command play \
    --es perf_mode offload \
    --ez background true
```

### Timed Playback (10 seconds)
```bash
adb shell am start -n com.google.oboe.samples.powerplay/.MainActivity \
    --es command play \
    --es perf_mode offload \
    --ei duration_ms 10000
```

### Compare Performance Modes
```bash
# Low Latency
adb shell am start -n com.google.oboe.samples.powerplay/.MainActivity \
    --es command play --es perf_mode lowlat

# Power Saving (non-offload)
adb shell am start -n com.google.oboe.samples.powerplay/.MainActivity \
    --es command play --es perf_mode powersave

# PCM Offload
adb shell am start -n com.google.oboe.samples.powerplay/.MainActivity \
    --es command play --es perf_mode offload
```

### Stop Playback
```bash
adb shell am start -n com.google.oboe.samples.powerplay/.MainActivity \
    --es command stop
```

## Machine-Readable Status Logs

The app outputs status to logcat with the `PowerPlay` tag in a parseable format:

```
POWERPLAY_STATUS: PLAYING | SONG=0 | OFFLOAD=true | MMAP=true
POWERPLAY_STATUS: PAUSED
POWERPLAY_STATUS: STOPPED | REASON=AUTO_STOP
```

### Status Values
| Status            | Meaning                               |
| ----------------- | ------------------------------------- |
| `PLAYING`         | Audio playback is active              |
| `PAUSED`          | Audio playback is paused              |
| `STOPPED`         | Audio playback has stopped            |
| `ERROR`           | An error occurred                     |
| `OFFLOAD_REVOKED` | PCM Offload was revoked by the system |

### Log Monitoring
```bash
# Real-time monitoring
adb logcat -s PowerPlay:V

# Filter for status only
adb logcat -s PowerPlay:V | grep "POWERPLAY_STATUS"

# Save to file
adb logcat -s PowerPlay:V > powerplay_test.log
```

## Using the Test Action

For automation scripts, you can also use the dedicated test action:

```bash
adb shell am start -a com.google.oboe.samples.powerplay.TEST \
    --es command play \
    --es perf_mode offload
```

## Battery Testing

To measure power consumption with offload vs non-offload:

```bash
# Reset battery stats
adb shell dumpsys batterystats --reset

# Run offload test for 5 minutes
adb shell am start -n com.google.oboe.samples.powerplay/.MainActivity \
    --es command play \
    --es perf_mode offload \
    --ez background true \
    --ei duration_ms 300000

# Wait for test to complete, then capture stats
adb shell dumpsys batterystats > battery_offload.txt
```

## Troubleshooting

### Offload Not Activating
- Ensure device supports PCM Offload: check `AudioManager.isOffloadedPlaybackSupported()`
- Some devices only support offload with specific sample rates (usually 48kHz)
- Display must be off for some implementations

### App Doesn't Respond to Commands
- Make sure the app is installed: `adb shell pm list packages | grep powerplay`
- Check if activity is exported: `adb shell dumpsys package com.google.oboe.samples.powerplay`
