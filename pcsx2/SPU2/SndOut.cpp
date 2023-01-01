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

#include "PrecompiledHeader.h"

#include "SPU2/Global.h"
#include "SPU2/spu2.h"

#include "common/Assertions.h"
#include "common/Timer.h"

#include "SoundTouch.h"

StereoOut32 StereoOut32::Empty(0, 0);

StereoOut32::StereoOut32(const StereoOut16& src)
	: Left(src.Left)
	, Right(src.Right)
{
}

StereoOut32::StereoOut32(const StereoOutFloat& src)
	: Left((s32)(src.Left * 2147483647.0f))
	, Right((s32)(src.Right * 2147483647.0f))
{
}

StereoOut16 StereoOut32::DownSample() const
{
	return StereoOut16(
		Left >> SndOutVolumeShift,
		Right >> SndOutVolumeShift);
}

StereoOut32 StereoOut16::UpSample() const
{
	return StereoOut32(
		Left << SndOutVolumeShift,
		Right << SndOutVolumeShift);
}

namespace {
class NullOutModule final : public SndOutModule
{
public:
	bool Init() override { return true; }
	void Close() override {}
	void SetPaused(bool paused) override {}
	int GetEmptySampleCount() override { return 0; }

	const char* GetIdent() const override
	{
		return "nullout";
	}

	const char* GetLongName() const override
	{
		return "No Sound (Emulate SPU2 only)";
	}

	const char* const* GetBackendNames() const override
	{
		return nullptr;
	}

	std::vector<SndOutDeviceInfo> GetOutputDeviceList(const char* driver) const override
	{
		return {};
	}
};
}

static NullOutModule s_NullOut;
static SndOutModule* NullOut = &s_NullOut;

static SndOutModule* mods[] =
	{
		NullOut,
#ifdef _WIN32
		XAudio2Out,
#endif
#if defined(SPU2X_CUBEB)
		CubebOut,
#endif
};

static SndOutModule* s_output_module;

static SndOutModule* FindOutputModule(const char* name)
{
	for (u32 i = 0; i < std::size(mods); i++)
	{
		if (std::strcmp(mods[i]->GetIdent(), name) == 0)
			return mods[i];
	}

	return nullptr;
}

const char* const* GetOutputModuleBackends(const char* omodid)
{
	if (SndOutModule* mod = FindOutputModule(omodid))
		return mod->GetBackendNames();

	return nullptr;
}

SndOutDeviceInfo::SndOutDeviceInfo(std::string name_, std::string display_name_, u32 minimum_latency_)
	: name(std::move(name_)), display_name(std::move(display_name_)), minimum_latency_frames(minimum_latency_)
{
}

SndOutDeviceInfo::~SndOutDeviceInfo() = default;

std::vector<SndOutDeviceInfo> GetOutputDeviceList(const char* omodid, const char* driver)
{
	std::vector<SndOutDeviceInfo> ret;
	if (SndOutModule* mod = FindOutputModule(omodid))
		ret = mod->GetOutputDeviceList(driver);
	return ret;
}

StereoOut32* SndBuffer::m_buffer;
s32 SndBuffer::m_size;
alignas(4) volatile s32 SndBuffer::m_rpos;
alignas(4) volatile s32 SndBuffer::m_wpos;

bool SndBuffer::m_underrun_freeze;
StereoOut32* SndBuffer::sndTempBuffer = nullptr;
StereoOut16* SndBuffer::sndTempBuffer16 = nullptr;
int SndBuffer::sndTempProgress = 0;

int GetAlignedBufferSize(int comp)
{
	return (comp + SndOutPacketSize - 1) & ~(SndOutPacketSize - 1);
}

// Returns TRUE if there is data to be output, or false if no data
// is available to be copied.
bool SndBuffer::CheckUnderrunStatus(int& nSamples, int& quietSampleCount)
{
	quietSampleCount = 0;

	int data = _GetApproximateDataInBuffer();
	if (m_underrun_freeze)
	{
		int toFill = m_size / ((EmuConfig.SPU2.SynchMode == Pcsx2Config::SPU2Options::SynchronizationMode::NoSync) ? 32 : 400); // TimeStretch and Async off?
		toFill = GetAlignedBufferSize(toFill);

		// toFill is now aligned to a SndOutPacket

		if (data < toFill)
		{
			quietSampleCount = nSamples;
			nSamples = 0;
			return false;
		}

		m_underrun_freeze = false;
		if (SPU2::MsgOverruns())
			SPU2::ConLog(" * SPU2 > Underrun compensation (%d packets buffered)\n", toFill / SndOutPacketSize);
		lastPct = 0.0; // normalize timestretcher
	}
	else if (data < nSamples)
	{
		quietSampleCount = nSamples - data;
		nSamples = data;
		m_underrun_freeze = true;

		if (EmuConfig.SPU2.SynchMode == Pcsx2Config::SPU2Options::SynchronizationMode::TimeStretch) // TimeStrech on
			timeStretchUnderrun();

		return nSamples != 0;
	}

	return true;
}

