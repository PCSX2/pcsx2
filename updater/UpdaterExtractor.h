// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/FileSystem.h"
#include "common/ScopedGuard.h"
#include "common/StringUtil.h"

#include "fmt/core.h"

#if defined(_WIN32)
#include "7z.h"
#include "7zAlloc.h"
#include "7zCrc.h"
#include "7zFile.h"
#include "SZErrors.h"
#endif

#include <cstdio>
#include <string>
#include <vector>

#ifdef _WIN32
static constexpr char UPDATER_EXECUTABLE[] = "updater.exe";
static constexpr char UPDATER_ARCHIVE_NAME[] = "update.7z";
#endif

static inline bool ExtractUpdater(const char* archive_path, const char* destination_path, std::string* error)
{
#if defined(_WIN32)
	static constexpr size_t kInputBufSize = ((size_t)1 << 18);
	static constexpr ISzAlloc g_Alloc = {SzAlloc, SzFree};

	CFileInStream instream = {};
	CLookToRead2 lookstream = {};
	CSzArEx archive = {};

	FileInStream_CreateVTable(&instream);
	LookToRead2_CreateVTable(&lookstream, False);
	CrcGenerateTable();

	lookstream.buf = (Byte*)ISzAlloc_Alloc(&g_Alloc, kInputBufSize);
	if (!lookstream.buf)
	{
		*error = "Failed to allocate input buffer?!";
		return false;
	}

	lookstream.bufSize = kInputBufSize;
	lookstream.realStream = &instream.vt;
	LookToRead2_Init(&lookstream);
	ScopedGuard buffer_guard([&lookstream]() {
		ISzAlloc_Free(&g_Alloc, lookstream.buf);
	});

#ifdef _WIN32
	WRes wres = InFile_OpenW(&instream.file, StringUtil::UTF8StringToWideString(archive_path).c_str());
#else
	WRes wres = InFile_Open(&instream.file, archive_path);
#endif
	if (wres != 0)
	{
		*error = fmt::format("Failed to open '{0}': {1}", archive_path, wres);
		return false;
	}

	ScopedGuard file_guard([&instream]() {
		File_Close(&instream.file);
	});

	SzArEx_Init(&archive);

	SRes res = SzArEx_Open(&archive, &lookstream.vt, &g_Alloc, &g_Alloc);
	if (res != SZ_OK)
	{
		*error = fmt::format("SzArEx_Open() failed: {0} [{1}]", SZErrorToString(res), res);
		return false;
	}
	ScopedGuard archive_guard([&archive]() {
		SzArEx_Free(&archive, &g_Alloc);
	});

	std::vector<UInt16> filename_buffer;
	u32 updater_file_index = archive.NumFiles;
	for (u32 file_index = 0; file_index < archive.NumFiles; file_index++)
	{
		if (SzArEx_IsDir(&archive, file_index))
			continue;

		size_t filename_len = SzArEx_GetFileNameUtf16(&archive, file_index, nullptr);
		if (filename_len <= 1)
			continue;

		filename_buffer.resize(filename_len);
		filename_len = SzArEx_GetFileNameUtf16(&archive, file_index, filename_buffer.data());

		// TODO: This won't work on Linux (4-byte wchar_t).
		const std::string filename(StringUtil::WideStringToUTF8String(reinterpret_cast<wchar_t*>(filename_buffer.data())));
		if (filename != UPDATER_EXECUTABLE)
			continue;

		updater_file_index = file_index;
		break;
	}

	if (updater_file_index == archive.NumFiles)
	{
		*error = fmt::format("Updater executable ({}) not found in archive.", UPDATER_EXECUTABLE);
		return false;
	}

	UInt32 block_index = 0xFFFFFFFF; /* it can have any value before first call (if outBuffer = 0) */
	Byte* out_buffer = 0; /* it must be 0 before first call for each new archive. */
	size_t out_buffer_size = 0; /* it can have any value before first call (if outBuffer = 0) */
	ScopedGuard out_buffer_guard([&out_buffer]() {
		if (out_buffer)
			ISzAlloc_Free(&g_Alloc, out_buffer);
	});

	size_t out_offset = 0;
	size_t extracted_size = 0;
	res = SzArEx_Extract(&archive, &lookstream.vt, updater_file_index,
		&block_index, &out_buffer, &out_buffer_size, &out_offset, &extracted_size, &g_Alloc, &g_Alloc);
	if (res != SZ_OK)
	{
		*error = fmt::format("Failed to decompress {0} from 7z (file index=%u, error=%s)",
			UPDATER_EXECUTABLE, updater_file_index, SZErrorToString(res));
		return false;
	}

	std::FILE* fp = FileSystem::OpenCFile(destination_path, "wb");
	if (!fp)
	{
		*error = fmt::format("Failed to open '{0}' for writing.", destination_path);
		return false;
	}

	const bool wrote_completely = std::fwrite(out_buffer + out_offset, extracted_size, 1, fp) == 1 && std::fflush(fp) == 0;
	if (std::fclose(fp) != 0 || !wrote_completely)
	{
		*error = fmt::format("Failed to write output file '{}'", destination_path);
		FileSystem::DeleteFilePath(destination_path);
		return false;
	}

	error->clear();
	return true;
#else
	*error = "Not supported on this platform";
	return false;
#endif
}
