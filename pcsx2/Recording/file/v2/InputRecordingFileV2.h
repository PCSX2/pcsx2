/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#ifndef DISABLE_RECORDING

// ---- Binary File Layout
// ---- Header
// v2.0 - Magic String = "pcsx2-input-recording"
// v2.0 - (u8) File Version major
// v2.0 - (u8) File Version minor
// v2.0 - (int) Offset to Header Metadata
// v2.0 - (int) Offset to Frame Counter
// v2.0 - (int) Offset to Redo Counter
// v2.0 - (int) Offset to Start of Frame Data
// ---- Header Metadata
// v2.0 - (u8) emulator version major
// v2.0 - (u8) emulator version minor
// v2.0 - (u8) emulator version patch
// v2.0 - (string) emulator version name
// v2.0 - (string) recording author
// v2.0 - (string) game name
// v2.0 - (long) - total frames
// v2.0 - (long) - redo count
// v2.0 - (int) - recording type (enum)
// v2.0 - (u8) - number of controllers per frame
// ---- Frame Data (Input Recording Type (Power-On or Savestate))
// v2.0 - controller[0] - PadData (see below)
// ...
// v2.0 - controller[num_controllers_per_frame] - PadData (see below)
// ...
// ---- Frame Data (Input Recording Macro Type)
// v2.0 - controller[0] - PadData (see below), each value is preceeded by a bool to indicate if it should be ignored or not
// ...
// v2.0 - controller[num_controllers_per_frame] - PadData (see below), each value is preceeded by a bool to indicate if it should be ignored or not
// ...

// ---- PadData
// ---- Pressed Flags Bitfield - (0 means pressed!)
// Left, Down, Right, Up, Start, R3, L3, Select
// ---- Pressed Flags Bitfield - (0 means pressed!)
// Square, Cross, Circle, Triangle, R1, L1, R2, L2
// ---- Analog Sticks Bytes
// Right Analog X
// Right Analog Y
// Left Analog X
// Left Analog Y
// ---- Pressure Bytes
// Right, Left, Up, Down, Triangle, Circle, Cross, Square, L1, R1, L2, R2

#include "System.h"
#include "Utilities/FileUtils.h"
#include "Recording/file/v1/InputRecordingFileV1.h"

#include <string>

class InputRecordingFileV2
{
public:
	inline static const std::string s_extension = ".pir";
	inline static const std::string s_extension_filter = "Input Recording Files (*.pir)|*.pir";
	inline static const std::string s_extension_macro = ".pirm";
	inline static const std::string s_extension_macro_filter = "Input Recording Macro Files (*.pirm)|*.pirm";
	
	inline static const int s_controller_bytes_per_frame = 18;

	enum class InputRecordingType
	{
		INPUT_RECORDING_POWER_ON = 0,
		INPUT_RECORDING_SAVESTATE = 1,
		INPUT_RECORDING_MACRO = 2
	};

private:
	class InputRecordingFileHeader
	{
	public:
		friend InputRecordingFileV2;

		InputRecordingFileHeader() = default;

		bool readHeader(std::fstream& file_stream);
		bool writeHeader(std::fstream& file_stream);
		bool replaceHeader(std::fstream& file_stream, fs::path path);
		/**
		 * @brief Write only the values to the header that won't potentially resize the file.
		 * @param fileStream 
		 * @return 
		*/
		bool updateHeaderInplace(std::fstream& file_stream);

	private:
		// --- Initial Header Data
		inline static const std::string s_magic_string = "pcsx2-input-recording";
		// TODO - a checksum / hash of the header contents would be ideal to prevent bit-rot style errors
		u8 m_file_version_major = 2;
		u8 m_file_version_minor = 0;
		// - Useful Offsets
		int m_offset_to_frame_counter = 0;
		int m_offset_to_redo_counter = 0;
		int m_offset_to_frame_data = 0;

		// --- Fields
		std::string m_emulator_version = "";
		std::string m_recording_author = "";
		// TODO - test unicode game names
		std::string m_game_name = "";
		// An signed 32-bit frame limit is equivalent to 1.13 years of continuous 60fps footage
		long m_total_frames = 0;
		long m_redo_count = 0;
		InputRecordingType m_recording_type = InputRecordingType::INPUT_RECORDING_POWER_ON;
		u8 m_num_controllers_per_frame = 2;
	};

public:
	InputRecordingFileV2() = default;
	~InputRecordingFileV2() { closeFile(); }

	bool convertFromV1(InputRecordingFileV1& legacy_file, fs::path legacy_path);
	bool createNewFile(const fs::path& path, const std::string& recording_author, const std::string& game_name, const InputRecordingType& recording_type);
	bool openExistingFile(const fs::path& path);
	bool closeFile();
	fs::path getFileName();

	std::string getRecordingFileVersion();
	std::string getEmulatorVersion();
	std::string getAuthor();
	std::string getGameName();
	long getTotalFrames();
	long getRedoCount();
	InputRecordingType getRecordingType();
	bool isFromBoot();
	bool isMacro();
	bool isFromSavestate();
	u8 numberOfControllersPerFrame();

	void incrementRedoCount();
	void incrementFrameCounter();
	void setFrameCounter(const long& num_frames);
	// TODO - add mechanisms to update the author / gameName / emulator version in the future
	// TODO - allow changing the number of controllers per frame (multi-tap support)
	bool readInputIntoBuffer(u8& result, const uint& frame, const uint& port, const uint& buf_index);
	bool writeInputFromBuffer(const uint& frame, const uint& port, const uint& buf_index, const u8& buf);
	bool writeDefaultMacroInputFromBuffer(const uint& frame, const uint& port, const uint& buf_index, const u8& buf);
	// TODO - support inserting/deleting/etc of frames

private:
	static const int s_controller_input_bytes = 18;

	fs::path m_file_path;
	std::fstream m_file_stream;
	InputRecordingFileHeader m_header;

	long bytesPerController();
	long bytesPerFrame();
	long getFrameSeekpoint(const uint& frame);
	long getOffsetWithinFrame(const long& frame_seekpoint, const uint& port, const uint& buf_index);
};

#endif