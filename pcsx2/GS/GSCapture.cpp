// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GS/GSCapture.h"
#include "GS/GSPng.h"
#include "GS/GSUtil.h"
#include "GS/GSExtra.h"
#include "GS/Renderers/Common/GSDevice.h"
#include "GS/Renderers/Common/GSTexture.h"
#include "SPU2/spu2.h"
#include "SPU2/SndOut.h"
#include "Host.h"
#include "IconsFontAwesome5.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/BitUtils.h"
#include "common/DynamicLibrary.h"
#include "common/Path.h"
#include "common/SmallString.h"
#include "common/StringUtil.h"
#include "common/Threading.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>

#if defined(_MSC_VER)
#pragma warning(disable:4996) // warning C4996: 'AVCodecContext::channels': was declared deprecated
#elif defined (__clang__)
// We're using deprecated fields because we're targeting multiple ffmpeg versions.
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavcodec/version.h"
#include "libavformat/avformat.h"
#include "libavformat/version.h"
#include "libavutil/dict.h"
#include "libavutil/opt.h"
#include "libavutil/version.h"
#include "libswscale/swscale.h"
#include "libswscale/version.h"
#include "libswresample/swresample.h"
#include "libswresample/version.h"
}

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
	X(avcodec_get_hw_config) \
	X(av_codec_iterate) \
	X(av_packet_alloc) \
	X(av_packet_free) \
	X(av_packet_rescale_ts) \
	X(av_packet_unref)

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
	X(av_frame_make_writable) \
	X(av_strerror) \
	X(av_reduce) \
	X(av_dict_parse_string) \
	X(av_dict_get) \
	X(av_dict_free) \
	X(av_opt_set_int) \
	X(av_opt_set_sample_fmt) \
	X(av_compare_ts) \
	X(av_get_bytes_per_sample) \
	X(av_sample_fmt_is_planar) \
	X(av_d2q) \
	X(av_hwdevice_get_type_name) \
	X(av_hwdevice_ctx_create) \
	X(av_hwframe_ctx_alloc) \
	X(av_hwframe_ctx_init) \
	X(av_hwframe_transfer_data) \
	X(av_hwframe_get_buffer) \
	X(av_buffer_ref) \
	X(av_buffer_unref)

#define VISIT_SWSCALE_IMPORTS(X) \
	X(sws_getCachedContext) \
	X(sws_scale) \
	X(sws_freeContext)

#define VISIT_SWRESAMPLE_IMPORTS(X) \
	X(swr_alloc) \
	X(swr_init) \
	X(swr_free) \
	X(swr_convert) \
	X(swr_next_pts)

namespace GSCapture
{
	static constexpr u32 NUM_FRAMES_IN_FLIGHT = 3;
	static constexpr u32 MAX_PENDING_FRAMES = NUM_FRAMES_IN_FLIGHT * 2;
	static constexpr u32 AUDIO_BUFFER_SIZE = Common::AlignUpPow2((MAX_PENDING_FRAMES * 48000) / 60, SndOutPacketSize);
	static constexpr u32 AUDIO_CHANNELS = 2;

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
	static void UnloadFFmpeg();
	static std::string GetCaptureTypeForMessage(bool capture_video, bool capture_audio);
	static bool IsUsingHardwareVideoEncoding();
	static void ProcessFramePendingMap(std::unique_lock<std::mutex>& lock);
	static void ProcessAllInFlightFrames(std::unique_lock<std::mutex>& lock);
	static void EncoderThreadEntryPoint();
	static void StartEncoderThread();
	static void StopEncoderThread(std::unique_lock<std::mutex>& lock);
	static bool SendFrame(const PendingFrame& pf);
	static bool ReceivePackets(AVCodecContext* codec_context, AVStream* stream, AVPacket* packet);
	static bool ProcessAudioPackets(s64 video_pts);
	static void InternalEndCapture(std::unique_lock<std::mutex>& lock);
	static CodecList GetCodecListForContainer(const char* container, AVMediaType type);

	static std::mutex s_lock;
	static GSVector2i s_size{};
	static std::string s_filename;
	static std::atomic_bool s_capturing{false};
	static std::atomic_bool s_encoding_error{false};

	static AVFormatContext* s_format_context = nullptr;

	static AVCodecContext* s_video_codec_context = nullptr;
	static AVStream* s_video_stream = nullptr;
	static AVFrame* s_converted_video_frame = nullptr; // YUV
	static AVFrame* s_hw_video_frame = nullptr;
	static AVPacket* s_video_packet = nullptr;
	static SwsContext* s_sws_context = nullptr;
	static AVDictionary* s_video_codec_arguments = nullptr;
	static AVBufferRef* s_video_hw_context = nullptr;
	static AVBufferRef* s_video_hw_frames = nullptr;
	static s64 s_next_video_pts = 0;

	static AVCodecContext* s_audio_codec_context = nullptr;
	static AVStream* s_audio_stream = nullptr;
	static AVFrame* s_converted_audio_frame = nullptr;
	static AVPacket* s_audio_packet = nullptr;
	static SwrContext* s_swr_context = nullptr;
	static AVDictionary* s_audio_codec_arguments = nullptr;
	static s64 s_next_audio_pts = 0;
	static u32 s_audio_frame_bps = 0;
	static u32 s_audio_frame_size = 0;
	static u32 s_audio_frame_pos = 0;
	static bool s_audio_frame_planar = false;

	static Threading::Thread s_encoder_thread;
	static std::condition_variable s_frame_ready_cv;
	static std::condition_variable s_frame_encoded_cv;
	static std::array<PendingFrame, MAX_PENDING_FRAMES> s_pending_frames = {};
	static u32 s_pending_frames_pos = 0;
	static u32 s_frames_pending_map = 0;
	static u32 s_frames_map_consume_pos = 0;
	static u32 s_frames_pending_encode = 0;
	static u32 s_frames_encode_consume_pos = 0;

