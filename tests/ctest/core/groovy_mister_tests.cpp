// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Deliberately does not include GroovyMiSTerOutput.h, which pulls in the GS device, sockets
// and threads. What is tested here is pure logic living in its own translation units.
#include "GroovyMiSTer/GroovyMiSTerAudioTap.h"
#include "GroovyMiSTer/GroovyMiSTerKeepAlive.h"
#include "GroovyMiSTer/GroovyMiSTerModeline.h"
#include "GroovyMiSTer/GroovyMiSTerPixels.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

// =====================================================================================
//  Pixel packing
// =====================================================================================
//
// Guard against a red/blue swap, which produces a subtly wrong picture that still looks
// like a working stream.
//
// PCSX2 reads back RGBA8 (byte 0 = R). The MiSTer's wire layout is not what its format
// names suggest: the FPGA unpacks a pixel as `{r,g,b} <= word64[0 +: 24]`, a Verilog
// concat, so blue ends up in the least-significant byte. Every mode needs a channel swap.

namespace
{
	// One pixel of PCSX2 readback: R=0x12 G=0x34 B=0x56 A=0x78.
	constexpr u8 kR = 0x12, kG = 0x34, kB = 0x56, kA = 0x78;

	std::vector<u8> MakeRgbaSource(u32 w, u32 h, u32 pitch)
	{
		std::vector<u8> src(static_cast<size_t>(pitch) * h, 0);
		for (u32 y = 0; y < h; y++)
		{
			u8* row = src.data() + static_cast<size_t>(y) * pitch;
			for (u32 x = 0; x < w; x++)
			{
				row[x * 4 + 0] = kR;
				row[x * 4 + 1] = kG;
				row[x * 4 + 2] = kB;
				row[x * 4 + 3] = kA;
			}
		}
		return src;
	}
} // namespace

TEST(GroovyMiSTerPack, Rgb888IsBgrOnTheWire)
{
	const u32 w = 4, h = 2, pitch = w * 4;
	const std::vector<u8> src = MakeRgbaSource(w, h, pitch);

	std::vector<u8> dst;
	GroovyMiSTer::PackFrame(GroovyMiSTerRgbMode::RGB888, src.data(), pitch, w, h, dst);

	ASSERT_EQ(dst.size(), static_cast<size_t>(w) * h * 3);
	for (u32 i = 0; i < w * h; i++)
	{
		EXPECT_EQ(dst[i * 3 + 0], kB) << "byte 0 must be blue, pixel " << i;
		EXPECT_EQ(dst[i * 3 + 1], kG) << "byte 1 must be green, pixel " << i;
		EXPECT_EQ(dst[i * 3 + 2], kR) << "byte 2 must be red, pixel " << i;
	}
}

TEST(GroovyMiSTerPack, Rgba8888IsBgraOnTheWire)
{
	const u32 w = 3, h = 3, pitch = w * 4;
	const std::vector<u8> src = MakeRgbaSource(w, h, pitch);

	std::vector<u8> dst;
	GroovyMiSTer::PackFrame(GroovyMiSTerRgbMode::RGBA8888, src.data(), pitch, w, h, dst);

	ASSERT_EQ(dst.size(), static_cast<size_t>(w) * h * 4);
	for (u32 i = 0; i < w * h; i++)
	{
		EXPECT_EQ(dst[i * 4 + 0], kB);
		EXPECT_EQ(dst[i * 4 + 1], kG);
		EXPECT_EQ(dst[i * 4 + 2], kR);
		EXPECT_EQ(dst[i * 4 + 3], kA);
	}
}

