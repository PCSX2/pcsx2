/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2019  PCSX2 Dev Team
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

#include "PadData.h"
#include "System.h"


#ifndef DISABLE_RECORDING
struct InputRecordingHeader
{
	u8 version = 1;
	char emu[50] = "PCSX2-1.5.X";
	char author[255] = "";
	char gameName[255] = "";

public:
	void SetAuthor(wxString author);
	void SetGameName(wxString cdrom);
	void Init();
};

static const int RecordingHeaderSize = sizeof(InputRecordingHeader) + 4 + 4;

// Contains info about the starting point of the movie
struct InputRecordingSavestate
{
	// Whether we start from the savestate or from power-on
	bool fromSavestate = false;
};

class InputRecordingFile
{
public:
	InputRecordingFile() {}
	~InputRecordingFile() { Close(); }

	// Movie File Manipulation
	bool Open(const wxString fn, bool fNewOpen, bool fromSaveState);
	bool Close();
	bool WriteKeyBuf(const uint & frame, const uint port, const uint bufIndex, const u8 & buf);
	bool ReadKeyBuf(u8 & result, const uint & frame, const uint port, const uint bufIndex);

	// Controller Data
	void GetPadData(PadData & result_pad, unsigned long frame);
	bool DeletePadData(unsigned long frame);
	bool InsertPadData(unsigned long frame, const PadData& key);
	bool UpdatePadData(unsigned long frame, const PadData& key);

	// Header
	InputRecordingHeader& GetHeader();
	unsigned long& GetMaxFrame();
	unsigned long& GetUndoCount();
	const wxString & GetFilename();

	bool WriteHeader();
	bool WriteMaxFrame();
	bool WriteSaveState();

	bool ReadHeaderAndCheck();
	void UpdateFrameMax(unsigned long frame);
	void AddUndoCount();

private:
	static const int RecordingSavestateHeaderSize = sizeof(bool);
	static const int RecordingBlockHeaderSize = 0;
	static const int RecordingBlockDataSize = 18 * 2;
	static const int RecordingBlockSize = RecordingBlockHeaderSize + RecordingBlockDataSize;
	static const int RecordingSeekpointFrameMax = sizeof(InputRecordingHeader);
	static const int RecordingSeekpointUndoCount = sizeof(InputRecordingHeader) + 4;
	static const int RecordingSeekpointSaveState = RecordingSeekpointUndoCount + 4;

	// Movie File
	FILE * recordingFile = NULL;
	wxString filename = "";
	long GetBlockSeekPoint(const long & frame);

	// Header
	InputRecordingHeader header;
	InputRecordingSavestate savestate;
	unsigned long MaxFrame = 0;
	unsigned long UndoCount = 0;
};
#endif
