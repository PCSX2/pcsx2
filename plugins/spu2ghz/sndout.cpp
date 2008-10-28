//GiGaHeRz's SPU2 Driver
//Copyright (c) 2003-2008, David Quintana <gigaherz@gmail.com>
//
//This library is free software; you can redistribute it and/or
//modify it under the terms of the GNU Lesser General Public
//License as published by the Free Software Foundation; either
//version 2.1 of the License, or (at your option) any later version.
//
//This library is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//Lesser General Public License for more details.
//
//You should have received a copy of the GNU Lesser General Public
//License along with this library; if not, write to the Free Software
//Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
#include "spu2.h"
#include "SoundTouch/SoundTouch.h"
#include "SoundTouch/WavFile.h"

static int ts_stats_stretchblocks = 0;
static int ts_stats_normalblocks = 0;
static int ts_stats_logcounter = 0;

class NullOutModule: public SndOutModule
{
public:
	s32  Init(SndBuffer *)  { return 0; }
	void Close() { }
	s32  Test() const { return 0; }
	void Configure(HWND parent)  { }
	bool Is51Out() const { return false; }
	int GetEmptySampleCount() const { return 0; }
	
	const char* GetIdent() const
	{
		return "nullout";
	}

	const char* GetLongName() const
	{
		return "No Sound (Emulate SPU2 only)";
	}

} NullOut;

SndOutModule* mods[]=
{
	&NullOut,
	WaveOut,
	DSoundOut,
	DSound51Out,
	ASIOOut,
	XAudio2Out,
	NULL		// signals the end of our list
};

int FindOutputModuleById( const char* omodid )
{
	int modcnt = 0;
	while( mods[modcnt] != NULL )
	{
		if( strcmp( mods[modcnt]->GetIdent(), omodid ) == 0 )
			break;
		++modcnt;
	}
	return modcnt;
}


// Overall master volume shift.
// Converts the mixer's 32 bit value into a 16 bit value.
int SndOutVolumeShift = SndOutVolumeShiftBase + 1;

static __forceinline s16 SndScaleVol( s32 inval )
{
	return inval >> SndOutVolumeShift;
}


// records last buffer status (fill %, range -100 to 100, with 0 being 50% full)
double lastPct;

float cTempo=1;

soundtouch::SoundTouch* pSoundTouch=NULL;


//usefull when timestretch isn't available 
//#define DYNAMIC_BUFFER_LIMITING

class SndBufferImpl: public SndBuffer
{
private:
	s32 *buffer;
	s32 size;
	s32 rpos;
	s32 wpos;
	s32 data;

	// data prediction amount, used to "commit" data that hasn't
	// finished timestretch processing.
	s32 predictData;

	bool pw;

	bool underrun_freeze;
	HANDLE hSyncEvent;
	CRITICAL_SECTION cs;

protected:
	int GetAlignedBufferSize( int comp )
	{
		return (comp + SndOutPacketSize-1) & ~(SndOutPacketSize-1);
	}

public:
	SndBufferImpl( double latencyMS )
	{
		rpos=0;
		wpos=0;
		data=0;
		size=GetAlignedBufferSize( (int)(latencyMS * SampleRate / 500.0 ) );
		buffer = new s32[size];
		pw=false;
		underrun_freeze = false;
		predictData = 0;

		lastPct = 0.0;

#ifdef DYNAMIC_BUFFER_LIMITING
		overflows=0;
		underflows=0;
		writewaits=0;
		buffer_limit=size;
#endif
		InitializeCriticalSection(&cs);
		hSyncEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
	}

	virtual ~SndBufferImpl()
	{
		pw=false;
		PulseEvent(hSyncEvent);
		Sleep(10);
		EnterCriticalSection(&cs);
		LeaveCriticalSection(&cs);
		DeleteCriticalSection(&cs);
		CloseHandle(hSyncEvent);
		delete buffer;
	}

