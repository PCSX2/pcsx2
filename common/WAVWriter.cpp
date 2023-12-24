// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/WAVWriter.h"
#include "common/FileSystem.h"
#include "common/Console.h"

#pragma pack(push, 1)
struct WAV_HEADER
{
	u32 chunk_id; // RIFF
	u32 chunk_size;
	u32 format; // WAVE

	struct FormatChunk
	{
		u32 chunk_id; // "fmt "
		u32 chunk_size;
		u16 audio_format; // pcm = 1
		u16 num_channels;
		u32 sample_rate;
		u32 byte_rate;
		u16 block_align;
		u16 bits_per_sample;
	} fmt_chunk;

	struct DataChunkHeader
	{
		u32 chunk_id; // "data "
		u32 chunk_size;
	} data_chunk_header;
};
#pragma pack(pop)

using namespace Common;

WAVWriter::WAVWriter() = default;

WAVWriter::~WAVWriter()
{
	if (IsOpen())
		Close();
}

bool WAVWriter::Open(const char* filename, u32 sample_rate, u32 num_channels)
{
	if (IsOpen())
		Close();

	m_file = FileSystem::OpenCFile(filename, "wb");
	if (!m_file)
		return false;

	m_sample_rate = sample_rate;
	m_num_channels = num_channels;

	if (!WriteHeader())
	{
		Console.Error("Failed to write header to file");
		m_sample_rate = 0;
		m_num_channels = 0;
		std::fclose(m_file);
		m_file = nullptr;
		return false;
	}

	return true;
}

void WAVWriter::Close()
{
	if (!IsOpen())
		return;

	if (std::fseek(m_file, 0, SEEK_SET) != 0 || !WriteHeader())
		Console.Error("Failed to re-write header on file, file may be unplayable");

	std::fclose(m_file);
	m_file = nullptr;
	m_sample_rate = 0;
	m_num_channels = 0;
	m_num_frames = 0;
}

void WAVWriter::WriteFrames(const s16* samples, u32 num_frames)
{
	const u32 num_frames_written =
		static_cast<u32>(std::fwrite(samples, sizeof(s16) * m_num_channels, num_frames, m_file));
	if (num_frames_written != num_frames)
		Console.Error("Only wrote %u of %u frames to output file", num_frames_written, num_frames);

	m_num_frames += num_frames_written;
}

bool WAVWriter::WriteHeader()
{
	const u32 data_size = sizeof(SampleType) * m_num_channels * m_num_frames;

	WAV_HEADER header = {};
	header.chunk_id = 0x46464952; // 0x52494646
	header.chunk_size = sizeof(WAV_HEADER) - 8 + data_size;
	header.format = 0x45564157; // 0x57415645
	header.fmt_chunk.chunk_id = 0x20746d66; // 0x666d7420
	header.fmt_chunk.chunk_size = sizeof(header.fmt_chunk) - 8;
	header.fmt_chunk.audio_format = 1;
	header.fmt_chunk.num_channels = static_cast<u16>(m_num_channels);
	header.fmt_chunk.sample_rate = m_sample_rate;
	header.fmt_chunk.byte_rate = m_sample_rate * m_num_channels * sizeof(SampleType);
	header.fmt_chunk.block_align = static_cast<u16>(m_num_channels * sizeof(SampleType));
	header.fmt_chunk.bits_per_sample = 16;
	header.data_chunk_header.chunk_id = 0x61746164; // 0x64617461
	header.data_chunk_header.chunk_size = data_size;

	return (std::fwrite(&header, sizeof(header), 1, m_file) == 1);
}