TEST(GroovyMiSTerPack, Rgb565IsLittleEndianRedHigh)
{
	const u32 w = 2, h = 1, pitch = w * 4;
	const std::vector<u8> src = MakeRgbaSource(w, h, pitch);

	std::vector<u8> dst;
	GroovyMiSTer::PackFrame(GroovyMiSTerRgbMode::RGB565, src.data(), pitch, w, h, dst);

	ASSERT_EQ(dst.size(), static_cast<size_t>(w) * h * 2);

	// (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3), stored little-endian.
	const u16 expected = static_cast<u16>(((kR >> 3) << 11) | ((kG >> 2) << 5) | (kB >> 3));
	for (u32 i = 0; i < w * h; i++)
	{
		const u16 got = static_cast<u16>(dst[i * 2] | (dst[i * 2 + 1] << 8));
		EXPECT_EQ(got, expected);
	}
}

TEST(GroovyMiSTerPack, HonoursSourcePitchPadding)
{
	// Download textures are commonly padded, so the packer must not assume pitch == w * 4.
	const u32 w = 2, h = 2;
	const u32 pitch = w * 4 + 16; // deliberate padding
	std::vector<u8> src(static_cast<size_t>(pitch) * h, 0xEE); // garbage in the padding

	for (u32 y = 0; y < h; y++)
	{
		u8* row = src.data() + static_cast<size_t>(y) * pitch;
		for (u32 x = 0; x < w; x++)
		{
			row[x * 4 + 0] = kR;
			row[x * 4 + 1] = kG;
			row[x * 4 + 2] = kB;
			row[x * 4 + 3] = kA;
		}
	}

	std::vector<u8> dst;
	GroovyMiSTer::PackFrame(GroovyMiSTerRgbMode::RGB888, src.data(), pitch, w, h, dst);

	ASSERT_EQ(dst.size(), static_cast<size_t>(w) * h * 3);
	for (u32 i = 0; i < w * h; i++)
	{
		// If the padding leaked in we would see 0xEE here.
		EXPECT_EQ(dst[i * 3 + 0], kB);
		EXPECT_EQ(dst[i * 3 + 1], kG);
		EXPECT_EQ(dst[i * 3 + 2], kR);
	}
}

// =====================================================================================
//  CRT safety cap
// =====================================================================================
//
// A modeline far outside a CRT's designed envelope can physically damage an arcade
// monitor. The cap is on by default; these tests hold the envelope where it is.

namespace
{
	// Bytes per pixel of each wire format, so the expectations below read as the setting
	// rather than a magic number.
	constexpr u32 BPP_565 = 2, BPP_888 = 3, BPP_8888 = 4;

	GroovyMiSTer::Modeline MakeModeline(u16 h_active, u16 v_active)
	{
		GroovyMiSTer::Modeline m{};
		m.pclock = 13.5;
		m.h_active = h_active;
		m.h_begin = static_cast<u16>(h_active + 16);
		m.h_end = static_cast<u16>(h_active + 48);
		m.h_total = static_cast<u16>(h_active + 100);
		m.v_active = v_active;
		m.v_begin = static_cast<u16>(v_active + 3);
		m.v_end = static_cast<u16>(v_active + 6);
		m.v_total = static_cast<u16>(v_active + 20);
		m.interlace = 0;
		return m;
	}
} // namespace

TEST(GroovyMiSTerModeline, AcceptsTypicalPs2Modes)
{
	// The modes PS2 games actually use on a 15kHz CRT.
	for (const auto& [w, h] : {std::pair<u16, u16>{640, 448}, {640, 224}, {512, 448}, {640, 480}, {720, 576}})
	{
		const GroovyMiSTer::Modeline m = MakeModeline(w, h);
		EXPECT_TRUE(GroovyMiSTer::IsModelineAcceptable(m, BPP_888, /*enforce_cap=*/true))
			<< w << "x" << h << " should be accepted";
	}
}

TEST(GroovyMiSTerModeline, CapRejectsModesBeyondCrtEnvelope)
{
	// 720p / 1080i class modes must be refused while the cap is on.
	EXPECT_FALSE(GroovyMiSTer::IsModelineAcceptable(MakeModeline(1280, 720), BPP_888, true));
	EXPECT_FALSE(GroovyMiSTer::IsModelineAcceptable(MakeModeline(1920, 1080), BPP_888, true));

	// v_active is the dangerous axis; one line over the limit is still over the limit.
	EXPECT_TRUE(GroovyMiSTer::IsModelineAcceptable(MakeModeline(640, 576), BPP_888, true));
	EXPECT_FALSE(GroovyMiSTer::IsModelineAcceptable(MakeModeline(640, 577), BPP_888, true));
}