	virtual void WriteSamples(s32 *bData, int nSamples)
	{
		EnterCriticalSection(&cs);
		int free = size-data;
		predictData = 0;

		jASSUME( data <= size );

		if( pw && ( free < nSamples ) )
		{
			// Wait for a ReadSamples to pull some stuff out of the buffer.
			// One SyncEvent will do the trick.
			ResetEvent( hSyncEvent );
			LeaveCriticalSection(&cs);
			WaitForSingleObject(hSyncEvent,20);
			EnterCriticalSection(&cs);
		}

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
		
		if( free < nSamples )
		{
			// Buffer overrun!
			// Dump samples from the read portion of the buffer instead of dropping
			// the newly written stuff.

			// Toss half the buffer plus whatever's being written anew:
			s32 comp = GetAlignedBufferSize( (size + nSamples ) / 2 );
			if( comp > (size-SndOutPacketSize) ) comp = size-SndOutPacketSize;

			if( timeStretchEnabled )
			{
				// If we overran it means the timestretcher failed.  We need to speed
				// up audio playback.

				cTempo += cTempo * 1.5f;
				if( cTempo > 5.0f ) cTempo = 5.0f;
				pSoundTouch->setTempo( cTempo );
			}

			data-=comp;
			rpos=(rpos+comp)%size;
			if( MsgOverruns() )
				ConLog(" * SPU2 > Overrun Compensation (%d packets tossed)\n", comp / SndOutPacketSize );
			lastPct = 0.0;		// normalize the timestretcher
		}

		// copy in two phases, since there's a chance the packet
		// wraps around the buffer (it'd be nice to deal in packets only, but
		// the timestretcher and DSP options require flexibility).

		const int endPos = wpos + nSamples;
		const int secondCopyLen = endPos - size;
		s32* wposbuffer = &buffer[wpos];

		data += nSamples;
		if( secondCopyLen > 0 )
		{
			nSamples -= secondCopyLen;
			memcpy( buffer, &bData[nSamples], secondCopyLen * sizeof( *bData ) );
			wpos = secondCopyLen;
		}
		else
			wpos += nSamples;

		memcpy( wposbuffer, bData, nSamples * sizeof( *bData ) );
		
		LeaveCriticalSection(&cs);
	}

	protected:
	// Returns TRUE if there is data to be output, or false if no data
	// is available to be copied.
	bool CheckUnderrunStatus( int& nSamples, int& quietSampleCount )
	{
		quietSampleCount = 0;
		if( data < nSamples )
		{
			nSamples = data;
			quietSampleCount = SndOutPacketSize - data;
			underrun_freeze = true;

			if( timeStretchEnabled )
			{
				// timeStretcher failed it's job.  We need to slow down the audio some.

				cTempo -= (cTempo * 0.25f);
				if( cTempo < 0.2f ) cTempo = 0.2f;
				pSoundTouch->setTempo( cTempo );
			}

			return nSamples != 0;
		}
		else if( underrun_freeze )
		{			
			int toFill = (int)(size * ( timeStretchEnabled ? 0.45 : 0.70 ) );
			toFill = GetAlignedBufferSize( toFill );

			// toFill is now aligned to a SndOutPacket

			if( data < toFill )
			{
				quietSampleCount = nSamples;
				return false;
			}

			underrun_freeze = false;
			if( MsgOverruns() )
				ConLog(" * SPU2 > Underrun compensation (%d packets buffered)\n", toFill / SndOutPacketSize );
			lastPct = 0.0;		// normalize timestretcher
		}
		return true;
	}

	public:
	void ReadSamples( s16* bData )
	{
		int nSamples = SndOutPacketSize;

		EnterCriticalSection(&cs);
		
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
		if( CheckUnderrunStatus( nSamples, quietSamples ) )
		{
			jASSUME( nSamples <= SndOutPacketSize );

			// [Air] [TODO]: This loop is probably a candidiate for SSE2 optimization.

			const int endPos = rpos + nSamples;
			const int secondCopyLen = endPos - size;
			const s32* rposbuffer = &buffer[rpos];

			data -= nSamples;

			if( secondCopyLen > 0 )
			{
				nSamples -= secondCopyLen;
				for( int i=0; i<secondCopyLen; i++ )
					bData[nSamples+i] = SndScaleVol( buffer[i] );
				rpos = secondCopyLen;
			}
			else
				rpos += nSamples;

			for( int i=0; i<nSamples; i++ )
				bData[i] = SndScaleVol( rposbuffer[i] );
		}

		// If quietSamples != 0 it means we have an underrun...
		// Let's just dull out some silence, because that's usually the least
		// painful way of dealing with underruns:
		memset( bData, 0, quietSamples * sizeof(*bData) );
		SetEvent( hSyncEvent );
		LeaveCriticalSection(&cs);
	}