int SndBuffer::_GetApproximateDataInBuffer()
{
	// WARNING: not necessarily 100% up to date by the time it's used, but it will have to do.
	return (m_wpos + m_size - m_rpos) % m_size;
}

void SndBuffer::_WriteSamples_Internal(StereoOut32* bData, int nSamples)
{
	// WARNING: This assumes the write will NOT wrap around,
	// and also assumes there's enough free space in the buffer.

	memcpy(m_buffer + m_wpos, bData, nSamples * sizeof(StereoOut32));
	m_wpos = (m_wpos + nSamples) % m_size;
}

void SndBuffer::_DropSamples_Internal(int nSamples)
{
	m_rpos = (m_rpos + nSamples) % m_size;
}

void SndBuffer::_ReadSamples_Internal(StereoOut32* bData, int nSamples)
{
	// WARNING: This assumes the read will NOT wrap around,
	// and also assumes there's enough data in the buffer.
	memcpy(bData, m_buffer + m_rpos, nSamples * sizeof(StereoOut32));
	_DropSamples_Internal(nSamples);
}

void SndBuffer::_WriteSamples_Safe(StereoOut32* bData, int nSamples)
{
	// WARNING: This code assumes there's only ONE writing process.
	if ((m_size - m_wpos) < nSamples)
	{
		int b1 = m_size - m_wpos;
		int b2 = nSamples - b1;

		_WriteSamples_Internal(bData, b1);
		_WriteSamples_Internal(bData + b1, b2);
	}
	else
	{
		_WriteSamples_Internal(bData, nSamples);
	}
}

void SndBuffer::_ReadSamples_Safe(StereoOut32* bData, int nSamples)
{
	// WARNING: This code assumes there's only ONE reading process.
	if ((m_size - m_rpos) < nSamples)
	{
		int b1 = m_size - m_rpos;
		int b2 = nSamples - b1;

		_ReadSamples_Internal(bData, b1);
		_ReadSamples_Internal(bData + b1, b2);
	}
	else
	{
		_ReadSamples_Internal(bData, nSamples);
	}
}

// Note: When using with 32 bit output buffers, the user of this function is responsible
// for shifting the values to where they need to be manually.  The fixed point depth of
// the sample output is determined by the SndOutVolumeShift, which is the number of bits
// to shift right to get a 16 bit result.
template <typename T>
void SndBuffer::ReadSamples(T* bData, int nSamples)
{
	// Problem:
	//  If the SPU2 gets even the least bit out of sync with the SndOut device,
	//  the readpos of the circular buffer will overtake the writepos,
	//  leading to a prolonged period of hopscotching read/write accesses (ie,
	//  lots of staticy crap sound for several seconds).
	//
	// Fix:
	//  If the read position overtakes the write position, abort the
	//  transfer immediately and force the SndOut driver to wait until
	//  the read buffer has filled up again before proceeding.
	//  This will cause one brief hiccup that can never exceed the user's
	//  set buffer length in duration.

	int quietSamples;
	if (CheckUnderrunStatus(nSamples, quietSamples))
	{
		pxAssume(nSamples <= SndOutPacketSize);

		// WARNING: This code assumes there's only ONE reading process.
		int b1 = m_size - m_rpos;

		if (b1 > nSamples)
			b1 = nSamples;

		// First part
		for (int i = 0; i < b1; i++)
			bData[i].ResampleFrom(m_buffer[i + m_rpos]);

		// Second part
		int b2 = nSamples - b1;
		for (int i = 0; i < b2; i++)
			bData[i + b1].ResampleFrom(m_buffer[i]);
	
		_DropSamples_Internal(nSamples);
	}

	// If quietSamples != 0 it means we have an underrun...
	// Let's just dull out some silence, because that's usually the least
	// painful way of dealing with underruns:
	if (quietSamples > 0)
		std::memset(bData + nSamples, 0, sizeof(T) * quietSamples);
}

template void SndBuffer::ReadSamples(StereoOut16*, int);
template void SndBuffer::ReadSamples(StereoOut32*, int);

//template void SndBuffer::ReadSamples(StereoOutFloat*);
template void SndBuffer::ReadSamples(Stereo21Out16*, int);
template void SndBuffer::ReadSamples(Stereo40Out16*, int);
template void SndBuffer::ReadSamples(Stereo41Out16*, int);
template void SndBuffer::ReadSamples(Stereo51Out16*, int);
template void SndBuffer::ReadSamples(Stereo51Out16Dpl*, int);
template void SndBuffer::ReadSamples(Stereo51Out16DplII*, int);
template void SndBuffer::ReadSamples(Stereo71Out16*, int);

