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

#include "InputRecordingFileV2.h"

#include "Utilities/FileUtils.h"
#include "fmt/core.h"
#include "Utilities/wxGuiTools.h"

#include "Recording/Utilities/InputRecordingLogger.h"
#include "Recording/file/v1/InputRecordingFileV1.h"

#include <iostream>
#include <fstream>

bool InputRecordingFileV2::InputRecordingFileHeader::readHeader(std::fstream& file_stream)
{
	if (!file_stream.is_open())
	{
		return false;
	}

	file_stream.seekg(0, std::ios::beg);
	std::string magic_string = "";
	std::getline(file_stream, magic_string, '\0');
	if (magic_string != s_magic_string)
	{
		inputRec::log("Invalid Input Recording File");
		return false;
	}
	file_stream.read((char*)&m_file_version_major, sizeof(m_file_version_major));
	file_stream.read((char*)&m_file_version_minor, sizeof(m_file_version_minor));

	if (m_file_version_major == 2)
	{
		std::getline(file_stream, m_emulator_version, '\0');
		std::getline(file_stream, m_recording_author, '\0');
		std::getline(file_stream, m_game_name, '\0');

		file_stream.read((char*)&m_offset_to_frame_counter, sizeof(int));
		file_stream.read((char*)&m_total_frames, sizeof(m_total_frames));
		file_stream.read((char*)&m_offset_to_redo_counter, sizeof(int));
		file_stream.read((char*)&m_redo_count, sizeof(m_redo_count));
		int recording_type = 0;
		file_stream.read((char*)&recording_type, sizeof(recording_type));
		m_recording_type = static_cast<InputRecordingType>(recording_type);
		file_stream.read((char*)&m_num_controllers_per_frame, sizeof(m_num_controllers_per_frame));
		file_stream.read((char*)&m_offset_to_frame_data, sizeof(int));
	}
	return true;
}

bool InputRecordingFileV2::InputRecordingFileHeader::replaceHeader(std::fstream& file_stream, fs::path path)
{
	if (!file_stream.is_open())
	{
		return false;
	}
	// If the file is existing, we need to re-write the rest of the file as fields may have gotten larger
	fs::path temp = fs::path(path);
	temp.replace_extension("tmp");
	std::ofstream create_temp = FileUtils::binaryFileOutputStream(temp);
	create_temp << file_stream.rdbuf();
	const int original_frame_data_seek_point = m_offset_to_frame_data;
	create_temp.close(); // can't use an `fstream` because it won't create the file if it doesn't exist
	std::ifstream original_file = FileUtils::binaryFileInputStream(temp);

	// Write the header!
	writeHeader(file_stream);

	// Write the rest of the recording file back to the original file
	original_file.seekg(original_frame_data_seek_point, std::ios::beg);
	file_stream << original_file.rdbuf();
	original_file.close();
	fs::remove(temp.wstring());

	return true;
}

bool InputRecordingFileV2::InputRecordingFileHeader::writeHeader(std::fstream& file_stream)
{
	if (!file_stream.is_open())
	{
		return false;
	}

	// Write new header to actual recording file
	if (m_file_version_major == 2)
	{
		// Initial Header Data
		file_stream.seekp(0, std::ios::beg);
		file_stream.write(s_magic_string.c_str(), s_magic_string.size() + 1);
		file_stream.write((char*)&m_file_version_major, sizeof(m_file_version_major));
		file_stream.write((char*)&m_file_version_minor, sizeof(m_file_version_minor));
		// Header Fields
		file_stream.write(m_emulator_version.c_str(), m_emulator_version.size() + 1);
		file_stream.write(m_recording_author.c_str(), m_recording_author.size() + 1);
		file_stream.write(m_game_name.c_str(), m_game_name.size() + 1);

		m_offset_to_frame_counter = int(file_stream.tellp()) + sizeof(int);
		file_stream.write((char*)&m_offset_to_frame_counter, sizeof(int));
		file_stream.write((char*)&m_total_frames, sizeof(m_total_frames));

		m_offset_to_redo_counter = int(file_stream.tellp()) + sizeof(int);
		file_stream.write((char*)&m_offset_to_redo_counter, sizeof(int));
		file_stream.write((char*)&m_redo_count, sizeof(m_redo_count));

		const int recordingType = static_cast<int>(m_recording_type);
		file_stream.write((char*)&recordingType, sizeof(recordingType));
		file_stream.write((char*)&m_num_controllers_per_frame, sizeof(m_num_controllers_per_frame));

		m_offset_to_frame_data = int(file_stream.tellp()) + sizeof(int);
		file_stream.write((char*)&m_offset_to_frame_data, sizeof(int));
	}

	return true;
}