	void ReadSamples( s32* bData )
	{
		int nSamples = SndOutPacketSize;

		EnterCriticalSection(&cs);
		
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
		if( CheckUnderrunStatus( nSamples, quietSamples ) )
		{
			// nSamples is garaunteed non-zero if CheckUnderrunStatus
			// returned true.

			const int endPos = rpos + nSamples;
			const int secondCopyLen = endPos - size;
			const int oldrpos = rpos;

			data -= nSamples;

			if( secondCopyLen > 0 )
			{
				nSamples -= secondCopyLen;
				memcpy( &bData[nSamples], buffer, secondCopyLen * sizeof( *bData ) );
				rpos = secondCopyLen;
			}
			else
				rpos += nSamples;

			memcpy( bData, &buffer[oldrpos], nSamples * sizeof( *bData ) );
		}

		// If quietSamples != 0 it means we have an underrun...
		// Let's just dull out some silence, because that's usually the least
		// painful way of dealing with underruns:
		memset( bData, 0, quietSamples * sizeof(*bData) );
		PulseEvent(hSyncEvent);
		LeaveCriticalSection(&cs);
	}

	void PredictDataWrite( int samples )
	{
		predictData += samples;
	}

	virtual void PauseOnWrite(bool doPause) { pw = doPause; }

	// Calculate the buffer status percentage.
	// Returns range from -1.0 to 1.0
	//    1.0 = buffer overflow!
	//    0.0 = buffer nominal (50% full)
	//   -1.0 = buffer underflow!
	double GetStatusPct()
	{
		EnterCriticalSection(&cs);

		// Get the buffer status of the output driver too, so that we can
		// obtain a more accurate overall buffer status.

		int drvempty = mods[OutputModule]->GetEmptySampleCount() / 2;

		double result = (data + predictData - drvempty) - (size/2);
		result /= (size/2);
		LeaveCriticalSection(&cs);
		return result;
	}

};

SndBufferImpl *sndBuffer;

s32* sndTempBuffer;
s32 sndTempProgress;
s16* sndTempBuffer16;
//float* sndTempBufferFloat;

void ResetTempoChange()
{
	pSoundTouch->setTempo(1);
}

void UpdateTempoChange()
{
	double statusPct = sndBuffer->GetStatusPct();
	double pctChange = statusPct - lastPct;

	double tempoChange;

	// We have two factors.
	//   * Distance from nominal buffer status (50% full)
	//   * The change from previous update to this update.

	// The most important factor is the change from update to update.
	// But the synchronization between emulator, mixer, and audio driver
	// is rarely consistent so drifting away from nominal buffer status (50%)
	// is inevitable.  So we need to use the nominal buffer status to
	// help temper things.

	double relation = statusPct / pctChange;

	if( relation < 0.0 )
	{
		// The buffer is already shrinking toward
		// nominal value, so let's not do "too much"
		// We only want to adjust if the shrink rate seems too fast
		// or slow compared to our distance from nominal (50%).

		tempoChange = ( pow( statusPct, 3.0 ) * 0.33 ) + pctChange * 0.23;
	}
	else
	{
		tempoChange = pctChange * 0.30;

		// Sudden spikes in framerate can cause the nominal buffer status
		// to go critical, in which case we have to enact an emergency
		// stretch. The following cubic formula does that.

		// Constants:
		// Weight - weights the statusPct's "emergency" consideration.
		//   higher values here will make the buffer perform more drastic
		//   compensations.

		// Range - scales the adjustment to the given range (more or less).
		//   The actual range is dependent on the weight used, so if you increase
		//   Weight you'll usually want to decrease Range somewhat to compensate.

		const double weight = 1.55;
		const double range = 0.12;

		double nominalAdjust = (statusPct * 0.10) + ( pow( statusPct*weight, 3.0 ) * range);

		tempoChange = tempoChange + nominalAdjust;
	}

	// Threshold - Ignore small values between -.005 and +.005.
	// We don't need to "pollute" our timestretcher with pointless
	// tempo change overhead.
	if( abs( tempoChange ) < 0.005 ) return;

	lastPct = statusPct;

	// Apply tempoChange as a scale of cTempo.  That way the effect is proportional
	// to the current tempo.  (otherwise tempos would change too slowly/quickly at the extremes)
	cTempo += (float)( tempoChange * cTempo );
	
	if( statusPct < -0.20 || statusPct > 0.20 || cTempo < 0.980 || cTempo > 1.020 )
	{
		if( cTempo < 0.20f ) cTempo = 0.20f;
		else if( cTempo > 5.0f ) cTempo = 5.0f;
		pSoundTouch->setTempo( cTempo );
		ts_stats_stretchblocks++;

		ConLog(" %s * SPU2: TempoChange by %d%% (tempo: %d%%) (buffer: %d%%)\n",
			(relation < 0.0) ? "Normalize" : "",
			(int)(tempoChange * 100.0), (int)(cTempo * 100.0),
			(int)(statusPct * 100.0)
		);
	}
	else
	{
		// Nominal operation -- turn off stretching.
		pSoundTouch->setTempo( 1.0f );
		ts_stats_normalblocks++;
	}
}


