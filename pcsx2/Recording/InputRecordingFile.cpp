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

#include "PrecompiledHeader.h"

#include "App.h"
#include "Common.h"
#include "Counters.h"
#include "MainFrame.h"
#include "MemoryTypes.h"

#include "InputRecordingFile.h"

#ifndef DISABLE_RECORDING
long InputRecordingFile::GetBlockSeekPoint(const long & frame)
{
	if (savestate.fromSavestate)
	{
		return RecordingHeaderSize
			+ RecordingSavestateHeaderSize
			+ frame * RecordingBlockSize;
	}
	else
	{
		return RecordingHeaderSize + sizeof(bool) + (frame)*RecordingBlockSize;
	}
}

// Inits the new (or existing) input recording file
bool InputRecordingFile::Open(const wxString path, bool fNewOpen, bool fromSaveState)
{
	Close();
	wxString mode = L"rb+";
	if (fNewOpen)
	{
		mode = L"wb+";
		MaxFrame = 0;
		UndoCount = 0;
		header.Init();
	}
	recordingFile = wxFopen(path, mode);
	if ( recordingFile == NULL )
	{
		recordingConLog(wxString::Format("[REC]: Movie file opening failed. Error - %s\n", strerror(errno)));
		return false;
	}
	filename = path;

	if (fNewOpen)
	{
		if (fromSaveState)
		{
			savestate.fromSavestate = true;
			FILE* ssFileCheck = wxFopen(path + "_SaveState.p2s", "r");
			if (ssFileCheck != NULL)
			{
				wxCopyFile(path + "_SaveState.p2s", path + "_SaveState.p2s.bak", false);
				fclose(ssFileCheck);
			}
			StateCopy_SaveToFile(path + "_SaveState.p2s");
		}
		else
		{
			sApp.SysExecute();
		}
	}
	return true;
}

// Gracefully close the current recording file
bool InputRecordingFile::Close()
{
	if (recordingFile == NULL)
	{
		return false;
	}
	WriteHeader();
	WriteSaveState();
	fclose(recordingFile);
	recordingFile = NULL;
	filename = "";
	return true;
}

// Write savestate flag to file
bool InputRecordingFile::WriteSaveState() {
	if (recordingFile == NULL)
	{
		return false;
	}

	fseek(recordingFile, RecordingSeekpointSaveState, SEEK_SET);
	if (fwrite(&savestate.fromSavestate, sizeof(bool), 1, recordingFile) != 1)
	{
		return false;
	}

	return true;
}

// Write controller input buffer to file (per frame)
bool InputRecordingFile::WriteKeyBuf(const uint & frame, const uint port, const uint bufIndex, const u8 & buf)
{
	if (recordingFile == NULL)
	{
		return false;
	}

	long seek = GetBlockSeekPoint(frame) + RecordingBlockHeaderSize + 18 * port + bufIndex;

	if (fseek(recordingFile, seek, SEEK_SET) != 0
		|| fwrite(&buf, 1, 1, recordingFile) != 1)
	{
		return false;
	}

	fflush(recordingFile);
	return true;
}

// Read controller input buffer from file (per frame)
bool InputRecordingFile::ReadKeyBuf(u8 & result,const uint & frame, const uint port, const uint  bufIndex)
{
	if (recordingFile == NULL)
	{
		return false;
	}

	long seek = GetBlockSeekPoint(frame) + RecordingBlockHeaderSize + 18 * port + bufIndex;
	if (fseek(recordingFile, seek, SEEK_SET) != 0)
	{
		return false;
	}
	if (fread(&result, 1, 1, recordingFile) != 1)
	{
		return false;
	}

	return true;
}


void InputRecordingFile::GetPadData(PadData & result, unsigned long frame)
{
	result.fExistKey = false;
	if (recordingFile == NULL)
	{
		return;
	}

	long seek = GetBlockSeekPoint(frame) + RecordingBlockHeaderSize;
	if (fseek(recordingFile, seek, SEEK_SET) != 0
		|| fread(result.buf, 1, RecordingBlockDataSize, recordingFile) == 0)
	{
		return;
	}

	result.fExistKey = true;
}

bool InputRecordingFile::DeletePadData(unsigned long frame)
{
	if (recordingFile == NULL)
	{
		return false;
	}

	for (unsigned long i = frame; i < MaxFrame - 1; i++)
	{
		long seek1 = GetBlockSeekPoint(i+1) + RecordingBlockHeaderSize;
		long seek2 = GetBlockSeekPoint(i) + RecordingBlockHeaderSize;

		u8 buf[2][18];
		fseek(recordingFile, seek1, SEEK_SET);
		int rSize = fread(buf, 1, RecordingBlockDataSize, recordingFile);
		if (rSize != RecordingBlockDataSize)
		{
			recordingConLog(wxString::Format("[REC]: Error encountered when reading from file: Expected %d bytes, read %d instead.\n", RecordingBlockDataSize, rSize));
			return false;
		}
		fseek(recordingFile, seek2, SEEK_SET);
		rSize = fwrite(buf, 1, RecordingBlockDataSize, recordingFile);
		if (rSize != RecordingBlockDataSize)
		{
			recordingConLog(wxString::Format("[REC]: Error encountered when writing to file: Expected %d bytes, read %d instead.\n", RecordingBlockDataSize, rSize));
			return false;
		}
	}
	MaxFrame--;
	WriteMaxFrame();
	fflush(recordingFile);

	return true;
}

