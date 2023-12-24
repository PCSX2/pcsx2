// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Updater.h"

#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/ScopedGuard.h"
#include "common/StringUtil.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>

#ifdef _WIN32
#include <shellapi.h>
#endif

#ifdef _WIN32

#include "7zAlloc.h"
#include "7zCrc.h"
#include "SZErrors.h"

static constexpr size_t kInputBufSize = ((size_t)1 << 18);
static constexpr ISzAlloc g_Alloc = {SzAlloc, SzFree};
#endif

static std::FILE* s_file_console_stream;
static constexpr IConsoleWriter s_file_console_writer = {
	[](const char* fmt) { // WriteRaw
		std::fputs(fmt, s_file_console_stream);
		std::fflush(s_file_console_stream);
	},
	[](const char* fmt) { // DoWriteLn
		std::fputs(fmt, s_file_console_stream);
		std::fputc('\n', s_file_console_stream);
		std::fflush(s_file_console_stream);
	},
	[](ConsoleColors) { // DoSetColor
	},
	[](const char* fmt) { // DoWriteFromStdout
		std::fputs(fmt, s_file_console_stream);
		std::fflush(s_file_console_stream);
	},
	[]() { // Newline
		std::fputc('\n', s_file_console_stream);
		std::fflush(s_file_console_stream);
	},
	[](const char*) { // SetTitle
	}};

static void CloseConsoleFile()
{
	if (s_file_console_stream)
		std::fclose(s_file_console_stream);
}

Updater::Updater(ProgressCallback* progress)
	: m_progress(progress)
{
	progress->SetTitle("PCSX2 Update Installer");
}

Updater::~Updater()
{
	CloseUpdateZip();
}

void Updater::SetupLogging(ProgressCallback* progress, const std::string& destination_directory)
{
	const std::string log_path(Path::Combine(destination_directory, "updater.log"));
	s_file_console_stream = FileSystem::OpenCFile(log_path.c_str(), "w");
	if (!s_file_console_stream)
	{
		progress->DisplayFormattedModalError("Failed to open log file '%s'", log_path.c_str());
		return;
	}

	Console_SetActiveHandler(s_file_console_writer);
	std::atexit(CloseConsoleFile);
}

bool Updater::Initialize(std::string destination_directory)
{
	m_destination_directory = std::move(destination_directory);
	m_staging_directory = StringUtil::StdStringFromFormat("%s" FS_OSPATH_SEPARATOR_STR "%s",
		m_destination_directory.c_str(), "UPDATE_STAGING");
	m_progress->DisplayFormattedInformation("Destination directory: '%s'", m_destination_directory.c_str());
	m_progress->DisplayFormattedInformation("Staging directory: '%s'", m_staging_directory.c_str());
	return true;
}

bool Updater::OpenUpdateZip(const char* path)
{
#ifdef _WIN32
	FileInStream_CreateVTable(&m_archive_stream);
	LookToRead2_CreateVTable(&m_look_stream, False);
	CrcGenerateTable();

	m_zip_path = path;

	m_look_stream.buf = (Byte*)ISzAlloc_Alloc(&g_Alloc, kInputBufSize);
	if (!m_look_stream.buf)
	{
		m_progress->DisplayFormattedError("Failed to allocate input buffer?!");
		return false;
	}

	m_look_stream.bufSize = kInputBufSize;
	m_look_stream.realStream = &m_archive_stream.vt;
	LookToRead2_Init(&m_look_stream);

#ifdef _WIN32
	WRes wres = InFile_OpenW(&m_archive_stream.file, StringUtil::UTF8StringToWideString(path).c_str());
#else
	WRes wres = InFile_Open(&m_archive_stream.file, path);
#endif
	if (wres != 0)
	{
		m_progress->DisplayFormattedModalError("Failed to open '%s': %d", path, wres);
		return false;
	}

	m_file_opened = true;
	SzArEx_Init(&m_archive);

	SRes res = SzArEx_Open(&m_archive, &m_look_stream.vt, &g_Alloc, &g_Alloc);
	if (res != SZ_OK)
	{
		m_progress->DisplayFormattedModalError("SzArEx_Open() failed: %s [%d]", SZErrorToString(res), res);
		return false;
	}

	m_archive_opened = true;
	m_progress->SetStatusText("Parsing update zip...");
	return ParseZip();
#else
	return false;
#endif
}

