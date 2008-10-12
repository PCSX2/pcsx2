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
#include "dialogs.h"
#include <initguid.h>
#include <windows.h>
#include <dsound.h>
#include <strsafe.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <conio.h>
#include <assert.h>

struct ds_device_data {
	char name[256];
	GUID guid;
	bool hasGuid;
} devices[32];
int ndevs;
GUID DevGuid;
bool haveGuid;

HRESULT GUIDFromString(const char *str, LPGUID guid)
{
	// "{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}"

	struct T{	// this is a hack because for some reason sscanf writes too much :/
		GUID g;
		int k;
	} t;

	int r = sscanf_s(str,"{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
		&t.g.Data1,
		&t.g.Data2,
		&t.g.Data3,
		&t.g.Data4[0],
		&t.g.Data4[1],
		&t.g.Data4[2],
		&t.g.Data4[3],
		&t.g.Data4[4],
		&t.g.Data4[5],
		&t.g.Data4[6],
		&t.g.Data4[7]
	);

	if(r!=11) return -1;

	*guid = t.g;
	return 0;
}

class DSound: public SndOutModule
{
private:
#	define PI 3.14159265f

#	define BufferSize      (CurBufferSize<<1)
#	define BufferSizeBytes (BufferSize<<1)

#	define TBufferSize     (BufferSize*CurBufferCount)

	FILE *voicelog;

	int channel;

	bool dsound_running;
	HANDLE thread;
	DWORD tid;

#	define MAX_BUFFER_COUNT 5

	IDirectSound8* dsound;
	IDirectSoundBuffer8* buffer;
	IDirectSoundNotify8* buffer_notify;
	HANDLE buffer_events[MAX_BUFFER_COUNT];

	WAVEFORMATEX wfx;

	HANDLE waitEvent;

	SndBuffer *buff;

	s32 *tbuffer;

#	define STRFY(x) #x

#	define verifyc(x) if(Verifyc(x,STRFY(x))) return -1;

	int __forceinline Verifyc(HRESULT hr,const char* fn)
	{
		if(FAILED(hr))
		{
			SysMessage("ERROR: Call to %s Failed.",fn);
			return -1;
		}
		return 0;
	}

	static DWORD CALLBACK RThread(DSound*obj)
	{
		return obj->Thread();
	}

	DWORD CALLBACK Thread()
	{
		while( dsound_running )
		{
			u32 rv = WaitForMultipleObjects(MAX_BUFFER_COUNT,buffer_events,FALSE,400);
	 
			LPVOID p1,p2;
			DWORD s1,s2;
	 
			for(int i=0;i<MAX_BUFFER_COUNT;i++)
			{
				if (rv==WAIT_OBJECT_0+i)
				{
					u32 poffset=BufferSizeBytes * i;

					buff->ReadSamples(tbuffer,BufferSize);

					verifyc(buffer->Lock(poffset,BufferSizeBytes,&p1,&s1,&p2,&s2,0));
					s16 *t = (s16*)p1;
					s32 *s = (s32*)tbuffer;
					for(int i=0;i<BufferSize;i++)
					{
						*(t++) = (s16)((*(s++))>>8);
					}
					verifyc(buffer->Unlock(p1,s1,p2,s2));

				}
			}
		}
		return 0;
	}

public:
	s32 Init(SndBuffer *sb)
	{
		buff = sb;

		//
		// Initialize DSound
		//
		GUID cGuid;

		if((strlen(DSoundDevice)>0)&&(!FAILED(GUIDFromString(DSoundDevice,&cGuid))))
		{
			verifyc(DirectSoundCreate8(&cGuid,&dsound,NULL));
		}
		else
		{
			verifyc(DirectSoundCreate8(NULL,&dsound,NULL));
		}

		verifyc(dsound->SetCooperativeLevel(GetDesktopWindow(),DSSCL_PRIORITY));
		IDirectSoundBuffer* buffer_;
 		DSBUFFERDESC desc; 
	 
		// Set up WAV format structure. 
	 
		memset(&wfx, 0, sizeof(WAVEFORMATEX)); 
		wfx.wFormatTag = WAVE_FORMAT_PCM;
		wfx.nSamplesPerSec = SampleRate;
		wfx.nChannels=2;
		wfx.wBitsPerSample = 16;
		wfx.nBlockAlign = 2*2;
		wfx.nAvgBytesPerSec = SampleRate * 2 * 2;
		wfx.cbSize=0;
	 
		// Set up DSBUFFERDESC structure. 
	 
		memset(&desc, 0, sizeof(DSBUFFERDESC)); 
		desc.dwSize = sizeof(DSBUFFERDESC); 
		desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLPOSITIONNOTIFY;// _CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY; 
		desc.dwBufferBytes = BufferSizeBytes * MAX_BUFFER_COUNT; 
		desc.lpwfxFormat = &wfx; 
	 
		desc.dwFlags |=DSBCAPS_LOCSOFTWARE;
		desc.dwFlags|=DSBCAPS_GLOBALFOCUS;
	 
		verifyc(dsound->CreateSoundBuffer(&desc,&buffer_,0));
		verifyc(buffer_->QueryInterface(IID_IDirectSoundBuffer8,(void**)&buffer));
		buffer_->Release();
	 
		verifyc(buffer->QueryInterface(IID_IDirectSoundNotify8,(void**)&buffer_notify));

		DSBPOSITIONNOTIFY not[MAX_BUFFER_COUNT];
	 
		for(int i=0;i<MAX_BUFFER_COUNT;i++)
		{
			buffer_events[i]=CreateEvent(NULL,FALSE,FALSE,NULL);
			not[i].dwOffset=(wfx.nBlockAlign*10 + BufferSizeBytes*(i+1))%desc.dwBufferBytes;
			not[i].hEventNotify=buffer_events[i];
		}
	 
		buffer_notify->SetNotificationPositions(MAX_BUFFER_COUNT,not);
	 
		LPVOID p1=0,p2=0;
		DWORD s1=0,s2=0;
	 
		verifyc(buffer->Lock(0,desc.dwBufferBytes,&p1,&s1,&p2,&s2,0));
		assert(p2==0);
		memset(p1,0,s1);
		verifyc(buffer->Unlock(p1,s1,p2,s2));
	 
		//Play the buffer !
		verifyc(buffer->Play(0,0,DSBPLAY_LOOPING));

		tbuffer = new s32[BufferSize];

		// Start Thread
		dsound_running=true;
			thread=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)RThread,this,0,&tid);
		SetThreadPriority(thread,THREAD_PRIORITY_TIME_CRITICAL);
 

		return 0;
	}

	void Close()
	{

		// Stop Thread
		fprintf(stderr," * SPU2: Waiting for DSound thread to finish...");
		dsound_running=false;
			
		WaitForSingleObject(thread,INFINITE);
		CloseHandle(thread);

		fprintf(stderr," Done.\n");

		//
		// Clean up
		//
		buffer->Stop();
	 
		for(int i=0;i<MAX_BUFFER_COUNT;i++)
			CloseHandle(buffer_events[i]);
	 
		buffer_notify->Release();
		buffer->Release();
		dsound->Release();

		delete tbuffer;
	}

