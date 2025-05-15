// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include <string>
#include <utility>
#include <vector>

#include "common/SmallString.h"
#include "GSVector.h"
#include "GS/Renderers/Common/GSTexture.h"

namespace Threading
{
class ThreadHandle;
}

namespace GSCapture
{
	// Any conversion from game buffer to a capture	buffer requires 8bit per channel for simplicity (no HDR nor 10bit support)
	static const GSTexture::Format CAPTURE_TEX_FORMAT = GSTexture::Format::Color;

	bool BeginCapture(float fps, GSVector2i recommendedResolution, float aspect, std::string filename);
	bool DeliverVideoFrame(GSTexture* stex);
	void DeliverAudioPacket(const s16* frames); // AudioStream::CHUNK_SIZE
	void EndCapture();

	bool IsCapturing();
	bool IsCapturingVideo();
	bool IsCapturingAudio();
	TinyString GetElapsedTime();
	const Threading::ThreadHandle& GetEncoderThreadHandle();
	GSVector2i GetSize();
	std::string GetNextCaptureFileName();
	void Flush();

	using CodecName = std::pair<std::string, std::string>; // shortname,longname
	using CodecList = std::vector<CodecName>;
	CodecList GetVideoCodecList(const char* container);
	CodecList GetAudioCodecList(const char* container);

	using FormatName = std::pair<int , std::string>; // id,name
	using FormatList = std::vector<FormatName>;
	FormatList GetVideoFormatList(const char* codec);
}; // namespace GSCapture