	// NOTE: So this doesn't need locking, we allocate it once, and leave it.
	static std::unique_ptr<s16[]> s_audio_buffer;
	static std::atomic<u32> s_audio_buffer_size{0};
	static u32 s_audio_buffer_write_pos = 0;
	alignas(64) static u32 s_audio_buffer_read_pos = 0;
} // namespace GSCapture

#ifndef USE_LINKED_FFMPEG
#define DECLARE_IMPORT(X) static decltype(X)* wrap_##X;
#else
#define DECLARE_IMPORT(X) static constexpr decltype(X)* wrap_##X = X;
#endif
VISIT_AVCODEC_IMPORTS(DECLARE_IMPORT);
VISIT_AVFORMAT_IMPORTS(DECLARE_IMPORT);
VISIT_AVUTIL_IMPORTS(DECLARE_IMPORT);
VISIT_SWSCALE_IMPORTS(DECLARE_IMPORT);
VISIT_SWRESAMPLE_IMPORTS(DECLARE_IMPORT);
#undef DECLARE_IMPORT

// We could refcount this, but really, may as well just load it and pay the cost once.
// Not like we need to save a few megabytes of memory...
#ifndef USE_LINKED_FFMPEG
static void UnloadFFmpegFunctions(std::unique_lock<std::mutex>& lock);

static Common::DynamicLibrary s_avcodec_library;
static Common::DynamicLibrary s_avformat_library;
static Common::DynamicLibrary s_avutil_library;
static Common::DynamicLibrary s_swscale_library;
static Common::DynamicLibrary s_swresample_library;
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
	result = result && open_dynlib(s_swresample_library, "swresample", LIBSWRESAMPLE_VERSION_MAJOR);

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

#define RESOLVE_IMPORT(X) result = result && s_swresample_library.GetSymbol(#X, &wrap_##X);
	VISIT_SWRESAMPLE_IMPORTS(RESOLVE_IMPORT);
#undef RESOLVE_IMPORT

	if (result)
	{
		s_library_loaded = true;
		return true;
	}

	UnloadFFmpegFunctions(lock);
	lock.unlock();

	if (report_errors)
	{
		Host::ReportErrorAsync("Failed to load FFmpeg",
			fmt::format("You may be missing one or more files, or are using the incorrect version. This build of PCSX2 requires:\n"
						"  libavcodec: {}\n"
						"  libavformat: {}\n"
						"  libavutil: {}\n"
						"  libswscale: {}\n"
				"  libswresample: {}\n", LIBAVCODEC_VERSION_MAJOR, LIBAVFORMAT_VERSION_MAJOR, LIBAVUTIL_VERSION_MAJOR,
				LIBSWSCALE_VERSION_MAJOR, LIBSWRESAMPLE_VERSION_MAJOR));
	}

	return false;
}

void UnloadFFmpegFunctions(std::unique_lock<std::mutex>& lock)
{
#define CLEAR_IMPORT(X) wrap_##X = nullptr;
	VISIT_AVCODEC_IMPORTS(CLEAR_IMPORT);
	VISIT_AVFORMAT_IMPORTS(CLEAR_IMPORT);
	VISIT_AVUTIL_IMPORTS(CLEAR_IMPORT);
	VISIT_SWSCALE_IMPORTS(CLEAR_IMPORT);
	VISIT_SWRESAMPLE_IMPORTS(CLEAR_IMPORT);
#undef CLEAR_IMPORT

	s_swresample_library.Close();
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
	UnloadFFmpegFunctions(lock);
}

#else

bool GSCapture::LoadFFmpeg(bool report_errors)
{
	return true;
}

void GSCapture::UnloadFFmpeg()
{
}

#endif

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

std::string GSCapture::GetCaptureTypeForMessage(bool capture_video, bool capture_audio)
{
	return capture_video ? (capture_audio ? "capturing audio and video" : "capturing video") : "capturing audio";
}

bool GSCapture::IsUsingHardwareVideoEncoding()
{
	return (s_video_hw_context != nullptr);
}