void soundtouchInit() {
	pSoundTouch = new soundtouch::SoundTouch();
	pSoundTouch->setSampleRate(SampleRate);
    pSoundTouch->setChannels(2);

    pSoundTouch->setSetting(SETTING_USE_QUICKSEEK, 0);
    pSoundTouch->setSetting(SETTING_USE_AA_FILTER, 0);
}

s32 SndInit()
{
	if( mods[OutputModule] == NULL )
	{
		// force us to the NullOut module if nothing assigned.
		OutputModule = FindOutputModuleById( NullOut.GetIdent() );
	}

	// initialize sound buffer
	// Buffer actually attempts to run ~50%, so allocate near double what
	// the requested latency is:
	sndBuffer = new SndBufferImpl( SndOutLatencyMS * 1.75 );
	sndTempProgress = 0;
	sndTempBuffer = new s32[SndOutPacketSize];
	sndTempBuffer16 = new s16[SndOutPacketSize];
	//sndTempBufferFloat = new float[sndTempSize];

	cTempo = 1.0;
	soundtouchInit();

	ResetTempoChange();

	if(LimitMode!=0)
	{
		sndBuffer->PauseOnWrite(true);
	}

	// some crap
	spdif_set51(mods[OutputModule]->Is51Out());

	// initialize module
	return mods[OutputModule]->Init(sndBuffer);
}

void SndClose()
{
	mods[OutputModule]->Close();

	delete sndBuffer;
	delete sndTempBuffer;
	delete sndTempBuffer16;

	delete pSoundTouch;
}

void SndUpdateLimitMode()
{
	//sndBuffer->PauseOnWrite(LimitMode!=0);

	if(LimitMode!=0) {
		timeStretchEnabled = true;
		//printf(" * SPU2 limiter is now ON.\n");
		printf(" * SPU2 timestretch is now ON.\n");
	}
	else {
		//printf(" * SPU2 limiter is now OFF.\n");
		printf(" * SPU2 timestretch is now OFF.\n");
		timeStretchEnabled = false;
	}

}


s32 SndWrite(s32 ValL, s32 ValR)
{
	#ifndef PUBLIC
	if(WaveLog() && wavedump_ok)
	{
		wavedump_write(SndScaleVol(ValL),SndScaleVol(ValR));
	}
	#endif

	if(recording!=0)
		RecordWrite(SndScaleVol(ValL),SndScaleVol(ValR));
 
	if(mods[OutputModule] == &NullOut) // null output doesn't need buffering or stretching! :p
		return 0;
 
	//inputSamples+=2;
 
	sndTempBuffer[sndTempProgress++] = ValL;
	sndTempBuffer[sndTempProgress++] = ValR;
 
	// If we haven't accumulated a full packet yet, do nothing more:
	if(sndTempProgress < SndOutPacketSize) return 1;

	if(dspPluginEnabled)
	{
		for(int i=0;i<SndOutPacketSize;i++) { sndTempBuffer16[i] = SndScaleVol( sndTempBuffer[i] ); }

		// send to winamp DSP
		sndTempProgress = DspProcess(sndTempBuffer16,sndTempProgress>>1)<<1;

		for(int i=0;i<sndTempProgress;i++) { sndTempBuffer[i] = sndTempBuffer16[i]<<SndOutVolumeShift; }
	}

	static int equalized = 0;
	if(timeStretchEnabled)
	{
		bool progress = false;

		// data prediction helps keep the tempo adjustments more accurate.
		sndBuffer->PredictDataWrite( (int)( sndTempProgress / cTempo ) );
		for(int i=0;i<sndTempProgress;i++) { ((float*)sndTempBuffer)[i] = sndTempBuffer[i]/2147483648.0f; }

		pSoundTouch->putSamples((float*)sndTempBuffer, sndTempProgress>>1);

		while( ( sndTempProgress = pSoundTouch->receiveSamples((float*)sndTempBuffer, sndTempProgress>>1)<<1 ) != 0 )
		{
			// The timestretcher returns packets in belated "clump" form.
			// Meaning that most of the time we'll get nothing back, and then
			// suddenly we'll get several chunks back at once.  That's
			// why we only update the tempo below after a set of blocks has been
			// released (otherwise the tempo rates will be skewed by backlogged data)
			
			// [Air] [TODO] : Implement an SSE downsampler to int.
			for(int i=0;i<sndTempProgress;i++)
			{
				sndTempBuffer[i] = (s32)(((float*)sndTempBuffer)[i]*2147483648.0f);
			}
			sndBuffer->WriteSamples(sndTempBuffer, sndTempProgress);
			progress = true;
		}

		if( progress )
		{
			UpdateTempoChange();

			if( MsgOverruns() )
			{
				if( ++ts_stats_logcounter > 300 )
				{
					ts_stats_logcounter = 0;
					ConLog( " * SPU2 > Timestretch Stats > %d%% of packets stretched.\n",
						( ts_stats_stretchblocks * 100 ) / ( ts_stats_normalblocks + ts_stats_stretchblocks ) );
					ts_stats_normalblocks = 0;
					ts_stats_stretchblocks = 0;
				}
			}
		}
	}
	else
	{
		sndBuffer->WriteSamples(sndTempBuffer, sndTempProgress);
		sndTempProgress=0;
	}

	return 1;
}