private:

	static BOOL CALLBACK DSEnumCallback( LPGUID lpGuid, LPCSTR lpcstrDescription, LPCSTR lpcstrModule, LPVOID lpContext )
	{
		//strcpy(devices[ndevs].name,lpcstrDescription);
		_snprintf_s(devices[ndevs].name,256,255,"%s",lpcstrDescription);

		if(lpGuid)
		{
			devices[ndevs].guid=*lpGuid;
			devices[ndevs].hasGuid = true;
		}
		else
		{
		devices[ndevs].hasGuid = false;
		}
		ndevs++;

		if(ndevs<32) return TRUE;
		return FALSE;
	}

	static BOOL CALLBACK ConfigProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
	{
		int wmId,wmEvent;
		int tSel=0;

		switch(uMsg)
		{
			case WM_INITDIALOG:

				haveGuid = ! FAILED(GUIDFromString(DSoundDevice,&DevGuid));
				SendMessage(GetDlgItem(hWnd,IDC_DS_DEVICE),CB_RESETCONTENT,0,0); 

				ndevs=0;
				DirectSoundEnumerate(DSEnumCallback,NULL);

				tSel=-1;
				for(int i=0;i<ndevs;i++)
				{
					SendMessage(GetDlgItem(hWnd,IDC_DS_DEVICE),CB_ADDSTRING,0,(LPARAM)devices[i].name);
					if(haveGuid && IsEqualGUID(devices[i].guid,DevGuid))
					{
						tSel=i;
					}
				}

				if(tSel>=0)
				{
					SendMessage(GetDlgItem(hWnd,IDC_DS_DEVICE),CB_SETCURSEL,tSel,0);
				}

				break;
			case WM_COMMAND:
				wmId    = LOWORD(wParam); 
				wmEvent = HIWORD(wParam); 
				// Parse the menu selections:
				switch (wmId)
				{
					case IDOK:
						{
							int i = (int)SendMessage(GetDlgItem(hWnd,IDC_DS_DEVICE),CB_GETCURSEL,0,0);
							
							if(!devices[i].hasGuid)
							{
								DSoundDevice[0]=0; // clear device name to ""
							}
							else
							{
								sprintf_s(DSoundDevice,256,"{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
									devices[i].guid.Data1,
									devices[i].guid.Data2,
									devices[i].guid.Data3,
									devices[i].guid.Data4[0],
									devices[i].guid.Data4[1],
									devices[i].guid.Data4[2],
									devices[i].guid.Data4[3],
									devices[i].guid.Data4[4],
									devices[i].guid.Data4[5],
									devices[i].guid.Data4[6],
									devices[i].guid.Data4[7]
									);
							}
						}
						EndDialog(hWnd,0);
						break;
					case IDCANCEL:
						EndDialog(hWnd,0);
						break;
					default:
						return FALSE;
				}
				break;
			default:
				return FALSE;
		}
		return TRUE;
	}

public:
	virtual void Configure(HWND parent)
	{
		INT_PTR ret;
		ret=DialogBoxParam(hInstance,MAKEINTRESOURCE(IDD_DSOUND),GetActiveWindow(),(DLGPROC)ConfigProc,1);
		if(ret==-1)
		{
			MessageBoxEx(GetActiveWindow(),"Error Opening the config dialog.","OMG ERROR!",MB_OK,0);
			return;
		}
	}

	virtual bool Is51Out() { return false; }

	s32 Test()
	{
		return 0;
	}
} DS;

SndOutModule *DSoundOut=&DS;