template void SndBuffer::ReadSamples(Stereo20Out32*, int);
template void SndBuffer::ReadSamples(Stereo21Out32*, int);
template void SndBuffer::ReadSamples(Stereo40Out32*, int);
template void SndBuffer::ReadSamples(Stereo41Out32*, int);
template void SndBuffer::ReadSamples(Stereo51Out32*, int);
template void SndBuffer::ReadSamples(Stereo51Out32Dpl*, int);
template void SndBuffer::ReadSamples(Stereo51Out32DplII*, int);
template void SndBuffer::ReadSamples(Stereo71Out32*, int);

void SndBuffer::_WriteSamples(StereoOut32* bData, int nSamples)
{
	m_predictData = 0;

	// Problem:
	//  If the SPU2 gets out of sync with the SndOut device, the writepos of the
	//  circular buffer will overtake the readpos, leading to a prolonged period
	//  of hopscotching read/write accesses (ie, lots of staticy crap sound for
	//  several seconds).
	//
	// Compromise:
	//  When an overrun occurs, we adapt by discarding a portion of the buffer.
	//  The older portion of the buffer is discarded rather than incoming data,
	//  so that the overall audio synchronization is better.

	int free = m_size - _GetApproximateDataInBuffer(); // -1, but the <= handles that
	if (free <= nSamples)
	{
// Disabled since the lock-free queue can't handle changing the read end from the write thread
#if 0
		// Buffer overrun!
		// Dump samples from the read portion of the buffer instead of dropping
		// the newly written stuff.

		s32 comp;

		if( SynchMode == 0 ) // TimeStrech on
		{
			comp = timeStretchOverrun();
		}
		else
		{
			// Toss half the buffer plus whatever's being written anew:
			comp = GetAlignedBufferSize( (m_size + nSamples ) / 16 );
			if( comp > (m_size-SndOutPacketSize) ) comp = m_size-SndOutPacketSize;
		}

		_DropSamples_Internal(comp);

		if( MsgOverruns() )
			ConLog(" * SPU2 > Overrun Compensation (%d packets tossed)\n", comp / SndOutPacketSize );
		lastPct = 0.0;		// normalize the timestretcher
#else
		if (SPU2::MsgOverruns())
			SPU2::ConLog(" * SPU2 > Overrun! 1 packet tossed)\n");
		lastPct = 0.0; // normalize the timestretcher
		return;
#endif
	}

	_WriteSamples_Safe(bData, nSamples);
}

bool SndBuffer::Init(const char* modname)
{
	s_output_module = FindOutputModule(modname);
	if (!s_output_module)
		return false;

	// initialize sound buffer
	// Buffer actually attempts to run ~50%, so allocate near double what
	// the requested latency is:

	m_rpos = 0;
	m_wpos = 0;

	const float latencyMS = EmuConfig.SPU2.Latency * 16;
	m_size = GetAlignedBufferSize((int)(latencyMS * SampleRate / 1000.0f));
	m_buffer = new StereoOut32[m_size];
	m_underrun_freeze = false;

	sndTempBuffer = new StereoOut32[SndOutPacketSize];
	sndTempBuffer16 = new StereoOut16[SndOutPacketSize * 2]; // in case of leftovers.
	sndTempProgress = 0;

	soundtouchInit(); // initializes the timestretching

	// initialize module
	if (!s_output_module->Init())
	{
		Cleanup();
		return false;
	}

	return true;
}

void SndBuffer::Cleanup()
{
	if (s_output_module)
	{
		s_output_module->Close();
		s_output_module = nullptr;
	}

	soundtouchCleanup();

	safe_delete_array(m_buffer);
	safe_delete_array(sndTempBuffer);
	safe_delete_array(sndTempBuffer16);
}

int SndBuffer::m_dsp_progress = 0;

int SndBuffer::m_timestretch_progress = 0;
int SndBuffer::ssFreeze = 0;

void SndBuffer::ClearContents()
{
	SndBuffer::soundtouchClearContents();
	SndBuffer::ssFreeze = 256; //Delays sound output for about 1 second.
}

void SndBuffer::ResetBuffers()
{
	m_rpos = 0;
	m_wpos = 0;
}

void SPU2::SetOutputPaused(bool paused)
{
	s_output_module->SetPaused(paused);
}

