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

#include "System.h"
#include "Utilities/FileUtils.h"
#include "Recording/file/v1/InputRecordingFileV1.h"

#include <string>

/**
 * @brief Handles all operations on the input recording file.
 *        You can find documentation on this file format in `pcsx2/Recording/docs/recording-file-schema.md`
*/
class InputRecordingFileV2
{
public:
	inline static const std::string s_extension = ".pir";
	inline static const std::string s_extension_filter = "Input Recording Files (*.pir)|*.pir";
	inline static const std::string s_extension_macro = ".pirm";
	inline static const std::string s_extension_macro_filter = "Input Recording Macro Files (*.pirm)|*.pirm";

	enum class InputRecordingType
	{
		INPUT_RECORDING_POWER_ON = 0,
		INPUT_RECORDING_SAVESTATE = 1,
		INPUT_RECORDING_MACRO = 2
	};

private:
	struct InputRecordingFileHeader
	{
		/**
		 * @brief Read the input recording file's header metadata, initializing the struct
		 * @param file_stream Stream pointing at a valid input recording file
		 * @return Indicates success
		*/
		bool readHeader(std::fstream& file_stream);
		/**
		 * @brief Write the input recording header in it's entirety. This WILL truncate any existing data.  
		 *				If you need to re-write the header for an existing recording file, you're looking for either `replaceHeader` or `updateHeaderInplace`
		 * @param file_stream Stream initialized for a new input recording file
		 * @return Indicates success
		*/
		bool writeHeader(std::fstream& file_stream);
		/**
		 * @brief Re-write the entire header, but retain whatever originally followed the file (ex. input data).  This is a comparatively expensive operation.
		 * @param file_stream Stream pointing to a valid input recording file
		 * @param path The path of the current file is needed, so we can retain it to copy over the existing non-header data.
		 * @return Indicates success
		*/
		bool replaceHeader(std::fstream& file_stream, fs::path path);
		/**
		 * @brief Update in-place, only the header values that can be done so safely (those that are not variable size).
		 * @param file_stream Stream pointing to a valid input recording file
		 * @return Indicates success
		*/
		bool updateHeaderInplace(std::fstream& file_stream);

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
		std::string m_game_name = "";
		long m_total_frames = 0;
		long m_redo_count = 0;
		InputRecordingType m_recording_type = InputRecordingType::INPUT_RECORDING_POWER_ON;
		u8 m_num_controllers_per_frame = 2;
	};

public:
	InputRecordingFileV2() = default;
	~InputRecordingFileV2() { closeFile(); }

	/**
	 * @brief Converts a legacy V1 input recording file to the modern V2 format.
	 * @param legacy_file Valid legacy input recording
	 * @param legacy_path The file-path associated with the respective input recording file
	 * @return Indicates success
	*/
	bool convertFromV1(InputRecordingFileV1& legacy_file, fs::path legacy_path);
	/**
	 * @brief Creates a new input recording file
	 * @param path Desired file-path
	 * @param recording_author Author for the recording
	 * @param game_name Name of the game that's being recorded
	 * @param recording_type Indicates what how the recording should be played back / handled
	 * @return Indicates success
	*/
	bool createNewFile(const fs::path& path, const std::string& recording_author, const std::string& game_name, const InputRecordingType& recording_type);
	/**
	 * @brief Opens a valid, existing input recording
	 * @param path File-path where it is located
	 * @return Indicates success
	*/
	bool openExistingFile(const fs::path& path);
	/**
	 * @brief Gracefully closes the currently accessed input recording file
	 * @return Indicates success
	*/
	bool closeFile();

	/**
	 * @return The input recording's file-name, omitting the rest of the path.
	*/
	fs::path getFileName();
	/**
	 * @return The recording file's version in v{MAJOR}.{MINOR} format
	*/
	std::string getRecordingFileVersion();
	/**
	 * @return The emulator version associated with this recording
	*/
	std::string getEmulatorVersion();
	/**
	 * @return The author who originally created this recording
	*/
	std::string getAuthor();
	/**
	 * @return The name of the game this recording was created for
	*/
	std::string getGameName();
	/**
	 * @return The total frame count for this recording
	*/
	long getTotalFrames();
	/**
	 * @return The number of redos the recording underwent.  Essentially, the amount of times a save-state was loaded while creating it.
	*/
	long getRedoCount();
	/**
	 * @return The type of input recording
	*/
	InputRecordingType getRecordingType();
	/**
	 * @return If the recording should be played back from power-on/boot
	*/
	bool isFromBoot();
	/**
	 * @return If the recording should be played back from an associated savestate
	*/
	bool isFromSavestate();
	/**
	 * @return If the recording is a macro
	*/
	bool isMacro();
	/**
	 * @return How many controllers are recorded per frame (typically 2, or 8 with multi-tap)
	*/
	u8 numberOfControllersPerFrame();

	/**
	 * @brief Increment the recording's redo count
	*/
	void incrementRedoCount();
	/**
	 * @brief Increment the recording's frame counter
	*/
	void incrementFrameCounter();
	/**
	 * @brief Explicitly set the recording's frame counter
	 * @param num_frames The value to set the frame counter to
	*/
	void setFrameCounter(const long& num_frames);
	/**
	 * @brief Reads a single byte from the recording file into the provided buffer
	 * @param buffer The input buffer being read to
	 * @param frame The frame we are interested in
	 * @param port The controller port we are interested in
	 * @param buf_index The current position in the buffer
	 * @return Indicates success
	*/
	bool readInputIntoBuffer(u8& buffer, const uint& frame, const uint& port, const uint& buf_index);
	/**
	 * @brief Writes a single byte into the recording file from the provided buffer
	 * @param frame The frame we are interested in
	 * @param port The controller port we are interested in
	 * @param buf_index The current position in the buffer
	 * @param buffer The input buffer being written to
	 * @return Indicates success
	*/
	bool writeInputFromBuffer(const uint& frame, const uint& port, const uint& buf_index, const u8& buffer);
	/**
	 * @brief Writes a single byte into the recording macro file from the provided buffer
	 *			  The assumption is that this will be used to quickly spike out a macro with a controller, and then hand-edit.  
	 *				As such, all inputs are marked to NOT be ignored, just like a normal recording would be
	 * @param frame The frame we are interested in
	 * @param port The controller port we are interested in
	 * @param buf_index The current position in the buffer
	 * @param buffer The input buffer being written to
	 * @return Indicates success
	*/
	bool writeDefaultMacroInputFromBuffer(const uint& frame, const uint& port, const uint& buf_index, const u8& buffer);
	// TODO - add mechanisms to update the author / gameName / emulator version in the future
	// TODO - allow changing the number of controllers per frame (multi-tap support)
	// TODO - support inserting/deleting/etc of frames

private:
	static const int s_controller_input_bytes = 18;

	fs::path m_file_path;
	std::fstream m_file_stream;
	InputRecordingFileHeader m_header;

	/**
	 * @return The number of bytes stored per controller
	*/
	long bytesPerController();
	/**
	 * @return The number of bytes per frame
	*/
	long bytesPerFrame();
	/**
	 * @param frame The frame number we would want to seek to
	 * @return The byte offset to the beginning of the frame
	*/
	long getFrameSeekpoint(const uint& frame);
	/**
	 * @param frame_seekpoint The offset to the beginning of the frame
	 * @param port The controller port number 
	 * @param buf_index The position in the input buffer we are trying to read/write
	 * @return The precise offset within the frame to read/write
	*/
	long getOffsetWithinFrame(const long& frame_seekpoint, const uint& port, const uint& buf_index);
};

#endif