void Updater::CloseUpdateZip()
{
#ifdef _WIN32
	if (m_archive_opened)
	{
		SzArEx_Free(&m_archive, &g_Alloc);
		m_archive_opened = false;
	}

	if (m_look_stream.buf)
	{
		ISzAlloc_Free(&g_Alloc, m_look_stream.buf);
		m_look_stream.buf = nullptr;
	}

	if (m_file_opened)
	{
		File_Close(&m_archive_stream.file);
		m_file_opened = false;
	}
#endif
}

bool Updater::RecursiveDeleteDirectory(const char* path)
{
#ifdef _WIN32
	// making this safer on Win32...
	std::wstring wpath(StringUtil::UTF8StringToWideString(path));
	wpath += L'\0';

	SHFILEOPSTRUCTW op = {};
	op.wFunc = FO_DELETE;
	op.pFrom = wpath.c_str();
	op.fFlags = FOF_NOCONFIRMATION;

	return (SHFileOperationW(&op) == 0 && !op.fAnyOperationsAborted);
#else
	return FileSystem::RecursiveDeleteDirectory(path);
#endif
}

bool Updater::ParseZip()
{
#ifdef _WIN32
	std::vector<UInt16> filename_buffer;

	for (u32 file_index = 0; file_index < m_archive.NumFiles; file_index++)
	{
		// skip directories, we handle them ourselves
		if (SzArEx_IsDir(&m_archive, file_index))
			continue;

		size_t filename_len = SzArEx_GetFileNameUtf16(&m_archive, file_index, nullptr);
		if (filename_len <= 1)
			continue;

		filename_buffer.resize(filename_len);
		SzArEx_GetFileNameUtf16(&m_archive, file_index, filename_buffer.data());

		// TODO: This won't work on Linux (4-byte wchar_t).
		FileToUpdate entry;
		entry.file_index = file_index;
		entry.destination_filename = StringUtil::WideStringToUTF8String(reinterpret_cast<wchar_t*>(filename_buffer.data()));
		if (entry.destination_filename.empty())
			continue;

		// replace forward slashes with backslashes
		for (size_t i = 0; i < entry.destination_filename.length(); i++)
		{
			if (entry.destination_filename[i] == '/' || entry.destination_filename[i] == '\\')
				entry.destination_filename[i] = FS_OSPATH_SEPARATOR_CHARACTER;
		}

		// should never have a leading slash. just in case.
		while (entry.destination_filename[0] == FS_OSPATH_SEPARATOR_CHARACTER)
			entry.destination_filename.erase(0, 1);

		// skip directories (we sort them out later)
		if (!entry.destination_filename.empty() && entry.destination_filename.back() != FS_OSPATH_SEPARATOR_CHARACTER)
		{
			// skip updater itself, since it was already pre-extracted.
			// also skips portable.ini to not mess with future non-portable installs.
			if (StringUtil::Strcasecmp(entry.destination_filename.c_str(), "updater.exe") != 0 &&
				StringUtil::Strcasecmp(entry.destination_filename.c_str(), "portable.ini") != 0)
			{
				m_progress->DisplayFormattedInformation("Found file in zip: '%s'", entry.destination_filename.c_str());
				m_update_paths.push_back(std::move(entry));
			}
		}
	}

	if (m_update_paths.empty())
	{
		m_progress->ModalError("No files found in update zip.");
		return false;
	}

	for (const FileToUpdate& ftu : m_update_paths)
	{
		const size_t len = ftu.destination_filename.length();
		for (size_t i = 0; i < len; i++)
		{
			if (ftu.destination_filename[i] == FS_OSPATH_SEPARATOR_CHARACTER)
			{
				std::string dir(ftu.destination_filename.begin(), ftu.destination_filename.begin() + i);
				while (!dir.empty() && dir[dir.length() - 1] == FS_OSPATH_SEPARATOR_CHARACTER)
					dir.erase(dir.length() - 1);

				if (std::find(m_update_directories.begin(), m_update_directories.end(), dir) == m_update_directories.end())
					m_update_directories.push_back(std::move(dir));
			}
		}
	}

	std::sort(m_update_directories.begin(), m_update_directories.end());
	for (const std::string& dir : m_update_directories)
		m_progress->DisplayFormattedDebugMessage("Directory: %s", dir.c_str());

	return true;
#else
	return false;
#endif
}

