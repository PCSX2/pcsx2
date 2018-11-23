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

struct InputRecordingHeader
{
	u8 version = 1;
	char emu[50] = "PCSX2-1.5.X";
	char author[255] = "";
	char gameName[255] = "";

public:
	void setAuthor(wxString author);
	void setGameName(wxString cdrom);
	void init();
};

// Contains info about the starting point of the movie
struct InputRecordingSavestate
{
	// Whether we start from the savestate or from power-on
	bool fromSavestate = false;
};


class InputRecordingFile {
public:
	InputRecordingFile() {}
	~InputRecordingFile() { Close(); }

	// Movie File Manipulation
	bool Open(const wxString fn, bool fNewOpen, bool fromSaveState);
	bool Close();
	bool writeKeyBuf(const uint & frame, const uint port, const uint bufIndex, const u8 & buf);
	bool readKeyBuf(u8 & result, const uint & frame, const uint port, const uint bufIndex);

	// Controller Data
	void getPadData(PadData & result_pad, unsigned long frame);
	bool DeletePadData(unsigned long frame);
	bool InsertPadData(unsigned long frame, const PadData& key);
	bool UpdatePadData(unsigned long frame, const PadData& key);

	// Header
	InputRecordingHeader& getHeader() { return header; }
	unsigned long& getMaxFrame() { return MaxFrame; }
	unsigned long& getUndoCount() { return UndoCount; }
	const wxString & getFilename() { return filename; }

	bool writeHeader();
	bool writeMaxFrame();
	bool writeSaveState();

	bool readHeaderAndCheck();
	void updateFrameMax(unsigned long frame);
	void addUndoCount();

private:
	// Movie File
	FILE * recordingFile = NULL;
	wxString filename = "";
	long _getBlockSeekPoint(const long & frame);

	// Header
	InputRecordingHeader header;
	InputRecordingSavestate savestate;
	unsigned long  MaxFrame = 0;
	unsigned long  UndoCount = 0;
};
