/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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
#include "GS/GSCapture.h"
#include "GS/GSPng.h"
#include "GS/GSUtil.h"
#include "GS/GSExtra.h"
#include "GS/Renderers/Common/GSDevice.h"
#include "GS/Renderers/Common/GSTexture.h"
#include "Host.h"
#include "IconsFontAwesome5.h"
#include "common/Assertions.h"
#include "common/Align.h"
#include "common/DynamicLibrary.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/Threading.h"

#include <condition_variable>
#include <mutex>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavcodec/version.h"
#include "libavformat/avformat.h"
#include "libavformat/version.h"
#include "libavutil/dict.h"
#include "libavutil/version.h"
#include "libswscale/swscale.h"
#include "libswscale/version.h"
}

#include <mutex>

// Compatibility with both ffmpeg 4.x and 5.x.
#if (LIBAVFORMAT_VERSION_MAJOR < 59)
#define ff_const59
#else
#define ff_const59 const
#endif

#define VISIT_AVCODEC_IMPORTS(X) \
	X(avcodec_find_encoder_by_name) \
	X(avcodec_find_encoder) \
	X(avcodec_alloc_context3) \
	X(avcodec_open2) \
	X(avcodec_free_context) \
	X(avcodec_send_frame) \
	X(avcodec_receive_packet) \
	X(avcodec_parameters_from_context) \
	X(av_codec_iterate) \
	X(av_packet_alloc) \
	X(av_packet_free) \
	X(av_packet_rescale_ts)

#define VISIT_AVFORMAT_IMPORTS(X) \
	X(avformat_alloc_output_context2) \
	X(avformat_new_stream) \
	X(avformat_write_header) \
	X(av_guess_format) \
	X(av_interleaved_write_frame) \
	X(av_write_trailer) \
	X(avformat_free_context) \
	X(avformat_query_codec) \
	X(avio_open) \
	X(avio_closep)

#define VISIT_AVUTIL_IMPORTS(X) \
	X(av_frame_alloc) \
	X(av_frame_get_buffer) \
	X(av_frame_free) \
	X(av_strerror) \
	X(av_reduce) \
	X(av_dict_get_string) \
	X(av_dict_parse_string) \
	X(av_dict_set) \
	X(av_dict_free)

#define VISIT_SWSCALE_IMPORTS(X) \
	X(sws_getCachedContext) \
	X(sws_scale) \
	X(sws_freeContext)

namespace GSCapture
{
	static constexpr u32 NUM_FRAMES_IN_FLIGHT = 3;
	static constexpr u32 MAX_PENDING_FRAMES = NUM_FRAMES_IN_FLIGHT * 2;

	struct PendingFrame
	{
		enum class State
		{
			Unused,
			NeedsMap,
			NeedsEncoding
		};

		std::unique_ptr<GSDownloadTexture> tex;
		s64 pts;
		State state;
	};

	static void LogAVError(int errnum, const char* format, ...);
	static bool LoadFFmpeg(bool report_errors);
	static void UnloadFFmpeg(std::unique_lock<std::mutex>& lock);
	static void UnloadFFmpeg();
	static void ProcessFramePendingMap(std::unique_lock<std::mutex>& lock);
	static void ProcessAllInFlightFrames(std::unique_lock<std::mutex>& lock);
	static void EncoderThreadEntryPoint();
	static void StartEncoderThread();
	static void StopEncoderThread(std::unique_lock<std::mutex>& lock);
	static bool SendFrame(const PendingFrame& pf);
	static bool ReceivePackets();
	static void InternalEndCapture(std::unique_lock<std::mutex>& lock);

	static std::mutex s_lock;
	static GSVector2i s_size{};
	static std::string s_filename;
	static bool s_capturing = false;
	static bool s_encoding_error = false;

