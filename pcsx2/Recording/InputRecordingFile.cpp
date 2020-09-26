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

#include "PrecompiledHeader.h"

#ifndef DISABLE_RECORDING

#include "DebugTools/Debug.h"
#include "MainFrame.h"
#include "MemoryTypes.h"

#include "InputRecordingFile.h"
#include "Utilities/InputRecordingLogger.h"

void InputRecordingFile::InputRecordingFileHeader::Init() noexcept
{
	m_fileVersion = 2;
	m_totalFrames = 0;
	m_redoCount = 0;
}

bool InputRecordingFile::InputRecordingFileHeader::ReadHeader(FILE* m_recordingFile)
{
	return (fread(this, s_seekpointTotalFrames, 1, m_recordingFile) == 1 && fread(&m_totalFrames, 9, 1, m_recordingFile) == 1); // Reads in totalFrames, redoCount, & startType
}

void InputRecordingFile::InputRecordingFileHeader::SetEmulatorVersion()
{
	const wxString emulatorVersion = wxString::Format("%s-%d.%d.%d", pxGetAppName().c_str(), PCSX2_VersionHi, PCSX2_VersionMid, PCSX2_VersionLo);
	m_emulatorVersion.fill(0);
	strncpy(m_emulatorVersion.data(), emulatorVersion.c_str(), m_emulatorVersion.size() - 1);
}

void InputRecordingFile::InputRecordingFileHeader::SetAuthor(wxString author)
{
	m_author.fill(0);
	strncpy(m_author.data(), author.c_str(), m_author.size() - 1);
}

void InputRecordingFile::InputRecordingFileHeader::SetGameName(wxString gameName)
{
	m_gameName.fill(0);
	strncpy(m_gameName.data(), gameName.c_str(), m_gameName.size() - 1);
}

u8 InputRecordingFile::GetFileVersion() const noexcept
{
	return m_header.m_fileVersion;
}

const char* InputRecordingFile::GetEmulatorVersion() const noexcept
{
	return m_header.m_emulatorVersion.data();
}

const char* InputRecordingFile::GetAuthor() const noexcept
{
	return m_header.m_author.data();
}

const char* InputRecordingFile::GetGameName() const noexcept
{
	return m_header.m_gameName.data();
}

long InputRecordingFile::GetTotalFrames() const noexcept
{
	return m_header.m_totalFrames;
}

unsigned long InputRecordingFile::GetRedoCount() const noexcept
{
	return m_header.m_redoCount;
}

InputRecordingStartType InputRecordingFile::GetStartType() const noexcept
{
	return m_header.m_startType;
}

bool InputRecordingFile::FromSavestate() const noexcept
{
	return m_header.m_startType == InputRecordingStartType::Savestate;
}

wxByte InputRecordingFile::GetPads() const noexcept
{
	return m_header.m_pads;
}

bool InputRecordingFile::Close()
{
	if (m_recordingFile == nullptr)
		return false;
	fclose(m_recordingFile);
	m_recordingFile = nullptr;
	m_filename = "";
	m_padCount = 0;
	return true;
}

const wxString& InputRecordingFile::GetFilename() const noexcept
{
	return m_filename;
}

InputRecordingFile::InputRecordingFileHeader& InputRecordingFile::GetHeader() noexcept
{
	return m_header;
}

int InputRecordingFile::GetPadCount() const noexcept
{
	return m_padCount;
}

bool InputRecordingFile::IsPortUsed(const int port) const noexcept
{
	return m_header.m_pads & (15 << 4 * port);
}

bool InputRecordingFile::IsMultitapUsed(const int port) const noexcept
{
	return m_header.m_pads & (14 << 4 * port);
}

bool InputRecordingFile::IsSlotUsed(const int port, const int slot) const noexcept
{
	return m_header.m_pads & (1 << (4 * port + slot));
}

void InputRecordingFile::IncrementRedoCount()
{
	if (m_recordingFile != nullptr)
	{
		m_header.m_redoCount++;
		fseek(m_recordingFile, InputRecordingFileHeader::s_seekpointRedoCount, SEEK_SET);
		fwrite(&m_header.m_redoCount, 4, 1, m_recordingFile);
	}
}

bool InputRecordingFile::open(const wxString path, const bool newRecording)
{
	if (newRecording)
	{
		if ((m_recordingFile = wxFopen(path, L"wb+")) != nullptr)
		{
			m_filename = path;
			m_header.Init();
			return true;
		}
	}
	else if ((m_recordingFile = wxFopen(path, L"rb+")) != nullptr)
	{
		if (verifyRecordingFileHeader())
		{
			m_filename = path;
			return true;
		}
		Close();
		inputRec::consoleLog("Input recording file header is invalid");
		return false;
	}
	inputRec::consoleLog(fmt::format("Input recording file opening failed. Error - {}", strerror(errno)));
	return false;
}