bool GSCapture::BeginCapture(float fps, GSVector2i recommendedResolution, float aspect, std::string filename)
{
	const bool capture_video = GSConfig.EnableVideoCapture;
	const bool capture_audio = GSConfig.EnableAudioCapture;

	Console.WriteLn("Recommended resolution: %d x %d, DAR for muxing: %.4f", recommendedResolution.x, recommendedResolution.y, aspect);
	if (filename.empty() || !LoadFFmpeg(true))
		return false;

	std::unique_lock<std::mutex> lock(s_lock);

	pxAssert(fps != 0);

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

	int res = wrap_avformat_alloc_output_context2(&s_format_context, output_format, nullptr, s_filename.c_str());
	if (res < 0)
	{
		LogAVError(res, "avformat_alloc_output_context2() failed: ");
		InternalEndCapture(lock);
		return false;
	}

	// find the codec id
	if (capture_video)
	{
		const float sample_aspect_ratio = aspect / (static_cast<float>(s_size.x) / static_cast<float>(s_size.y));

		const AVCodec* vcodec = nullptr;
		if (!GSConfig.VideoCaptureCodec.empty())
		{
			vcodec = wrap_avcodec_find_encoder_by_name(GSConfig.VideoCaptureCodec.c_str());
			if (!vcodec)
			{
				Host::AddIconOSDMessage("GSCaptureCodecNotFound", ICON_FA_CAMERA,
					fmt::format("Video codec {} not found, using default.", GSConfig.VideoCaptureCodec), Host::OSD_ERROR_DURATION);
			}
		}

		// FFmpeg decides whether mp4, mkv, etc should use h264 or mpeg4 as their default codec by whether x264 was enabled
		// But there's a lot of other h264 encoders (e.g. hardware encoders) we may want to use instead
		if (!vcodec && wrap_avformat_query_codec(output_format, AV_CODEC_ID_H264, FF_COMPLIANCE_NORMAL))
			vcodec = wrap_avcodec_find_encoder(AV_CODEC_ID_H264);
		if (!vcodec)
			vcodec = wrap_avcodec_find_encoder(output_format->video_codec);

		if (!vcodec)
		{
			Host::AddIconOSDMessage("GSCaptureError", ICON_FA_CAMERA, "Failed to find video encoder.", Host::OSD_ERROR_DURATION);
			InternalEndCapture(lock);
			return false;
		}

		s_video_codec_context = wrap_avcodec_alloc_context3(vcodec);
		if (!s_video_codec_context)
		{
			Host::AddIconOSDMessage("GSCaptureError", ICON_FA_CAMERA, "Failed to allocate video codec context.", Host::OSD_ERROR_DURATION);
			InternalEndCapture(lock);
			return false;
		}

		s_video_codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
		s_video_codec_context->bit_rate = GSConfig.VideoCaptureBitrate * 1000;
		s_video_codec_context->width = s_size.x;
		s_video_codec_context->height = s_size.y;
		s_video_codec_context->sample_aspect_ratio = wrap_av_d2q(sample_aspect_ratio, 100000);
		wrap_av_reduce(&s_video_codec_context->time_base.num, &s_video_codec_context->time_base.den, 10000,
			static_cast<s64>(static_cast<double>(fps) * 10000.0), std::numeric_limits<s32>::max());

		// Default to YUV 4:2:0 if the codec doesn't specify a pixel format.
		AVPixelFormat sw_pix_fmt = AV_PIX_FMT_YUV420P;
		if (vcodec->pix_fmts)
		{
			// Prefer YUV420 given the choice, but otherwise fall back to whatever it supports.
			sw_pix_fmt = vcodec->pix_fmts[0];
			for (u32 i = 0; vcodec->pix_fmts[i] != AV_PIX_FMT_NONE; i++)
			{
				if (vcodec->pix_fmts[i] == AV_PIX_FMT_YUV420P)
				{
					sw_pix_fmt = vcodec->pix_fmts[i];
					break;
				}
			}
		}
		s_video_codec_context->pix_fmt = sw_pix_fmt;

		// Can we use hardware encoding?
		const AVCodecHWConfig* hwconfig = wrap_avcodec_get_hw_config(vcodec, 0);
		if (hwconfig && hwconfig->pix_fmt != AV_PIX_FMT_NONE && hwconfig->pix_fmt != sw_pix_fmt)
		{
			// First index isn't our preferred pixel format, try the others, but fall back if one doesn't exist.
			int index = 1;
			while (const AVCodecHWConfig* next_hwconfig = wrap_avcodec_get_hw_config(vcodec, index++))
			{
				if (next_hwconfig->pix_fmt == sw_pix_fmt)
				{
					hwconfig = next_hwconfig;
					break;
				}
			}
		}

		if (hwconfig)
		{
			Console.WriteLn(Color_StrongGreen, fmt::format("Trying to use {} hardware device for video encoding.",
												   wrap_av_hwdevice_get_type_name(hwconfig->device_type)));
			res = wrap_av_hwdevice_ctx_create(&s_video_hw_context, hwconfig->device_type, nullptr, nullptr, 0);
			if (res < 0)
			{
				LogAVError(res, "av_hwdevice_ctx_create() failed: ");
			}
			else
			{
				s_video_hw_frames = wrap_av_hwframe_ctx_alloc(s_video_hw_context);
				if (!s_video_hw_frames)
				{
					Console.Error("s_video_hw_frames() failed");
					wrap_av_buffer_unref(&s_video_hw_context);
				}
				else
				{
					AVHWFramesContext* frames_ctx = reinterpret_cast<AVHWFramesContext*>(s_video_hw_frames->data);
					frames_ctx->format = (hwconfig->pix_fmt != AV_PIX_FMT_NONE) ? hwconfig->pix_fmt : sw_pix_fmt;
					frames_ctx->sw_format = sw_pix_fmt;
					frames_ctx->width = s_video_codec_context->width;
					frames_ctx->height = s_video_codec_context->height;
					res = wrap_av_hwframe_ctx_init(s_video_hw_frames);
					if (res < 0)
					{
						LogAVError(res, "av_hwframe_ctx_init() failed: ");
						wrap_av_buffer_unref(&s_video_hw_frames);
						wrap_av_buffer_unref(&s_video_hw_context);
					}
					else
					{
						s_video_codec_context->hw_frames_ctx = wrap_av_buffer_ref(s_video_hw_frames);
						if (hwconfig->pix_fmt != AV_PIX_FMT_NONE)
							s_video_codec_context->pix_fmt = hwconfig->pix_fmt;
					}
				}
			}

			if (!s_video_hw_context)
			{
				Host::AddIconOSDMessage("GSCaptureHWError", ICON_FA_CAMERA,
					"Failed to create hardware encoder, using software encoding.", Host::OSD_ERROR_DURATION);
				hwconfig = nullptr;
			}
		}

		if (GSConfig.EnableVideoCaptureParameters)
		{
			res = wrap_av_dict_parse_string(&s_video_codec_arguments, GSConfig.VideoCaptureParameters.c_str(), "=", ":", 0);
			if (res < 0)
			{
				LogAVError(res, "av_dict_parse_string() for video failed: ");
				InternalEndCapture(lock);
				return false;
			}
		}

		if (output_format->flags & AVFMT_GLOBALHEADER)
			s_video_codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		bool has_pixel_format_override = wrap_av_dict_get(s_video_codec_arguments, "pixel_format", nullptr, 0);

		res = wrap_avcodec_open2(s_video_codec_context, vcodec, &s_video_codec_arguments);
		if (res < 0)
		{
			LogAVError(res, "avcodec_open2() for video failed: ");
			InternalEndCapture(lock);
			return false;
		}

		// If the user overrode the pixel format, get that now
		if (has_pixel_format_override)
			sw_pix_fmt = s_video_codec_context->pix_fmt;

		s_converted_video_frame = wrap_av_frame_alloc();
		s_hw_video_frame = IsUsingHardwareVideoEncoding() ? wrap_av_frame_alloc() : nullptr;
		if (!s_converted_video_frame || (IsUsingHardwareVideoEncoding() && !s_hw_video_frame))
		{
			LogAVError(AVERROR(ENOMEM), "Failed to allocate frame: ");
			InternalEndCapture(lock);
			return false;
		}

		s_converted_video_frame->format = sw_pix_fmt;
		s_converted_video_frame->width = s_video_codec_context->width;
		s_converted_video_frame->height = s_video_codec_context->height;
		res = wrap_av_frame_get_buffer(s_converted_video_frame, 0);
		if (res < 0)
		{
			LogAVError(res, "av_frame_get_buffer() for converted frame failed: ");
			InternalEndCapture(lock);
			return false;
		}

		if (IsUsingHardwareVideoEncoding())
		{
			s_hw_video_frame->format = s_video_codec_context->pix_fmt;
			s_hw_video_frame->width = s_video_codec_context->width;
			s_hw_video_frame->height = s_video_codec_context->height;
			res = wrap_av_hwframe_get_buffer(s_video_hw_frames, s_hw_video_frame, 0);
			if (res < 0)
			{
				LogAVError(res, "av_frame_get_buffer() for HW frame failed: ");
				InternalEndCapture(lock);
				return false;
			}
		}

		s_video_stream = wrap_avformat_new_stream(s_format_context, vcodec);
		if (!s_video_stream)
		{
			LogAVError(AVERROR(ENOMEM), "avformat_new_stream() for video failed: ");
			InternalEndCapture(lock);
			return false;
		}

		res = wrap_avcodec_parameters_from_context(s_video_stream->codecpar, s_video_codec_context);
		if (res < 0)
		{
			LogAVError(AVERROR(ENOMEM), "avcodec_parameters_from_context() for video failed: ");
			InternalEndCapture(lock);
			return false;
		}

		s_video_stream->time_base = s_video_codec_context->time_base;
		s_video_stream->sample_aspect_ratio = s_video_codec_context->sample_aspect_ratio;

		s_video_packet = wrap_av_packet_alloc();
		if (!s_video_packet)
		{
			LogAVError(AVERROR(ENOMEM), "av_packet_alloc() for video failed: ");
			InternalEndCapture(lock);
			return false;
		}

		s_next_video_pts = 0;
	}

	if (capture_audio)
	{
		// The CPU thread might have dumped some frames in here from the last capture, so clear it out.
		s_audio_buffer_read_pos = 0;
		s_audio_buffer_write_pos = 0;
		s_audio_buffer_size.store(0, std::memory_order_release);
		if (!s_audio_buffer)
			s_audio_buffer = std::make_unique<s16[]>(AUDIO_BUFFER_SIZE * AUDIO_CHANNELS);

		const AVCodec* acodec = nullptr;
		if (!GSConfig.AudioCaptureCodec.empty())
		{
			acodec = wrap_avcodec_find_encoder_by_name(GSConfig.AudioCaptureCodec.c_str());
			if (!acodec)
			{
				Host::AddIconOSDMessage("GSCaptureCodecNotFound", ICON_FA_CAMERA,
					fmt::format("Audio codec {} not found, using default.", GSConfig.VideoCaptureCodec), Host::OSD_ERROR_DURATION);
			}
		}
		if (!acodec)
			acodec = wrap_avcodec_find_encoder(output_format->audio_codec);
		if (!acodec)
		{
			Host::AddIconOSDMessage("GSCaptureError", ICON_FA_CAMERA, "Failed to find audio encoder.", Host::OSD_ERROR_DURATION);
			InternalEndCapture(lock);
			return false;
		}

		s_audio_codec_context = wrap_avcodec_alloc_context3(acodec);
		if (!s_audio_codec_context)
		{
			Host::AddIconOSDMessage("GSCaptureError", ICON_FA_CAMERA, "Failed to allocate audio codec context.", Host::OSD_ERROR_DURATION);
			InternalEndCapture(lock);
			return false;
		}

		const s32 sample_rate = SPU2::GetConsoleSampleRate();
		s_audio_codec_context->codec_type = AVMEDIA_TYPE_AUDIO;
		s_audio_codec_context->bit_rate = GSConfig.AudioCaptureBitrate * 1000;
		s_audio_codec_context->channels = AUDIO_CHANNELS;
		s_audio_codec_context->sample_fmt = AV_SAMPLE_FMT_S16;
		s_audio_codec_context->sample_rate = sample_rate;
		s_audio_codec_context->time_base = {1, sample_rate};

		bool supports_format = false;
		for (const AVSampleFormat* p = acodec->sample_fmts; *p != AV_SAMPLE_FMT_NONE; p++)
		{
			if (*p == s_audio_codec_context->sample_fmt)
			{
				supports_format = true;
				break;
			}
		}
		if (!supports_format)
		{
			Console.WriteLn(fmt::format("Audio codec '{}' does not support S16 samples, using default.", acodec->name));
			s_audio_codec_context->sample_fmt = acodec->sample_fmts[0];
			s_swr_context = wrap_swr_alloc();
			if (!s_swr_context)
			{
				LogAVError(AVERROR(ENOMEM), "swr_alloc() failed: ");
				InternalEndCapture(lock);
				return false;
			}

			wrap_av_opt_set_int(s_swr_context, "in_channel_count", AUDIO_CHANNELS, 0);
			wrap_av_opt_set_int(s_swr_context, "in_sample_rate", sample_rate, 0);
			wrap_av_opt_set_sample_fmt(s_swr_context, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
			wrap_av_opt_set_int(s_swr_context, "out_channel_count", AUDIO_CHANNELS, 0);
			wrap_av_opt_set_int(s_swr_context, "out_sample_rate", sample_rate, 0);
			wrap_av_opt_set_sample_fmt(s_swr_context, "out_sample_fmt", s_audio_codec_context->sample_fmt, 0);
			res = wrap_swr_init(s_swr_context);
			if (res < 0)
			{
				LogAVError(res, "swr_init() failed: ");
				InternalEndCapture(lock);
				return false;
			}
		}

		// TODO: Check channel layout support, this is different in v4.x and v5.x.
		s_audio_codec_context->channel_layout = AV_CH_LAYOUT_STEREO;

		if (GSConfig.EnableAudioCaptureParameters)
		{
			res = wrap_av_dict_parse_string(&s_audio_codec_arguments, GSConfig.AudioCaptureParameters.c_str(), "=", ":", 0);
			if (res < 0)
			{
				LogAVError(res, "av_dict_parse_string() for audio failed: ");
				InternalEndCapture(lock);
				return false;
			}
		}

		if (output_format->flags & AVFMT_GLOBALHEADER)
			s_audio_codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		res = wrap_avcodec_open2(s_audio_codec_context, acodec, &s_audio_codec_arguments);
		if (res < 0)
		{
			LogAVError(res, "avcodec_open2() for audio failed: ");
			InternalEndCapture(lock);
			return false;
		}

		// Use packet size for frame if it supports it... but most don't.
		if (acodec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
			s_audio_frame_size = SndOutPacketSize;
		else
			s_audio_frame_size = s_audio_codec_context->frame_size;
		if (s_audio_frame_size >= AUDIO_BUFFER_SIZE)
		{
			LogAVError(AVERROR(EINVAL), "Audio frame size %u exceeds buffer size %u", s_audio_frame_size, AUDIO_BUFFER_SIZE);
			InternalEndCapture(lock);
			return false;
		}

		s_audio_frame_bps = wrap_av_get_bytes_per_sample(s_audio_codec_context->sample_fmt);
		s_audio_frame_planar = (wrap_av_sample_fmt_is_planar(s_audio_codec_context->sample_fmt) != 0);

		s_converted_audio_frame = wrap_av_frame_alloc();
		if (!s_converted_audio_frame)
		{
			LogAVError(AVERROR(ENOMEM), "Failed to allocate audio frame: ");
			InternalEndCapture(lock);
			return false;
		}

		s_converted_audio_frame->format = s_audio_codec_context->sample_fmt;
		s_converted_audio_frame->channels = AUDIO_CHANNELS;
		s_converted_audio_frame->channel_layout = s_audio_codec_context->channel_layout;
		s_converted_audio_frame->nb_samples = s_audio_frame_size;
		res = wrap_av_frame_get_buffer(s_converted_audio_frame, 0);
		if (res < 0)
		{
			LogAVError(res, "av_frame_get_buffer() for audio frame failed: ");
			InternalEndCapture(lock);
			return false;
		}

		s_audio_stream = wrap_avformat_new_stream(s_format_context, acodec);
		if (!s_audio_stream)
		{
			LogAVError(AVERROR(ENOMEM), "avformat_new_stream() for audio failed: ");
			InternalEndCapture(lock);
			return false;
		}

		res = wrap_avcodec_parameters_from_context(s_audio_stream->codecpar, s_audio_codec_context);
		if (res < 0)
		{
			LogAVError(res, "avcodec_parameters_from_context() for audio failed: ");
			InternalEndCapture(lock);
			return false;
		}

		s_audio_stream->time_base = s_audio_codec_context->time_base;

		s_audio_packet = wrap_av_packet_alloc();
		if (!s_audio_packet)
		{
			LogAVError(AVERROR(ENOMEM), "av_packet_alloc() for audio failed: ");
			InternalEndCapture(lock);
			return false;
		}

		s_next_audio_pts = 0;
	}

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

	Host::AddIconOSDMessage("GSCapture", ICON_FA_CAMERA,
		fmt::format("Starting {} to '{}'.", GetCaptureTypeForMessage(capture_video, capture_audio), Path::GetFileName(s_filename)),
		Host::OSD_INFO_DURATION);

	if (capture_audio)
		SPU2::SetAudioCaptureActive(true);

	s_capturing.store(true, std::memory_order_release);
	StartEncoderThread();

	lock.unlock();
	Host::OnCaptureStarted(s_filename);
	return true;
}

bool GSCapture::DeliverVideoFrame(GSTexture* stex)
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
		s_frame_encoded_cv.wait(lock, [&pf]() { return pf.state == PendingFrame::State::Unused; });
	}

	if (!pf.tex || pf.tex->GetWidth() != static_cast<u32>(stex->GetWidth()) || pf.tex->GetHeight() != static_cast<u32>(stex->GetHeight()))
	{
		pf.tex.reset();
		pf.tex = g_gs_device->CreateDownloadTexture(stex->GetWidth(), stex->GetHeight(), stex->GetFormat());
		if (!pf.tex)
		{
			Console.Error("GSCapture: Failed to create %x%d download texture", stex->GetWidth(), stex->GetHeight());
			return false;
		}

#ifdef PCSX2_DEVBUILD
		pf.tex->SetDebugName(TinyString::from_fmt("GSCapture {}x{} Download Texture", stex->GetWidth(), stex->GetHeight()));
#endif
	}

	const GSVector4i rc(0, 0, stex->GetWidth(), stex->GetHeight());
	pf.tex->CopyFromTexture(rc, stex, rc, 0);
	pf.pts = s_next_video_pts++;
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
	Threading::SetNameOfCurrentThread("GS Capture Encoding");

	std::unique_lock<std::mutex> lock(s_lock);

	for (;;)
	{
		s_frame_ready_cv.wait(lock, []() { return (s_frames_pending_encode > 0 || !s_capturing.load(std::memory_order_acquire)); });
		if (s_frames_pending_encode == 0 && !s_capturing.load(std::memory_order_acquire))
			break;

		PendingFrame& pf = s_pending_frames[s_frames_encode_consume_pos];
		pxAssert(pf.state == PendingFrame::State::NeedsEncoding);

		lock.unlock();

		bool okay = !s_encoding_error;

		// If the frame failed to map, this will be false, and we'll just skip it.
		if (okay && s_video_stream && pf.tex->IsMapped())
			okay = SendFrame(pf);

		// Encode as many audio frames while the video is ahead.
		if (okay && s_audio_stream)
			okay = ProcessAudioPackets(pf.pts);

		lock.lock();

		// If we had an encoding error, tell the GS thread to shut down the capture (later).
		if (!okay)
			s_encoding_error = true;

		// Done with this frame! Wait for the next.
		pf.state = PendingFrame::State::Unused;
		s_frames_encode_consume_pos = (s_frames_encode_consume_pos + 1) % MAX_PENDING_FRAMES;
		s_frames_pending_encode--;
		s_frame_encoded_cv.notify_all();
	}
}

