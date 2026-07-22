// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Host/AudioStream.h"
#include "VMManager.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/Error.h"

#include "oboe/Oboe.h"

#include <atomic>
#include <chrono>
#include <thread>
#if defined(__ANDROID__)
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace {
	class OboeAudioStream final : public AudioStream,
	                               oboe::AudioStreamDataCallback,
	                               oboe::AudioStreamErrorCallback
	{
	public:
		OboeAudioStream(u32 sample_rate, const AudioStreamParameters& parameters);
		~OboeAudioStream() override;

		void SetPaused(bool paused) override;

		bool Initialize(bool stretch_enabled);
		bool Open();
		bool Start();
		void Stop();
		void Close();

		oboe::DataCallbackResult onAudioReady(oboe::AudioStream* p_audioStream,
			void* p_audioData, int32_t p_numFrames) override;
		bool onError(oboe::AudioStream* oboeStream, oboe::Result error) override;

	private:
		bool m_playing = false;
		bool m_stop_requested = false;

		std::shared_ptr<oboe::AudioStream> m_stream;

		// Performance mode the stream is (re)opened with. Starts at LowLatency;
		// Initialize() downgrades it to None if the device refuses the fast path
		// (some Adreno/AAudio devices fail requestStart() with ErrorDisconnected
		// at boot). onError()'s reopen then reuses whatever mode actually worked.
		oboe::PerformanceMode m_perf_mode = oboe::PerformanceMode::LowLatency;

		// Affinity pin latch. Oboe spawns its own audio data thread; we don't
		// see its TID until the callback fires the first time. After the
		// first callback we apply the perf-cluster affinity once. Audio
		// callbacks compete with EE for cache lines + share the same big
		// cluster — without pinning, the audio thread can land on a little
		// core (jitter) or migrate onto EE's core (L2 pollution).
		std::atomic<bool> m_audio_thread_pinned{false};
	};
} // namespace

oboe::DataCallbackResult OboeAudioStream::onAudioReady(oboe::AudioStream* p_audioStream,
	void* p_audioData, int32_t p_numFrames)
{
#if defined(__ANDROID__)
	// Affinity pin. Oboe owns the audio data thread; we only see its TID
	// inside this callback. Pin onto the same perf-cluster as EE/VU/GS so
	// the audio thread doesn't (a) get scheduled to a little core and
	// inject jitter into the callback's deadline, or (b) land on EE's
	// core and pollute L2.
	//
	// VMManager's SetEmuThreadAffinities runs when the VM transitions to
	// Running, which is typically AFTER Oboe has opened its stream and
	// fired the first callback. So the first few callbacks see
	// perf_mask=0 (pinning not yet active) and skip. Latch only after a
	// SUCCESSFUL pin so we keep polling cheaply (one atomic-acquire +
	// `s_thread_affinities_set` bool check inside
	// GetPerformanceClusterAffinityMask) until pinning actually turns on.
	if (!m_audio_thread_pinned.load(std::memory_order_acquire))
	{
		const u64 perf_mask = VMManager::Internal::GetPerformanceClusterAffinityMask();
		if (perf_mask != 0)
		{
			const pid_t tid = static_cast<pid_t>(syscall(SYS_gettid));
			cpu_set_t set;
			CPU_ZERO(&set);
			for (u32 i = 0; i < 64; i++)
			{
				if (perf_mask & (static_cast<u64>(1) << i))
					CPU_SET(i, &set);
			}
			if (sched_setaffinity(tid, sizeof(set), &set) == 0)
			{
				INFO_LOG("(Oboe) audio thread tid={} pinned to perf-cluster mask 0x{:x}", tid, perf_mask);
				m_audio_thread_pinned.store(true, std::memory_order_release);
			}
			else
			{
				WARNING_LOG("(Oboe) sched_setaffinity tid={} failed (errno {}) — will retry next callback", tid, errno);
			}
		}
		// else: pinning not active yet (VM hasn't reached Running). Skip
		// the syscall + don't latch — next callback retries.
	}
#endif

	if (p_audioData != nullptr)
		ReadFrames(reinterpret_cast<SampleType*>(p_audioData), p_numFrames);
	return oboe::DataCallbackResult::Continue;
}

bool OboeAudioStream::onError(oboe::AudioStream* oboeStream, oboe::Result error)
{
	Console.Error("(Oboe) ErrorCB %d", error);
	if (error == oboe::Result::ErrorDisconnected && !m_stop_requested)
	{
		Console.Error("(Oboe) Stream disconnected, reopening...");
		Stop();
		Close();
		if (!Open() || !Start())
			Console.Error("(Oboe) Failed to reopen stream after disconnection.");
		return true;
	}
	return false;
}

