/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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
#include <array>
#include "PadData.h"

enum class InputRecordingStartType : char
{
	UnspecifiedBoot = -1,
	FullBoot,
	FastBoot,
	Savestate,
};

// Handles all operations on the input recording file
class InputRecordingFile
{
	struct InputRecordingFileHeader
	{
		friend class InputRecordingFile;
		void Init() noexcept;
		void SetEmulatorVersion();
		void SetAuthor(wxString author);
		void SetGameName(wxString gameName);
		bool ReadHeader(FILE* m_recordingFile);

	private:
		u8 m_fileVersion = 1;
		std::array<char, 50> m_emulatorVersion;
		std::array<char, 255> m_author;
		std::array<char, 255> m_gameName;
		// An signed 32-bit frame limit is equivalent to 1.13 years of continuous 60fps footage
		long m_totalFrames = 0;
		unsigned long m_redoCount = 0;
		InputRecordingStartType m_startType = InputRecordingStartType::FullBoot;
		wxByte m_pads = 0;
		static const int s_seekpointTotalFrames = 561;
		static const int s_seekpointRedoCount = s_seekpointTotalFrames + 4;
		static const int s_seekpointPads = s_seekpointRedoCount + 5; // Skips savestate: seekpointRedoCount + 4
	};

public:
	//
	// Header-related functions
	//
	// Retieve the version of the current p2m2 file
	// 1 - No Multitap (Pads 1A & 2A only)
	// 2 - Multitap
	u8 GetFileVersion() const noexcept;
	// Retrieve the version of PCSX2 that the recording originated from
	const char* GetEmulatorVersion() const noexcept;
	// Retrieve the orginal author of the recording
	const char* GetAuthor() const noexcept;
	// Retrieve the name of the game/iso that the file is paired with
	const char* GetGameName() const noexcept;
	// Retrieve the maximum number of frames, or in other words, the length of the recording
	long GetTotalFrames() const noexcept;
	// Retrieve the number of times a save-state has been loaded while recording this movie
	// this is also often referred to as a "re-record"
	unsigned long GetRedoCount() const noexcept;
	// Retrieve how the recording will load its the first frame
	InputRecordingStartType GetStartType() const noexcept;
	// Retrieve whether the recording begins from a savestate
	bool FromSavestate() const noexcept;
	// Retrieve the byte that represents the array of set pads
	wxByte GetPads() const noexcept;

	//
	// General File Related Functions
	//

	static const int s_controllerInputBytes = 18;
	~InputRecordingFile() { Close(); }

	// Closes the underlying input recording file, writing the header and
	// prepares for a possible new recording to be started
	bool Close();
	// Retrieve the input recording's filename (not the path)
	const wxString& GetFilename() const noexcept;
	// Retrieve the input recording's header which contains high-level metadata on the recording
	InputRecordingFileHeader& GetHeader() noexcept;
	// Retrieve the number of pads being used
	int GetPadCount() const noexcept;
	// Whether the selected pad is activated
	bool IsPortUsed(const int port) const noexcept;
	// Whether the selected port has slot 2, 3, or 4 activated
	bool IsMultitapUsed(const int port) const noexcept;
	// Whether the selected port has any slots activated
	bool IsSlotUsed(const int port, const int slot) const noexcept;
	// Increment the number of redo actions and commit it to the recording file
	void IncrementRedoCount();
	// Open an existing recording file
	bool OpenExisting(const wxString path);
	// Create and open a brand new input recording, either starting from a save-state or from
	// booting the game
	bool OpenNew(const wxString path, const char startType, const wxByte slots);
	// Updates the total frame counter and commit it to the recording file
	bool SetTotalFrames(const long frames) noexcept;
	// Reads the current frame's input data from the file in order to intercept and overwrite
	// the current frame's value from the emulator
	bool ReadKeyBuffer(u8& result, const u32 frame, const u32 seekOffset) const;
	// Persist the input recording file header's current state to the file
	bool WriteHeader() const;
	// Writes the current frame's input data to the file so it can be replayed
	bool WriteKeyBuffer(const u8 buf, const u32 frame, const u32 seekOffset) const;

private:
	int m_seekpointInputData;
	int m_recordingBlockSize;
	int m_padCount = 0;

	InputRecordingFileHeader m_header;
	wxString m_filename = "";
	FILE* m_recordingFile = nullptr;

	// Calculates the position of the current frame in the input recording
	u64 getRecordingBlockSeekPoint(const u32 frame) const;
	bool open(const wxString path, const bool newRecording);
	bool verifyRecordingFileHeader();
};
#endif