	static AVFormatContext* s_format_context = nullptr;
	static AVCodecContext* s_codec_context = nullptr;
	static AVStream* s_video_stream = nullptr;
	static AVFrame* s_converted_frame = nullptr; // YUV
	static AVPacket* s_video_packet = nullptr;
	static SwsContext* s_sws_context = nullptr;
	static AVDictionary* s_codec_arguments = nullptr;
	static s64 s_next_pts = 0;

	static Threading::Thread s_encoder_thread;
	static std::condition_variable s_frame_ready_cv;
	static std::condition_variable s_frame_encoded_cv;
	static std::array<PendingFrame, MAX_PENDING_FRAMES> s_pending_frames = {};
	static u32 s_pending_frames_pos = 0;
	static u32 s_frames_pending_map = 0;
	static u32 s_frames_map_consume_pos = 0;
	static u32 s_frames_pending_encode = 0;
	static u32 s_frames_encode_consume_pos = 0;
} // namespace GSCapture

#define DECLARE_IMPORT(X) static decltype(X)* wrap_##X;
VISIT_AVCODEC_IMPORTS(DECLARE_IMPORT);
VISIT_AVFORMAT_IMPORTS(DECLARE_IMPORT);
VISIT_AVUTIL_IMPORTS(DECLARE_IMPORT);
VISIT_SWSCALE_IMPORTS(DECLARE_IMPORT);
#undef DECLARE_IMPORT

// We could refcount this, but really, may as well just load it and pay the cost once.
// Not like we need to save a few megabytes of memory...
static Common::DynamicLibrary s_avcodec_library;
static Common::DynamicLibrary s_avformat_library;
static Common::DynamicLibrary s_avutil_library;
static Common::DynamicLibrary s_swscale_library;
static bool s_library_loaded = false;
static std::mutex s_load_mutex;

bool GSCapture::LoadFFmpeg(bool report_errors)
{
	std::unique_lock lock(s_load_mutex);
	if (s_library_loaded)
		return true;

	const auto open_dynlib = [](Common::DynamicLibrary& lib, const char* name, int major_version) {
		std::string full_name(Common::DynamicLibrary::GetVersionedFilename(name, major_version));
		return lib.Open(full_name.c_str());
	};

	bool result = true;

	result = result && open_dynlib(s_avutil_library, "avutil", LIBAVUTIL_VERSION_MAJOR);
	result = result && open_dynlib(s_avcodec_library, "avcodec", LIBAVCODEC_VERSION_MAJOR);
	result = result && open_dynlib(s_avformat_library, "avformat", LIBAVFORMAT_VERSION_MAJOR);
	result = result && open_dynlib(s_swscale_library, "swscale", LIBSWSCALE_VERSION_MAJOR);

#define RESOLVE_IMPORT(X) result = result && s_avcodec_library.GetSymbol(#X, &wrap_##X);
	VISIT_AVCODEC_IMPORTS(RESOLVE_IMPORT);
#undef RESOLVE_IMPORT

#define RESOLVE_IMPORT(X) result = result && s_avformat_library.GetSymbol(#X, &wrap_##X);
	VISIT_AVFORMAT_IMPORTS(RESOLVE_IMPORT);
#undef RESOLVE_IMPORT

#define RESOLVE_IMPORT(X) result = result && s_avutil_library.GetSymbol(#X, &wrap_##X);
	VISIT_AVUTIL_IMPORTS(RESOLVE_IMPORT);
#undef RESOLVE_IMPORT

#define RESOLVE_IMPORT(X) result = result && s_swscale_library.GetSymbol(#X, &wrap_##X);
	VISIT_SWSCALE_IMPORTS(RESOLVE_IMPORT);
#undef RESOLVE_IMPORT

	if (result)
	{
		s_library_loaded = true;
		return true;
	}

	UnloadFFmpeg(lock);
	lock.unlock();

	if (report_errors)
	{
		Host::ReportErrorAsync("Failed to load FFmpeg",
			fmt::format(
				"You may be missing one or more files, or are using the incorrect version. This build of PCSX2 requires:\n"
				"  libavcodec: {}\n"
				"  libavformat: {}\n"
				"  libavutil: {}\n"
				"  libswscale: {}",
				LIBAVCODEC_VERSION_MAJOR, LIBAVFORMAT_VERSION_MAJOR, LIBAVUTIL_VERSION_MAJOR, LIBSWSCALE_VERSION_MAJOR));
	}

	return false;
}

