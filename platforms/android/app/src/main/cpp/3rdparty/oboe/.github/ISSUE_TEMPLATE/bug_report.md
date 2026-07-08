---
name: Bug report
about: Create a report to help us improve Oboe
title: ''
labels: bug
assignees: ''

---

Android version(s):
Android device(s):
Oboe version:
App name used for testing: 
(Please try to reproduce the issue using the OboeTester or an Oboe sample.)
 
**Short description**
(Please only report one bug per Issue. Do not combine multiple bugs.)

**Steps to reproduce**

**Expected behavior**

**Actual behavior**

**Device**

Please list which devices have this bug.
If device specific, and you are on Linux or a Macintosh, connect the device and please share the result for the following script. This gets properties of the device. 

```
for p in \
    ro.product.brand ro.product.manufacturer ro.product.model \
    ro.product.device ro.product.cpu.abi ro.build.description \
    ro.hardware ro.hardware.chipname ro.arch "| grep aaudio";
    do echo "$p = $(adb shell getprop $p)"; done
```

**Any additional context**

If applicable, please attach a few seconds of an uncompressed recording of the sound in a WAV or AIFF file.