bool InputRecordingFile::InsertPadData(unsigned long frame, const PadData& key)
{
	if (recordingFile == NULL || !key.fExistKey)
	{
		return false;
	}

	for (unsigned long i = MaxFrame - 1; i >= frame; i--)
	{
		long seek1 = GetBlockSeekPoint(i) + RecordingBlockHeaderSize;
		long seek2 = GetBlockSeekPoint(i+1) + RecordingBlockHeaderSize;

		u8 buf[2][18];
		fseek(recordingFile, seek1, SEEK_SET);
		int rSize = fread(buf, 1, RecordingBlockDataSize, recordingFile);
		if (rSize != RecordingBlockDataSize)
		{
			recordingConLog(wxString::Format("[REC]: Error encountered when reading from file: Expected %d bytes, read %d instead.\n", RecordingBlockDataSize, rSize));
			return false;
		}
		fseek(recordingFile, seek2, SEEK_SET);
		rSize = fwrite(buf, 1, RecordingBlockDataSize, recordingFile);
		if (rSize != RecordingBlockDataSize)
		{
			recordingConLog(wxString::Format("[REC]: Error encountered when writing to file: Expected %d bytes, wrote %d instead.\n", RecordingBlockDataSize, rSize));
			return false;
		}
	}
	long seek = GetBlockSeekPoint(frame) + RecordingBlockHeaderSize;
	fseek(recordingFile, seek, SEEK_SET);
	int rSize = fwrite(key.buf, 1, RecordingBlockDataSize, recordingFile);
	if (rSize != RecordingBlockDataSize)
	{
		recordingConLog(wxString::Format("[REC]: Error encountered when writing to file: Expected %d bytes, wrote %d instead.\n", RecordingBlockDataSize, rSize));
		return false;
	}
	MaxFrame++;
	WriteMaxFrame();
	fflush(recordingFile);

	return true;
}

bool InputRecordingFile::UpdatePadData(unsigned long frame, const PadData& key)
{
	if (recordingFile == NULL)
	{
		return false;
	}
	if (!key.fExistKey)
	{
		return false;
	}

	long seek = GetBlockSeekPoint(frame) + RecordingBlockHeaderSize;
	fseek(recordingFile, seek, SEEK_SET);
	if (fwrite(key.buf, 1, RecordingBlockDataSize, recordingFile) == 0)
	{
		return false;
	}

	fflush(recordingFile);
	return true;
}

// Verify header of recording file
bool InputRecordingFile::ReadHeaderAndCheck()
{
	if (recordingFile == NULL)
	{
		return false;
	}
	rewind(recordingFile);
	if (fread(&header, sizeof(InputRecordingHeader), 1, recordingFile) != 1
		|| fread(&MaxFrame, 4, 1, recordingFile) != 1
		|| fread(&UndoCount, 4, 1, recordingFile) != 1
		|| fread(&savestate.fromSavestate, sizeof(bool), 1, recordingFile) != 1)
	{
		return false;
	}
	if (savestate.fromSavestate)
	{
		FILE* ssFileCheck = wxFopen(filename + "_SaveState.p2s", "r");
		if (ssFileCheck == NULL)
		{
			recordingConLog(wxString::Format("[REC]: Could not locate savestate file at location - %s\n", filename + "_SaveState.p2s"));
			return false;
		}
		fclose(ssFileCheck);
		StateCopy_LoadFromFile(filename + "_SaveState.p2s");
	}
	else
	{
		sApp.SysExecute();
	}

	// Check for current verison
	if (header.version != 1)
	{
		recordingConLog(wxString::Format("[REC]: Input recording file is not a supported version - %d\n", header.version));
		return false;
	}
	return true;
}
bool InputRecordingFile::WriteHeader()
{
	if (recordingFile == NULL)
	{
		return false;
	}
	rewind(recordingFile);
	if (fwrite(&header, sizeof(InputRecordingHeader), 1, recordingFile) != 1)
	{
		return false;
	}
	return true;
}

bool InputRecordingFile::WriteMaxFrame()
{
	if (recordingFile == NULL)
	{
		return false;
	}
	fseek(recordingFile, RecordingSeekpointFrameMax, SEEK_SET);
	if (fwrite(&MaxFrame, 4, 1, recordingFile) != 1)
	{
		return false;
	}
	return true;
}

void InputRecordingFile::UpdateFrameMax(unsigned long frame)
{
	if (recordingFile == NULL || MaxFrame >= frame)
	{
		return;
	}
	MaxFrame = frame;
	fseek(recordingFile, RecordingSeekpointFrameMax, SEEK_SET);
	fwrite(&MaxFrame, 4, 1, recordingFile);
}

void InputRecordingFile::AddUndoCount()
{
	UndoCount++;
	if (recordingFile == NULL)
	{
		return;
	}
	fseek(recordingFile, RecordingSeekpointUndoCount, SEEK_SET);
	fwrite(&UndoCount, 4, 1, recordingFile);
}

void InputRecordingHeader::SetAuthor(wxString _author)
{
	int max = ArraySize(author) - 1;
	strncpy(author, _author.c_str(), max);
	author[max] = 0;
}

void InputRecordingHeader::SetGameName(wxString _gameName)
{
	int max = ArraySize(gameName) - 1;
	strncpy(gameName, _gameName.c_str(), max);
	gameName[max] = 0;
}

void InputRecordingHeader::Init()
{
	memset(author, 0, ArraySize(author));
	memset(gameName, 0, ArraySize(gameName));
}

InputRecordingHeader& InputRecordingFile::GetHeader()
{
	return header; }

unsigned long& InputRecordingFile::GetMaxFrame()
{
	return MaxFrame;
}

unsigned long& InputRecordingFile::GetUndoCount()
{
	return UndoCount;
}

const wxString & InputRecordingFile::GetFilename()
{
	return filename;
}
#endif