bool Updater::PrepareStagingDirectory()
{
	if (FileSystem::DirectoryExists(m_staging_directory.c_str()))
	{
		m_progress->DisplayFormattedWarning("Update staging directory already exists, removing");
		if (!RecursiveDeleteDirectory(m_staging_directory.c_str()) ||
			FileSystem::DirectoryExists(m_staging_directory.c_str()))
		{
			m_progress->ModalError("Failed to remove old staging directory");
			return false;
		}
	}
	if (!FileSystem::CreateDirectoryPath(m_staging_directory.c_str(), false))
	{
		m_progress->DisplayFormattedModalError("Failed to create staging directory %s", m_staging_directory.c_str());
		return false;
	}

	// create subdirectories in staging directory
	for (const std::string& subdir : m_update_directories)
	{
		m_progress->DisplayFormattedInformation("Creating subdirectory in staging: %s", subdir.c_str());

		const std::string staging_subdir =
			StringUtil::StdStringFromFormat("%s" FS_OSPATH_SEPARATOR_STR "%s", m_staging_directory.c_str(), subdir.c_str());
		if (!FileSystem::CreateDirectoryPath(staging_subdir.c_str(), false))
		{
			m_progress->DisplayFormattedModalError("Failed to create staging subdirectory %s", staging_subdir.c_str());
			return false;
		}
	}

	return true;
}

bool Updater::StageUpdate()
{
	m_progress->SetProgressRange(static_cast<u32>(m_update_paths.size()));
	m_progress->SetProgressValue(0);

#ifdef _WIN32
	UInt32 block_index = 0xFFFFFFFF; /* it can have any value before first call (if outBuffer = 0) */
	Byte* out_buffer = 0; /* it must be 0 before first call for each new archive. */
	size_t out_buffer_size = 0; /* it can have any value before first call (if outBuffer = 0) */
	ScopedGuard out_buffer_guard([&out_buffer]() {
		if (out_buffer)
			ISzAlloc_Free(&g_Alloc, out_buffer);
	});

	for (const FileToUpdate& ftu : m_update_paths)
	{
		m_progress->SetFormattedStatusText("Extracting '%s'...", ftu.destination_filename.c_str());
		m_progress->DisplayFormattedInformation("Decompressing '%s'...", ftu.destination_filename.c_str());

		size_t out_offset = 0;
		size_t extracted_size = 0;
		SRes res = SzArEx_Extract(&m_archive, &m_look_stream.vt, ftu.file_index,
			&block_index, &out_buffer, &out_buffer_size, &out_offset, &extracted_size, &g_Alloc, &g_Alloc);
		if (res != SZ_OK)
		{
			m_progress->DisplayFormattedModalError("Failed to decompress file '%s' from 7z (file index=%u, error=%s)",
				ftu.destination_filename.c_str(), ftu.file_index, SZErrorToString(res));
			return false;
		}

		m_progress->DisplayFormattedInformation("Writing '%s' to staging (%zu bytes)...", ftu.destination_filename.c_str(), extracted_size);

		const std::string destination_file = StringUtil::StdStringFromFormat(
			"%s" FS_OSPATH_SEPARATOR_STR "%s", m_staging_directory.c_str(), ftu.destination_filename.c_str());
		std::FILE* fp = FileSystem::OpenCFile(destination_file.c_str(), "wb");
		if (!fp)
		{
			m_progress->DisplayFormattedModalError("Failed to open staging output file '%s'", destination_file.c_str());
			return false;
		}

		const bool wrote_completely = std::fwrite(out_buffer + out_offset, extracted_size, 1, fp) == 1 && std::fflush(fp) == 0;
		if (std::fclose(fp) != 0 || !wrote_completely)
		{
			m_progress->DisplayFormattedModalError("Failed to write output file '%s'", destination_file.c_str());
			FileSystem::DeleteFilePath(destination_file.c_str());
			return false;
		}

		m_progress->IncrementProgressValue();
	}

	return true;
#else
	return false;
#endif
}

