/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "System.h"
#include "PadData.h"

// NOTE / TODOs for Version 2
// - Move fromSavestate, undoCount, and total frames into the header

// Handles all operations on the input recording file
class InputRecordingFile
{
	struct InputRecordingFileHeader
	{
		u8 m_fileVersion = 1;
		char m_emulatorVersion[50]{};
		char m_author[255]{};
		char m_gameName[255]{};

	public:
		void init() noexcept;
	} m_header;


public:
	void setEmulatorVersion();
	void setAuthor(const std::string& author);
	void setGameName(const std::string& cdrom);
	const char* getEmulatorVersion() const noexcept;
	const char* getAuthor() const noexcept;
	const char* getGameName() const noexcept;

	~InputRecordingFile() { close(); }

	// Closes the underlying input recording file, writing the header and
	// prepares for a possible new recording to be started
	bool close() noexcept;
	
	// The number of times a save-state has been loaded while recording this movie
	// this is also often referred to as a "re-record"
	
	// Whether or not this input recording starts by loading a save-state or by booting the game fresh
	bool fromSaveState() const noexcept;
	// Increment the number of undo actions and commit it to the recording file
	void incrementUndoCount();
	// Open an existing recording file
	bool openExisting(const std::string& path);
	// Create and open a brand new input recording, either starting from a save-state or from
	// booting the game
	bool openNew(const std::string& path, bool fromSaveState);
	// Reads the current frame's input data from the file in order to intercept and overwrite
	// the current frame's value from the emulator
	std::optional<PadData> readPadData(const uint frame, const uint port, const uint slot);
	// Updates the total frame counter and commit it to the recording file
	void setTotalFrames(u32 frames);
	// Persist the input recording file header's current state to the file
	bool writeHeader() const;
	// Writes the current frame's input data to the file so it can be replayed
	bool writePadData(const uint frame, const PadData data) const;


	// Retrieve the input recording's filename (not the path)
	const std::string& getFilename() const noexcept;
	unsigned long getTotalFrames() const noexcept;
	unsigned long getUndoCount() const noexcept;

	void logRecordingMetadata();
	std::vector<PadData> bulkReadPadData(u32 frameStart, u32 frameEnd, const uint port);

private:
	static constexpr size_t s_controllerPortsSupported = 2;
	static constexpr size_t s_controllerInputBytes = 18;
	static constexpr size_t s_inputBytesPerFrame = s_controllerInputBytes * s_controllerPortsSupported;
	// TODO - version 2, this could be greatly simplified if everything was in the header
	// + 4 + 4 is the totalFrame and undoCount values
	static constexpr size_t s_headerSize = sizeof(InputRecordingFileHeader) + 4 + 4;
	// DEPRECATED / Slated for Removal
	static constexpr size_t s_recordingSavestateHeaderSize = sizeof(bool);
	static constexpr size_t s_seekpointTotalFrames = sizeof(InputRecordingFileHeader);
	static constexpr size_t s_seekpointUndoCount = sizeof(InputRecordingFileHeader) + 4;
	static constexpr size_t s_seekpointSaveStateHeader = s_seekpointUndoCount + 4;

	std::string m_filename = "";
	FILE* m_recordingFile = nullptr;
	bool m_savestate = false;

	// An signed 32-bit frame limit is equivalent to 1.13 years of continuous 60fps footage
	unsigned long m_totalFrames = 0;
	unsigned long m_undoCount = 0;

	// Calculates the position of the current frame in the input recording
	size_t getRecordingBlockSeekPoint(const u32 frame) const noexcept;
	bool verifyRecordingFileHeader();
};