void GSCapture::UnloadFFmpeg(std::unique_lock<std::mutex>& lock)
{
#define CLEAR_IMPORT(X) wrap_##X = nullptr;
	VISIT_AVCODEC_IMPORTS(CLEAR_IMPORT);
	VISIT_AVFORMAT_IMPORTS(CLEAR_IMPORT);
	VISIT_AVUTIL_IMPORTS(CLEAR_IMPORT);
	VISIT_SWSCALE_IMPORTS(CLEAR_IMPORT);
#undef CLEAR_IMPORT

	s_swscale_library.Close();
	s_avutil_library.Close();
	s_avformat_library.Close();
	s_avcodec_library.Close();
}

void GSCapture::UnloadFFmpeg()
{
	std::unique_lock lock(s_load_mutex);
	if (!s_library_loaded)
		return;

	s_library_loaded = false;
	UnloadFFmpeg(lock);
}

void GSCapture::LogAVError(int errnum, const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	std::string msg(StringUtil::StdStringFromFormatV(format, ap));
	va_end(ap);

	char errbuf[128];
	wrap_av_strerror(errnum, errbuf, sizeof(errbuf));

	Host::AddIconOSDMessage("GSCaptureError", ICON_FA_CAMERA, fmt::format("{}{} ({})", msg, errbuf, errnum), Host::OSD_ERROR_DURATION);
}

