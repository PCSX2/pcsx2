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

const u32 BufferBlockSize = 128;
const u32 BufferTotalSize = 128 * 32;

class NullOutModule: public SndOutModule
{
	s32  Init(SndBuffer *)  { return 0; }
	void Close() { }
	s32  Test()  { return 0; }
	void Configure(HWND parent)  { }
	bool Is51Out() { return false; }
} NullOut;

SndOutModule* mods[]=
{
	&NullOut,
	WaveOut,
	DSoundOut,
	DSound51Out,
	ASIOOut,
	XAudio2Out,
};

const u32 mods_count=sizeof(mods)/sizeof(SndOutModule*);

//#define DYNAMIC_BUFFER_LIMITING

class SndBufferImpl: public SndBuffer
{
private:
	s32 *buffer;
	s32 size;
	s32 rpos;
	s32 wpos;
	s32 data;

#ifdef DYNAMIC_BUFFER_LIMITING
	s32 buffer_limit;
#endif

	bool pr;
	bool pw;

	int overflows;
	int underflows;
	int writewaits;

	

	bool isWaiting;
	HANDLE hSyncEvent;
	CRITICAL_SECTION cs;

	u32 datawritten;
	u32 dataread;

public:
	SndBufferImpl(s32 _size)
	{
		rpos=0;
		wpos=0;
		data=0;
		size=(_size+1)&(~1);
		buffer = new s32[size];
		pr=false;
		pw=false;
		isWaiting=false;
#ifdef DYNAMIC_BUFFER_LIMITING
		overflows=0;
		underflows=0;
		writewaits=0;
		buffer_limit=size;
#endif
		datawritten=0;
		dataread=0;
		InitializeCriticalSection(&cs);
		hSyncEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
	}

	virtual ~SndBufferImpl()
	{
		pw=false;
		if(isWaiting) PulseEvent(hSyncEvent);
		Sleep(10);
		EnterCriticalSection(&cs);
		LeaveCriticalSection(&cs);
		DeleteCriticalSection(&cs);
		CloseHandle(hSyncEvent);
		delete buffer;
	}

	virtual void WriteSamples(s32 *bData, s32 nSamples)
	{
		EnterCriticalSection(&cs);
		datawritten+=nSamples;
		
#ifdef DYNAMIC_BUFFER_LIMITING
		int free = buffer_limit-data;
#else
		int free = size-data;
#endif

		if(pw)
		{
#ifdef DYNAMIC_BUFFER_LIMITING
			if(free<nSamples)
				writewaits++;
#endif
			while((free<nSamples)&&(pw)) 
			{
				isWaiting=true;
				LeaveCriticalSection(&cs);
				WaitForSingleObject(hSyncEvent,1000);
				EnterCriticalSection(&cs);
#ifdef DYNAMIC_BUFFER_LIMITING
				free = buffer_limit-data;
#else
				free = size-data;
#endif
				isWaiting=false;
			}
		}

		// either pw=false or free>nSamples
		while(nSamples>0)
		{
			buffer[wpos] = *(bData++);
			wpos=(wpos+1)%size;
			data++;
			nSamples--;
		}

		if(data>size)
		{
			do {
				data-=size;
			}
			while(data>size);
#ifdef DYNAMIC_BUFFER_LIMITING
			overflows++;
#endif
		}

		
		LeaveCriticalSection(&cs);
	}