TEST(GroovyMiSTerModeline, DisablingCapAllowsLargeModes)
{
	// With the cap off, modes outside the CRT envelope are permitted as long as they still
	// fit the client's blit buffer. 1280x480 at RGB565 is 1,228,800 bytes: well past the
	// 1024-wide cap, just inside the budget.
	EXPECT_FALSE(GroovyMiSTer::IsModelineAcceptable(MakeModeline(1280, 480), BPP_565, /*enforce_cap=*/true));
	EXPECT_TRUE(GroovyMiSTer::IsModelineAcceptable(MakeModeline(1280, 480), BPP_565, /*enforce_cap=*/false));
}

TEST(GroovyMiSTerModeline, MalformedIsAlwaysRejectedEvenWithCapOff)
{
	// Well-formedness is not the user's choice: a malformed modeline drives the FPGA's PLL
	// into an undefined state whatever the display tolerates.
	GroovyMiSTer::Modeline zero{};
	EXPECT_FALSE(GroovyMiSTer::IsModelineAcceptable(zero, BPP_888, false));

	GroovyMiSTer::Modeline bad = MakeModeline(640, 448);
	bad.h_total = bad.h_active; // blanking does not enclose the active area
	EXPECT_FALSE(GroovyMiSTer::IsModelineAcceptable(bad, BPP_888, false));

	GroovyMiSTer::Modeline no_clock = MakeModeline(640, 448);
	no_clock.pclock = 0.0;
	EXPECT_FALSE(GroovyMiSTer::IsModelineAcceptable(no_clock, BPP_888, false));
}

// =====================================================================================
//  Blit-buffer byte budget
// =====================================================================================
//
// The vendored client allocates its blit buffers once at BUFFER_SIZE (720x576x3) and takes
// the stream length from the modeline it is given, without clamping. Overrunning it is a
// heap overflow into RIO-registered memory rather than a graphical glitch.

TEST(GroovyMiSTerModeline, ByteBudgetTracksTheRgbMode)
{
	// 720x576 is a real PAL mode and the size the client's buffer was cut for: it fits in
	// RGB888 with ~1KB to spare and does not fit in RGBA8888.
	const GroovyMiSTer::Modeline pal = MakeModeline(720, 576);

	EXPECT_EQ(GroovyMiSTer::BlitBytes(pal, BPP_888), 1244160u);
	EXPECT_EQ(GroovyMiSTer::BlitBytes(pal, BPP_8888), 1658880u);

	EXPECT_TRUE(GroovyMiSTer::IsModelineAcceptable(pal, BPP_565, true));
	EXPECT_TRUE(GroovyMiSTer::IsModelineAcceptable(pal, BPP_888, true));
	EXPECT_FALSE(GroovyMiSTer::IsModelineAcceptable(pal, BPP_8888, true))
		<< "RGBA8888 at 720x576 overruns the client's blit buffer";

	// 640x576 is under the CRT cap on both axes and still does not fit in RGBA8888, so the
	// check has to be on the byte product rather than the resolution.
	EXPECT_FALSE(GroovyMiSTer::IsModelineAcceptable(MakeModeline(640, 576), BPP_8888, true));
}

TEST(GroovyMiSTerModeline, ByteBudgetIsNotTheUsersChoice)
{
	// Turning the CRT cap off must not turn this off with it: the cap is what a display
	// tolerates, the budget is what the client's allocation holds.
	EXPECT_FALSE(GroovyMiSTer::IsModelineAcceptable(MakeModeline(720, 576), BPP_8888, /*enforce_cap=*/false));

	// The cap-off path is also the one that can produce genuinely huge modes.
	EXPECT_FALSE(GroovyMiSTer::IsModelineAcceptable(MakeModeline(1920, 1080), BPP_888, /*enforce_cap=*/false));
}

