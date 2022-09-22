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

#include "PrecompiledHeader.h"

#ifndef PCSX2_CORE
// TODO - Vaser - kill with wxWidgets

#include "DebugTools/Debug.h"
#include "gui/MainFrame.h"
#include "MemoryTypes.h"

#include "InputRecordingFile.h"
#include "Utilities/InputRecordingLogger.h"

#include <fmt/format.h>

void InputRecordingFileHeader::Init()
{
	memset(author, 0, std::size(author));
	memset(gameName, 0, std::size(gameName));
}

void InputRecordingFileHeader::SetEmulatorVersion()
{
	wxString emuVersion = wxString::Format("%s-%d.%d.%d", pxGetAppName().c_str(), PCSX2_VersionHi, PCSX2_VersionMid, PCSX2_VersionLo);
	int max = std::size(emu) - 1;
	strncpy(emu, emuVersion.c_str(), max);
	emu[max] = 0;
}

void InputRecordingFileHeader::SetAuthor(wxString _author)
{
	int max = std::size(author) - 1;
	strncpy(author, _author.c_str(), max);
	author[max] = 0;
}

void InputRecordingFileHeader::SetGameName(wxString _gameName)
{
	int max = std::size(gameName) - 1;
	strncpy(gameName, _gameName.c_str(), max);
	gameName[max] = 0;
}

bool InputRecordingFile::Close()
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

const wxString& InputRecordingFile::GetFilename()
{
	return filename;
}

InputRecordingFileHeader& InputRecordingFile::GetHeader()
{
	return header;
}

long& InputRecordingFile::GetTotalFrames()
{
	return totalFrames;
}

unsigned long& InputRecordingFile::GetUndoCount()
{
	return undoCount;
}

bool InputRecordingFile::FromSaveState()
{
	return savestate.fromSavestate;
}

void InputRecordingFile::IncrementUndoCount()
{
	undoCount++;
	if (recordingFile == nullptr)
	{
		return;
	}
	fseek(recordingFile, seekpointUndoCount, SEEK_SET);
	fwrite(&undoCount, 4, 1, recordingFile);
}

bool InputRecordingFile::open(const wxString path, bool newRecording)
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
		InputRec::consoleLog("Input recording file header is invalid");
		return false;
	}
	InputRec::consoleLog(fmt::format("Input recording file opening failed. Error - {}", strerror(errno)));
	return false;
}

bool InputRecordingFile::OpenNew(const wxString& path, bool fromSavestate)
{
	if (!open(path, true))
		return false;
	savestate.fromSavestate = fromSavestate;
	return true;
}

bool InputRecordingFile::OpenExisting(const wxString& path)
{
	return open(path, false);
}

bool InputRecordingFile::ReadKeyBuffer(u8& result, const uint& frame, const uint port, const uint bufIndex)
{
	if (recordingFile == nullptr)
	{
		return false;
	}

	long seek = getRecordingBlockSeekPoint(frame) + controllerInputBytes * port + bufIndex;
	if (fseek(recordingFile, seek, SEEK_SET) != 0 || fread(&result, 1, 1, recordingFile) != 1)
	{
		return false;
	}

	return true;
}

void InputRecordingFile::SetTotalFrames(long frame)
{
	if (recordingFile == nullptr || totalFrames >= frame)
	{
		return;
	}
	totalFrames = frame;
	fseek(recordingFile, seekpointTotalFrames, SEEK_SET);
	fwrite(&totalFrames, 4, 1, recordingFile);
}

bool InputRecordingFile::WriteHeader()
{
	if (recordingFile == nullptr)
	{
		return false;
	}
	rewind(recordingFile);
	if (fwrite(&header, sizeof(InputRecordingFileHeader), 1, recordingFile) != 1 || fwrite(&totalFrames, 4, 1, recordingFile) != 1 || fwrite(&undoCount, 4, 1, recordingFile) != 1 || fwrite(&savestate, 1, 1, recordingFile) != 1)
	{
		return false;
	}
	return true;
}

bool InputRecordingFile::WriteKeyBuffer(const uint& frame, const uint port, const uint bufIndex, const u8& buf)
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

long InputRecordingFile::getRecordingBlockSeekPoint(const long& frame)
{
	return headerSize + sizeof(bool) + frame * inputBytesPerFrame;
}