void GSCapture::StartEncoderThread()
{
	Console.WriteLn("GSCapture: Starting encoder thread.");
	pxAssert(s_capturing.load(std::memory_order_acquire) && !s_encoder_thread.Joinable());
	s_encoder_thread.Start(EncoderThreadEntryPoint);
}

void GSCapture::StopEncoderThread(std::unique_lock<std::mutex>& lock)
{
	// Thread will exit when s_capturing is false.
	pxAssert(!s_capturing.load(std::memory_order_acquire));

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

	// In case a previous frame is still using the frame.
	wrap_av_frame_make_writable(s_converted_video_frame);

	s_sws_context = wrap_sws_getCachedContext(s_sws_context, source_width, source_height, source_format, s_converted_video_frame->width,
		s_converted_video_frame->height, static_cast<AVPixelFormat>(s_converted_video_frame->format), SWS_BICUBIC, nullptr, nullptr, nullptr);
	if (!s_sws_context)
	{
		Console.Error("sws_getCachedContext() failed");
		return false;
	}

	wrap_sws_scale(s_sws_context, reinterpret_cast<const u8**>(&source_ptr), &source_pitch, 0, source_height, s_converted_video_frame->data,
		s_converted_video_frame->linesize);

	AVFrame* frame_to_send = s_converted_video_frame;
	if (IsUsingHardwareVideoEncoding())
	{
		// Need to transfer the frame to hardware.
		const int res = wrap_av_hwframe_transfer_data(s_hw_video_frame, s_converted_video_frame, 0);
		if (res < 0)
		{
			LogAVError(res, "av_hwframe_transfer_data() failed: ");
			return false;
		}

		frame_to_send = s_hw_video_frame;
	}

	// Set the correct PTS before handing it off.
	frame_to_send->pts = pf.pts;

	const int res = wrap_avcodec_send_frame(s_video_codec_context, frame_to_send);
	if (res < 0)
	{
		LogAVError(res, "avcodec_send_frame() failed: ");
		return false;
	}

	return ReceivePackets(s_video_codec_context, s_video_stream, s_video_packet);
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

bool GSCapture::ReceivePackets(AVCodecContext* codec_context, AVStream* stream, AVPacket* packet)
{
	for (;;)
	{
		int res = wrap_avcodec_receive_packet(codec_context, packet);
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

		packet->stream_index = stream->index;

		// in case the frame rate changed...
		wrap_av_packet_rescale_ts(packet, codec_context->time_base, stream->time_base);

		res = wrap_av_interleaved_write_frame(s_format_context, packet);
		if (res < 0)
		{
			LogAVError(res, "av_interleaved_write_frame() failed: ");
			return false;
		}

		wrap_av_packet_unref(packet);
	}

	return true;
}

void GSCapture::DeliverAudioPacket(const s16* frames)
{
	// Since this gets called from the EE thread, we might race here after capture stops, and send a few too many samples
	// late. We don't really want to be grabbing the lock on the EE thread, so instead, we'll just YOLO push the frames
	// through and clear them out for the next capture. If we happen to fill the buffer, *then* we'll lock, and check if
	// the capture has stopped.

	static constexpr u32 num_frames = static_cast<u32>(SndOutPacketSize);

	if ((AUDIO_BUFFER_SIZE - s_audio_buffer_size.load(std::memory_order_acquire)) < num_frames)
	{
		// Need to wait for it to drain a bit.
		std::unique_lock<std::mutex> lock(s_lock);
		s_frame_encoded_cv.wait(lock, []() {
			return (!s_capturing.load(std::memory_order_acquire) ||
					((AUDIO_BUFFER_SIZE - s_audio_buffer_size.load(std::memory_order_acquire)) >= num_frames));
		});
		if (!s_capturing.load(std::memory_order_acquire))
			return;
	}

	// Since the buffer size is aligned to the SndOut packet size, we should always have space for at least one full packet.
	pxAssert((AUDIO_BUFFER_SIZE - s_audio_buffer_write_pos) >= num_frames);
	std::memcpy(s_audio_buffer.get() + (s_audio_buffer_write_pos * AUDIO_CHANNELS), frames, sizeof(s16) * AUDIO_CHANNELS * num_frames);
	s_audio_buffer_write_pos = (s_audio_buffer_write_pos + num_frames) % AUDIO_BUFFER_SIZE;

	const u32 buffer_size = s_audio_buffer_size.fetch_add(num_frames, std::memory_order_release) + num_frames;

	if (!s_video_stream && buffer_size >= s_audio_frame_size)
	{
		// If we're not capturing video, push "frames" when we hit the audio packet size.
		std::unique_lock<std::mutex> lock(s_lock);
		if (!s_capturing.load(std::memory_order_acquire))
			return;

		PendingFrame& pf = s_pending_frames[s_pending_frames_pos];
		pf.state = PendingFrame::State::NeedsEncoding;
		s_pending_frames_pos = (s_pending_frames_pos + 1) % MAX_PENDING_FRAMES;

		s_frames_pending_encode++;
		s_frame_ready_cv.notify_one();
	}
}

bool GSCapture::ProcessAudioPackets(s64 video_pts)
{
	u32 pending_frames = s_audio_buffer_size.load(std::memory_order_acquire);
	while (pending_frames > 0 && (!s_video_codec_context || wrap_av_compare_ts(video_pts, s_video_codec_context->time_base,
																s_next_audio_pts, s_audio_codec_context->time_base) > 0))
	{
		pxAssert(pending_frames >= static_cast<u32>(SndOutPacketSize));

		// In case the encoder is still using it.
		if (s_audio_frame_pos == 0)
			wrap_av_frame_make_writable(s_converted_audio_frame);

		// Grab as many source frames as we can.
		const u32 contig_frames = std::min(pending_frames, AUDIO_BUFFER_SIZE - s_audio_buffer_read_pos);
		const u32 this_batch = std::min(s_audio_frame_size - s_audio_frame_pos, contig_frames);

		// Do we need to convert the sample format?
		if (!s_swr_context)
		{
			// No, just copy frames out of staging buffer.
			if (s_audio_frame_planar)
			{
				// This is slow. Hopefully doesn't happen in too many configurations.
				for (u32 i = 0; i < AUDIO_CHANNELS; i++)
				{
					u8* output = s_converted_audio_frame->data[i] + s_audio_frame_pos * s_audio_frame_bps;
					const u8* input = reinterpret_cast<u8*>(&s_audio_buffer[s_audio_buffer_read_pos * AUDIO_CHANNELS + i]);
					for (u32 j = 0; j < this_batch; j++)
					{
						std::memcpy(output, input, sizeof(s16));
						input += sizeof(s16) * AUDIO_CHANNELS;
						output += s_audio_frame_bps;
					}
				}
			}
			else
			{
				// Direct copy - optimal.
				std::memcpy(s_converted_audio_frame->data[0] + s_audio_frame_pos * s_audio_frame_bps * AUDIO_CHANNELS,
					&s_audio_buffer[s_audio_buffer_read_pos * AUDIO_CHANNELS], this_batch * sizeof(s16) * AUDIO_CHANNELS);
			}
		}
		else
		{
			// Use swresample to convert.
			const u8* input = reinterpret_cast<u8*>(&s_audio_buffer[s_audio_buffer_read_pos * AUDIO_CHANNELS]);

			// Might be planar, so offset both buffers.
			u8* output[AUDIO_CHANNELS];
			if (s_audio_frame_planar)
			{
				for (u32 i = 0; i < AUDIO_CHANNELS; i++)
					output[i] = s_converted_audio_frame->data[i] + (s_audio_frame_pos * s_audio_frame_bps);
			}
			else
			{
				output[0] = s_converted_audio_frame->data[0] + (s_audio_frame_pos * s_audio_frame_bps * AUDIO_CHANNELS);
			}

			const int res = wrap_swr_convert(s_swr_context, output, this_batch, &input, this_batch);
			if (res < 0)
			{
				LogAVError(res, "swr_convert() failed: ");
				return false;
			}
		}

		s_audio_buffer_read_pos = (s_audio_buffer_read_pos + this_batch) % AUDIO_BUFFER_SIZE;
		s_audio_buffer_size.fetch_sub(this_batch);
		s_audio_frame_pos += this_batch;
		pending_frames -= this_batch;

		// Do we have a complete frame?
		if (s_audio_frame_pos == s_audio_frame_size)
		{
			s_audio_frame_pos = 0;

			if (!s_swr_context)
			{
				// PTS is simply frames.
				s_converted_audio_frame->pts = s_next_audio_pts;
			}
			else
			{
				s_converted_audio_frame->pts = wrap_swr_next_pts(s_swr_context, s_next_audio_pts);
			}

			// Increment PTS.
			s_next_audio_pts += s_audio_frame_size;

			// Send off for encoding.
			int res = wrap_avcodec_send_frame(s_audio_codec_context, s_converted_audio_frame);
			if (res < 0)
			{
				LogAVError(res, "avcodec_send_frame() for audio failed: ");
				return false;
			}

			// Write any packets back to the output file.
			if (!ReceivePackets(s_audio_codec_context, s_audio_stream, s_audio_packet))
				return false;
		}
	}

	return true;
}

void GSCapture::InternalEndCapture(std::unique_lock<std::mutex>& lock)
{
	int res;

	const bool was_capturing = s_capturing.load(std::memory_order_acquire);

	if (was_capturing)
	{
		SPU2::SetAudioCaptureActive(false);

		if (!s_encoding_error)
		{
			ProcessAllInFlightFrames(lock);
			Host::AddIconOSDMessage("GSCapture", ICON_FA_CAMERA,
				fmt::format(
					"Stopped {} to '{}'.", GetCaptureTypeForMessage(IsCapturingVideo(), IsCapturingAudio()), Path::GetFileName(s_filename)),
				Host::OSD_INFO_DURATION);
		}
		else
		{
			Host::AddIconOSDMessage("GSCapture", ICON_FA_CAMERA,
				fmt::format("Aborted {} due to encoding error in '{}'.", GetCaptureTypeForMessage(IsCapturingVideo(), IsCapturingAudio()),
					Path::GetFileName(s_filename)),
				Host::OSD_INFO_DURATION);
		}

		s_capturing.store(false, std::memory_order_release);
		StopEncoderThread(lock);

		s_pending_frames = {};
		s_pending_frames_pos = 0;
		s_frames_pending_map = 0;
		s_frames_map_consume_pos = 0;
		s_frames_pending_encode = 0;
		s_frames_encode_consume_pos = 0;

		s_audio_buffer_read_pos = 0;
		s_audio_buffer_write_pos = 0;
		s_audio_buffer_size.store(0, std::memory_order_release);
		s_audio_frame_pos = 0;

		s_filename = {};
		s_encoding_error = false;

		// end of stream
		if (s_video_stream)
		{
			res = wrap_avcodec_send_frame(s_video_codec_context, nullptr);
			if (res < 0)
				LogAVError(res, "avcodec_send_frame() for video EOS failed: ");
			else
				ReceivePackets(s_video_codec_context, s_video_stream, s_video_packet);
		}
		if (s_audio_stream)
		{
			res = wrap_avcodec_send_frame(s_audio_codec_context, nullptr);
			if (res < 0)
				LogAVError(res, "avcodec_send_frame() for audio EOS failed: ");
			else
				ReceivePackets(s_audio_codec_context, s_audio_stream, s_audio_packet);
		}

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
	if (s_converted_video_frame)
		wrap_av_frame_free(&s_converted_video_frame);
	if (s_hw_video_frame)
		wrap_av_frame_free(&s_hw_video_frame);
	if (s_video_hw_frames)
		wrap_av_buffer_unref(&s_video_hw_frames);
	if (s_video_hw_context)
		wrap_av_buffer_unref(&s_video_hw_context);
	if (s_video_codec_context)
		wrap_avcodec_free_context(&s_video_codec_context);
	s_video_stream = nullptr;

	if (s_swr_context)
		wrap_swr_free(&s_swr_context);
	if (s_audio_packet)
		wrap_av_packet_free(&s_audio_packet);
	if (s_converted_audio_frame)
		wrap_av_frame_free(&s_converted_audio_frame);
	if (s_audio_codec_context)
		wrap_avcodec_free_context(&s_audio_codec_context);
	s_audio_stream = nullptr;

	if (s_format_context)
	{
		wrap_avformat_free_context(s_format_context);
		s_format_context = nullptr;
	}
	if (s_video_codec_arguments)
		wrap_av_dict_free(&s_video_codec_arguments);
	if (s_audio_codec_arguments)
		wrap_av_dict_free(&s_audio_codec_arguments);

	if (was_capturing)
		UnloadFFmpeg();
}

void GSCapture::EndCapture()
{
	{
		std::unique_lock<std::mutex> lock(s_lock);
		InternalEndCapture(lock);
	}

	Host::OnCaptureStopped();
}

bool GSCapture::IsCapturing()
{
	return s_capturing.load(std::memory_order_acquire);
}

bool GSCapture::IsCapturingVideo()
{
	return (s_video_stream != nullptr);
}

bool GSCapture::IsCapturingAudio()
{
	return (s_audio_stream != nullptr);
}

const Threading::ThreadHandle& GSCapture::GetEncoderThreadHandle()
{
	return s_encoder_thread;
}

GSVector2i GSCapture::GetSize()
{
	return s_size;
}

std::string GSCapture::GetNextCaptureFileName()
{
	std::string ret;
	if (!IsCapturing())
		return ret;

	const std::string_view ext = Path::GetExtension(s_filename);
	std::string_view name = Path::GetFileTitle(s_filename);

	// Should end with a number.
	int partnum = 2;
	std::string_view::size_type pos = name.rfind("_part");
	if (pos >= 0)
	{
		std::string_view::size_type cpos = pos + 5;
		for (; cpos < name.length(); cpos++)
		{
			if (name[cpos] < '0' || name[cpos] > '9')
				break;
		}
		if (cpos == name.length())
		{
			// Has existing part number, so add to it.
			partnum = StringUtil::FromChars<int>(name.substr(pos + 5)).value_or(1) + 1;
			name = name.substr(0, pos);
		}
	}

	// If we haven't started a new file previously, add "_part2".
	ret = Path::BuildRelativePath(s_filename, fmt::format("{}_part{:03d}.{}", name, partnum, ext));
	return ret;
}

void GSCapture::Flush()
{
	std::unique_lock<std::mutex> lock(s_lock);

	if (s_encoding_error)
		return;

	ProcessAllInFlightFrames(lock);

	if (IsCapturingAudio())
	{
		// Clear any buffered audio frames out, we don't want to delay the CPU thread.
		const u32 audio_frames = s_audio_buffer_size.load(std::memory_order_acquire);
		if (audio_frames > 0)
			Console.Warning("Dropping %u audio frames on for buffer clear.", audio_frames);

		s_audio_buffer_read_pos = 0;
		s_audio_buffer_write_pos = 0;
		s_audio_buffer_size.store(0, std::memory_order_release);
	}
}

GSCapture::CodecList GSCapture::GetCodecListForContainer(const char* container, AVMediaType type)
{
	CodecList ret;

	if (!LoadFFmpeg(false))
		return ret;

	const AVOutputFormat* output_format =
		wrap_av_guess_format(nullptr, fmt::format("video.{}", container ? container : "mp4").c_str(), nullptr);
	if (!output_format)
	{
		Console.Error("(GetCodecListForContainer) av_guess_format() failed");
		return ret;
	}

	void* iter = nullptr;
	const AVCodec* codec;
	while ((codec = wrap_av_codec_iterate(&iter)) != nullptr)
	{
		// only get audio codecs
		if (codec->type != type || !wrap_avcodec_find_encoder(codec->id) || !wrap_avcodec_find_encoder_by_name(codec->name))
			continue;

		if (!wrap_avformat_query_codec(output_format, codec->id, FF_COMPLIANCE_NORMAL))
			continue;

		if (std::find_if(ret.begin(), ret.end(), [codec](const auto& it) { return it.first == codec->name; }) != ret.end())
			continue;

		ret.emplace_back(codec->name, codec->long_name ? codec->long_name : codec->name);
	}

	return ret;
}

GSCapture::CodecList GSCapture::GetVideoCodecList(const char* container)
{
	return GetCodecListForContainer(container, AVMEDIA_TYPE_VIDEO);
}

GSCapture::CodecList GSCapture::GetAudioCodecList(const char* container)
{
	return GetCodecListForContainer(container, AVMEDIA_TYPE_AUDIO);
}