bool GSCapture::BeginCapture(float fps, GSVector2i recommendedResolution, float aspect, std::string filename)
{
	Console.WriteLn("Recommended resolution: %d x %d, DAR for muxing: %.4f", recommendedResolution.x, recommendedResolution.y, aspect);
	if (filename.empty() || !LoadFFmpeg(true))
		return false;

	std::unique_lock<std::mutex> lock(s_lock);

	ASSERT(fps != 0);

	InternalEndCapture(lock);

	s_size = GSVector2i(Common::AlignUpPow2(recommendedResolution.x, 8), Common::AlignUpPow2(recommendedResolution.y, 8));
	s_filename = std::move(filename);

	ff_const59 AVOutputFormat* output_format = wrap_av_guess_format(nullptr, s_filename.c_str(), nullptr);
	if (!output_format)
	{
		Console.Error(fmt::format("Failed to get output format for '{}'", s_filename));
		InternalEndCapture(lock);
		return false;
	}

	// find the codec id
	const AVCodec* codec = nullptr;
	if (!GSConfig.VideoCaptureCodec.empty())
	{
		codec = wrap_avcodec_find_encoder_by_name(GSConfig.VideoCaptureCodec.c_str());
		if (!codec)
		{
			Host::AddIconOSDMessage("GSCaptureCodecNotFound", ICON_FA_CAMERA,
				fmt::format("Video codec {} not found, using default.", GSConfig.VideoCaptureCodec),
				Host::OSD_ERROR_DURATION);
		}
	}
	if (!codec)
		codec = wrap_avcodec_find_encoder(output_format->video_codec);
	if (!codec)
	{
		Host::AddIconOSDMessage("GSCaptureError", ICON_FA_CAMERA, "Failed to find encoder.", Host::OSD_ERROR_DURATION);
		InternalEndCapture(lock);
		return false;
	}

	int res = wrap_avformat_alloc_output_context2(&s_format_context, output_format, nullptr, s_filename.c_str());
	if (res < 0)
	{
		LogAVError(res, "avformat_alloc_output_context2() failed: ");
		InternalEndCapture(lock);
		return false;
	}

	s_codec_context = wrap_avcodec_alloc_context3(codec);
	if (!s_codec_context)
	{
		Host::AddIconOSDMessage("GSCaptureError", ICON_FA_CAMERA, "Failed to allocate codec context.", Host::OSD_ERROR_DURATION);
		InternalEndCapture(lock);
		return false;
	}

	s_codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
	s_codec_context->bit_rate = GSConfig.VideoCaptureBitrate * 1000;
	s_codec_context->width = s_size.x;
	s_codec_context->height = s_size.y;
	wrap_av_reduce(&s_codec_context->time_base.num, &s_codec_context->time_base.den,
		10000, static_cast<s64>(static_cast<double>(fps) * 10000.0), std::numeric_limits<s32>::max());

	// Default to YUV 4:2:0 if the codec doesn't specify a pixel format.
	if (!codec->pix_fmts)
	{
		s_codec_context->pix_fmt = AV_PIX_FMT_YUV420P;
	}
	else
	{
		// Prefer YUV420 given the choice, but otherwise fall back to whatever it supports.
		s_codec_context->pix_fmt = codec->pix_fmts[0];
		for (u32 i = 0; codec->pix_fmts[i] != AV_PIX_FMT_NONE; i++)
		{
			if (codec->pix_fmts[i] == AV_PIX_FMT_YUV420P)
			{
				s_codec_context->pix_fmt = codec->pix_fmts[i];
				break;
			}
		}
	}

	s_codec_arguments = nullptr;
	if (GSConfig.EnableVideoCaptureParameters)
	{
		res = wrap_av_dict_parse_string(&s_codec_arguments, GSConfig.VideoCaptureParameters.c_str(), "=", ":", 0);
		if (res < 0)
		{
			LogAVError(res, "av_dict_parse_string() failed: ");
			EndCapture();
			return false;
		}
	}

	if (output_format->flags & AVFMT_GLOBALHEADER)
		s_codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	res = wrap_avcodec_open2(s_codec_context, codec, &s_codec_arguments);
	if (res < 0)
	{
		LogAVError(res, "avcodec_open2() failed: ");
		InternalEndCapture(lock);
		return false;
	}

	s_converted_frame = wrap_av_frame_alloc();
	if (!s_converted_frame)
	{
		Console.Error("Failed to allocate frame");
		InternalEndCapture(lock);
		return false;
	}

	s_converted_frame->format = s_codec_context->pix_fmt;
	s_converted_frame->width = s_codec_context->width;
	s_converted_frame->height = s_codec_context->height;
	res = wrap_av_frame_get_buffer(s_converted_frame, 0);
	if (res < 0)
	{
		LogAVError(res, "av_frame_get_buffer() for converted frame failed: ");
		InternalEndCapture(lock);
		return false;
	}

	s_video_stream = wrap_avformat_new_stream(s_format_context, codec);
	if (!s_video_stream)
	{
		Console.Error("avformat_new_stream() failed");
		InternalEndCapture(lock);
		return false;
	}

	res = wrap_avcodec_parameters_from_context(s_video_stream->codecpar, s_codec_context);
	if (res < 0)
	{
		LogAVError(res, "avcodec_parameters_from_context() failed: ");
		InternalEndCapture(lock);
		return false;
	}

	s_video_stream->time_base = s_codec_context->time_base;
	res = wrap_avio_open(&s_format_context->pb, s_filename.c_str(), AVIO_FLAG_WRITE);
	if (res < 0)
	{
		LogAVError(res, "avio_open() failed: ");
		InternalEndCapture(lock);
		return false;
	}

	res = wrap_avformat_write_header(s_format_context, nullptr);
	if (res < 0)
	{
		LogAVError(res, "avformat_write_header() failed: ");
		InternalEndCapture(lock);
		return false;
	}

	s_video_packet = wrap_av_packet_alloc();
	if (!s_video_packet)
	{
		Console.Error("av_packet_alloc() failed");
		InternalEndCapture(lock);
		return false;
	}

	Host::AddIconOSDMessage("GSCapture", ICON_FA_CAMERA,
		fmt::format("Starting capturing video to '{}'.", Path::GetFileName(s_filename)),
		Host::OSD_INFO_DURATION);

	s_next_pts = 0;
	s_capturing = true;
	StartEncoderThread();
	return true;
}