bool OboeAudioStream::Initialize(bool stretch_enabled)
{
	static constexpr const std::array<SampleReader, static_cast<size_t>(AudioExpansionMode::Count)> sample_readers = {{
		&StereoSampleReaderImpl,
		&SampleReaderImpl<AudioExpansionMode::StereoLFE,
			READ_CHANNEL_FRONT_LEFT, READ_CHANNEL_FRONT_RIGHT, READ_CHANNEL_LFE>,
		&SampleReaderImpl<AudioExpansionMode::Quadraphonic,
			READ_CHANNEL_FRONT_LEFT, READ_CHANNEL_FRONT_RIGHT,
			READ_CHANNEL_REAR_LEFT, READ_CHANNEL_REAR_RIGHT>,
		&SampleReaderImpl<AudioExpansionMode::QuadraphonicLFE,
			READ_CHANNEL_FRONT_LEFT, READ_CHANNEL_FRONT_RIGHT, READ_CHANNEL_LFE,
			READ_CHANNEL_REAR_LEFT, READ_CHANNEL_REAR_RIGHT>,
		&SampleReaderImpl<AudioExpansionMode::Surround51,
			READ_CHANNEL_FRONT_LEFT, READ_CHANNEL_FRONT_RIGHT, READ_CHANNEL_FRONT_CENTER,
			READ_CHANNEL_LFE, READ_CHANNEL_REAR_LEFT, READ_CHANNEL_REAR_RIGHT>,
		&SampleReaderImpl<AudioExpansionMode::Surround71,
			READ_CHANNEL_FRONT_LEFT, READ_CHANNEL_FRONT_RIGHT, READ_CHANNEL_FRONT_CENTER,
			READ_CHANNEL_LFE, READ_CHANNEL_SIDE_LEFT, READ_CHANNEL_SIDE_RIGHT,
			READ_CHANNEL_REAR_LEFT, READ_CHANNEL_REAR_RIGHT>,
	}};
	BaseInitialize(sample_readers[static_cast<size_t>(m_parameters.expansion_mode)], stretch_enabled);

	// Resilient open: some devices (seen on Adreno/AAudio) refuse a low-latency /
	// fast-path output stream at boot and fail requestStart() with
	// ErrorDisconnected — the audio device was reclaimed the instant we tried to
	// start it. Rather than fall straight to permanent silent null output, retry,
	// and if the fast path keeps failing drop to the most compatible
	// PerformanceMode::None (shared slow-path) stream before giving up.
	static constexpr oboe::PerformanceMode kModes[] = {
		oboe::PerformanceMode::LowLatency,
		oboe::PerformanceMode::None,
	};
	for (const oboe::PerformanceMode mode : kModes)
	{
		m_perf_mode = mode;
		for (int attempt = 0; attempt < 2; attempt++)
		{
			if (Open() && Start())
			{
				if (mode != oboe::PerformanceMode::LowLatency || attempt != 0)
					Console.WriteLn("(Oboe) Audio stream opened with performance mode %d (attempt %d).",
						static_cast<int>(mode), attempt);
				return true;
			}
			// Open() failed, or Open() succeeded but Start() failed: tear the
			// half-open stream down before the next attempt / mode, then pause
			// briefly to let a transient device-reclaim settle.
			Close();
			std::this_thread::sleep_for(std::chrono::milliseconds(60));
		}
		Console.Warning("(Oboe) performance mode %d failed; trying a more compatible mode...",
			static_cast<int>(mode));
	}
	Console.Error("(Oboe) All open/start attempts failed; audio will be silent.");
	return false;
}

