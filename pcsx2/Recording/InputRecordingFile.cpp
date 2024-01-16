// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "InputRecordingFile.h"

#include "Utilities/InputRecordingLogger.h"

#include "common/FileSystem.h"
#include "DebugTools/Debug.h"
#include "MemoryTypes.h"
#include "svnrev.h"

#include <fmt/format.h>

#include <vector>
#include <array>

void InputRecordingFile::InputRecordingFileHeader::init() noexcept
{
	m_fileVersion = 1;
}

void InputRecordingFile::setEmulatorVersion()
{
	StringUtil::Strlcpy(m_header.m_emulatorVersion, "PCSX2-" GIT_REV, sizeof(m_header.m_emulatorVersion));
}

void InputRecordingFile::setAuthor(const std::string& _author)
{
	strncpy(m_header.m_author, _author.data(), sizeof(m_header.m_author) - 1);
}

void InputRecordingFile::setGameName(const std::string& _gameName)
{
	strncpy(m_header.m_gameName, _gameName.data(), sizeof(m_header.m_gameName) - 1);
}

const char* InputRecordingFile::getEmulatorVersion() const noexcept
{
	return m_header.m_emulatorVersion;
}

const char* InputRecordingFile::getAuthor() const noexcept
{
	return m_header.m_author;
}

const char* InputRecordingFile::getGameName() const noexcept
{
	return m_header.m_gameName;
}

bool InputRecordingFile::close() noexcept
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

unsigned long InputRecordingFile::getTotalFrames() const noexcept
{
	return m_totalFrames;
}

unsigned long InputRecordingFile::getUndoCount() const noexcept
{
	return m_undoCount;
}

bool InputRecordingFile::fromSaveState() const noexcept
{
	return m_savestate;
}

void InputRecordingFile::incrementUndoCount()
{
	m_undoCount++;
	if (m_recordingFile == nullptr)
	{
		return;
	}
	fseek(m_recordingFile, s_seekpointUndoCount, SEEK_SET);
	fwrite(&m_undoCount, 4, 1, m_recordingFile);
}

bool InputRecordingFile::openNew(const std::string& path, bool fromSavestate)
{
	if ((m_recordingFile = FileSystem::OpenCFile(path.data(), "wb+")) == nullptr)
	{
		InputRec::consoleLog(fmt::format("Input recording file opening failed. Error - {}", strerror(errno)));
		return false;
	}

	m_filename = path;
	m_totalFrames = 0;
	m_undoCount = 0;
	m_header.init();
	m_savestate = fromSavestate;
	return true;
}

bool InputRecordingFile::openExisting(const std::string& path)
{
	if ((m_recordingFile = FileSystem::OpenCFile(path.data(), "rb+")) == nullptr)
	{
		InputRec::consoleLog(fmt::format("Input recording file opening failed. Error - {}", strerror(errno)));
		return false;
	}

	if (!verifyRecordingFileHeader())
	{
		close();
		InputRec::consoleLog("Input recording file header is invalid");
		return false;
	}

	m_filename = path;
	return true;
}

std::optional<PadData> InputRecordingFile::readPadData(const uint frame, const uint port, const uint slot)
{
	if (m_recordingFile == nullptr)
	{
		return std::nullopt;
	}

	std::array<u8, s_controllerInputBytes> data{};

	// TODO - slot unused, use it in the new format
	const size_t seek = getRecordingBlockSeekPoint(frame) + s_controllerInputBytes * port;
	if (fseek(m_recordingFile, seek, SEEK_SET) != 0 || fread(&data, 1, 18, m_recordingFile) != 1)
	{
		return PadData(port, slot, data);
	}

	return std::nullopt;
}

void InputRecordingFile::setTotalFrames(u32 frame)
{
	if (m_recordingFile == nullptr || m_totalFrames >= frame)
	{
		return;
	}
	m_totalFrames = frame;
	fseek(m_recordingFile, s_seekpointTotalFrames, SEEK_SET);
	fwrite(&m_totalFrames, 4, 1, m_recordingFile);
}