bool GSCapture::DeliverFrame(GSTexture* stex)
{
	std::unique_lock<std::mutex> lock(s_lock);

	// If the encoder thread reported an error, stop the capture.
	if (s_encoding_error)
	{
		InternalEndCapture(lock);
		return false;
	}

	if (s_frames_pending_map >= NUM_FRAMES_IN_FLIGHT)
		ProcessFramePendingMap(lock);

	PendingFrame& pf = s_pending_frames[s_pending_frames_pos];

	// It shouldn't be pending map, but the encode thread might be lagging.
	pxAssert(pf.state != PendingFrame::State::NeedsMap);
	if (pf.state == PendingFrame::State::NeedsEncoding)
	{
		s_frame_encoded_cv.wait(lock, [&pf]() {
			return pf.state == PendingFrame::State::Unused;
		});
	}

	if (!pf.tex || pf.tex->GetWidth() != stex->GetWidth() || pf.tex->GetHeight() != stex->GetHeight())
	{
		pf.tex.reset();
		pf.tex = g_gs_device->CreateDownloadTexture(stex->GetWidth(), stex->GetHeight(), stex->GetFormat());
		if (!pf.tex)
		{
			Console.Error("GSCapture: Failed to create %x%d download texture", stex->GetWidth(), stex->GetHeight());
			return false;
		}
	}

	const GSVector4i rc(0, 0, stex->GetWidth(), stex->GetHeight());
	pf.tex->CopyFromTexture(rc, stex, rc, 0);
	pf.pts = s_next_pts++;
	pf.state = PendingFrame::State::NeedsMap;

	s_pending_frames_pos = (s_pending_frames_pos + 1) % MAX_PENDING_FRAMES;
	s_frames_pending_map++;
	return true;
}

void GSCapture::ProcessFramePendingMap(std::unique_lock<std::mutex>& lock)
{
	pxAssert(s_frames_pending_map > 0);

	PendingFrame& pf = s_pending_frames[s_frames_map_consume_pos];
	pxAssert(pf.state == PendingFrame::State::NeedsMap);

	// Flushing is potentially expensive, so we leave it unlocked in case the encode thread
	// needs to pick up another thread while we're waiting.
	lock.unlock();

	if (pf.tex->NeedsFlush())
		pf.tex->Flush();

	// Even if the map failed, we need to kick it to the encode thread anyway, because
	// otherwise our queue indices will get desynchronized.
	if (!pf.tex->Map(GSVector4i(0, 0, s_size.x, s_size.y)))
		Console.Warning("GSCapture: Failed to map previously flushed frame.");

	lock.lock();

	// Kick to encoder thread!
	pf.state = PendingFrame::State::NeedsEncoding;
	s_frames_map_consume_pos = (s_frames_map_consume_pos + 1) % MAX_PENDING_FRAMES;
	s_frames_pending_map--;
	s_frames_pending_encode++;
	s_frame_ready_cv.notify_one();
}

void GSCapture::EncoderThreadEntryPoint()
{
	std::unique_lock<std::mutex> lock(s_lock);

	for (;;)
	{
		s_frame_ready_cv.wait(lock, []() { return (s_frames_pending_encode > 0 || !s_capturing); });
		if (!s_capturing)
			break;

		PendingFrame& pf = s_pending_frames[s_frames_encode_consume_pos];
		pxAssert(pf.state == PendingFrame::State::NeedsEncoding);

		lock.unlock();

		// If the frame failed to map, this will be false, and we'll just skip it.
		bool okay = false;
		if (!s_encoding_error && pf.tex->IsMapped())
			okay = SendFrame(pf);

		lock.lock();

		// If we had an encoding error, tell the GS thread to shut down the capture (later).
		if (!okay)
			s_encoding_error = true;

		// Done with this frame! Wait for the next.
		pf.state = PendingFrame::State::Unused;
		s_frames_encode_consume_pos = (s_frames_encode_consume_pos + 1) % MAX_PENDING_FRAMES;
		s_frames_pending_encode--;
		s_frame_encoded_cv.notify_one();
	}
}

