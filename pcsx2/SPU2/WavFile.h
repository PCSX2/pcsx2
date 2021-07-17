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

#pragma once

// Note the file is mostly a copy paste of the WavFile.h from SoundTouch library. It was
// shrunken down and altered to utilize global PCSX2 variables directly

#include "Global.h"

#ifndef uint
typedef unsigned int uint;
#endif


/// WAV audio file 'riff' section header
struct WavRiff
{
	const char riff_char[4] = {'R', 'I', 'F', 'F'};
	int package_len;
	const char wave[4] = {'W', 'A', 'V', 'E'};
};

/// WAV audio file 'format' section header
struct WavFormat
{
	const char fmt[4] = {'f', 'm', 't', ' '};
	int format_len;
	short fixed;
	short channel_number;
	int sample_rate;
	int byte_rate;
	short bytes_per_sample;
	short bits_per_sample;
};

/// WAV audio file 'data' section header
struct WavData
{
	const char data_field[4] = {'d', 'a', 't', 'a'};
	uint data_len;
};


/// WAV audio file header
struct WavHeader
{
	WavRiff riff;
	WavFormat format;
	WavData data;
};


/// Class for writing WAV audio files.
class WavFile
{
private:
	/// Pointer to the WAV file
	FILE* fptr;

	/// WAV file header data.
	WavHeader header;

	/// Fills in WAV file header information.
	void fillInHeader(const bool isCore);

	/// Finishes the WAV file header by supplementing information of amount of
	/// data written to file etc
	void finishHeader();

	/// Writes the WAV file header.
	void writeHeader();

public:
	/// Constructor: Creates a new WAV file. Throws a 'runtime_error' exception
	/// if file creation fails. 
	WavFile(const char* fileName, const bool isCore = false);

	/// Destructor: Finalizes & closes the WAV file.
	~WavFile();

	/// Write data to WAV file. Throws a 'runtime_error' exception if writing to
	/// file fails.
	void write(const StereoOut16& samples, bool isCore = false);
};