TEST(GroovyMiSTerModeline, FieldBlitsCostHalf)
{
	// A true-field stream sends one half-height field per blit, so the same modeline that is
	// too large as a progressive framebuffer fits when sent as fields.
	GroovyMiSTer::Modeline fields = MakeModeline(720, 576);
	fields.interlace = static_cast<u8>(GroovyMiSTerInterlace::Field);

	EXPECT_EQ(GroovyMiSTer::BlitBytes(fields, BPP_8888), 829440u);
	EXPECT_TRUE(GroovyMiSTer::IsModelineAcceptable(fields, BPP_8888, true));

	// ProgressiveFB sends every line every blit, so it gets no discount.
	GroovyMiSTer::Modeline progressive_fb = MakeModeline(720, 576);
	progressive_fb.interlace = static_cast<u8>(GroovyMiSTerInterlace::ProgressiveFB);
	EXPECT_EQ(GroovyMiSTer::BlitBytes(progressive_fb, BPP_8888), 1658880u);
	EXPECT_FALSE(GroovyMiSTer::IsModelineAcceptable(progressive_fb, BPP_8888, true));
}

// =====================================================================================
//  Audio tap ring
// =====================================================================================

TEST(GroovyMiSTerAudioTap, InactiveTapDiscards)
{
	GroovyMiSTer::AudioTap tap;
	tap.Reset();

	const float samples[4] = {0.5f, -0.5f, 0.25f, -0.25f};
	tap.Write(samples, 2); // not active yet

	u8 out[64];
	EXPECT_EQ(tap.Read(out, sizeof(out)), 0u);
}

TEST(GroovyMiSTerAudioTap, RoundTripsStereoSamples)
{
	GroovyMiSTer::AudioTap tap;
	tap.Reset();
	tap.SetActive(true);

	const float samples[4] = {1.0f, -1.0f, 0.0f, 0.5f};
	tap.Write(samples, 2);

	u8 out[16] = {};
	const u32 n = tap.Read(out, sizeof(out));
	ASSERT_EQ(n, 2u * GroovyMiSTer::AudioTap::BYTES_PER_SAMPLE);

	s16 got[4];
	std::memcpy(got, out, sizeof(got));
	EXPECT_EQ(got[0], 32767);
	EXPECT_EQ(got[1], -32767);
	EXPECT_EQ(got[2], 0);
	EXPECT_NEAR(got[3], 16383, 2);
}

TEST(GroovyMiSTerAudioTap, ClampsOutOfRangeInput)
{
	// SPU2's DC filter and volume scaling can push samples slightly past +/-1. Without a
	// clamp these would wrap and click audibly.
	GroovyMiSTer::AudioTap tap;
	tap.Reset();
	tap.SetActive(true);

	const float samples[2] = {2.5f, -3.0f};
	tap.Write(samples, 1);

	u8 out[8] = {};
	ASSERT_EQ(tap.Read(out, sizeof(out)), GroovyMiSTer::AudioTap::BYTES_PER_SAMPLE);

	s16 got[2];
	std::memcpy(got, out, sizeof(got));
	EXPECT_EQ(got[0], 32767);
	EXPECT_EQ(got[1], -32767);
}