void GSCapture::StartEncoderThread()
{
	Console.WriteLn("GSCapture: Starting encoder thread.");
	pxAssert(s_capturing && !s_encoder_thread.Joinable());
	s_encoder_thread.Start(EncoderThreadEntryPoint);
}

void GSCapture::StopEncoderThread(std::unique_lock<std::mutex>& lock)
{
	// Thread will exit when s_capturing is false.
	pxAssert(!s_capturing);

	if (s_encoder_thread.Joinable())
	{
		Console.WriteLn("GSCapture: Stopping encoder thread.");

		// Might be sleeping, so wake it before joining.
		s_frame_ready_cv.notify_one();
		lock.unlock();
		s_encoder_thread.Join();
		lock.lock();
	}
}

bool GSCapture::SendFrame(const PendingFrame& pf)
{
	const AVPixelFormat source_format = g_gs_device->IsRBSwapped() ? AV_PIX_FMT_BGRA : AV_PIX_FMT_RGBA;
	const u8* source_ptr = pf.tex->GetMapPointer();
	const int source_width = static_cast<int>(pf.tex->GetWidth());
	const int source_height = static_cast<int>(pf.tex->GetHeight());
	const int source_pitch = static_cast<int>(pf.tex->GetMapPitch());

	s_sws_context = wrap_sws_getCachedContext(s_sws_context, source_width, source_height, source_format,
		s_converted_frame->width, s_converted_frame->height, s_codec_context->pix_fmt, SWS_BICUBIC,
		nullptr, nullptr, nullptr);
	if (!s_sws_context)
	{
		Console.Error("sws_getCachedContext() failed");
		return false;
	}

	wrap_sws_scale(s_sws_context, reinterpret_cast<const u8**>(&source_ptr), &source_pitch, 0, source_height,
		s_converted_frame->data, s_converted_frame->linesize);

	s_converted_frame->pts = pf.pts;

	const int res = wrap_avcodec_send_frame(s_codec_context, s_converted_frame);
	if (res < 0)
	{
		LogAVError(res, "avcodec_send_frame() failed: ");
		return false;
	}

	return ReceivePackets();
}

void GSCapture::ProcessAllInFlightFrames(std::unique_lock<std::mutex>& lock)
{
	while (s_frames_pending_map > 0)
		ProcessFramePendingMap(lock);

	while (s_frames_pending_encode > 0)
	{
		s_frame_encoded_cv.wait(lock, []() { return (s_frames_pending_encode == 0 || s_encoding_error); });
	}
}

bool GSCapture::ReceivePackets()
{
	for (;;)
	{
		int res = wrap_avcodec_receive_packet(s_codec_context, s_video_packet);
		if (res == AVERROR(EAGAIN) || res == AVERROR_EOF)
		{
			// no more data available
			break;
		}
		else if (res < 0)
		{
			LogAVError(res, "avcodec_receive_packet() failed: ");
			return false;
		}

		s_video_packet->stream_index = s_video_stream->index;

		// in case the frame rate changed...
		wrap_av_packet_rescale_ts(s_video_packet, s_codec_context->time_base, s_video_stream->time_base);

		res = wrap_av_interleaved_write_frame(s_format_context, s_video_packet);
		if (res < 0)
		{
			LogAVError(res, "av_interleaved_write_frame() failed: ");
			return false;
		}
	}

	return true;
}