	virtual void ReadSamples (s32 *bData, s32 nSamples)
	{
		EnterCriticalSection(&cs);
		dataread+=nSamples;
		
		while(nSamples>0)
		{
			*(bData++) = buffer[rpos];
			rpos=(rpos+1)%size;
			data--;
			nSamples--;
		}

#ifdef DYNAMIC_BUFFER_LIMITING
		if(data<0)
		{
			do
			{
				data+=buffer_limit;
			}
			while(data<0);
			underflows++;
		}
#else
		while(data<0)
		{
			data+=size;
		}
#endif

		if(isWaiting)
		{
			PulseEvent(hSyncEvent);
		}

#ifdef DYNAMIC_BUFFER_LIMITING

		// will never have BOTH overflows and write waits ;)
		while((overflows>0)&&(underflows>0)) 
		{ overflows--; underflows--; }
		while((writewaits>0)&&(underflows>0)) 
		{ writewaits--; underflows--; }
		int t=buffer_limit;
		if(underflows>0)
		{
			if(buffer_limit<size)
			{
				buffer_limit=min(size,buffer_limit*2);
				underflows=0;
			}
			if(underflows>3) underflows=3;
		}
		if(writewaits>0)
		{
			if(buffer_limit>(3*BufferBlockSize))
			{
				buffer_limit=max(3*BufferBlockSize,buffer_limit*3/4);
				writewaits=0;
			}
			if(writewaits>10) writewaits=10;
		}
		if(overflows>0)
		{
			if(buffer_limit>(3*BufferBlockSize))
			{
				buffer_limit=max(3*BufferBlockSize,buffer_limit*3/4);
				overflows=0;
			}
			if(overflows>3) overflows=3;
		}
		
			//printf(" ** SPU2 Dynamic limiter update: Buffer limit set to %d\n",buffer_limit);
#endif
		
		LeaveCriticalSection(&cs);
	}

	virtual void PauseOnWrite(bool doPause) { pw = doPause; }

	virtual s32  GetBufferUsage()
	{
		return data;
	}

	virtual s32  GetBufferSize()
	{
		return size;
	}

	bool GetStats(u32 &w, u32 &r, bool reset)
	{
		EnterCriticalSection(&cs);
		w = datawritten;
		r = dataread;
		if(reset) { datawritten=dataread=0; }
		LeaveCriticalSection(&cs);
		return true;
	}
} *sndBuffer;
s32* sndTempBuffer;
s32 sndTempProgress;
s32 sndTempSize;
s16* sndTempBuffer16;
float* sndTempBufferFloat;

s32 buffersize=0;

soundtouch::SoundTouch* pSoundTouch=NULL;

u32 inputSamples=0;

u32 oldWritten=0;
u32 oldRead=0;
u32 oldInput=0;

float valAccum1=0;
float valAccum2=0;
float valAccum3=0;
u32   numAccum=1;

const u32 numUpdates = 64;

float lastTempo=1;
float cTempo=1;

void ResetTempoChange()
{
	u32 cWritten;
	u32 cRead;
	u32 cInput = inputSamples;

	sndBuffer->GetStats(cWritten,cRead,false);

	oldWritten=cWritten;
	oldRead=cRead;
	oldInput=cInput;

	pSoundTouch->setTempo(1);
}