void SndBuffer::Write(const StereoOut32& Sample)
{
	// Log final output to wavefile.
	WaveDump::WriteCore(1, CoreSrc_External, Sample.DownSample());

	if (WavRecordEnabled)
		RecordWrite(Sample.DownSample());

	sndTempBuffer[sndTempProgress++] = Sample;

	// If we haven't accumulated a full packet yet, do nothing more:
	if (sndTempProgress < SndOutPacketSize)
		return;
	sndTempProgress = 0;

	//Don't play anything directly after loading a savestate, avoids static killing your speakers.
	if (ssFreeze > 0)
	{
		ssFreeze--;
		// Play silence
		std::fill_n(sndTempBuffer, SndOutPacketSize, StereoOut32{});
	}
	else
	{
		if (EmuConfig.SPU2.SynchMode == Pcsx2Config::SPU2Options::SynchronizationMode::TimeStretch) // TimeStrech on
			timeStretchWrite();
		else
			_WriteSamples(sndTempBuffer, SndOutPacketSize);
	}
}

//////////////////////////////////////////////////////////////////////////
// Time Stretching
//////////////////////////////////////////////////////////////////////////

//Uncomment the next line to use the old time stretcher
//#define SPU2X_USE_OLD_STRETCHER

static std::unique_ptr<soundtouch::SoundTouch> pSoundTouch = nullptr;

// data prediction amount, used to "commit" data that hasn't
// finished timestretch processing.
s32 SndBuffer::m_predictData;

// records last buffer status (fill %, range -100 to 100, with 0 being 50% full)
float SndBuffer::lastPct;
float SndBuffer::lastEmergencyAdj;

float SndBuffer::cTempo = 1;
float SndBuffer::eTempo = 1;

void SndBuffer::PredictDataWrite(int samples)
{
	m_predictData += samples;
}

// Calculate the buffer status percentage.
// Returns range from -1.0 to 1.0
//    1.0 = buffer overflow!
//    0.0 = buffer nominal (50% full)
//   -1.0 = buffer underflow!
float SndBuffer::GetStatusPct()
{
	// Get the buffer status of the output driver too, so that we can
	// obtain a more accurate overall buffer status.

	int drvempty = s_output_module->GetEmptySampleCount(); // / 2;

	//ConLog( "Data %d >>> driver: %d   predict: %d\n", m_data, drvempty, m_predictData );

	int data = _GetApproximateDataInBuffer();
	float result = (float)(data + m_predictData - drvempty) - (m_size / 16);
	result /= (m_size / 16);
	return result;
}


//Alternative simple tempo adjustment. Based only on the soundtouch buffer state.
//Base algorithm: aim at specific average number of samples at the buffer (by GUI), and adjust tempo simply by current/target.
//An extra mechanism is added to keep adjustment at perfect 1:1 ratio (when emulation speed is stable around 100%)
//  to prevent constant stretching/shrinking of packets if possible.
//  This mechanism is triggered when the adjustment is close to 1:1 for long enough (defaults to 100 iterations within hys_ok_factor - defaults to 3%).
//  1:1 state is aborted when required adjustment goes beyond hys_bad_factor (defaults to 20%).
//
//To compensate for wide variation of the <num of samples> ratio due to relatively small size of the buffer,
//  The required tempo is a running average of STRETCH_AVERAGE_LEN (defaults to 50) last calculations.
//  This averaging slows down the respons time of the algorithm, but greatly stablize it towards steady stretching.
//
//Keeping the buffer at required latency:
//  This algorithm stabilises when the actual latency is <speed>*<required_latency>. While this is just fine at 100% speed,
//  it's problematic especially for slow speeds, as the number of actual samples at the buffer gets very small on that case,
//  which may lead to underruns (or just too much latency when running very fast).
//To compensate for that, the algorithm has a slowly moving compensation factor which will eventually bring the actual latency to the required one.
//compensationDivider defines how slow this compensation changes. By default it's set to 100,
//  which will finalize the compensation after about 200 iterations.
//
// Note, this algorithm is intentionally simplified by not taking extreme actions at extreme scenarios (mostly underruns when speed drops sharply),
//  and let's the overrun/underrun protections do what they should (doesn't happen much though in practice, even at big FPS variations).
//
//  These params were tested to show good respond and stability, on all audio systems (dsound, wav, port audio, xaudio2),
//    even at extreme small latency of 50ms which can handle 50%-100% variations without audible glitches.

int targetIPS = 750;

//Dynamic tuning changes the values of the base algorithm parameters (derived from targetIPS) to adapt, in real time, to
//  different number of invocations/sec (mostly affects number of iterations to average).
//  Dynamic tuning can have a slight negative effect on the behavior of the algorithm, so it's preferred to have it off.
//Currently it looks like it's around 750/sec on all systems when playing at 100% speed (50/60fps),
//  and proportional to that speed otherwise.
//If changes are made to SPU2 which affects this number (but it's still the same on all systems), then just change targetIPS.
//If we find out that some systems are very different, we can turn on dynamic tuning by uncommenting the next line.
//#define NEWSTRETCHER_USE_DYNAMIC_TUNING