bool Updater::CommitUpdate()
{
	m_progress->SetStatusText("Committing update...");

	// create directories in target
	for (const std::string& subdir : m_update_directories)
	{
		const std::string dest_subdir = StringUtil::StdStringFromFormat("%s" FS_OSPATH_SEPARATOR_STR "%s",
			m_destination_directory.c_str(), subdir.c_str());

		if (!FileSystem::DirectoryExists(dest_subdir.c_str()) && !FileSystem::CreateDirectoryPath(dest_subdir.c_str(), false))
		{
			m_progress->DisplayFormattedModalError("Failed to create target directory '%s'", dest_subdir.c_str());
			return false;
		}
	}

	// move files to target
	for (const FileToUpdate& ftu : m_update_paths)
	{
		const std::string staging_file_name = StringUtil::StdStringFromFormat(
			"%s" FS_OSPATH_SEPARATOR_STR "%s", m_staging_directory.c_str(), ftu.destination_filename.c_str());
		const std::string dest_file_name = StringUtil::StdStringFromFormat(
			"%s" FS_OSPATH_SEPARATOR_STR "%s", m_destination_directory.c_str(), ftu.destination_filename.c_str());
		m_progress->DisplayFormattedInformation("Moving '%s' to '%s'", staging_file_name.c_str(), dest_file_name.c_str());
#ifdef _WIN32
		const bool result =
			MoveFileExW(StringUtil::UTF8StringToWideString(staging_file_name).c_str(),
				StringUtil::UTF8StringToWideString(dest_file_name).c_str(), MOVEFILE_REPLACE_EXISTING);
#else
		const bool result = (rename(staging_file_name.c_str(), dest_file_name.c_str()) == 0);
#endif
		if (!result)
		{
			m_progress->DisplayFormattedModalError("Failed to rename '%s' to '%s'", staging_file_name.c_str(),
				dest_file_name.c_str());
			return false;
		}
	}

	return true;
}

void Updater::CleanupStagingDirectory()
{
	// remove staging directory itself
	if (!RecursiveDeleteDirectory(m_staging_directory.c_str()))
		m_progress->DisplayFormattedError("Failed to remove staging directory '%s'", m_staging_directory.c_str());
}

void Updater::RemoveUpdateZip()
{
	if (m_zip_path.empty())
		return;

	CloseUpdateZip();

	if (!FileSystem::DeleteFilePath(m_zip_path.c_str()))
		m_progress->DisplayFormattedError("Failed to remove update zip '%s'", m_zip_path.c_str());
}

std::string Updater::FindPCSX2Exe() const
{
	for (const FileToUpdate& file : m_update_paths)
	{
		const std::string& name = file.destination_filename;
		if (name.find(FS_OSPATH_SEPARATOR_CHARACTER) != name.npos)
			continue; // Main exe is expected to be at the top level
		if (!StringUtil::StartsWithNoCase(name, "pcsx2"))
			continue;
		if (!StringUtil::EndsWithNoCase(name, "exe"))
			continue;
		return name;
	}
	return {};
}
