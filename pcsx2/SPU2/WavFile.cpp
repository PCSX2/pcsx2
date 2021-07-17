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

// Note the file is mostly a copy paste of the WavFile.cpp from SoundTouch library. It was
// shrunken down and altered to utilize global PCSX2 variables directly

#include "PrecompiledHeader.h"
#include "WavFile.h"
#include "App.h"

WavFile::WavFile(const char* fileName, const bool isCore)
{
	fptr = fopen(fileName, "wb");
	if (fptr == nullptr)
	{
		std::string msg = "Error : Unable to open file \"";
		msg += fileName;
		msg += "\" for writing.";
		throw std::runtime_error(msg);
	}

	fillInHeader(isCore);
	writeHeader();
}

WavFile::~WavFile()
{
	if (fptr)
	{
		finishHeader();
		fclose(fptr);
	}
}

void WavFile::fillInHeader(const bool isCore)
{
	// package_len unknown so far
	header.riff.package_len = 0;

	// fill in the 'format' part..
	
	header.format.format_len = 0x10;
	header.format.fixed = 1;
	if (g_Conf->AudioCapture.ChannelConfig == AudioCaptureSetting::Audio_Stereo || isCore)
		header.format.channel_number = 2;
	else
		header.format.channel_number = 1;

	header.format.sample_rate = SampleRate;
	header.format.bits_per_sample = 16;
	header.format.bytes_per_sample = 2;
	header.format.byte_rate = SampleRate * 2;
	
	// fill in the 'data' part..
	// data_len unknown so far
	header.data.data_len = 0;
}

void WavFile::finishHeader()
{
	// supplement the file length into the header structure
	header.data.data_len = (uint)ftell(fptr) - sizeof(WavHeader);
	if (header.data.data_len & 1)
		fputc(0, fptr);

	header.riff.package_len = header.data.data_len + 36;
	writeHeader();
}

void WavFile::writeHeader()
{
	// write the supplemented header in the beginning of the file
	fseek(fptr, 0, SEEK_SET);
	if (fwrite(&header, sizeof(header), 1, fptr) != 1)
		throw std::runtime_error("Error while writing to a wav file.");

	// jump back to the end of the file
	fseek(fptr, 0, SEEK_END);
}

void WavFile::write(const StereoOut16& samples, bool isCore)
{
	if (!isCore && g_Conf->AudioCapture.ChannelConfig == AudioCaptureSetting::Audio_Mono)
	{
		const s16 mono = (samples.Left >> 1) + (samples.Right >> 1);
		if (fwrite(&mono, 2, 1, fptr) != 1)
			throw std::runtime_error("Error while writing to a wav file.");
	}
	// Stereo
	else if (fwrite(&samples, 2, 2, fptr) != 2)
		throw std::runtime_error("Error while writing to a wav file.");
}