//Additional performance note: since MAX_STRETCH_AVERAGE_LEN = 128 (or any power of 2), the '%' below
//could be replaced with a faster '&'. The compiler is highly likely to do it since all the values are unsigned.
#define AVERAGING_BUFFER_SIZE 256U
unsigned int AVERAGING_WINDOW = 50.0 * targetIPS / 750;


#define STRETCHER_RESET_THRESHOLD 5
int gRequestStretcherReset = STRETCHER_RESET_THRESHOLD;
//Adds a value to the running average buffer, and return the new running average.
static float addToAvg(float val)
{
	static float avg_fullness[AVERAGING_BUFFER_SIZE];
	static unsigned int nextAvgPos = 0;
	static unsigned int available = 0; // Make sure we're not averaging AVERAGING_WINDOW items if we inserted less.
	if (gRequestStretcherReset >= STRETCHER_RESET_THRESHOLD)
		available = 0;

	if (available < AVERAGING_BUFFER_SIZE)
		available++;

	avg_fullness[nextAvgPos] = val;
	nextAvgPos = (nextAvgPos + 1U) % AVERAGING_BUFFER_SIZE;

	unsigned int actualWindow = std::min(available, AVERAGING_WINDOW);
	unsigned int first = (nextAvgPos - actualWindow + AVERAGING_BUFFER_SIZE) % AVERAGING_BUFFER_SIZE;

	// Possible optimization: if we know that actualWindow hasn't changed since
	// last invocation, we could calculate the running average in O(1) instead of O(N)
	// by keeping a running sum between invocations, and then
	// do "runningSum = runningSum + val - avg_fullness[(first-1)%...]" instead of the following loop.
	// Few gotchas: val overwrites first-1, handling actualWindow changes, etc.
	// However, this isn't hot code, so unless proven otherwise, we can live with unoptimized code.
	float sum = 0;
	for (unsigned int i = first; i < first + actualWindow; i++)
	{
		sum += avg_fullness[i % AVERAGING_BUFFER_SIZE];
	}
	sum = sum / actualWindow;

	return sum ? sum : 1; // 1 because that's the 100% perfect speed value
}

template <class T>
static bool IsInRange(const T& val, const T& min, const T& max)
{
	return (min <= val && val <= max);
}

