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

class WaveOutModule: public SndOutModule
{
private:

	#define MAX_BUFFER_COUNT 5

	#define BufferSize      (CurBufferSize<<1)
	#define BufferSizeBytes (BufferSize<<1)

	HWAVEOUT hwodevice;
	WAVEFORMATEX wformat;
	WAVEHDR whbuffer[MAX_BUFFER_COUNT];

	s32* tbuffer;
	s16* qbuffer;

	#define QBUFFER(x) (qbuffer + BufferSize * (x))

	bool waveout_running;
	HANDLE thread;
	DWORD tid;

	SndBuffer *buff;

	FILE *voicelog;

	char ErrText[256];

	static DWORD CALLBACK RThread(WaveOutModule*obj)
	{
		return obj->Thread();
	}

	DWORD CALLBACK Thread()
	{
		while( waveout_running )
		{
			bool didsomething = false;
			for(int i=0;i<MAX_BUFFER_COUNT;i++)
			{
				if(!(whbuffer[i].dwFlags & WHDR_DONE) ) continue;

				WAVEHDR *buf=whbuffer+i;

				buf->dwBytesRecorded = buf->dwBufferLength;

				buff->ReadSamples(tbuffer,BufferSize);
				s16 *t = (s16*)buf->lpData;
				s32 *s = (s32*)tbuffer;

				for(int bleh=0;bleh<BufferSize;bleh++)
				{
					*(t++) = (s16)((*(s++))>>8);
				}

				whbuffer[i].dwFlags&=~WHDR_DONE;
				waveOutWrite(hwodevice,buf,sizeof(WAVEHDR));
				didsomething = true;
			}

			if( didsomething )
				Sleep(1);
			else
				Sleep(0);
		}
		return 0;
	}

public:
	s32 Init(SndBuffer *sb)
	{
		buff = sb;

		MMRESULT woores;

		if (Test()) return -1;

		if(CurBufferSize<1024) CurBufferSize=1024;

		wformat.wFormatTag=WAVE_FORMAT_PCM;
		wformat.nSamplesPerSec=SampleRate;
		wformat.wBitsPerSample=16;
		wformat.nChannels=2;
		wformat.nBlockAlign=((wformat.wBitsPerSample * wformat.nChannels) / 8);
		wformat.nAvgBytesPerSec=(wformat.nSamplesPerSec * wformat.nBlockAlign);
		wformat.cbSize=0;
		
		qbuffer=new s16[BufferSize*MAX_BUFFER_COUNT];
		tbuffer=new s32[BufferSize];

		woores = waveOutOpen(&hwodevice,WAVE_MAPPER,&wformat,0,0,0);
		if (woores != MMSYSERR_NOERROR)
		{
			waveOutGetErrorText(woores,(char *)&ErrText,255);
			SysMessage("WaveOut Error: %s",ErrText);
			return -1;
		}

		for(int i=0;i<MAX_BUFFER_COUNT;i++)
		{
			whbuffer[i].dwBufferLength=BufferSizeBytes;
			whbuffer[i].dwBytesRecorded=BufferSizeBytes;
			whbuffer[i].dwFlags=0;
			whbuffer[i].dwLoops=0;
			whbuffer[i].dwUser=0;
			whbuffer[i].lpData=(LPSTR)QBUFFER(i);
			whbuffer[i].lpNext=0;
			whbuffer[i].reserved=0;
			waveOutPrepareHeader(hwodevice,whbuffer+i,sizeof(WAVEHDR));
			whbuffer[i].dwFlags|=WHDR_DONE; //avoid deadlock

			// Feed blocks into the device.
			// It'll all be empty samples, but it helps reduce some of the pop-on-init.

			//whbuffer[i].dwFlags&=~WHDR_DONE;
			//waveOutWrite(hwodevice,&whbuffer[i],sizeof(WAVEHDR));
		}

		// Start Thread
		// [Air]: The waveout code does not use wait objects, so setting a time critical
		// priority level is a bad idea.  Standard priority will do fine.  The buffer will get the
		// love it needs and won't suck resources idling pointlessly.
		waveout_running=true;
		thread=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)RThread,this,0,&tid);
		//SetThreadPriority( thread, THREAD_PRIORITY_TIME_CRITICAL );

		return 0;
	}

	void Close() 
	{
		// Stop Thread
		fprintf(stderr," * SPU2: Waiting for waveOut thread to finish...");
		waveout_running=false;
			
		WaitForSingleObject(thread,INFINITE);
		CloseHandle(thread);

		fprintf(stderr," Done.\n");

		//
		// Clean up
		//
		waveOutReset(hwodevice);
		for(int i=0;i<MAX_BUFFER_COUNT;i++)
		{
			waveOutUnprepareHeader(hwodevice,&whbuffer[i],sizeof(WAVEHDR));
		}
		waveOutClose(hwodevice);

		delete tbuffer;
		delete qbuffer;
	}


	virtual void Configure(HWND parent)
	{
	}

	virtual bool Is51Out() { return false; }

	s32 Test() {
		if (waveOutGetNumDevs() == 0) {
			SysMessage("No waveOut Devices Present\n"); return -1;
		}
		return 0;
	}
} WO;

SndOutModule *WaveOut=&WO;