void UpdateTempoChange()
{
	u32 cWritten;
	u32 cRead;
	u32 cInput = inputSamples;
 
	s32 bufferUsage = sndBuffer->GetBufferUsage();
	s32 bufferSize  = sndBuffer->GetBufferSize();
 
	bool a=(bufferUsage < BufferBlockSize * 4);
	bool b=(bufferUsage >= (bufferSize - BufferBlockSize * 4));
 
	if(a!=b)
	{
		if     (bufferUsage < BufferBlockSize)	 { cTempo*=0.75f; }
		else if(bufferUsage < BufferBlockSize * 2) { cTempo*=0.90f; }
		else if(bufferUsage < BufferBlockSize * 3) { cTempo*=0.95f; }
		else if(bufferUsage < BufferBlockSize * 4) { cTempo*=0.99f; }
 
		if     (bufferUsage > (bufferSize - BufferBlockSize))     { cTempo*=1.25f; }
		else if(bufferUsage > (bufferSize - BufferBlockSize * 2)) { cTempo*=1.10f; }
		else if(bufferUsage > (bufferSize - BufferBlockSize * 3)) { cTempo*=1.05f; }
		else if(bufferUsage > (bufferSize - BufferBlockSize * 4)) { cTempo*=1.01f; }
 
		pSoundTouch->setTempo(cTempo);
	}
	else
	{
		cTempo = cTempo * 0.9f + lastTempo * 0.1f;
	}

	sndBuffer->GetStats(cWritten,cRead,false);
  
	valAccum1 +=(cRead-oldRead);
	valAccum2 +=(cInput-oldInput);
	numAccum ++;
 
	oldRead=cRead;
	oldInput=cInput;
 
 	if(numAccum>=numUpdates)
	{
		float valAccum = 1.0;

		if (valAccum1!=0)
			valAccum=valAccum2 / valAccum1;
 
		if((valAccum<1.05)&&(valAccum>0.95)&&(valAccum!=1)) 
		{
			printf("Current Output Speed: %f (difference disregarded, using 1.0).\n",valAccum);
			valAccum = 1;
		}
		else
		{
			printf("Current Output Speed: %f\n",valAccum);
		}
 
		pSoundTouch->setTempo(valAccum);
 
		lastTempo =valAccum;
		cTempo=lastTempo;
 
		valAccum1=0;
		valAccum2=0;
		numAccum=0;
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
	if(OutputModule>=mods_count)
		return -1;

	// initialize sound buffer
	sndBuffer = new SndBufferImpl(BufferTotalSize * 2);
	sndTempSize = 512;
	sndTempProgress = 0;
	sndTempBuffer = new s32[sndTempSize];
	sndTempBuffer16 = new s16[sndTempSize];
	sndTempBufferFloat = new float[sndTempSize];
	buffersize=sndBuffer->GetBufferSize();

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
	if(OutputModule>=mods_count)
		return;

	mods[OutputModule]->Close();

	delete sndBuffer;
	delete sndTempBuffer;
	delete sndTempBuffer16;
	delete sndTempBufferFloat;

	delete pSoundTouch;
}

void SndUpdateLimitMode()
{
	sndBuffer->PauseOnWrite(LimitMode!=0);

	if(LimitMode!=0)
		printf(" * SPU2 limiter is now ON.\n");
	else
		printf(" * SPU2 limiter is now OFF.\n");

}

bool SndGetStats(u32 *written, u32 *played)
{
	return sndBuffer->GetStats(*written,*played,false);
}

s32 SndWrite(s32 ValL, s32 ValR)
{
	if(WaveLog && wavedump_ok)
	{
		wavedump_write(ValL>>8,ValR>>8);
	}
 
	if(recording!=0)
		RecordWrite(ValL>>8,ValR>>8);
 
	if(OutputModule>=mods_count)
		return -1;
 
	if(mods[OutputModule] == &NullOut) // null output doesn't need buffering or stretching! :p
		return 0;
 
	inputSamples+=2;
 
	sndTempBuffer[sndTempProgress++] = ValL;
	sndTempBuffer[sndTempProgress++] = ValR;
 
	if(sndTempProgress>=sndTempSize) 
	{
		if(dspPluginEnabled)
		{
			for(int i=0;i<sndTempProgress;i++) { sndTempBuffer16[i] = sndTempBuffer[i]>>8; }
 
			// send to winamp DSP
			sndTempProgress = DspProcess(sndTempBuffer16,sndTempProgress>>1)<<1;
 
			for(int i=0;i<sndTempProgress;i++) { sndTempBuffer[i] = sndTempBuffer16[i]<<8; }
		}
 
		if(timeStretchEnabled)
		{
			for(int i=0;i<sndTempProgress;i++) { sndTempBufferFloat[i] = sndTempBuffer[i]/2147483648.0f; }
 
			// send to timestretcher
			pSoundTouch->putSamples(sndTempBufferFloat, sndTempProgress>>1);
			UpdateTempoChange();
			do
			{
				sndTempProgress = pSoundTouch->receiveSamples(sndTempBufferFloat, sndTempSize>>1)<<1;
 
				if(sndTempProgress>0)
				{
 
					for(int i=0;i<sndTempProgress;i++) { sndTempBuffer[i] = (s32)(sndTempBufferFloat[i]*2147483648.0f); }
					sndBuffer->WriteSamples(sndTempBuffer,sndTempProgress);
				}
 
			} while (sndTempProgress != 0 );
		}
		else
		{
			sndBuffer->WriteSamples(sndTempBuffer,sndTempProgress);
			sndTempProgress=0;
		}
	}
 
	return 1;
}

s32 SndTest()
{
	if(OutputModule>=mods_count)
		return -1;

	return mods[OutputModule]->Test();
}

void SndConfigure(HWND parent)
{
	if(OutputModule>=mods_count)
		return;

	mods[OutputModule]->Configure(parent);
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