void GSCapture::InternalEndCapture(std::unique_lock<std::mutex>& lock)
{
	int res;

	const bool was_capturing = s_capturing;

	if (was_capturing)
	{
		if (!s_encoding_error)
		{
			ProcessAllInFlightFrames(lock);
			Host::AddIconOSDMessage("GSCapture", ICON_FA_CAMERA,
				fmt::format("Stopped capturing video to '{}'.", Path::GetFileName(s_filename)),
				Host::OSD_INFO_DURATION);
		}
		else
		{
			Host::AddIconOSDMessage("GSCapture", ICON_FA_CAMERA,
				fmt::format("Video capture aborted due to encoding error in '{}'.", Path::GetFileName(s_filename)),
				Host::OSD_INFO_DURATION);
		}

		s_capturing = false;
		StopEncoderThread(lock);

		s_pending_frames = {};
		s_pending_frames_pos = 0;
		s_frames_pending_map = 0;
		s_frames_map_consume_pos = 0;
		s_frames_pending_encode = 0;
		s_frames_encode_consume_pos = 0;

		s_filename = {};
		s_encoding_error = false;

		// end of stream
		res = wrap_avcodec_send_frame(s_codec_context, nullptr);
		if (res < 0)
			LogAVError(res, "avcodec_send_frame() for EOS failed: ");
		else
			ReceivePackets();

		// end of file!
		res = wrap_av_write_trailer(s_format_context);
		if (res < 0)
			LogAVError(res, "av_write_trailer() failed: ");
	}

	if (s_format_context)
	{
		res = wrap_avio_closep(&s_format_context->pb);
		if (res < 0)
			LogAVError(res, "avio_closep() failed: ");
	}

	if (s_sws_context)
	{
		wrap_sws_freeContext(s_sws_context);
		s_sws_context = nullptr;
	}
	if (s_video_packet)
		wrap_av_packet_free(&s_video_packet);
	if (s_converted_frame)
		wrap_av_frame_free(&s_converted_frame);
	if (s_codec_context)
		wrap_avcodec_free_context(&s_codec_context);
	s_video_stream = nullptr;
	if (s_format_context)
	{
		wrap_avformat_free_context(s_format_context);
		s_format_context = nullptr;
	}
	if (s_codec_arguments)
	{
		wrap_av_dict_free(&s_codec_arguments);
		s_codec_arguments = nullptr;
	}

	if (was_capturing)
		UnloadFFmpeg();
}

void GSCapture::EndCapture()
{
	std::unique_lock<std::mutex> lock(s_lock);
	InternalEndCapture(lock);
}

bool GSCapture::IsCapturing()
{
	return s_capturing;
}

const Threading::ThreadHandle& GSCapture::GetEncoderThreadHandle()
{
	return s_encoder_thread;
}

GSVector2i GSCapture::GetSize()
{
	return s_size;
}

std::vector<std::pair<std::string, std::string>> GSCapture::GetVideoCodecList(const char* container)
{
	std::vector<std::pair<std::string, std::string>> ret;

	if (!LoadFFmpeg(false))
		return ret;

	const AVOutputFormat* output_format = wrap_av_guess_format(nullptr, fmt::format("video.{}", container ? container : "mp4").c_str(), nullptr);
	if (!output_format)
	{
		Console.Error("(GetVideoCodecList) av_guess_format() failed");
		return ret;
	}

	void* iter = nullptr;
	const AVCodec* codec;
	while ((codec = wrap_av_codec_iterate(&iter)) != nullptr)
	{
		// only get audio codecs
		if (codec->type != AVMEDIA_TYPE_VIDEO || !wrap_avcodec_find_encoder(codec->id) || !wrap_avcodec_find_encoder_by_name(codec->name))
			continue;

		if (!wrap_avformat_query_codec(output_format, codec->id, FF_COMPLIANCE_NORMAL))
			continue;

		if (std::find_if(ret.begin(), ret.end(), [codec](const auto& it) { return it.first == codec->name; }) != ret.end())
			continue;

		ret.emplace_back(codec->name, codec->long_name ? codec->long_name : codec->name);
	}

	return ret;
}
