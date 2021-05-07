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

#include "PrecompiledHeader.h"

#ifndef DISABLE_RECORDING

#include "InputRecordingFileV1.h"

#include "DebugTools/Debug.h"
#include "MainFrame.h"
#include "MemoryTypes.h"

#include "Recording/Utilities/InputRecordingLogger.h"

void InputRecordingFileV1::InputRecordingFileHeader::Init()
{
	memset(author, 0, ArraySize(author));
	memset(gameName, 0, ArraySize(gameName));
}

void InputRecordingFileV1::InputRecordingFileHeader::SetEmulatorVersion()
{
	wxString emuVersion = wxString::Format("%s-%d.%d.%d", pxGetAppName().c_str(), PCSX2_VersionHi, PCSX2_VersionMid, PCSX2_VersionLo);
	int max = ArraySize(emu) - 1;
	strncpy(emu, emuVersion.c_str(), max);
	emu[max] = 0;
}

void InputRecordingFileV1::InputRecordingFileHeader::SetAuthor(wxString _author)
{
	int max = ArraySize(author) - 1;
	strncpy(author, _author.c_str(), max);
	author[max] = 0;
}

void InputRecordingFileV1::InputRecordingFileHeader::SetGameName(wxString _gameName)
{
	int max = ArraySize(gameName) - 1;
	strncpy(gameName, _gameName.c_str(), max);
	gameName[max] = 0;
}

bool InputRecordingFileV1::Close()
{
	if (recordingFile == nullptr)
	{
		return false;
	}
	fclose(recordingFile);
	recordingFile = nullptr;
	filename = "";
	return true;
}

const wxString &InputRecordingFileV1::GetFilename()
{
	return filename;
}

InputRecordingFileV1::InputRecordingFileHeader &InputRecordingFileV1::GetHeader()
{
	return header;
}

long &InputRecordingFileV1::GetTotalFrames()
{
	return totalFrames;
}

unsigned long &InputRecordingFileV1::GetUndoCount()
{
	return undoCount;
}

bool InputRecordingFileV1::FromSaveState()
{
	return savestate.fromSavestate;
}

void InputRecordingFileV1::IncrementUndoCount()
{
	undoCount++;
	if (recordingFile == nullptr)
	{
		return;
	}
	fseek(recordingFile, seekpointUndoCount, SEEK_SET);
	fwrite(&undoCount, 4, 1, recordingFile);
}

bool InputRecordingFileV1::open(const wxString path, bool newRecording)
{
	if (newRecording)
	{
		if ((recordingFile = wxFopen(path, L"wb+")) != nullptr)
		{
			filename = path;
			totalFrames = 0;
			undoCount = 0;
			header.Init();
			return true;
		}
	}
	else if ((recordingFile = wxFopen(path, L"rb+")) != nullptr)
	{
		if (verifyRecordingFileHeader())
		{
			filename = path;
			return true;
		}
		Close();
		inputRec::consoleLog("Input recording file header is invalid");
		return false;
	}
	inputRec::consoleLog(fmt::format("Input recording file opening failed. Error - {}", strerror(errno)));
	return false;
}

bool InputRecordingFileV1::OpenNew(const wxString& path, bool fromSavestate)
{
	if (!open(path, true))
		return false;
	savestate.fromSavestate = fromSavestate;
	return true;
}

bool InputRecordingFileV1::OpenExisting(const wxString& path)
{
	return open(path, false);
}

bool InputRecordingFileV1::ReadKeyBuffer(u8 &result, const uint &frame, const uint port, const uint bufIndex)
{
	if (recordingFile == nullptr)
	{
		return false;
	}

	long seek = getRecordingBlockSeekPoint(frame) + s_controllers_supported_per_frame * port + bufIndex;
	if (fseek(recordingFile, seek, SEEK_SET) != 0 || fread(&result, 1, 1, recordingFile) != 1)
	{
		return false;
	}

	return true;
}

void InputRecordingFileV1::SetTotalFrames(long frame)
{
	if (recordingFile == nullptr || totalFrames >= frame)
	{
		return;
	}
	totalFrames = frame;
	fseek(recordingFile, seekpointTotalFrames, SEEK_SET);
	fwrite(&totalFrames, 4, 1, recordingFile);
}

bool InputRecordingFileV1::WriteHeader()
{
	if (recordingFile == nullptr)
	{
		return false;
	}
	rewind(recordingFile);
	if (fwrite(&header, sizeof(InputRecordingFileHeader), 1, recordingFile) != 1
		|| fwrite(&totalFrames, 4, 1, recordingFile) != 1
		|| fwrite(&undoCount, 4, 1, recordingFile) != 1
		|| fwrite(&savestate, 1, 1, recordingFile) != 1)
	{
		return false;
	}
	return true;
}

bool InputRecordingFileV1::WriteKeyBuffer(const uint &frame, const uint port, const uint bufIndex, const u8 &buf)
{
	if (recordingFile == nullptr)
	{
		return false;
	}

	long seek = getRecordingBlockSeekPoint(frame) + 18 * port + bufIndex;

	if (fseek(recordingFile, seek, SEEK_SET) != 0 || fwrite(&buf, 1, 1, recordingFile) != 1)
	{
		return false;
	}

	fflush(recordingFile);
	return true;
}

long InputRecordingFileV1::getRecordingBlockSeekPoint(const long &frame)
{
	return headerSize + sizeof(bool) + frame * inputBytesPerFrame;
}

bool InputRecordingFileV1::verifyRecordingFileHeader()
{
	if (recordingFile == nullptr)
	{
		return false;
	}
	// Verify header contents
	rewind(recordingFile);
	if (fread(&header, sizeof(InputRecordingFileHeader), 1, recordingFile) != 1
		|| fread(&totalFrames, 4, 1, recordingFile) != 1
		|| fread(&undoCount, 4, 1, recordingFile) != 1
		|| fread(&savestate.fromSavestate, sizeof(bool), 1, recordingFile) != 1)
	{
		return false;
	}
	
	// Check for current verison
	if (header.version != 1)
	{
		inputRec::consoleLog(fmt::format("Input recording file is not a supported version - {}", header.version));
		return false;
	}
	return true;
}
#endif