bool InputRecordingFile::verifyRecordingFileHeader()
{
	if (recordingFile == nullptr)
	{
		return false;
	}
	// Verify header contents
	rewind(recordingFile);
	if (fread(&header, sizeof(InputRecordingFileHeader), 1, recordingFile) != 1 || fread(&totalFrames, 4, 1, recordingFile) != 1 || fread(&undoCount, 4, 1, recordingFile) != 1 || fread(&savestate.fromSavestate, sizeof(bool), 1, recordingFile) != 1)
	{
		return false;
	}

	// Check for current verison
	if (header.version != 1)
	{
		InputRec::consoleLog(fmt::format("Input recording file is not a supported version - {}", header.version));
		return false;
	}
	return true;
}

#else

#include "InputRecordingFile.h"

#include "Utilities/InputRecordingLogger.h"

#include "common/FileSystem.h"
#include "DebugTools/Debug.h"
#include "MemoryTypes.h"

#include <fmt/format.h>

#include <vector>
#include <array>

void InputRecordingFileHeader::Init() noexcept
{
	m_fileVersion = 1;
}

void InputRecordingFileHeader::SetEmulatorVersion()
{
	static const std::string emuVersion = fmt::format("PCSX2-{}.{}.{}", PCSX2_VersionHi, PCSX2_VersionMid, PCSX2_VersionLo);
	strncpy(m_emulatorVersion, emuVersion.c_str(), sizeof(m_emulatorVersion) - 1);
}

void InputRecordingFileHeader::SetAuthor(const std::string& _author)
{
	strncpy(m_author, _author.data(), sizeof(m_author) - 1);
}

void InputRecordingFileHeader::SetGameName(const std::string& _gameName)
{
	strncpy(m_gameName, _gameName.data(), sizeof(m_gameName) - 1);
}

bool InputRecordingFile::Close() noexcept
{
	if (m_recordingFile == nullptr)
	{
		return false;
	}
	fclose(m_recordingFile);
	m_recordingFile = nullptr;
	m_filename.clear();
	return true;
}

const std::string& InputRecordingFile::getFilename() const noexcept
{
	return m_filename;
}

InputRecordingFileHeader& InputRecordingFile::getHeader() noexcept
{
	return m_header;
}

unsigned long InputRecordingFile::getTotalFrames() const noexcept
{
	return m_totalFrames;
}

unsigned long InputRecordingFile::getUndoCount() const noexcept
{
	return m_undoCount;
}

bool InputRecordingFile::FromSaveState() const noexcept
{
	return m_savestate.fromSavestate;
}

void InputRecordingFile::IncrementUndoCount()
{
	m_undoCount++;
	if (m_recordingFile == nullptr)
	{
		return;
	}
	fseek(m_recordingFile, s_seekpointUndoCount, SEEK_SET);
	fwrite(&m_undoCount, 4, 1, m_recordingFile);
}

bool InputRecordingFile::OpenNew(const std::string& path, bool fromSavestate)
{
	if ((m_recordingFile = FileSystem::OpenCFile(path.data(), "wb+")) == nullptr)
	{
		InputRec::consoleLog(fmt::format("Input recording file opening failed. Error - {}", strerror(errno)));
		return false;
	}

	m_filename = path;
	m_totalFrames = 0;
	m_undoCount = 0;
	m_header.Init();
	m_savestate.fromSavestate = fromSavestate;
	return true;
}

bool InputRecordingFile::OpenExisting(const std::string& path)
{
	if ((m_recordingFile = FileSystem::OpenCFile(path.data(), "rb+")) == nullptr)
	{
		InputRec::consoleLog(fmt::format("Input recording file opening failed. Error - {}", strerror(errno)));
		return false;
	}

	if (!verifyRecordingFileHeader())
	{
		Close();
		InputRec::consoleLog("Input recording file header is invalid");
		return false;
	}

	m_filename = path;
	return true;
}

bool InputRecordingFile::ReadKeyBuffer(u8& result, const uint frame, const uint port, const uint bufIndex)
{
	if (m_recordingFile == nullptr)
	{
		return false;
	}

	const size_t seek = getRecordingBlockSeekPoint(frame) + s_controllerInputBytes * port + bufIndex;
	if (fseek(m_recordingFile, seek, SEEK_SET) != 0 || fread(&result, 1, 1, m_recordingFile) != 1)
	{
		return false;
	}

	return true;
}