//actual stretch algorithm implementation
void SndBuffer::UpdateTempoChangeSoundTouch2()
{

	long targetSamplesReservoir = 48 * EmuConfig.SPU2.Latency; //48000*SndOutLatencyMS/1000
	//base aim at buffer filled %
	float baseTargetFullness = (double)targetSamplesReservoir; ///(double)m_size;//0.05;

	//state vars
	static bool inside_hysteresis;      //=false;
	static int hys_ok_count;            //=0;
	static float dynamicTargetFullness; //=baseTargetFullness;
	if (gRequestStretcherReset >= STRETCHER_RESET_THRESHOLD)
	{
		if (SPU2::MsgOverruns())
			SPU2::ConLog("______> stretch: Reset.\n");
		inside_hysteresis = false;
		hys_ok_count = 0;
		dynamicTargetFullness = baseTargetFullness;
	}

	int data = _GetApproximateDataInBuffer();
	float bufferFullness = (float)data; ///(float)m_size;

#ifdef NEWSTRETCHER_USE_DYNAMIC_TUNING
	{ //test current iterations/sec every 0.5s, and change algo params accordingly if different than previous IPS more than 30%
		static long iters = 0;
		static wxDateTime last = wxDateTime::UNow();
		wxDateTime unow = wxDateTime::UNow();
		wxTimeSpan delta = unow.Subtract(last);
		if (delta.GetMilliseconds() > 500)
		{
			int pot_targetIPS = 1000.0 / delta.GetMilliseconds().ToDouble() * iters;
			if (!IsInRange(pot_targetIPS, int((float)targetIPS / 1.3f), int((float)targetIPS * 1.3f)))
			{
				if (SPU2::MsgOverruns())
					SPU2::ConLog("Stretcher: setting iters/sec from %d to %d\n", targetIPS, pot_targetIPS);
				targetIPS = pot_targetIPS;
				AVERAGING_WINDOW = std::clamp((int)(50.0f * (float)targetIPS / 750.0f), 3, (int)AVERAGING_BUFFER_SIZE);
			}
			last = unow;
			iters = 0;
		}
		iters++;
	}
#endif

	//Algorithm params: (threshold params (hysteresis), etc)
	const float hys_ok_factor = 1.04f;
	const float hys_bad_factor = 1.2f;
	int hys_min_ok_count = std::clamp((int)(50.0 * (float)targetIPS / 750.0), 2, 100); //consecutive iterations within hys_ok before going to 1:1 mode
	int compensationDivider = std::clamp((int)(100.0 * (float)targetIPS / 750), 15, 150);

	float tempoAdjust = bufferFullness / dynamicTargetFullness;
	float avgerage = addToAvg(tempoAdjust);
	tempoAdjust = avgerage;

	// Dampen the adjustment to avoid overshoots (this means the average will compensate to the other side).
	// This is different than simply bigger averaging window since bigger window also has bigger "momentum",
	// so it's slower to slow down when it gets close to the equilibrium state and can therefore resonate.
	// The dampening (sqrt was chosen for no very good reason) manages to mostly prevent that.
	tempoAdjust = sqrt(tempoAdjust);

	tempoAdjust = std::clamp(tempoAdjust, 0.05f, 10.0f);

	if (tempoAdjust < 1)
		baseTargetFullness /= sqrt(tempoAdjust); // slightly increase latency when running slow.

	dynamicTargetFullness += (baseTargetFullness / tempoAdjust - dynamicTargetFullness) / (double)compensationDivider;
	if (IsInRange(tempoAdjust, 0.9f, 1.1f) && IsInRange(dynamicTargetFullness, baseTargetFullness * 0.9f, baseTargetFullness * 1.1f))
		dynamicTargetFullness = baseTargetFullness;

	if (!inside_hysteresis)
	{
		if (IsInRange(tempoAdjust, 1.0f / hys_ok_factor, hys_ok_factor))
			hys_ok_count++;
		else
			hys_ok_count = 0;

		if (hys_ok_count >= hys_min_ok_count)
		{
			inside_hysteresis = true;
			if (SPU2::MsgOverruns())
				SPU2::ConLog("======> stretch: None (1:1)\n");
		}
	}
	else if (!IsInRange(tempoAdjust, 1.0f / hys_bad_factor, hys_bad_factor))
	{
		if (SPU2::MsgOverruns())
			SPU2::ConLog("~~~~~~> stretch: Dynamic\n");
		inside_hysteresis = false;
		hys_ok_count = 0;
	}

	if (inside_hysteresis)
		tempoAdjust = 1.0;

	if (SPU2::MsgOverruns())
	{
		static int iters = 0;
		static u64 last = 0;

		const u64 now = Common::Timer::GetCurrentValue();

		if (Common::Timer::ConvertValueToSeconds(now - last) > 1.0f)
		{ //report buffers state and tempo adjust every second
			SPU2::ConLog("buffers: %4d ms (%3.0f%%), tempo: %f, comp: %2.3f, iters: %d, (N-IPS:%d -> avg:%d, minokc:%d, div:%d) reset:%d\n",
				   (int)(data / 48), (double)(100.0 * bufferFullness / baseTargetFullness), (double)tempoAdjust, (double)(dynamicTargetFullness / baseTargetFullness), iters, (int)targetIPS, AVERAGING_WINDOW, hys_min_ok_count, compensationDivider, gRequestStretcherReset);
			last = now;
			iters = 0;
		}
		iters++;
	}

	pSoundTouch->setTempo(tempoAdjust);
	if (gRequestStretcherReset >= STRETCHER_RESET_THRESHOLD)
		gRequestStretcherReset = 0;

	return;
}