bool InputRecordingFileV2::InputRecordingFileHeader::updateHeaderInplace(std::fstream& fileStream)
{
	if (!fileStream.is_open())
	{
		return false;
	}

	if (m_file_version_major == 2)
	{
		fileStream.seekp(m_offset_to_frame_counter, std::ios::beg);
		fileStream.write((char*)&m_total_frames, sizeof(m_total_frames));

		fileStream.seekp(m_offset_to_redo_counter, std::ios::beg);
		fileStream.write((char*)&m_redo_count, sizeof(m_redo_count));
	}

	return true;
}

/// --- Input Recording File

bool InputRecordingFileV2::convertFromV1(InputRecordingFileV1& legacy_file, fs::path legacy_path)
{
	fs::path new_path = legacy_path.replace_extension(s_extension);
	const InputRecordingType type = legacy_file.FromSaveState() ? InputRecordingType::INPUT_RECORDING_SAVESTATE : InputRecordingType::INPUT_RECORDING_POWER_ON;
	// Copy the savestate file from the legacy file
	if (type == InputRecordingType::INPUT_RECORDING_SAVESTATE)
	{
		fs::copy_file(legacy_path.replace_extension("p2m2_SaveState.p2s"),
					  FileUtils::appendToFilename(new_path, "_SaveState").replace_extension("p2s"),
					  fs::copy_options::overwrite_existing);
	}
	// Create the file
	bool created_successfully = createNewFile(new_path, legacy_file.GetHeader().author, legacy_file.GetHeader().gameName, type);
	if (!created_successfully)
	{
		return false;
	}

	setFrameCounter(legacy_file.GetTotalFrames());

	for (long i = 0; i < getTotalFrames(); i++)
	{
		// TODO - get this from a constant
		for (int port = 0; port < 2; port++)
		{
			// TODO - get this from a constant
			for (int bufIndex = 0; bufIndex < 18; bufIndex++)
			{
				u8 result;
				legacy_file.ReadKeyBuffer(result, i, port, bufIndex);
				writeInputFromBuffer(i, port, bufIndex, result);
			}
		}
	}

	return true;
}

bool InputRecordingFileV2::createNewFile(const fs::path& path, const std::string& recording_author, const std::string& game_name, const InputRecordingType& recording_type)
{
	m_file_stream.close();
	m_file_path = path;
	m_header = InputRecordingFileHeader();
	m_file_stream = FileUtils::binaryFileStream(m_file_path, true);

	// Init header values
	m_header.m_emulator_version = fmt::format("{} - {}.{}.{}", pxGetAppName().c_str(), PCSX2_VersionHi, PCSX2_VersionMid, PCSX2_VersionLo);
	m_header.m_recording_author = recording_author;
	m_header.m_game_name = game_name;
	m_header.m_recording_type = recording_type;

	// Write the header to the file
	return m_header.writeHeader(m_file_stream);
}

bool InputRecordingFileV2::openExistingFile(const fs::path& path)
{
	m_file_stream.close();
	m_file_path = path;
	m_header = InputRecordingFileHeader();
	if (!fs::exists(m_file_path))
	{
		// TODO - if file doesn't exist, throw an error
		return false;
	}
	m_file_stream = FileUtils::binaryFileStream(m_file_path);

	// Read header values, it will return false if it's invalid / do appropriate logging
	return m_header.readHeader(m_file_stream);
}

bool InputRecordingFileV2::closeFile()
{
	m_file_stream.close();
	return true;
}

fs::path InputRecordingFileV2::getFileName()
{
	return m_file_path.filename();
}

std::string InputRecordingFileV2::getRecordingFileVersion()
{
	return fmt::format("v{}.{}", m_header.m_file_version_major, m_header.m_file_version_minor);
}

std::string InputRecordingFileV2::getEmulatorVersion()
{
	return m_header.m_emulator_version;
}

std::string InputRecordingFileV2::getAuthor()
{
	return m_header.m_recording_author;
}

std::string InputRecordingFileV2::getGameName()
{
	return m_header.m_game_name;
}

long InputRecordingFileV2::getTotalFrames()
{
	return m_header.m_total_frames;
}

long InputRecordingFileV2::getRedoCount()
{
	return m_header.m_redo_count;
}

InputRecordingFileV2::InputRecordingType InputRecordingFileV2::getRecordingType()
{
	return m_header.m_recording_type;
}

bool InputRecordingFileV2::isFromBoot()
{
	return m_header.m_recording_type == InputRecordingType::INPUT_RECORDING_POWER_ON;
}