bool InputRecordingFile::OpenNew(const wxString path, const char startType, const wxByte slots)
{
	if (!open(path, true))
		return false;
	m_header.m_startType = static_cast<InputRecordingStartType>(startType);
	m_header.m_pads = slots;
	for (u8 pad = 0; pad < 8; pad++)
		if (m_header.m_pads & (1 << pad))
			m_padCount++;
	m_recordingBlockSize = s_controllerInputBytes * m_padCount;
	m_seekpointInputData = InputRecordingFileHeader::s_seekpointPads + 1;
	return true;
}

bool InputRecordingFile::OpenExisting(const wxString path)
{
	return open(path, false);
}

bool InputRecordingFile::SetTotalFrames(const long frame) noexcept
{
	bool ret = false;
	if (m_recordingFile != nullptr)
	{
		if (m_header.m_totalFrames < frame)
		{
			m_header.m_totalFrames = frame;
			fseek(m_recordingFile, InputRecordingFileHeader::s_seekpointTotalFrames, SEEK_SET);
			fwrite(&m_header.m_totalFrames, 4, 1, m_recordingFile);
			ret = true;
		}
		else if (m_header.m_totalFrames == frame)
			ret = true;
		fflush(m_recordingFile);
	}
	return ret;
}

bool InputRecordingFile::ReadKeyBuffer(u8& result, const u32 frame, const u32 seekOffset) const
{
	if (m_recordingFile == nullptr)
		return false;

	const u64 seek = getRecordingBlockSeekPoint(frame) + seekOffset;
	return fseek(m_recordingFile, seek, SEEK_SET) == 0 && fread(&result, 1, 1, m_recordingFile) == 1;
}

bool InputRecordingFile::WriteHeader() const
{
	if (m_recordingFile == nullptr)
		return false;
	rewind(m_recordingFile);
	return fwrite(&m_header, InputRecordingFileHeader::s_seekpointTotalFrames, 1, m_recordingFile) == 1 && fwrite(&m_header.m_totalFrames, 10, 1, m_recordingFile) == 1; // Writes totalFrames, redoCount, startType, & pads
}

bool InputRecordingFile::WriteKeyBuffer(const u8 buf, const u32 frame, const u32 seekOffset) const
{
	if (m_recordingFile == nullptr)
		return false;

	const u64 seek = getRecordingBlockSeekPoint(frame) + seekOffset;
	return fseek(m_recordingFile, seek, SEEK_SET) == 0 && fwrite(&buf, 1, 1, m_recordingFile) == 1;
}

u64 InputRecordingFile::getRecordingBlockSeekPoint(const u32 frame) const
{
	return m_seekpointInputData + frame * m_recordingBlockSize;
}

bool InputRecordingFile::verifyRecordingFileHeader()
{
	if (m_recordingFile == nullptr)
		return false;

	// Verify header contents
	m_header.ReadHeader(m_recordingFile);

	// Check for valid verison
	switch (m_header.m_fileVersion)
	{
		case 1: // Official version 1 with no slot info written to file; Backwards compatibility
			m_padCount = 2;
			m_recordingBlockSize = 36;
			m_seekpointInputData = InputRecordingFileHeader::s_seekpointPads;
			m_header.m_startType = (char)m_header.m_startType == 0 ? InputRecordingStartType::UnspecifiedBoot : InputRecordingStartType::Savestate;
			m_header.m_pads = 17; // Pads 1A (1) & 2A (16)
			break;
		case 2: // Official Version 2
			// Additional: header -> pads
			if (fread(&m_header.m_pads, 1, 1, m_recordingFile) != 1)
				return false;

			if (m_header.m_pads == 0)
			{
				inputRec::log("Input Recording File must have at least 1 controller");
				return false;
			}
			for (u8 pad = 0; pad < 8; pad++)
				if (m_header.m_pads & (1 << pad))
					m_padCount++;
			m_recordingBlockSize = s_controllerInputBytes * m_padCount;
			m_seekpointInputData = InputRecordingFileHeader::s_seekpointPads + 1;
			break;
		default:
			inputRec::consoleLog(fmt::format("Input recording file is not a supported version - {}", m_header.m_fileVersion));
			return false;
	}
	return true;
}
#endif