void SndBuffer::UpdateTempoChangeSoundTouch()
{
	float statusPct = GetStatusPct();
	float pctChange = statusPct - lastPct;

	float tempoChange;
	float emergencyAdj = 0;
	float newcee = cTempo; // workspace var. for cTempo

	// IMPORTANT!
	// If you plan to tweak these values, make sure you're using a release build
	// OUTSIDE THE DEBUGGER to test it!  The Visual Studio debugger can really cause
	// erratic behavior in the audio buffers, and makes the timestretcher seem a
	// lot more inconsistent than it really is.

	// We have two factors.
	//   * Distance from nominal buffer status (50% full)
	//   * The change from previous update to this update.

	// Prediction based on the buffer change:
	// (linear seems to work better here)

	tempoChange = pctChange * 0.75f;

	if (statusPct * tempoChange < 0.0f)
	{
		// only apply tempo change if it is in synch with the buffer status.
		// In other words, if the buffer is high (over 0%), and is decreasing,
		// ignore it.  It'll just muck things up.

		tempoChange = 0;
	}

	// Sudden spikes in framerate can cause the nominal buffer status
	// to go critical, in which case we have to enact an emergency
	// stretch. The following cubic formulas do that.  Values near
	// the extremeites give much larger results than those near 0.
	// And the value is added only this time, and does not accumulate.
	// (otherwise a large value like this would cause problems down the road)

	// Constants:
	// Weight - weights the statusPct's "emergency" consideration.
	//   higher values here will make the buffer perform more drastic
	//   compensations at the outer edges of the buffer (at -75 or +75%
	//   or beyond, for example).

	// Range - scales the adjustment to the given range (more or less).
	//   The actual range is dependent on the weight used, so if you increase
	//   Weight you'll usually want to decrease Range somewhat to compensate.

	// Prediction based on the buffer fill status:

	const float statusWeight = 2.99f;
	const float statusRange = 0.068f;

	// "non-emergency" deadzone:  In this area stretching will be strongly discouraged.
	// Note: due tot he nature of timestretch latency, it's always a wee bit harder to
	// cope with low fps (underruns) than it is high fps (overruns).  So to help out a
	// little, the low-end portions of this check are less forgiving than the high-sides.

	if (cTempo < 0.965f || cTempo > 1.060f ||
		pctChange < -0.38f || pctChange > 0.54f ||
		statusPct < -0.42f || statusPct > 0.70f ||
		eTempo < 0.89f || eTempo > 1.19f)
	{
		//printf("Emergency stretch: cTempo = %f eTempo = %f pctChange = %f statusPct = %f\n",cTempo,eTempo,pctChange,statusPct);
		emergencyAdj = (pow(statusPct * statusWeight, 3.0f) * statusRange);
	}

	// Smooth things out by factoring our previous adjustment into this one.
	// It helps make the system 'feel' a little smarter by  giving it at least
	// one packet worth of history to help work off of:

	emergencyAdj = (emergencyAdj * 0.75f) + (lastEmergencyAdj * 0.25f);

	lastEmergencyAdj = emergencyAdj;
	lastPct = statusPct;

	// Accumulate a fraction of the tempo change into the tempo itself.
	// This helps the system run "smarter" to games that run consistently
	// fast or slow by altering the base tempo to something closer to the
	// game's active speed.  In tests most games normalize within 2 seconds
	// at 100ms latency, which is pretty good (larger buffers normalize even
	// quicker).

	newcee += newcee * (tempoChange + emergencyAdj) * 0.03f;

	// Apply tempoChange as a scale of cTempo.  That way the effect is proportional
	// to the current tempo.  (otherwise tempos rate of change at the extremes would
	// be too drastic)

	float newTempo = newcee + (emergencyAdj * cTempo);

	// ... and as a final optimization, only stretch if the new tempo is outside
	// a nominal threshold.  Keep this threshold check small, because it could
	// cause some serious side effects otherwise. (enlarging the cTempo check above
	// is usually better/safer)
	if (newTempo < 0.970f || newTempo > 1.045f)
	{
		cTempo = (float)newcee;

		if (newTempo < 0.10f)
			newTempo = 0.10f;
		else if (newTempo > 10.0f)
			newTempo = 10.0f;

		if (cTempo < 0.15f)
			cTempo = 0.15f;
		else if (cTempo > 7.5f)
			cTempo = 7.5f;

		pSoundTouch->setTempo(eTempo = (float)newTempo);

		/*ConLog("* SPU2: [Nominal %d%%] [Emergency: %d%%] (baseTempo: %d%% ) (newTempo: %d%%) (buffer: %d%%)\n",
			//(relation < 0.0) ? "Normalize" : "",
			(int)(tempoChange * 100.0 * 0.03),
			(int)(emergencyAdj * 100.0),
			(int)(cTempo * 100.0),
			(int)(newTempo * 100.0),
			(int)(statusPct * 100.0)
		);*/
	}
	else
	{
		// Nominal operation -- turn off stretching.
		// note: eTempo 'slides' toward 1.0 for smoother audio and better
		// protection against spikes.
		if (cTempo != 1.0f)
		{
			cTempo = 1.0f;
			eTempo = (1.0f + eTempo) * 0.5f;
			pSoundTouch->setTempo(eTempo);
		}
		else
		{
			if (eTempo != cTempo)
				pSoundTouch->setTempo(eTempo = cTempo);
		}
	}
}

extern uint TickInterval;
void SndBuffer::UpdateTempoChangeAsyncMixing()
{
	float statusPct = GetStatusPct();

	lastPct = statusPct;
	if (statusPct < -0.1f)
	{
		TickInterval -= 4;
		if (statusPct < -0.3f)
			TickInterval = 64;
		if (TickInterval < 64)
			TickInterval = 64;
		//printf("-- %d, %f\n",TickInterval,statusPct);
	}
	else if (statusPct > 0.2f)
	{
		TickInterval += 1;
		if (TickInterval >= 7000)
			TickInterval = 7000;
		//printf("++ %d, %f\n",TickInterval,statusPct);
	}
	else
		TickInterval = 768;
}

