[Home](README.md)

# Automated Testing

OboeTester can be used to measure the round trip latency and glitches.
It can be launched from a shell script by using an Android Intent.

Before running the app from an Intent, it should be launched manually and a Round Trip Latency test run. Then you can give permission for using the microphone to record the looped back sound, and give permission to write to external storage for saving the test result.

## Requirements

All tests require:

* host computer
* ADB installed
* ADB USB cable
  
The latency, glitch and data_paths tests also need:

* [loopback adapter](https://source.android.com/devices/audio/latency/loopback)
* a 3.5 mm jack on the phone*

\* If you don't have a 3.5 mm jack then you can use a USB-C to 3.5mm adapter.
In order to use ADB at the same time you will also need a USB switching device
or  use ADB/Wifi.

## Start App from Intent

The app can be started by sending a Start comment to the OboeTester class.
The app will run and the results will be written to a file.

    adb shell am start -n com.mobileer.oboetester/.MainActivity {parameters}

String parameters are sent using:

    --es {parameterName} {parameterValue}

For example:

    --es test latency

Integer parameters are sent using:

    --ei {parameterName} {parameterValue}

For example:

    --ei buffer_bursts 8

Boolean parameters are sent using:

    --ez {parameterName} {parameterValue}

For example:

    --ez use_input_presets false

## Parameters

There are two required parameters for all tests:

    --es test {latency, glitch, data_paths, input, output, cpu_load}
            The "latency" test will perform a Round Trip Latency test.
            It will request EXCLUSIVE mode for minimal latency.
            The "glitch" test will perform a single Glitch test.
            The "data_paths" test will verify input and output streams in many possible configurations.
            The "input" test will open and start an input stream.
            The "output" test will open and start an output stream.
            The "cpu_load" test will run the CPU LOAD activity.

    --es file {name of resulting file}

The file will be stored in a directory that can be written by OboeTester without any special permissions.
This is typically "/storage/emulated/0/Android/data/com.mobileer.oboetester/files/".

There are some optional parameter in common for all tests:

    --ef volume             {volume} // normalized volume in the range of 0.0 to 1.0
    --es volume_type        {"accessibility", "alarm", "dtmf", "music", "notification", "ring", "system", "voice_call"}
                            Stream type for the setStreamVolume() call. Default is "music".
    --ez background         {"true", 1, "false", 0} // if true then Oboetester will continue to run in the background
    --ez foreground_service {"true", 1, "false", 0} // if true then Oboetester will ask for record/play permissions via a foreground service
    --ez audio_focus        {"true", 1, "false", 0} // if true then Oboetester will request for audio focus. Default is true.

There are several optional parameters in common for glitch, latency, input, and output tests:

    --ei buffer_capacity    {capacity} // number of frames in the buffer, default is UNSPECIFIED.
    --ei buffer_bursts      {bursts}     // number of bursts in the buffer, 2 for "double buffered". Do not use together with buffer_frames to avoid conflict. When both set, buffer_bursts will be ignored.
    --ei buffer_frames      {frames}     // number of frames in the buffer, do not use together with buffer_bursts to avoid conflict. When both set, buffer_bursts will be ignored.
    --es in_api             {"unspecified", "opensles", "aaudio"}  // native input API, default is "unspecified"
    --es out_api            {"unspecified", "opensles", "aaudio"}  // native output API, default is "unspecified"
    --es in_channel_mask    {"mono", "stereo", "2.1", "tri", "triBack", "3.1", "2.0.2", "2.1.2", "3.0.2", "3.1.2", "quad", "quadSide", "surround", "penta", "5.1", "5.1Side", "6.1", "7.1", "5.1.2", "5.1.4", "7.1.2", "7.1.4", "9.1.4", "9.1.6", "frontBack"}
    --es out_channel_mask    {"mono", "stereo", "2.1", "tri", "triBack", "3.1", "2.0.2", "2.1.2", "3.0.2", "3.1.2", "quad", "quadSide", "surround", "penta", "5.1", "5.1Side", "6.1", "7.1", "5.1.2", "5.1.4", "7.1.2", "7.1.4", "9.1.4", "9.1.6", "frontBack"}
    --ei in_channels        {samples}    // number of input channels, default is 2. This is ignored if in_channel_mask is set.
    --ei out_channels       {samples}    // number of output channels, default is 2. This is ignored if out_channel_mask is set.
    --es in_format          {"pcm_16_bit", "pcm_float", "pcm_24_bit", "pcm_32_bit", "iec61937", "mp3"}
    --es out_format         {"pcm_16_bit", "pcm_float", "pcm_24_bit", "pcm_32_bit", "iec61937", "mp3"}
    --ei sample_rate        {hertz}
    --es in_perf            {"none", "lowlat", "powersave"}  // input performance mode, default is "lowlat"
    --es out_perf           {"none", "lowlat", "powersave", "powersave_offload"}  // output performance mode, default is "lowlat", "powersave_offload" will only be available for output stream
    --es out_usage          {"media", "voice_communication", "alarm", "notification", "game"} // default is media
    --es in_sharing         {"shared", "exclusive"} // input sharing mode, default is "exclusive"
    --es out_sharing        {"shared", "exclusive"} // output sharing mode, default is "exclusive"
    --ez in_use_mmap        {"true", 1, "false", 0} // if true then MMAP is allowed, if false then MMAP will be disabled
    --ez out_use_mmap       {"true", 1, "false", 0} // if true then MMAP is allowed, if false then MMAP will be disabled

There are some optional parameters in common for glitch, input, and output tests:

    --ei duration           {seconds}    // glitch test duration, default is 10 seconds
    --ez restart_if_closed  {"true", 1, "false", 0} // if true, restart stream if its closed or disconnected

There are several optional parameters for just the "glitch" test:

    --ef tolerance          {tolerance}  // amount of deviation from expected that is considered a glitch
                                         // Range of tolerance is 0.0 to 1.0. Default is 0.1. Note use of "-ef".
                            // input preset, default is "voicerec"
    --es in_preset          ("generic", "camcorder", "voicerec", "voicecomm", "unprocessed", "performance"}

There are several optional parameters for just the "data_paths" test. Note the  Note the use of "-ez" for the boolean parameters.

    --ez use_input_presets  {"true", 1, "false", 0}  // Whether to test various input presets.
    --ez use_all_sample_rates {"true", 1, "false", 0}  // Whether to test all sample rates. Note use of "-ez". Default is false
    --ei single_test_index  {testId}  // Index for testing one specific test

These parameters are used with the "data_paths" test starting with v2.5.11.

    --ez use_input_channel_masks {"true", 1, "false", 0}  // Whether to test the reported input channel MASKS. Default is false.
    --ez use_all_channel_counts {"true", 1, "false", 0}   // Whether to test all the supported channel COUNTS. Default is true.
    --ei output_channel_masks_level {0, 1, 2}    // Whether to test NONE=0, SOME=1, or ALL=2 channel masks. Default is false.

These parameters were used with the "data_paths" test prior to v2.5.11.

    --ez use_input_devices  {"true", 1, "false", 0}  // Whether to test various input devices.
    --ez use_output_devices {"true", 1, "false", 0}  // Whether to test various output devices.
    --ez use_all_output_channel_masks {"true", 1, "false", 0}  // Whether to test all output channel masks. Default is false.

There are some optional parameters for just the "output" test:

    --es signal_type        {sine, sawtooth, freq_sweep, pitch_sweep, white_noise} // type of sound to play, default is sine

There are some optional parameters for just the "cpu_load" test:

    --ez use_adpf         {true, false} // if true, use work boost from performance hints. Default is false.
    --ez use_workload     {true, false} // if true and using ADPF then report workload changes. Default is false.
    --ez scroll_graphics  {true, false} // if true then continually update the power scope. Default is false.
    --ez use_workload_increase_api {true, false} // if true and using ADPF, notify adpf with workload increase/reset apis. Default is false.

For example, a complete command for a "latency" test might be:

    adb shell am start -n com.mobileer.oboetester/.MainActivity \
        --es test latency \
        --ei buffer_bursts 2 \
        --ef volume 0.8 \
        --es volume_type music \
        --ei out_channels 1 \
        --es out_usage game \
        --es file latency20230608.txt

or for a "glitch" test:

    adb shell am start -n com.mobileer.oboetester/.MainActivity \
        --es test glitch \
        --es in_perf lowlat \
        --es out_perf lowlat \
        --es in_sharing exclusive \
        --es out_sharing exclusive \
        --ei buffer_bursts 2 \
        --ei sample_rate 48000 \
        --ef tolerance 0.123 \
        --ei in_channels 2 \
        --es file glitch20230608.txt

or for a "data_paths" test:

    adb shell am start -n com.mobileer.oboetester/.MainActivity \
        --es test data_paths \
        --ez use_input_presets true \
        --ez use_input_devices false \
        --ez use_output_devices true \
        --es file datapaths20230608.txt

## Interpreting Test Results

Test results are simple files with "name = value" pairs.
After running the test you can determine where the results file was written by entering:

    adb logcat | grep EXTFILE

The test results can be obtained using adb pull. For example:

    adb pull /storage/emulated/0/Android/data/com.mobileer.oboetester/files/glitch20230608.txt .

The beginning of the report is common to latency and glitch tests:

```
build.fingerprint = google/cheetah/cheetah:14/MASTER/eng.philbu.20230518.172104:userdebug/dev-keys
test.version = 2.5.1
test.version.code = 72
time.millis = 1686156503523
in.channels = 2
in.perf = ll
in.preset = voicerec
in.sharing = ex
in.api = aaudio
in.rate = 48000
in.device = 22
in.mmap = yes
in.rate.conversion.quality = 0
in.hardware.channels = 2
in.hardware.sampleRate = 48000
in.hardware.format = i32
in.burst.frames = 96
in.xruns = 0
out.channels = 1
out.perf = ll
out.usage = game
out.contentType = music
out.sharing = ex
out.api = aaudio
out.rate = 48000
out.device = 3
out.mmap = yes
out.rate.conversion.quality = 0
out.hardware.channels = 2
out.hardware.sampleRate = 48000
out.hardware.format = float
out.burst.frames = 96
out.buffer.size.frames = 192
out.buffer.capacity.frames = 1920
out.xruns = 0
```

### Latency Report

Each test also adds specific value. For "latency". If the test fails then some values will be unavailable.

Here is a report from a good test. The '#' comments were added for this document and are not in the report.

```
confidence = 0.892          # quality of the latency result between 0.0 and 1.0, higher is better
result.text = OK            # text equivalent of the result
latency.msec = 23.27        # round trip latency in milliseconds
latency.frames = 1117       # round trip latency in frames
latency.empty.msec = 19.27  # round trip latency if the top output buffer was empty
latency.empty.frames = 925  # same but translated to frames
rms.signal = 0.03142        # Root Mean Square of the signal, if it can be detected
rms.noise = 0.00262         # Root Mean Square of the background noise before the signal is detected
correlation = 0.975         # raw normalized cross-correlation peak
timestamp.latency.msec = 10.35 # latency based on timestamps
timestamp.latency.mad = 0.05   # Mean absolute deviation
timestamp.latency.count = 12   # number of measurements
reset.count = 1             # number of times the full duplex stream input underflowed and had to resynchronize
result = 0                  # 0 or a negative error
```

Here is a report from a test that failed because the output was muted. Note the latency.msec is
missing because it could not be measured.

    rms.signal = 0.00000
    rms.noise = 0.00048
    reset.count = 3
    result = -96
    result.text = ERROR_CONFIDENCE
    confidence =  0.009

### Glitch Report

Here is a report from a good test. The '#' comments were added for this document and are not in the report.

    tolerance = 0.123
    state = LOCKED
    unlocked.frames = 2528   # frames spent trying to lock onto the signal
    locked.frames = 384084   # frames spent locked onto a good signal with no glitches
    glitch.frames = 0        # frames spent glitching or recovering from a glitch
    reset.count = 208        # number of times the full duplex stream input underflowed and had to resynchronize
    peak.amplitude = 0.057714  # peak amplitude of the input signal, between 0.0 and 1.0
    signal.noise.ratio.db =  96.3
    time.total =     9.96 seconds  # close to your specified duration
    time.no.glitches =     9.96    # time we have been running with no glitches
    max.time.no.glitches =     9.96 # max time with no glitches
    glitch.count = 0               # number of glitch events, actual number may be higher if close together

Here is a report from a test that failed because the output was muted. Note the glitch.count is
missing because it could not be measured.

    state = WAITING_FOR_SIGNAL
    unlocked.frames = 0
    locked.frames = 0
    glitch.frames = 0
    reset.count = 1
    time.total =     9.95 seconds

### Data Paths Report

The report first goes through the info about the specific device before going through input preset tests,
input devices tests, and output tests.
Each will show the specific configuration of a test before showing whether it passed or failed.
At the end of the report, an analysis of the failed tests will be given
followed by the number of passed, failed, and skipped tests.
