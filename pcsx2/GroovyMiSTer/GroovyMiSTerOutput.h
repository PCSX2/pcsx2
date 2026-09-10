// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Config.h"
#include "GroovyMiSTer/GroovyMiSTerAudioTap.h"
#include "GroovyMiSTer/GroovyMiSTerKeepAlive.h"
#include "GroovyMiSTer/GroovyMiSTerModeline.h"
#include "GroovyMiSTer/GroovyMiSTerPixels.h"

#include "GS/GSVector.h"
#include "common/Pcsx2Defs.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class GSTexture;
class GSDownloadTexture;

namespace GroovyMiSTer
{
	/// What the settings UI / OSD shows about the live stream.
	struct Status
	{
		bool connected = false;
		int native_width = 0; // the PS2's real display resolution (PCRTC output)
		int native_height = 0;
		float refresh_hz = 0.0f;
		bool interlaced = false;
		Modeline modeline{};
		std::string monitor_preset;
		// Latency from the FPGA: how far the raster had travelled when it received our
		// frame, against where it is scanning out now.
		float latency_ms = 0.0f;
		u32 frames_sent = 0;
		u32 frames_dropped = 0;
		u32 last_encoded_bytes = 0;
		float encoded_mbps = 0.0f;
	};

	/// Owns the streaming pipeline. One instance, created by Open().
	///
	/// Threading:
	///
	///   GS thread     Capture(): scale, read back and pack pixels, push to m_queue. Never
	///                 touches the socket and never encodes - NLC costs ~7ms and would
	///                 throttle emulation through the MTGS ring.
	///
	///   Sender thread Sole owner of the Groovy video/audio socket (UDP 32100). Applies
	///                 pending switchres, drains the audio tap, encodes, blits, paces.
	///                 Close belongs here too: the Windows RIO send path defers sends, so
	///                 a CMD_CLOSE issued from another thread is dropped and the MiSTer
	///                 holds our last frame. It also owns the idle keepalive, because
	///                 every way PCSX2 goes quiet - pause, savestate load, disc swap, a
	///                 refused modeline - stops the GS thread, not this one, and nothing
	///                 can stall the sender (under MisterMaster the GS thread blocks on
	///                 it, never the reverse).
	///
	///   SPU2 thread   AudioTap::Write() only.
	///
	///   CPU thread    GroovyMiSTerInputSource polls the input socket (UDP 32101), which
	///                 is a different socket and never races the sender.
	class Output
	{
	public:
		Output();
		~Output();

		bool Open();
		void Close();

		bool IsActive() const { return m_active.load(std::memory_order_relaxed); }
		bool IsAudioActive() const { return m_audio_tap.IsActive(); }

		void Capture(GSTexture* current, const GSVector4i& src_rect, u32 field);
		void WriteAudio(const float* samples, u32 frames) { m_audio_tap.Write(samples, frames); }

		/// The console rate is baked into CMD_INIT, so a change means a reconnect.
		void RequestReconnect() { m_reconnect_requested.store(true, std::memory_order_relaxed); }

		Status GetStatus() const;

	private:
		// --- connection ---------------------------------------------------------------
		bool TryConnect();
		void DoGroovyClose();

		// --- switchres ----------------------------------------------------------------
		bool InitSwitchres();
		void ShutdownSwitchres();
		/// Recompute the modeline if the PS2's video mode changed. False when there is no
		/// usable mode, in which case nothing is streamed.
		bool EnsureMode(int src_w, int src_h, float refresh_hz, bool interlaced);

		// --- capture ------------------------------------------------------------------
		bool EnsureGpuResources();
		void ReleaseGpuResources();
		void PushFrame(std::vector<u8>&& pixels, u8 field);

		// --- sender -------------------------------------------------------------------
		void SenderLoop();

		struct OutFrame
		{
			std::vector<u8> pixels;
			u8 field = 0;
		};

		struct Readback
		{
			std::unique_ptr<GSDownloadTexture> tex;
			bool pending = false;
			u8 field = 0;
		};

		std::atomic_bool m_active{false};
		std::atomic_bool m_quit{false};
		std::atomic_bool m_reconnect_requested{false};

		Pcsx2Config::GroovyMiSTerOptions m_cfg;

		// Negotiated at CMD_INIT and fixed for the session.
		u8 m_codec = 0;
		u8 m_rgb_mode = 0;
		u32 m_bpp = 3;
		u32 m_sound_rate = 0;
		u8 m_sound_chan = 0;

		// --- switchres / modeline -----------------------------------------------------
		bool m_sr_inited = false;
		Modeline m_modeline{};
		bool m_have_mode = false;
		// Last source geometry we ran through switchres, so we only recompute on change.
		int m_src_w = 0, m_src_h = 0;
		float m_src_hz = 0.0f;
		bool m_src_interlaced = false;
		bool m_mode_refused = false; // safety cap said no; stay quiet until the mode changes

		mutable std::mutex m_sr_lock;
		Modeline m_pending_modeline{};
		bool m_switchres_pending = false;

		// --- GPU capture --------------------------------------------------------------
		GSTexture* m_scratch_rt = nullptr;
		Readback m_readback[2];
		u32 m_readback_idx = 0;
		int m_dst_w = 0, m_dst_h = 0;
		bool m_logged_first_frame = false;
		// One-shot: the sender's oversized-frame guard must not log per frame.
		bool m_logged_oversized_frame = false;

		// --- GS -> sender queue --------------------------------------------------------
		// Deliberately tiny. Pcsx2Master drops the older frame rather than build a backlog;
		// MisterMaster blocks the push, which is how the raster throttles the EE.
		static constexpr size_t MAX_QUEUED_FRAMES = 1;
		std::mutex m_queue_lock;
		std::condition_variable m_queue_cv;
		std::condition_variable m_space_cv;
		std::deque<OutFrame> m_queue;

		std::thread m_sender;

		// --- status -------------------------------------------------------------------
		mutable std::mutex m_status_lock;
		Status m_status;

		// Sender-thread-only, no locking.
		u32 m_blit_frame = 0;
		// Tracks the client's internal auto-reconnect so we can realign m_blit_frame: the
		// core restarts its own frame counter on a fresh session, and ours must follow.
		u32 m_reconnect_epoch = 0;
		KeepAliveScheduler m_keepalive;

		AudioTap m_audio_tap;
	};
} // namespace GroovyMiSTer
