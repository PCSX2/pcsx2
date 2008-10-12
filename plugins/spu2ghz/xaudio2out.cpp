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
#define _WIN32_DCOM
#include "spu2.h"
#include <windows.h>
#include <xaudio2.h>
#include <strsafe.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <conio.h>

class XAudio2Mod: public SndOutModule
{
private:

//#define PI 3.14159265f

#define BufferSize      (CurBufferSize<<1)
#define BufferSizeBytes (BufferSize<<1)

	s32* tbuffer;
	s16* qbuffer;

	s32 out_num;

#define QBUFFER(num) (qbuffer+(BufferSize*(num)))

	SndBuffer *buff;

	bool xaudio2_running;
	HANDLE thread;
	DWORD tid;

	//--------------------------------------------------------------------------------------
	// Helper macros
	//--------------------------------------------------------------------------------------
#ifndef SAFE_DELETE_ARRAY
#	define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=NULL; } }
#endif
#ifndef SAFE_RELEASE
#	define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#endif

#define MAX_BUFFER_COUNT 3

	//--------------------------------------------------------------------------------------
	// Callback structure
	//--------------------------------------------------------------------------------------
	struct StreamingVoiceContext : public IXAudio2VoiceCallback
	{
		STDMETHOD_(void, OnVoiceProcessingPassStart) () {}
		STDMETHOD_(void, OnVoiceProcessingPassStart) (UINT32) { };
		STDMETHOD_(void, OnVoiceProcessingPassEnd) () {}
		STDMETHOD_(void, OnStreamEnd) () {}
		STDMETHOD_(void, OnBufferStart) ( void* ) {}
		STDMETHOD_(void, OnBufferEnd) ( void* ) { SetEvent( hBufferEndEvent ); }
		STDMETHOD_(void, OnLoopEnd) ( void* ) {}   
		STDMETHOD_(void, OnVoiceError) (THIS_ void* pBufferContext, HRESULT Error) { };

		HANDLE hBufferEndEvent;

		StreamingVoiceContext(): hBufferEndEvent( CreateEvent( NULL, FALSE, FALSE, NULL ) ){}
		~StreamingVoiceContext(){ CloseHandle( hBufferEndEvent ); }
	} voiceContext;

	IXAudio2* pXAudio2;
	IXAudio2MasteringVoice* pMasteringVoice;
	IXAudio2SourceVoice* pSourceVoice;

	WAVEFORMATEX wfx;


	static DWORD CALLBACK RThread(XAudio2Mod*obj)
	{
		return obj->Thread();
	}

	DWORD CALLBACK Thread()
	{
		while( xaudio2_running )
		{
			XAUDIO2_VOICE_STATE state;
			while( pSourceVoice->GetState( &state ), state.BuffersQueued >= MAX_BUFFER_COUNT - 1)
			{
				WaitForSingleObject( voiceContext.hBufferEndEvent, INFINITE );
			}

			s16 *qb=QBUFFER(out_num);
			out_num=(out_num+1)%MAX_BUFFER_COUNT;

			XAUDIO2_BUFFER buf = {0};
			buff->ReadSamples(tbuffer,BufferSize);

			buf.AudioBytes = BufferSizeBytes;
			s16 *t = qb;
			s32 *s = (s32*)tbuffer;
			for(int i=0;i<BufferSize;i++)
			{
				*(t++) = (s16)((*(s++))>>8);
			}

			buf.pAudioData=(const BYTE*)qb;

			pSourceVoice->SubmitSourceBuffer( &buf );
		}
		return 0;
	}

public:
	s32  Init(SndBuffer *sb)
	{
		HRESULT hr;

		buff=sb;

		//
		// Initialize XAudio2
		//
		CoInitializeEx( NULL, COINIT_MULTITHREADED );

		UINT32 flags = 0;
#ifdef _DEBUG
		flags |= XAUDIO2_DEBUG_ENGINE;
#endif

		if ( FAILED(hr = XAudio2Create( &pXAudio2, flags ) ) )
		{
			SysMessage( "Failed to init XAudio2 engine: %#X\n", hr );
			CoUninitialize();
			return -1;
		}

		//
		// Create a mastering voice
		//
		if ( FAILED(hr = pXAudio2->CreateMasteringVoice( &pMasteringVoice ) ) )
		{
			SysMessage( "Failed creating mastering voice: %#X\n", hr );
			SAFE_RELEASE( pXAudio2 );
			CoUninitialize();
			return -1;
		}

		wfx.wFormatTag = WAVE_FORMAT_PCM;
		wfx.nSamplesPerSec = SampleRate;
		wfx.nChannels=2;
		wfx.wBitsPerSample = 16;
		wfx.nBlockAlign = 2*2;
		wfx.nAvgBytesPerSec = SampleRate * 2 * 2;
		wfx.cbSize=0;

		//
		// Create an XAudio2 voice to stream this wave
		//
		if( FAILED(hr = pXAudio2->CreateSourceVoice( &pSourceVoice, &wfx, 0, 1.0f, &voiceContext ) ) )
		{
			SysMessage( "Error %#X creating source voice\n", hr );
			SAFE_RELEASE( pXAudio2 );
			return -1;
		}
		pSourceVoice->Start( 0, 0 );

		tbuffer = new s32[BufferSize];
		qbuffer = new s16[BufferSize*MAX_BUFFER_COUNT];
		ZeroMemory(qbuffer,BufferSize*MAX_BUFFER_COUNT);

		// Start Thread
		xaudio2_running=true;
		thread=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)RThread,this,0,&tid);

		if(thread==INVALID_HANDLE_VALUE) return -1;

		SetThreadPriority(thread,THREAD_PRIORITY_TIME_CRITICAL);

		return 0;
	}

	void Close()
	{
		// Stop Thread
		fprintf(stderr," * SPU2: Waiting for XAudio2 thread to finish...");
		xaudio2_running=false;
			
		WaitForSingleObject(thread,INFINITE);
		CloseHandle(thread);

		fprintf(stderr," Done.\n");

		//
		// Clean up
		//
		pSourceVoice->Stop( 0 );
		pSourceVoice->DestroyVoice();

		Sleep(100);

		//
		// Cleanup XAudio2
		//

		// All XAudio2 interfaces are released when the engine is destroyed, but being tidy
		pMasteringVoice->DestroyVoice();

		SAFE_RELEASE( pXAudio2 );
		CoUninitialize();

		delete tbuffer;
	}

	virtual void Configure(HWND parent)
	{
	}

	virtual bool Is51Out() { return false; }

	s32  Test()
	{
		return 0;
	}
} XA2;

SndOutModule *XAudio2Out=&XA2;
