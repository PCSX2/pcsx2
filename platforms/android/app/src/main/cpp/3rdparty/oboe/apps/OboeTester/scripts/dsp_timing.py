#!/usr/bin/python2.7
"""
Copyright (C) 2019 The Android Open Source Project

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

# Run OboeTester with progressive timing changes
# to measure the DSP timing profile.
# Print a CSV table of offsets and glitch counts.
#
# Run Automated Test using an Intent
# https://github.com/google/oboe/blob/main/apps/OboeTester/docs/AutomatedTesting.md

import array
import collections
import os
import os.path
import sys
import subprocess
import time

from datetime import datetime

kPropertyOutputOffset = "aaudio.out_mmap_offset_usec"
kMinPeakAmplitude = 0.04
kOffsetMin = 0000
kOffsetMax = 4000
kOffsetIncrement = 100
kOutputFileBase = "/sdcard/dsp_timing_"
gOutputFile = kOutputFileBase + "now.txt"

def launchLatencyTest():
    command = ["adb", "shell", "am", \
               "start", "-n", "com.mobileer.oboetester/.MainActivity", \
               "--es", "test", "latency", \
               "--es", "file", gOutputFile, \
               "--ei", "buffer_bursts", "1"]
    return subprocess.check_output(command)

def launchGlitchTest():
    command = ["adb", "shell", "am", \
               "start", "-n", "com.mobileer.oboetester/.MainActivity", \
               "--es", "test", "glitch", \
               "--es", "file", gOutputFile, \
               "--es", "in_perf", "lowlat", \
               "--es", "out_perf", "lowlat", \
               "--es", "in_sharing", "exclusive", \
               "--es", "out_sharing", "exclusive", \
               "--ei", "buffer_bursts", "1"]
    return subprocess.check_output(command)

def setAndroidProperty(property, value):
    return subprocess.check_output(["adb", "shell", "setprop", property, value])

def getAndroidProperty(property):
    return subprocess.check_output(["adb", "shell", "getprop", property])

def setOutputOffset(offset):
    setAndroidProperty(kPropertyOutputOffset, str(offset))

def getOutputOffset():
    offsetText = getAndroidProperty(kPropertyOutputOffset).strip()
    if len(offsetText) == 0:
        return 0;
    return int(offsetText)

def loadNameValuePairsFromFile(filename):
    myvars = {}
    with open(filename) as myfile:
        for line in myfile:
            name, var = line.partition("=")[::2]
            myvars[name.strip()] = var
    return myvars

def loadNameValuePairsFromString(text):
    myvars = {}
    listOutput = text.splitlines()
    for line in listOutput:
            name, var = line.partition("=")[::2]
            myvars[name.strip()] = var
    return myvars

def waitForTestResult():
    testOutput = ""
    for i in range(10):
        if subprocess.call(["adb", "shell", "ls", gOutputFile, "2>/dev/null"]) == 0:
            testOutput = subprocess.check_output(["adb", "shell", "cat", gOutputFile])
            break
        else:
            print str(i) + ": waiting until test finishes..."
            time.sleep(2)
    # print testOutput
    subprocess.call(["adb", "shell", "rm", gOutputFile])
    return loadNameValuePairsFromString(testOutput)

# volume too low?
# return true if bad
def checkPeakAmplitude(pairs):
    if not pairs.has_key('peak.amplitude'):
        print "ERROR no peak.amplitude"
        return True
    peakAmplitude = float(pairs['peak.amplitude'])
    if peakAmplitude < kMinPeakAmplitude:
        print "ERROR peakAmplitude = " + str(peakAmplitude) \
            + " < " + str(kMinPeakAmplitude) \
            + ", turn up volume"
        return True
    return False

def startOneTest(offset):
    print "=========================="
    setOutputOffset(offset)
    print "try offset = " + getAndroidProperty(kPropertyOutputOffset)
    subprocess.call(["adb", "shell", "rm", gOutputFile, "2>/dev/null"])

def runOneGlitchTest(offset):
    startOneTest(offset)
    print launchGlitchTest()
    pairs = waitForTestResult()
    if checkPeakAmplitude(pairs):
        return -1
    if not pairs.has_key('glitch.count'):
        print "ERROR no glitch.count"
        return -1
    return int(pairs['glitch.count'])

def runOneLatencyTest(offset):
    startOneTest(offset)
    print launchLatencyTest()
    pairs = waitForTestResult()
    if not pairs.has_key('latency.msec'):
        print "ERROR no latency.msec"
        return -1
    return float(pairs['latency.msec'])

def runGlitchSeries():
    offsets = array.array('i')
    glitches = array.array('i')
    for offset in range(kOffsetMin, kOffsetMax, kOffsetIncrement):
        offsets.append(offset)
        result = runOneGlitchTest(offset)
        glitches.append(result)
        if result < 0:
            break
        print "offset = " + str(offset) + ", glitches = " + str(result)
    print "offsetUs, glitches"
    for i in range(len(offsets)):
        print "  " + str(offsets[i]) + ", " + str(glitches[i])

# return true if bad
def checkLatencyValid():
    startOneTest(0)
    print launchLatencyTest()
    pairs = waitForTestResult()
    print "burst = " + pairs['out.burst.frames']
    capacity = int(pairs['out.buffer.capacity.frames'])
    if capacity < 0:
        print "ERROR capacity = " + str(capacity)
        return True
    sampleRate = int(pairs['out.rate'])
    capacityMillis = capacity * 1000.0 / sampleRate
    print "capacityMillis = " + str(capacityMillis)
    if pairs['in.mmap'].strip() != "yes":
        print "ERROR Not using input MMAP"
        return True
    if pairs['out.mmap'].strip() != "yes":
        print "ERROR Not using output MMAP"
        return True
    # Check whether we can change latency a moving the DSP pointer
    # past the CPU pointer and wrapping the buffer.
    latencyMin = runOneLatencyTest(kOffsetMin)
    latencyMax = runOneLatencyTest(kOffsetMax + 1000)
    print "latency = " + str(latencyMin) + " => " + str(latencyMax)
    if latencyMax < (latencyMin + (capacityMillis / 2)):
        print "ERROR Latency not affected by changing offset!"
        return True
    return False

def isRunningAsRoot():
    userName = subprocess.check_output(["adb", "shell", "whoami"]).strip()
    if userName != "root":
        print "WARNING: changing to 'adb root'"
        subprocess.call(["adb", "root"])
        userName = subprocess.check_output(["adb", "shell", "whoami"]).strip()
        if userName != "root":
            print "ERROR: cannot set 'adb root'"
            return False
    return True

def isMMapSupported():
    mmapPolicy = int(getAndroidProperty("aaudio.mmap_policy").strip())
    if mmapPolicy < 2:
        print "ERROR: AAudio MMAP not enabled, aaudio.mmap_policy = " + str(mmapPolicy)
        return False
    if checkLatencyValid():
        return False;
    return True

def isTimingSeriesSupported():
    if not isRunningAsRoot():
        return False
    if not isMMapSupported():
        return False
    return True

def main():
    global gOutputFile
    print "gOutputFile = " + gOutputFile
    now = datetime.now() # current date and time
    gOutputFile = kOutputFileBase \
            + now.strftime("%Y%m%d_%H%M%S") \
            + ".txt"
    print "gOutputFile = " + gOutputFile

    initialOffset = getOutputOffset()
    print "initial offset = " + str(initialOffset)

    print "Android version = " + \
            getAndroidProperty("ro.build.id").strip()
    print "    release " + \
            getAndroidProperty("ro.build.version.release").strip()
    if (isTimingSeriesSupported()):
        runGlitchSeries()
        setOutputOffset(initialOffset)

if __name__ == '__main__':
    main()

