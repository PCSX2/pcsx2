// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include <string>
#include <utility>
#include <vector>

#include "GSVector.h"

namespace Threading
{
class ThreadHandle;
}

class GSTexture;
class GSDownloadTexture;

namespace GSCapture
{
	bool BeginCapture(float fps, GSVector2i recommendedResolution, float aspect, std::string filename);
	bool DeliverVideoFrame(GSTexture* stex);
	void DeliverAudioPacket(const s16* frames); // SndOutPacketSize
	void EndCapture();

	bool IsCapturing();
	bool IsCapturingVideo();
	bool IsCapturingAudio();
	const Threading::ThreadHandle& GetEncoderThreadHandle();
	GSVector2i GetSize();
	std::string GetNextCaptureFileName();
	void Flush();

	using CodecName = std::pair<std::string, std::string>; // shortname,longname
	using CodecList = std::vector<CodecName>;
	CodecList GetVideoCodecList(const char* container);
	CodecList GetAudioCodecList(const char* container);
}; // namespace GSCapture
