/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

#include <span>
#include <vector>

// Number of stereo samples per SndOut block.
// All drivers must work in units of this size when communicating with
// SndOut.
static constexpr int SndOutPacketSize = 64;

// Samplerate of the SPU2. For accurate playback we need to match this
// exactly.  Trying to scale samplerates and maintain SPU2's Ts timing accuracy
// is too problematic. :)
extern int SampleRate;

// Returns a null-terminated list of backends for the specified module.
// nullptr is returned if the specified module does not have multiple backends.
extern const char* const* GetOutputModuleBackends(const char* omodid);

// Returns a list of output devices and their associated minimum latency.
struct SndOutDeviceInfo
{
	std::string name;
	std::string display_name;
	u32 minimum_latency_frames;

	SndOutDeviceInfo(std::string name_, std::string display_name_, u32 minimum_latency_);
	~SndOutDeviceInfo();
};
std::vector<SndOutDeviceInfo> GetOutputDeviceList(const char* omodid, const char* driver);

struct StereoOut16;

struct Stereo51Out16DplII;
struct Stereo51Out32DplII;

struct Stereo51Out16Dpl; // similar to DplII but without rear balancing
struct Stereo51Out32Dpl;

extern void ResetDplIIDecoder();
extern void ProcessDplIISample16(const StereoOut16& src, Stereo51Out16DplII* s);
extern void ProcessDplIISample32(const StereoOut16& src, Stereo51Out32DplII* s);
extern void ProcessDplSample16(const StereoOut16& src, Stereo51Out16Dpl* s);
extern void ProcessDplSample32(const StereoOut16& src, Stereo51Out32Dpl* s);

struct StereoOut32
{
	static const StereoOut32 Empty;

	s32 Left;
	s32 Right;

	StereoOut32()
		: Left(0)
		, Right(0)
	{
	}

	StereoOut32(s32 left, s32 right)
		: Left(left)
		, Right(right)
	{
	}

	StereoOut32 operator*(const int& factor) const
	{
		return StereoOut32(
			Left * factor,
			Right * factor);
	}

	StereoOut32& operator*=(const int& factor)
	{
		Left *= factor;
		Right *= factor;
		return *this;
	}

	StereoOut32 operator+(const StereoOut32& right) const
	{
		return StereoOut32(
			Left + right.Left,
			Right + right.Right);
	}

	StereoOut32 operator/(int src) const
	{
		return StereoOut32(Left / src, Right / src);
	}
};

struct StereoOut16
{
	s16 Left;
	s16 Right;

	__fi StereoOut16()
		: Left(0)
		, Right(0)
	{
	}

	__fi StereoOut16(const StereoOut32& src)
		: Left((s16)src.Left)
		, Right((s16)src.Right)
	{
	}

	__fi StereoOut16(s16 left, s16 right)
		: Left(left)
		, Right(right)
	{
	}

	__fi StereoOut16 ApplyVolume(float volume)
	{
		return StereoOut16(
			static_cast<s16>(std::clamp(static_cast<float>(Left) * volume, -32768.0f, 32767.0f)),
			static_cast<s16>(std::clamp(static_cast<float>(Right) * volume, -32768.0f, 32767.0f))
		);
	}

	__fi void SetFrom(const StereoOut16& src)
	{
		Left = src.Left;
		Right = src.Right;
	}
};

struct Stereo21Out16
{
	s16 Left;
	s16 Right;
	s16 LFE;

	__fi void SetFrom(const StereoOut16& src)
	{
		Left = src.Left;
		Right = src.Right;
		LFE = (src.Left + src.Right) >> 1;
	}
};

struct Stereo40Out16
{
	s16 Left;
	s16 Right;
	s16 LeftBack;
	s16 RightBack;

	__fi void SetFrom(const StereoOut16& src)
	{
		Left = src.Left;
		Right = src.Right;
		LeftBack = src.Left;
		RightBack = src.Right;
	}
};

struct Stereo41Out16
{
	s16 Left;
	s16 Right;
	s16 LFE;
	s16 LeftBack;
	s16 RightBack;