bool OboeAudioStream::Open()
{
	// Each Open() spawns a fresh Oboe audio thread with a new TID, so the
	// per-stream pin latch needs to clear here. Without this, an error-
	// recovery re-Open() (onError → Stop/Close/Open) keeps the latch set
	// from the previous instance and the new audio thread runs un-pinned.
	m_audio_thread_pinned.store(false, std::memory_order_release);

	oboe::AudioStreamBuilder builder;
	builder.setDirection(oboe::Direction::Output);
	builder.setPerformanceMode(m_perf_mode);
	// Opt-in legacy OpenSL ES output. AAudio's low-latency fast path is the one
	// Android silently reclaims when the stream sits idle (e.g. the in-game pause
	// menu), which then forces a full Close/Open stream rebuild on resume — the
	// ~1s hitch users see toggling fast-forward through the menu, and the cause of
	// audio dying a few seconds into a pause (#333). OpenSL ES is a higher-latency
	// buffer-queue path Android does NOT aggressively reclaim, so pause→resume
	// stays a cheap requestPause/requestStart with no rebuild. Off by default; the
	// trade is a little more output latency.
	if (m_parameters.android_use_opensles)
		builder.setAudioApi(oboe::AudioApi::OpenSLES);
	builder.setSharingMode(oboe::SharingMode::Shared);
	builder.setFormat(oboe::AudioFormat::Float);
	builder.setSampleRate(m_sample_rate);
	builder.setChannelCount(m_output_channels == 2 ? oboe::ChannelCount::Stereo : oboe::ChannelCount::Mono);
	builder.setDeviceId(oboe::kUnspecified);
	builder.setBufferCapacityInFrames(2048 * 2);
	builder.setFramesPerDataCallback(2048);
	builder.setDataCallback(this);
	builder.setErrorCallback(this);

	Console.WriteLn("(Oboe) Opening stream...");
	oboe::Result result = builder.openStream(m_stream);
	if (result != oboe::Result::OK)
	{
		Console.Error("(Oboe) openStream() failed: %d", result);
		return false;
	}
	return true;
}

bool OboeAudioStream::Start()
{
	if (m_playing)
		return true;

	Console.WriteLn("(Oboe) Starting stream...");
	m_stop_requested = false;

	oboe::Result result = m_stream->requestStart();
	if (result != oboe::Result::OK)
	{
		Console.Error("(Oboe) requestStart() failed: %d", result);
		return false;
	}
	m_playing = true;
	return true;
}

void OboeAudioStream::Stop()
{
	if (!m_playing)
		return;

	Console.WriteLn("(Oboe) Stopping stream...");
	m_stop_requested = true;

	oboe::Result result = m_stream->requestStop();
	if (result != oboe::Result::OK)
		Console.Error("(Oboe) requestStop() failed: %d", result);

	m_playing = false;
}

void OboeAudioStream::Close()
{
	Console.WriteLn("(Oboe) Closing stream...");
	if (m_playing)
		Stop();
	if (m_stream)
	{
		m_stream->close();
		m_stream.reset();
	}
}

void OboeAudioStream::SetPaused(bool paused)
{
	if (m_paused == paused)
		return;

	if (paused)
	{
		if (m_stream)
		{
			oboe::Result result = m_stream->requestPause();
			if (result != oboe::Result::OK)
				Console.Error("(Oboe) requestPause() failed: %d", result);
		}
		// Mark not-playing even if requestPause() failed, so the paused/
		// playing bookkeeping can't desync and strand a later resume.
		m_playing = false;
	}
	else
	{
		// Resume must be authoritative. If m_playing desynced to true (e.g.
		// an error-recovery reopen ran while we thought the stream was
		// paused), Start()'s `if (m_playing) return true;` guard would
		// swallow the restart and leave audio dead. Clear it first so the
		// resume always actually re-issues requestStart().
		m_playing = false;
		if (!Start())
		{
			// requestStart() failing here means the OS took the device away while we
			// were parked: Android reclaims an idle low-latency stream after a few
			// seconds, so just sitting in the in-game menu (issue #333) — or
			// backgrounding, or a call/BT switch — left audio dead for the rest of
			// the session. A paused stream never runs its data callback, so onError()
			// CANNOT fire for this; the resume is the only place that can notice.
			// Rebuild the stream exactly like the disconnect path does. Open() keeps
			// the negotiated performance-mode latch, so we don't re-lose the fast path.
			Console.Error("(Oboe) requestStart() on resume failed; reopening stream...");
			Close();
			if (!Open() || !Start())
				Console.Error("(Oboe) Failed to reopen the stream on resume.");
		}
	}
	m_paused = paused;
}

OboeAudioStream::OboeAudioStream(u32 sample_rate, const AudioStreamParameters& parameters)
	: AudioStream(sample_rate, parameters)
{
}

OboeAudioStream::~OboeAudioStream()
{
	Close();
}

std::unique_ptr<AudioStream> AudioStream::CreateOboeAudioStream(u32 sample_rate,
	const AudioStreamParameters& parameters, bool stretch_enabled, Error* error)
{
	std::unique_ptr<OboeAudioStream> stream = std::make_unique<OboeAudioStream>(sample_rate, parameters);
	if (!stream->Initialize(stretch_enabled))
		stream.reset();
	return stream;
}