bool InputRecordingFileV2::isMacro()
{
	return m_header.m_recording_type == InputRecordingType::INPUT_RECORDING_MACRO;
}

bool InputRecordingFileV2::isFromSavestate()
{
	return m_header.m_recording_type == InputRecordingType::INPUT_RECORDING_SAVESTATE;
}

u8 InputRecordingFileV2::numberOfControllersPerFrame()
{
	return m_header.m_num_controllers_per_frame;
}

void InputRecordingFileV2::incrementRedoCount()
{
	m_header.m_redo_count++;
	m_header.updateHeaderInplace(m_file_stream);
}

void InputRecordingFileV2::incrementFrameCounter()
{
	m_header.m_total_frames++;
	m_header.updateHeaderInplace(m_file_stream);
}

void InputRecordingFileV2::setFrameCounter(const long& num_frames)
{
	m_header.m_total_frames = num_frames;
	m_header.updateHeaderInplace(m_file_stream);
}

long InputRecordingFileV2::bytesPerController()
{
	return (isMacro() ? s_controller_bytes_per_frame * 2 : s_controller_bytes_per_frame);
}

long InputRecordingFileV2::bytesPerFrame()
{
	return m_header.m_num_controllers_per_frame * bytesPerController();
}

long InputRecordingFileV2::getFrameSeekpoint(const uint& frame)
{
	return m_header.m_offset_to_frame_data + (frame * bytesPerFrame());
}

long InputRecordingFileV2::getOffsetWithinFrame(const long& frame_seekpoint, const uint& port, const uint& buf_index)
{
	return frame_seekpoint + (port * bytesPerController()) + (isMacro() ? buf_index * 2 : buf_index);
}

bool InputRecordingFileV2::readInputIntoBuffer(u8& result, const uint& frame, const uint& port, const uint& buf_index)
{
	if (!m_file_stream.is_open())
	{
		return false;
	}

	const long seekPos = getOffsetWithinFrame(getFrameSeekpoint(frame), port, buf_index);
	m_file_stream.seekg(seekPos, std::ios::beg);
	if (m_header.m_recording_type == InputRecordingType::INPUT_RECORDING_MACRO)
	{
		// First two bytes are bitfields, not bools.  Use the bitfield to ignore / discard values from the recording
		if (buf_index == 0 || buf_index == 1)
		{
			u8 ignore_bitfield;
			m_file_stream.read((char*)&ignore_bitfield, sizeof(ignore_bitfield));
			ignore_bitfield = ~ignore_bitfield; // Flip the bits so the logic makes sense
			u8 temp_result;
			m_file_stream.read((char*)&temp_result, sizeof(temp_result));
			temp_result &= ignore_bitfield; // Discard the bits we want to ignore
			result |= temp_result;          // Combine with existing buffer value
			return !m_file_stream.fail();
		}
		else
		{
			bool ignore_input;
			m_file_stream.read((char*)&ignore_input, sizeof(ignore_input));
			if (ignore_input)
			{
				// Ignore the input, don't modify the result!
				return !m_file_stream.fail();
			}
		}
	}
	m_file_stream.read((char*)&result, sizeof(result));
	return !m_file_stream.fail();
}

bool InputRecordingFileV2::writeInputFromBuffer(const uint& frame, const uint& port, const uint& buf_index, const u8& buf)
{
	if (!m_file_stream.is_open())
	{
		return false;
	}
	const long seekPos = getOffsetWithinFrame(getFrameSeekpoint(frame), port, buf_index);
	m_file_stream.seekp(seekPos, std::ios::beg);
	m_file_stream.write((char*)&buf, sizeof(buf));
	return !m_file_stream.fail();
}

bool InputRecordingFileV2::writeDefaultMacroInputFromBuffer(const uint& frame, const uint& port, const uint& buf_index, const u8& buf)
{
	if (!m_file_stream.is_open())
	{
		return false;
	}
	const long seekPos = getOffsetWithinFrame(getFrameSeekpoint(frame), port, buf_index);
	m_file_stream.seekp(seekPos, std::ios::beg);
	// For the first two bytes, we have to store bitfields instead of simple booleans
	// A macro cannot be effectively created through the controller, so this serves as the default starting case
	// Inputs can be flagged to be ignored in the recording viewer/editor later
	bool dont_ignore = false;
	m_file_stream.write((char*)&dont_ignore, sizeof(dont_ignore));
	m_file_stream.write((char*)&buf, sizeof(buf));
	return !m_file_stream.fail();
}

#endif