void InputRecordingFile::SetTotalFrames(long frame)
{
	if (m_recordingFile == nullptr || m_totalFrames >= frame)
	{
		return;
	}
	m_totalFrames = frame;
	fseek(m_recordingFile, s_seekpointTotalFrames, SEEK_SET);
	fwrite(&m_totalFrames, 4, 1, m_recordingFile);
}

bool InputRecordingFile::WriteHeader() const
{
	if (m_recordingFile == nullptr)
	{
		return false;
	}
	rewind(m_recordingFile);
	if (fwrite(&m_header, sizeof(InputRecordingFileHeader), 1, m_recordingFile) != 1 ||
		fwrite(&m_totalFrames, 4, 1, m_recordingFile) != 1 ||
		fwrite(&m_undoCount, 4, 1, m_recordingFile) != 1 ||
		fwrite(&m_savestate, 1, 1, m_recordingFile) != 1)
	{
		return false;
	}
	return true;
}

bool InputRecordingFile::WriteKeyBuffer(const uint frame, const uint port, const uint bufIndex, const u8 buf) const
{
	if (m_recordingFile == nullptr)
	{
		return false;
	}

	const size_t seek = getRecordingBlockSeekPoint(frame) + s_controllerInputBytes * port + bufIndex;
	
	if (fseek(m_recordingFile, seek, SEEK_SET) != 0 ||
		fwrite(&buf, 1, 1, m_recordingFile) != 1)
	{
		return false;
	}

	fflush(m_recordingFile);
	return true;
}

void InputRecordingFile::logRecordingMetadata()
{
	InputRec::consoleMultiLog({fmt::format("File: {}", getFilename()),
		fmt::format("PCSX2 Version Used: {}", getHeader().m_emulatorVersion),
		fmt::format("Recording File Version: {}", getHeader().m_fileVersion),
		fmt::format("Associated Game Name or ISO Filename: {}", getHeader().m_gameName),
		fmt::format("Author: {}", getHeader().m_author),
		fmt::format("Total Frames: {}", getTotalFrames()),
		fmt::format("Undo Count: {}", getUndoCount())});
}

std::vector<PadData> InputRecordingFile::bulkReadPadData(long frameStart, long frameEnd, const uint port)
{
	std::vector<PadData> data = {};

	if (m_recordingFile == nullptr)
	{
		return data;
	}

	frameStart = frameStart < 0 ? 0 : frameStart;

	std::array<u8, s_controllerInputBytes> padBytes;

	// TODO - there are probably issues here if the file is too small / the frame counters are invalid!
	for (int frame = frameStart; frame < frameEnd; frame++)
	{
		const long seek = getRecordingBlockSeekPoint(frame) + s_controllerInputBytes * port;
		fseek(m_recordingFile, seek, SEEK_SET);
		if (fread(&padBytes, 1, padBytes.size(), m_recordingFile))
		{
			PadData frameData;
			for (int i = 0; i < padBytes.size(); i++)
			{
				frameData.UpdateControllerData(i, padBytes.at(i));
			}
			data.push_back(frameData);
		}
	}

	return data;
}

size_t InputRecordingFile::getRecordingBlockSeekPoint(const long frame) const noexcept
{
	return s_headerSize + sizeof(bool) + frame * s_inputBytesPerFrame;
}

bool InputRecordingFile::verifyRecordingFileHeader()
{
	if (m_recordingFile == nullptr)
	{
		return false;
	}
	// Verify header contents
	rewind(m_recordingFile);
	if (fread(&m_header, sizeof(InputRecordingFileHeader), 1, m_recordingFile) != 1 ||
		fread(&m_totalFrames, 4, 1, m_recordingFile) != 1 ||
		fread(&m_undoCount, 4, 1, m_recordingFile) != 1 ||
		fread(&m_savestate.fromSavestate, sizeof(bool), 1, m_recordingFile) != 1)
	{
		return false;
	}

	// Check for current verison
	if (m_header.m_fileVersion != 1)
	{
		InputRec::consoleLog(fmt::format("Input recording file is not a supported version - {}", m_header.m_fileVersion));
		return false;
	}
	return true;
}

#endif