void SndBuffer::timeStretchUnderrun()
{
	gRequestStretcherReset++;
	// timeStretcher failed it's job.  We need to slow down the audio some.

	cTempo -= (cTempo * 0.12f);
	eTempo -= (eTempo * 0.30f);
	if (eTempo < 0.1f)
		eTempo = 0.1f;
	//	pSoundTouch->setTempo( eTempo );
	//pSoundTouch->setTempoChange(-30); // temporary (until stretcher is called) slow down
}

s32 SndBuffer::timeStretchOverrun()
{
	// If we overran it means the timestretcher failed.  We need to speed
	// up audio playback.
	cTempo += cTempo * 0.12f;
	eTempo += eTempo * 0.40f;
	if (eTempo > 7.5f)
		eTempo = 7.5f;
	//pSoundTouch->setTempo( eTempo );
	//pSoundTouch->setTempoChange(30);// temporary (until stretcher is called) speed up

	// Throw out just a little bit (two packets worth) to help
	// give the TS some room to work:
	gRequestStretcherReset++;
	return SndOutPacketSize * 2;
}

static void CvtPacketToFloat(StereoOut32* srcdest)
{
	StereoOutFloat* dest = (StereoOutFloat*)srcdest;
	const StereoOut32* src = (StereoOut32*)srcdest;
	for (uint i = 0; i < SndOutPacketSize; ++i, ++dest, ++src)
		*dest = (StereoOutFloat)*src;
}

// Parameter note: Size should always be a multiple of 128, thanks!
static void CvtPacketToInt(StereoOut32* srcdest, uint size)
{
	//pxAssume( (size & 127) == 0 );

	const StereoOutFloat* src = (StereoOutFloat*)srcdest;
	StereoOut32* dest = srcdest;

	for (uint i = 0; i < size; ++i, ++dest, ++src)
		*dest = (StereoOut32)*src;
}

void SndBuffer::timeStretchWrite()
{
	// data prediction helps keep the tempo adjustments more accurate.
	// The timestretcher returns packets in belated "clump" form.
	// Meaning that most of the time we'll get nothing back, and then
	// suddenly we'll get several chunks back at once.  Thus we use
	// data prediction to make the timestretcher more responsive.

	PredictDataWrite((int)(SndOutPacketSize / eTempo));
	CvtPacketToFloat(sndTempBuffer);

	pSoundTouch->putSamples((float*)sndTempBuffer, SndOutPacketSize);

	int tempProgress;
	while (tempProgress = pSoundTouch->receiveSamples((float*)sndTempBuffer, SndOutPacketSize),
		   tempProgress != 0)
	{
		// Hint: It's assumed that pSoundTouch will return chunks of 128 bytes (it always does as
		// long as the SSE optimizations are enabled), which means we can do our own SSE opts here.

		CvtPacketToInt(sndTempBuffer, tempProgress);
		_WriteSamples(sndTempBuffer, tempProgress);
	}

#ifdef SPU2X_USE_OLD_STRETCHER
	UpdateTempoChangeSoundTouch();
#else
	UpdateTempoChangeSoundTouch2();
#endif
}

void SndBuffer::soundtouchInit()
{
	pSoundTouch = std::make_unique<soundtouch::SoundTouch>();
	pSoundTouch->setSampleRate(SampleRate);
	pSoundTouch->setChannels(2);

	pSoundTouch->setSetting(SETTING_USE_QUICKSEEK, 0);
	pSoundTouch->setSetting(SETTING_USE_AA_FILTER, 0);

	pSoundTouch->setSetting(SETTING_SEQUENCE_MS, EmuConfig.SPU2.SequenceLenMS);
	pSoundTouch->setSetting(SETTING_SEEKWINDOW_MS, EmuConfig.SPU2.SeekWindowMS);
	pSoundTouch->setSetting(SETTING_OVERLAP_MS, EmuConfig.SPU2.OverlapMS);

	pSoundTouch->setTempo(1);

	// some timestretch management vars:

	cTempo = 1.0;
	eTempo = 1.0;
	lastPct = 0;
	lastEmergencyAdj = 0;

	m_predictData = 0;
}

// reset timestretch management vars, and delay updates a bit:
void SndBuffer::soundtouchClearContents()
{
	if (pSoundTouch == nullptr)
		return;

	pSoundTouch->clear();
	pSoundTouch->setTempo(1);

	cTempo = 1.0;
	eTempo = 1.0;
	lastPct = 0;
	lastEmergencyAdj = 0;

	m_predictData = 0;
}

void SndBuffer::soundtouchCleanup()
{
	pSoundTouch.reset();
}
