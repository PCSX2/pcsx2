<!--
This is technical documentation and hence didn't feel relevant for including in a release
Therefore, it's located here instead of the pcsx2/Docs folder, as there isn't any other location
For technical documentation in the repo
-->

# Input Recording File Schema

*   [Current Version (v2.X)](#current-version-v2x)
    *   [Overview](#overview)
        *   [Recording Types](#recording-types)
    *   [Header / Header Data](#header--header-data)
    *   [Frame Data](#frame-data)
*   [Legacy Version](#legacy-version)
    *   [Overview](#overview-1)
    *   [Legacy Header](#legacy-header)
    *   [Legacy Frame Data](#legacy-frame-data)

## Current Version (v2.X)

Normal Recording Extension - `.pir` (PS2 Input Recording)
Recording Macro Extension (Not yet Fully Implemented) - `.pirm` (PS2 Input Recording Macro)

### Overview

An improved file format from the legacy version, with future extensibillity in mind.  Includes better metadata to support backwards/forwards compatibility and removes arbitrary size limits on fields that vary.

#### Recording Types

| Type (enum value) | Description |
| ---- | ----------- |
| `INPUT_RECORDING_POWER_ON (0)` | Starts the recording from booting the game, this is preferred as save-states are volatile and may break across emulator versions!
| `INPUT_RECORDING_SAVESTATE (1)` | Starts the recording from a save-state stored along-side the recording file.  Volatile, but sometimes preferred for a quick recording / demo / etc.

<!-- | `INPUT_RECORDING_MACRO (2)` | **WIP** Not to be played back like a typical input recording, instead it can be played during recording creation / general use to quickly execute a series of inputs. -->

### Header / Header Data

| Version Added | Field | Description | Type | Size | Default |
| ------------- | ----- | ----------- | ---- | ---- | ------- |
| 2.0 | Magic String | File identification purposes | string | `"pcsx2-input-recording"` | -- |
| 2.0 | `m_file_version_major` | Major component of file version | unsigned byte | 1-byte | 2 |
| 2.0 | `m_file_version_minor` | Minor component of file version | unsigned byte | 1-byte | 0 |
| 2.0 | `m_offset_to_frame_counter` | Useful to jump to the frame counter within the header | signed int | 4-bytes | 0 |
| 2.0 | `m_offset_to_redo_counter` | Useful to jump to the redo counter within the header | signed int | 4-bytes | 0 |
| 2.0 | `m_offset_to_frame_data` | Useful to jump past the header and the beginning of the input data | signed int | 4-bytes | 0 |
| 2.0 | `m_emulator_version` | The version of PCSX2 used to create the recording | std::string (UTF-8) | Arbitrary size, null-terminated | `""` |
| 2.0 | `m_recording_author` | The author(s) of the input recording | std::string (UTF-8) | Arbitrary size, null-terminated | `""` |
| 2.0 | `m_game_name` | The name of the game the recording was created for | std::string (UTF-8) | Arbitrary m_game_name, null-terminated | `""` |
| 2.0 | `m_total_frames` | How many frames are a part of this input recording | signed `long` | 8-bytes | 0 |
| 2.0 | `m_redo_count` | How many times the input recording had a save-state loaded during creation | signed `long` | 8-bytes | 0 |
| 2.0 | `m_recording_type` | Identifies how the input recording should be handled / played-back / etc.  See the list above for all types | signed `long` | 8-bytes | 0 (`INPUT_RECORDING_POWER_ON`) |
| 2.0 | `m_redo_count` | How many times the input recording had a save-state loaded during creation | signed `long` | 8-bytes | 0 |
| 2.0 | `m_num_controllers_per_frame` | How many controllers's input data is stored per frame | unsigned byte | 1-byte | 2 |

### Frame Data

Similar to the legacy format, immediately after the header the frame data begins.  The difference is, in this format we support an arbitrary number of controllers per frame specified in the header, rather than a constant of `2`.  Each frame is composed of *same* number of controllers, and their respective PAD data

Each controller has 18-bytes of input data per frame from that is intercepted from the SIO interrupts.  These 18 bytes are as follows:

| Byte Index | Description | Notes |
| ---------- | ----------- | ----- |
| 0 | Pressed Bitfield (1/2):<ul><li>D-Pad Left</li><li>D-Pad Down</li><li>D-Pad Right</li><li>D-Pad Up</li><li>Start</li><li>R3</li><li>L3</li><li>Select</li></ul> | A bit being unset (`0`) indicates it is pressed
| 1 | Pressed Bitfield (2/2):<ul><li>Square</li><li>Cross</li><li>Circle</li><li>Triangle</li><li>L1</li><li>R1</li><li>L2</li><li>R2</li></ul> | A bit being unset (`0`) indicates it is pressed
| 2 | Right Analog X-Vector | 0-255 (127 is Neutral)
| 3 | Right Analog Y-Vector | 0-255 (127 is Neutral)
| 4 | Left Analog X-Vector | 0-255 (127 is Neutral)
| 5 | Left Analog Y-Vector | 0-255 (127 is Neutral)
| 6 | D-Pad Right (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 7 | D-Pad Left (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 8 | D-Pad Up (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 9 | D-Pad Down (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 10 | Triangle (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 11 | Circle (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 12 | Cross (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 13 | Square (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 14 | L1 (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 15 | R1 (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 16 | L2 (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 17 | R2 (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)

To further clarify/illustrate, this means the input data would be organized something like this:

*   Frame 0
    *   Controller Port 0 - 18 bytes
    *   ...
    *   Controller Port n - 18 bytes
*   Frame 1
    *   Controller Port 0 - 18 bytes
    *   ...
    *   Controller Port n - 18 bytes
*   ...and so on...

<!--
#### Input Recording Macro Differences

For the purposes of input recording macros, its beneficial to explicitly mark certain portions of input data is ignored.  This is because typically an input recording is all-or-nothing.  It either overwrites everything in the SIO buffer with the content of the recording file, or nothing at all.

Macros need to be more nuanced than this.  To do so, each byte that was described in the previous second is preceeded by a bool to mark whether or not it should be ignored.  Or in the event of the bitfields, it will store a bitfield of it's own to do the same.  This means that a macro stores twice as much data per frame:

| Byte Index | Description | Notes |
| ---------- | ----------- | ----- |
| 0 | Ignore Pressed Bitfield (1/2):<ul><li>D-Pad Left</li><li>D-Pad Down</li><li>D-Pad Right</li><li>D-Pad Up</li><li>Start</li><li>R3</li><li>L3</li><li>Select</li></ul> | A bit being set indicates it should be ignored
| 1 | Pressed Bitfield (1/2):<ul><li>D-Pad Left</li><li>D-Pad Down</li><li>D-Pad Right</li><li>D-Pad Up</li><li>Start</li><li>R3</li><li>L3</li><li>Select</li></ul> | A bit being unset (`0`) indicates it is pressed
| 2 | Ignore Pressed Bitfield (2/2):<ul><li>Square</li><li>Cross</li><li>Circle</li><li>Triangle</li><li>L1</li><li>R1</li><li>L2</li><li>R2</li></ul> | A bit being set indicates it should be ignored
| 3 | Pressed Bitfield (2/2):<ul><li>Square</li><li>Cross</li><li>Circle</li><li>Triangle</li><li>L1</li><li>R1</li><li>L2</li><li>R2</li></ul> | A bit being unset (`0`) indicates it is pressed
| 4 | Ignore Right Analog X-Vector | boolean
| 5 | Right Analog X-Vector | 0-255 (127 is Neutral)
| 6 | Ignore Right Analog Y-Vector | boolean
| 7 | Right Analog Y-Vector | 0-255 (127 is Neutral)
| 8 | Ignore Left Analog X-Vector | boolean
| 9 | Left Analog X-Vector | 0-255 (127 is Neutral)
| 10 | Ignore Left Analog Y-Vector | boolean
| 11 | Left Analog Y-Vector | 0-255 (127 is Neutral)
| 12 | Ignore D-Pad Right (Pressure) | boolean
| 13 | D-Pad Right (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 14 | Ignore D-Pad Left (Pressure) | boolean
| 15 | D-Pad Left (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 16 | Ignore D-Pad Up (Pressure) | boolean
| 17 | D-Pad Up (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 18 | Ignore D-Pad Down (Pressure) | boolean
| 19 | D-Pad Down (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 20 | Ignore Triangle (Pressure) | boolean
| 21 | Triangle (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 22 | Ignore Circle (Pressure) | boolean
| 23 | Circle (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 24 | Ignore Cross (Pressure) | boolean
| 25 | Cross (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 26 | Ignore Square (Pressure) | boolean
| 27 | Square (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 28 | Ignore L1 (Pressure) | boolean
| 29 | L1 (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 30 | Ignore R1 (Pressure) | boolean
| 31 | R1 (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 32 | Ignore L2 (Pressure) | boolean
| 33 | L2 (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 34 | Ignore R2 (Pressure) | boolean
| 35 | R2 (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
 -->

## Legacy Version

Extension - `.p2m2`

### Overview

Very basic file format, only supports 2 controller ports.  Basic metadata support in the header with upper-limits on string sizes.

### Legacy Header

| Field | Description | Type | Size |
| ----- | ----------- | ---- | ---- |
| `version` | Specifies version of recording file | unsigned byte | 1-byte |
| `emu` | The version of pcsx2 used when creating the recording | string | reserved length of 50 bytes |
| `author` | The author(s) of the input recording | string | reserved length of 255 bytes |
| `gameName` | Name of the game the input recording was made for | string | reserved length of 255 bytes |
| `totalFrames` | The total number of frames involved in the input recording | signed long | 8-bytes |
| `undoCount` | The total number of times a save-state was loaded when creating the recording | unsigned long | 8-bytes |
| `savestate` | Indicates if the input recording is supposed to be played from booting or from a save-state stored alongside the file | boolean | 1-byte |

### Legacy Frame Data

The frame data is laid out very simplistically.  Immediately after the header data, the frame data begins.  Each frame is composed of two controller port's respective PAD data.

Each controller has 18-bytes of input data per frame from that is intercepted from the SIO interrupts.  These 18 bytes are as follows:

| Byte Index | Description | Notes |
| ---------- | ----------- | ----- |
| 0 | Pressed Bitfield (1/2):<ul><li>D-Pad Left</li><li>D-Pad Down</li><li>D-Pad Right</li><li>D-Pad Up</li><li>Start</li><li>R3</li><li>L3</li><li>Select</li></ul> | A bit being unset (`0`) indicates it is pressed
| 1 | Pressed Bitfield (2/2):<ul><li>Square</li><li>Cross</li><li>Circle</li><li>Triangle</li><li>L1</li><li>R1</li><li>L2</li><li>R2</li></ul> | A bit being unset (`0`) indicates it is pressed
| 2 | Right Analog X-Vector | 0-255 (127 is Neutral)
| 3 | Right Analog Y-Vector | 0-255 (127 is Neutral)
| 4 | Left Analog X-Vector | 0-255 (127 is Neutral)
| 5 | Left Analog Y-Vector | 0-255 (127 is Neutral)
| 6 | D-Pad Right (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 7 | D-Pad Left (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 8 | D-Pad Up (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 9 | D-Pad Down (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 10 | Triangle (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 11 | Circle (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 12 | Cross (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 13 | Square (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 14 | L1 (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 15 | R1 (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 16 | L2 (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)
| 17 | R2 (Pressure) | 0-255 (255 being the maximum pressure, and 0 being unpressed)

To further clarify/illustrate, this means the input data would be organized something like this:

*   Frame 0
    *   Controller Port 0 - 18 bytes
    *   Controller Port 1 - 18 bytes
*   Frame 1
    *   Controller Port 0 - 18 bytes
    *   Controller Port 1 - 18 bytes
*   ...and so on...