s32 SndTest()
{
	if( mods[OutputModule] == NULL )
		return -1;

	return mods[OutputModule]->Test();
}

void SndConfigure(HWND parent, u32 module )
{
	if( mods[module] == NULL )
		return;

	mods[module]->Configure(parent);
}

#if 0
//////////////////////////////////////////////////////////////
// Basic Timestretcher (50% to 150%)
const s32 StretchBufferSize = 2048;

s32 stretchBufferL[StretchBufferSize*2];
s32 stretchBufferR[StretchBufferSize*2];
s32 stretchPosition=0;

s32 stretchOutputSize = 2048; // valid values from 1024 to 3072

s32 blah;

extern float cspeed;
void TimestretchUpdate(int bufferusage,int buffersize)
{
	if(cspeed>1.01)
	{
		stretchOutputSize+=10;
	}
	else if (cspeed<0.99)
	{
		stretchOutputSize-=10;
	}

	blah++;
	if(blah>=2)
	{
		blah=0;

		printf(" * Stretch = %d of %d\n",stretchOutputSize,StretchBufferSize);
	}
}

s32 SndWriteStretch(s32 ValL, s32 ValR)
{
	// TODO: update stretchOutputSize according to speed :P

	stretchBufferL[stretchPosition] = ValL;
	stretchBufferR[stretchPosition] = ValR;

	stretchPosition++;
	if(stretchPosition>=StretchBufferSize)
	{
		stretchPosition=0;

		if(stretchOutputSize < (StretchBufferSize/2))
			stretchOutputSize=(StretchBufferSize/2);
		if(stretchOutputSize > (StretchBufferSize*3/2))
			stretchOutputSize=(StretchBufferSize*3/2);

		if(stretchOutputSize>StretchBufferSize)
		{
			int K = (stretchOutputSize-StretchBufferSize);
			int J = StretchBufferSize - K;

			// K samples offset
			for(int i=StretchBufferSize;i<stretchOutputSize;i++)
			{
				stretchBufferL[i+K]=stretchBufferL[i];
				stretchBufferR[i+K]=stretchBufferR[i];
			}

			// blend along J samples from K to stretchbuffersize
			for(int i=K;i<StretchBufferSize;i++)
			{
				int QL = stretchBufferL[i-K] - stretchBufferL[i];
				stretchBufferL[i] = stretchBufferL[i] + MulDiv(QL,(i-K),J);

				int QR = stretchBufferR[i-K] - stretchBufferR[i];
				stretchBufferR[i] = stretchBufferR[i] + MulDiv(QR,(i-K),J);
			}

		}
		else if( stretchOutputSize < StretchBufferSize)
		{
			int K = (StretchBufferSize-stretchOutputSize);

			// blend along K samples from 0 to stretchoutputsize
			for(int i=0;i<stretchOutputSize;i++)
			{
				int QL = stretchBufferL[i+K] - stretchBufferL[i];
				stretchBufferL[i] = stretchBufferL[i] + MulDiv(QL,i,stretchOutputSize);

				int QR = stretchBufferR[i+K] - stretchBufferR[i];
				stretchBufferR[i] = stretchBufferR[i] + MulDiv(QR,i,stretchOutputSize);
			}
		}

		int K=stretchOutputSize; // stretchOutputSize might be modified in the middle of writing!
		for(int i=0;i<K;i++)
		{
			int t = SndWriteOut(stretchBufferL[i],stretchBufferR[i]);
			if(t) return t;
		}
	}
	return 0;
}
#endif
