# Test Output

DRAFT for testing image embedding.


<table style="width:100%">
  <tr>
    <th>Notes</th>
    <th>Screenshot</th> 
  </tr>
  <tr>
    <td>
      Tap on the green bar to hide or show the detailed settings dialog.<br/>
      The resulting setting will displayed on the far right when the stream is opened.<br/>
      API: select between OpenSL ES or AAudio (default)<br/>
      Device: setect output device by type.<br/>
      Format: note that the 24 and 32-bit formats are only supported in Android 12+. MP3 is only supported on Android 16+. The test uses a 44.1kHz stereo MP3 file for playback. The stream must be configured as 44.1kHz stereo selecting MP3 format.<br/>
      MMAP: will be disabled if device does not support MMAP<br/>
      Effect: will enable a simple effect, may prevent LOW_LATENCY<br/>
      Convert: conversion done in Oboe may allow you to get LOW_LATENCY<br/>
      SRC: sample rate conversion quality<br/>
      <br/><br/><br/><br/><br/><br/><br/><br/>
    </td>
    <td><img src="/apps/OboeTester/docs/images/test_output.png" width=400></td>
  </tr>
</table>