bool InputRecordingFile::writeHeader() const
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

bool InputRecordingFile::writePadData(const uint frame, const PadData data) const
{
	if (m_recordingFile == nullptr)
	{
		return false;
	}

	// TODO - use the slot in the future
	const size_t seek = getRecordingBlockSeekPoint(frame) + s_controllerInputBytes * data.m_port;

	// seek to the correct position and write data to the file
	if (fseek(m_recordingFile, seek, SEEK_SET) != 0 ||
		fwrite(&data.m_compactPressFlagsGroupOne, 1, 1, m_recordingFile) != 1 ||
		fwrite(&data.m_compactPressFlagsGroupTwo, 1, 1, m_recordingFile) != 1 ||
		fwrite(&std::get<0>(data.m_rightAnalog), 1, 1, m_recordingFile) != 1 ||
		fwrite(&std::get<1>(data.m_rightAnalog), 1, 1, m_recordingFile) != 1 ||
		fwrite(&std::get<0>(data.m_leftAnalog), 1, 1, m_recordingFile) != 1 ||
		fwrite(&std::get<1>(data.m_leftAnalog), 1, 1, m_recordingFile) != 1 ||
		fwrite(&std::get<1>(data.m_right), 1, 1, m_recordingFile) != 1 ||
		fwrite(&std::get<1>(data.m_left), 1, 1, m_recordingFile) != 1 ||
		fwrite(&std::get<1>(data.m_up), 1, 1, m_recordingFile) != 1 ||
		fwrite(&std::get<1>(data.m_down), 1, 1, m_recordingFile) != 1 ||
		fwrite(&std::get<1>(data.m_triangle), 1, 1, m_recordingFile) != 1 ||
		fwrite(&std::get<1>(data.m_circle), 1, 1, m_recordingFile) != 1 ||
		fwrite(&std::get<1>(data.m_cross), 1, 1, m_recordingFile) != 1 ||
		fwrite(&std::get<1>(data.m_square), 1, 1, m_recordingFile) != 1 ||
		fwrite(&std::get<1>(data.m_l1), 1, 1, m_recordingFile) != 1 ||
		fwrite(&std::get<1>(data.m_r1), 1, 1, m_recordingFile) != 1 ||
		fwrite(&std::get<1>(data.m_l2), 1, 1, m_recordingFile) != 1 ||
		fwrite(&std::get<1>(data.m_r2), 1, 1, m_recordingFile) != 1)
	{
		return false;
	}

	fflush(m_recordingFile);
	return true;
}

void InputRecordingFile::logRecordingMetadata()
{
	InputRec::consoleMultiLog({fmt::format("File: {}", getFilename()),
		fmt::format("PCSX2 Version Used: {}", m_header.m_emulatorVersion),
		fmt::format("Recording File Version: {}", m_header.m_fileVersion),
		fmt::format("Associated Game Name or ISO Filename: {}", m_header.m_gameName),
		fmt::format("Author: {}", m_header.m_author),
		fmt::format("Total Frames: {}", getTotalFrames()),
		fmt::format("Undo Count: {}", getUndoCount())});
}

std::vector<PadData> InputRecordingFile::bulkReadPadData(u32 frameStart, u32 frameEnd, const uint port)
{
	std::vector<PadData> data;

	if (m_recordingFile == nullptr || frameEnd < frameStart)
	{
		return data;
	}

	// TODO - no multi-tap support
	for (uint64_t currFrame = frameStart; currFrame < frameEnd; currFrame++)
	{
		const auto padData = readPadData(currFrame, port, 0);
		if (padData)
		{
			data.push_back(padData.value());
		}
	}
	return data;
}

size_t InputRecordingFile::getRecordingBlockSeekPoint(const u32 frame) const noexcept
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
		fread(&m_savestate, sizeof(bool), 1, m_recordingFile) != 1)
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