TEST(GroovyMiSTerAudioTap, ReadReturnsWholeFramesOnly)
{
	// A half-frame read would desync the L/R interleave for everything after it.
	GroovyMiSTer::AudioTap tap;
	tap.Reset();
	tap.SetActive(true);

	const float samples[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
	tap.Write(samples, 4);

	u8 out[16] = {};
	// Ask for 6 bytes: not a multiple of the 4-byte stereo frame.
	const u32 n = tap.Read(out, 6);
	EXPECT_EQ(n % GroovyMiSTer::AudioTap::BYTES_PER_SAMPLE, 0u);
	EXPECT_EQ(n, 4u);
}

TEST(GroovyMiSTerAudioTap, DropsOldestOnOverflowAndKeepsWorking)
{
	// Overflow must not corrupt the ring or wedge the stream - it should shed the oldest
	// audio and carry on, so latency self-corrects after a sender stall.
	GroovyMiSTer::AudioTap tap;
	tap.Reset();
	tap.SetActive(true);

	const u32 frames_capacity = GroovyMiSTer::AudioTap::CAPACITY / GroovyMiSTer::AudioTap::BYTES_PER_SAMPLE;

	std::vector<float> chunk(256 * 2, 0.25f);
	for (u32 written = 0; written < frames_capacity * 2; written += 256)
		tap.Write(chunk.data(), 256);

	EXPECT_GT(tap.GetDroppedBytes(), 0u) << "overflow should have been recorded";

	// The ring is still coherent and still serves whole frames.
	std::vector<u8> out(GroovyMiSTer::AudioTap::CAPACITY);
	const u32 n = tap.Read(out.data(), static_cast<u32>(out.size()));
    EXPECT_GT(n, 0u);
	EXPECT_EQ(n % GroovyMiSTer::AudioTap::BYTES_PER_SAMPLE, 0u);
	EXPECT_LE(n, GroovyMiSTer::AudioTap::CAPACITY);
}

TEST(GroovyMiSTerAudioTap, SurvivesRingWraparound)
{
	GroovyMiSTer::AudioTap tap;
	tap.Reset();
	tap.SetActive(true);

	// Push/drain repeatedly so the read and write cursors wrap the buffer several times.
	const std::vector<float> chunk(64 * 2, 0.5f);
	std::vector<u8> out(64 * GroovyMiSTer::AudioTap::BYTES_PER_SAMPLE);

	u64 total = 0;
	for (int i = 0; i < 10000; i++)
	{
		tap.Write(chunk.data(), 64);
		total += tap.Read(out.data(), static_cast<u32>(out.size()));
	}

	EXPECT_EQ(tap.GetDroppedBytes(), 0u) << "steady-state drain should never drop";
	EXPECT_GT(total, 0u);
}

// =====================================================================================
//  Keepalive scheduling
// =====================================================================================
//
// Having advertised GM_CAP_KEEPALIVE at CMD_INIT, the core ends a session that puts nothing
// on the video socket for its idle timeout (OSD: Server -> Idle timeout, 5s default) and
// frees the CRT. PCSX2 is silent whenever it is alive but not blitting - paused, loading a
// savestate, swapping discs, or held off because no acceptable modeline could be produced -
// so without a keepalive, pausing kills the stream.
//
// Two properties matter, neither observable from the socket code, which is why the decision
// lives in its own header:
//
//   1. It must never fire during normal play. It is gated on the timestamp of the last
//      outbound datagram, so at 60fps the threshold is never reached.
//   2. Worst-case silence is `idle threshold + poll period`, not `idle threshold`, since a
//      poll landing just under the threshold defers the send by a whole period. That total
//      has to stay inside half the core's timeout for one lost datagram to be survivable.

TEST(GroovyMiSTerKeepAlive, NeverFiresDuringNormalPlay)
{
	GroovyMiSTer::KeepAliveScheduler ka;
	ka.Reset(0);

	// 60fps for 3 simulated seconds, with the scheduler polled far more often than frames
	// arrive. Not one poll may ask for a keepalive.
	constexpr u64 kFramePeriodMs = 16;
	constexpr u64 kRunMs = 3000;
	u64 next_frame = 0;

	for (u64 t = 0; t <= kRunMs; t++)
	{
		if (t >= next_frame)
		{
			ka.NotifyWireActivity(t);
			next_frame = t + kFramePeriodMs;
		}
		ASSERT_FALSE(ka.ShouldSend(t)) << "keepalive competed with the video stream at t=" << t;
	}
}

TEST(GroovyMiSTerKeepAlive, FiresOnceTheThresholdIsReached)
{
	GroovyMiSTer::KeepAliveScheduler ka;
	ka.Reset(0);

	constexpr u64 kThreshold = GroovyMiSTer::KeepAliveScheduler::IDLE_THRESHOLD_MS;
	EXPECT_FALSE(ka.ShouldSend(kThreshold - 1));
	EXPECT_TRUE(ka.ShouldSend(kThreshold));
	EXPECT_TRUE(ka.ShouldSend(kThreshold * 10));

	// Sending re-arms it.
	ka.NotifyWireActivity(kThreshold);
	EXPECT_FALSE(ka.ShouldSend(kThreshold));
	EXPECT_TRUE(ka.ShouldSend(kThreshold * 2));
}

TEST(GroovyMiSTerKeepAlive, WorstCaseSilenceSurvivesOneLostDatagram)
{
	constexpr u64 kThreshold = GroovyMiSTer::KeepAliveScheduler::IDLE_THRESHOLD_MS;
	constexpr u64 kPoll = GroovyMiSTer::KeepAliveScheduler::POLL_PERIOD_MS;
	// The shortest timeout the core offers. Everything below is measured against it.
	constexpr u64 kCoreIdleTimeoutMs = 5000;

	// Worst case: activity lands one tick AFTER a poll, so the poll that would have caught
	// the threshold misses it by a hair and the send waits a whole further period.
	u64 worst = 0;
	for (u64 offset = 0; offset < kPoll; offset++)
	{
		GroovyMiSTer::KeepAliveScheduler ka;
		const u64 activity = offset;
		ka.Reset(activity);

		// Polls happen on a fixed grid that is unaware of when activity stopped.
		u64 sent_at = 0;
		for (u64 t = 0; t <= kThreshold + 2 * kPoll; t += kPoll)
		{
			if (t >= activity && ka.ShouldSend(t))
			{
				sent_at = t;
				break;
			}
		}
		ASSERT_GT(sent_at, 0u) << "no keepalive was scheduled at all (offset " << offset << ")";
		worst = std::max(worst, sent_at - activity);
	}

	EXPECT_LE(worst, kThreshold + kPoll)
		<< "worst-case silence must be bounded by threshold + poll period";
	EXPECT_LE(worst * 2, kCoreIdleTimeoutMs)
		<< "losing one keepalive must still leave us inside the core's idle timeout";
}

TEST(GroovyMiSTerKeepAlive, ActivityJustUnderTheThresholdDefersAFullInterval)
{
	GroovyMiSTer::KeepAliveScheduler ka;
	ka.Reset(0);

	constexpr u64 kThreshold = GroovyMiSTer::KeepAliveScheduler::IDLE_THRESHOLD_MS;

	// A single frame slipping out at 1900ms - a game that resumed briefly - must reset the
	// clock, not merely delay it. This is the difference between gating on wire activity
	// and running a free-standing heartbeat.
	ka.NotifyWireActivity(kThreshold - 100);
	EXPECT_FALSE(ka.ShouldSend(kThreshold));
	EXPECT_FALSE(ka.ShouldSend(kThreshold * 2 - 101));
	EXPECT_TRUE(ka.ShouldSend(kThreshold * 2 - 100));
}

TEST(GroovyMiSTerKeepAlive, SilenceMsIsRobustToAClockGoingBackwards)
{
	GroovyMiSTer::KeepAliveScheduler ka;
	ka.Reset(10000);

	// Defensive: a rewound clock must read as "fresh activity", never underflow into a
	// gigantic age that would spray keepalives.
	EXPECT_EQ(ka.SilenceMs(9000), 0u);
	EXPECT_FALSE(ka.ShouldSend(9000));
	EXPECT_EQ(ka.SilenceMs(12500), 2500u);
	EXPECT_TRUE(ka.ShouldSend(12500));
}