	__fi void SetFrom(const StereoOut16& src)
	{
		Left = src.Left;
		Right = src.Right;
		LFE = (src.Left + src.Right) >> 1;
		LeftBack = src.Left;
		RightBack = src.Right;
	}
};

struct Stereo51Out16
{
	s16 Left;
	s16 Right;
	s16 Center;
	s16 LFE;
	s16 LeftBack;
	s16 RightBack;

	// Implementation Note: Center and Subwoofer/LFE -->
	// This method is simple and sounds nice.  It relies on the speaker/soundcard
	// systems do to their own low pass / crossover.  Manual lowpass is wasted effort
	// and can't match solid state results anyway.

	__fi void SetFrom(const StereoOut16& src)
	{
		Left = src.Left;
		Right = src.Right;
		Center = (src.Left + src.Right) >> 1;
		LFE = Center;
		LeftBack = src.Left >> 1;
		RightBack = src.Right >> 1;
	}
};

struct Stereo51Out16DplII
{
	s16 Left;
	s16 Right;
	s16 Center;
	s16 LFE;
	s16 LeftBack;
	s16 RightBack;

	__fi void SetFrom(const StereoOut16& src)
	{
		ProcessDplIISample16(src, this);
	}
};

struct Stereo51Out32DplII
{
	s32 Left;
	s32 Right;
	s32 Center;
	s32 LFE;
	s32 LeftBack;
	s32 RightBack;

	__fi void SetFrom(const StereoOut32& src)
	{
		ProcessDplIISample32(src, this);
	}
};

struct Stereo51Out16Dpl
{
	s16 Left;
	s16 Right;
	s16 Center;
	s16 LFE;
	s16 LeftBack;
	s16 RightBack;

	__fi void SetFrom(const StereoOut16& src)
	{
		ProcessDplSample16(src, this);
	}
};

struct Stereo51Out32Dpl
{
	s32 Left;
	s32 Right;
	s32 Center;
	s32 LFE;
	s32 LeftBack;
	s32 RightBack;

	__fi void SetFrom(const StereoOut32& src)
	{
		ProcessDplSample32(src, this);
	}
};

struct Stereo71Out16
{
	s16 Left;
	s16 Right;
	s16 Center;
	s16 LFE;
	s16 LeftBack;
	s16 RightBack;
	s16 LeftSide;
	s16 RightSide;

	__fi void SetFrom(const StereoOut16& src)
	{
		Left = src.Left;
		Right = src.Right;
		Center = (src.Left + src.Right) >> 1;
		LFE = Center;
		LeftBack = src.Left;
		RightBack = src.Right;

		LeftSide = src.Left >> 1;
		RightSide = src.Right >> 1;
	}
};

namespace SndBuffer
{
	void UpdateTempoChangeAsyncMixing();
	bool Init(const char* modname);
	void Cleanup();
	void Write(StereoOut16 Sample);
	void ClearContents();
	void ResetBuffers();

	// Note: When using with 32 bit output buffers, the user of this function is responsible
	// for shifting the values to where they need to be manually.  The fixed point depth of
	// the sample output is determined by the SndOutVolumeShift, which is the number of bits
	// to shift right to get a 16 bit result.
	template <typename T>
	void ReadSamples(T* bData, int nSamples = SndOutPacketSize);
}

class SndOutModule
{
public:
	virtual ~SndOutModule() = default;

	// Returns a unique identification string for this driver.
	// (usually just matches the driver's cpp filename)
	virtual const char* GetIdent() const = 0;

	// Returns the full name for this driver, and can be translated.
	virtual const char* GetDisplayName() const = 0;

	// Returns a null-terminated list of backends, or nullptr.
	virtual const char* const* GetBackendNames() const = 0;

	// Returns a list of output devices and their associated minimum latency.
	virtual std::vector<SndOutDeviceInfo> GetOutputDeviceList(const char* driver) const = 0;

	virtual bool Init() = 0;
	virtual void Close() = 0;

	// Temporarily pauses the stream, preventing it from requesting data.
	virtual void SetPaused(bool paused) = 0;

	// Returns the number of empty samples in the output buffer.
	// (which is effectively the amount of data played since the last update)
	virtual int GetEmptySampleCount() = 0;
};

std::span<SndOutModule*> GetSndOutModules();
