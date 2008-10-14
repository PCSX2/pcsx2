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
#include "SPU2.h"
#include "resource.h"
#include <assert.h>

#include "regtable.h"

#define SYNC_DISTANCE 4800

void StartVoices(int core, u32 value);
void StopVoices(int core, u32 value);

void InitADSR();

const unsigned char version  = PS2E_SPU2_VERSION;
const unsigned char revision = 1;
const unsigned char build	 = 9;	// increase that with each version

static char *libraryName	  = "GiGaHeRz's SPU2 (" 
#ifdef _DEBUG
	"Playground Debug "
#endif
#ifdef PUBLIC
	"Playground Mod"
#endif
")";

DWORD CALLBACK TimeThread(PVOID /* unused param */);


const char *ParamNames[8]={"VOLL","VOLR","PITCH","ADSR1","ADSR2","ENVX","VOLXL","VOLXR"};
const char *AddressNames[6]={"SSAH","SSAL","LSAH","LSAL","NAXH","NAXL"};

double opitch;
int osps;

int Log = 1;

s8 spu2open=0;

void (* _irqcallback)();
void (* dma4callback)();
void (* dma7callback)();

short *spu2regs;
short *_spu2mem;
s32 uTicks;

u8 callirq;

HANDLE hThreadFunc;
u32	ThreadFuncID;

char fname[]="01234567890123456789012345";

V_Core Cores[2];
V_SPDIF Spdif;

s16 OutPos;
s16 InputPos;
u8 InpBuff;
u32 Cycles;
u32 Num;
u32 acumCycles;

u32* cPtr=NULL;
u32  lClocks=0;
u32  pClocks=0;

bool hasPtr=false;

int PlayMode;

s16 attrhack[2]={0,0};

HINSTANCE hInstance;

bool debugDialogOpen=false;
HWND hDebugDialog=NULL;

const char *tSyncName="SPU2DoMoreTicks";

CRITICAL_SECTION threadSync;

s32 logvolume[16384];

bool has_to_call_irq=false;

void SetIrqCall()
{
	has_to_call_irq=true;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL,DWORD dwReason,LPVOID lpvReserved)
{
	if(dwReason==DLL_PROCESS_ATTACH) hInstance=hinstDLL;
	return TRUE;
}

u32 CALLBACK PS2EgetLibType() 
{
	return PS2E_LT_SPU2;
}

char* CALLBACK PS2EgetLibName() 
{
	return libraryName;
}

u32 CALLBACK PS2EgetLibVersion2(u32 type) 
{
	return (version<<16)|(revision<<8)|build;
}

void SysMessage(char *fmt, ...) 
{
	va_list list;
	char tmp[512];

	va_start(list,fmt);
	_vsnprintf(tmp,512,fmt,list);
	va_end(list);
	MessageBox(0, tmp, "SPU2ghz Msg", 0);
}

s16 __forceinline *GetMemPtr(u32 addr)
{
	assert(addr<0x100000);
	return (_spu2mem+addr);
}

void CoreReset(int c)
{
	int v=0;

	ConLog(" * SPU2: Initializing core %d structures... ",c);

	memset(Cores+c,0,sizeof(Cores[c]));

	Cores[c].Regs.STATX=0;
	Cores[c].Regs.ATTR=0;
	Cores[c].ExtL=0x3FFF;
	Cores[c].ExtR=0x3FFF;
	Cores[c].InpL=0x3FFF;
	Cores[c].InpR=0x3FFF;
	Cores[c].FxL=0x0000;
	Cores[c].FxR=0x0000;
	Cores[c].MasterL.Reg_VOL=0x3FFF;
	Cores[c].MasterL.Value=0x3FFF;
	Cores[c].MasterR.Reg_VOL=0x3FFF;
	Cores[c].MasterR.Value=0x3FFF;
	Cores[c].ExtWetR=1;
	Cores[c].ExtWetL=1;
	Cores[c].ExtDryR=1;
	Cores[c].ExtDryL=1;
	Cores[c].InpWetR=1;
	Cores[c].InpWetL=1;
	Cores[c].InpDryR=1;
	Cores[c].InpDryL=1;
	Cores[c].SndWetR=0;
	Cores[c].SndWetL=0;
	Cores[c].SndDryR=1;
	Cores[c].SndDryL=1;
	Cores[c].Regs.MMIX = 0xFFCF;
	Cores[c].Regs.VMIXL = 0xFFFFFF;
	Cores[c].Regs.VMIXR = 0xFFFFFF;
	Cores[c].Regs.VMIXEL = 0xFFFFFF;
	Cores[c].Regs.VMIXER = 0xFFFFFF;
	Cores[c].EffectsStartA= 0xEFFF8 + 0x10000*c;
	Cores[c].EffectsEndA  = 0xEFFFF + 0x10000*c;
	Cores[c].FxEnable=0;
	Cores[c].IRQA=0xFFFF0;
	Cores[c].IRQEnable=1;

	for (v=0;v<24;v++) {
		Cores[c].Voices[v].VolumeL.Reg_VOL=0x3FFF;
		Cores[c].Voices[v].VolumeL.Value=0x3FFF;
		Cores[c].Voices[v].VolumeR.Reg_VOL=0x3FFF;
		Cores[c].Voices[v].VolumeR.Value=0x3FFF;
		Cores[c].Voices[v].ADSR.Value=0;
		Cores[c].Voices[v].ADSR.Phase=0;
		Cores[c].Voices[v].Pitch=0x3FFF;
		Cores[c].Voices[v].DryL=1;
		Cores[c].Voices[v].DryR=1;
		Cores[c].Voices[v].WetL=1;
		Cores[c].Voices[v].WetR=1;
		Cores[c].Voices[v].NextA=2800;
		Cores[c].Voices[v].StartA=2800;
		Cores[c].Voices[v].LoopStartA=2800;
		Cores[c].Voices[v].lastSetStartA=2800;
	}
	Cores[c].DMAICounter=0;
	Cores[c].AdmaInProgress=0;

	Cores[c].Regs.STATX=0x80;

	ConLog("done.\n");
}

extern void LowPassFilterInit();

s32 CALLBACK SPU2init() 
{
#define MAKESURE(a,b) \
		/*fprintf(stderr,"%08p: %08p == %08p\n",&(regtable[a>>1]),regtable[a>>1],U16P(b));*/ \
		assert(regtable[(a)>>1]==U16P(b))

	MAKESURE(0x800,zero);

	s32 c=0,v=0;
	ReadSettings();
	acumCycles=0;
#ifdef SPU2_LOG
	if(AccessLog) 
	{
		spu2Log = fopen(AccessLogFileName, "w");
		setvbuf(spu2Log, NULL,  _IONBF, 0);
		FileLog("SPU2init\n");
	}
#endif
	srand((unsigned)time(NULL));
	if (spu2open) return 0;
	spu2regs  = (short*)malloc(0x010000);
	_spu2mem  = (short*)malloc(0x200000);
	if ((spu2regs == NULL) || (_spu2mem == NULL)) 
	{
		SysMessage("Error allocating Memory\n"); return -1;
	}

	for(int mem=0;mem<0x800;mem++)
	{
		u16 *ptr=regtable[mem>>1];
		if(!ptr) {
			regtable[mem>>1] = &(spu2Ru16(mem));
		}
	}

	memset(spu2regs,0,0x010000);
	memset(_spu2mem,0,0x200000);
	memset(&Cores,0,(sizeof(V_Core) * 2));
	CoreReset(0);
	CoreReset(1);

	DMALogOpen();

	if(WaveLog) 
	{
		if(!wavedump_open())
		{
			SysMessage("Can't open '%s'.\nWave Log disabled.",WaveLogFileName);
		}
	}

	for(v=0;v<16384;v++)
	{
		logvolume[v]=(s32)(s32)floor(log((double)(v+1))*3376.7);
	}

	LowPassFilterInit();

	InitADSR();

#ifdef STREAM_DUMP
	il0=fopen("logs/spu2input0.pcm","wb");
	il1=fopen("logs/spu2input1.pcm","wb");
#endif

#ifdef EFFECTS_DUMP
	el0=fopen("logs/spu2fx0.pcm","wb");
	el1=fopen("logs/spu2fx1.pcm","wb");
#endif


#ifdef S2R_ENABLE
	if(!replay_mode)
		s2r_open("replay_dump.s2r");
#endif
	return 0;
}


BOOL CALLBACK DebugProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	int wmId,wmEvent;

	switch(uMsg)
	{
		case WM_PAINT:
			return FALSE;
		case WM_INITDIALOG:
			{
				debugDialogOpen=true;
			}
			break;
		case WM_COMMAND:
			wmId    = LOWORD(wParam); 
			wmEvent = HIWORD(wParam); 
			// Parse the menu selections:
			switch (wmId)
			{
				case IDOK:
				case IDCANCEL:
					debugDialogOpen=false;
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

s32 CALLBACK SPU2open(void *pDsp) {
	FileLog("[%10d] SPU2 Open\n",Cycles);

	/*if(debugDialogOpen==0)
	{
		hDebugDialog = CreateDialogParam(hInstance,MAKEINTRESOURCE(IDD_DEBUG),0,DebugProc,0);
		ShowWindow(hDebugDialog,SW_SHOWNORMAL);
		debugDialogOpen=1;
	}*/

	spu2open=1;
	if (!SndInit()) 
	{
		srate_pv=(double)SampleRate/48000.0;

		spdif_init();

		DspLoadLibrary(dspPlugin,dspPluginModule);

		return 0;
	}
	else 
	{
		SPU2close();
		return -1;
	};
}

void CALLBACK SPU2close() 
{
	FileLog("[%10d] SPU2 Close\n",Cycles);
	spu2open=0;

	DspCloseLibrary();

	spdif_shutdown();

	SndClose();
}

void CALLBACK SPU2shutdown() 
{
	if(spu2open) SPU2close();

#ifdef S2R_ENABLE
	if(!replay_mode)
		s2r_close();
#endif

	DoFullDump();
#ifdef STREAM_DUMP
	fclose(il0);
	fclose(il1);
#endif
#ifdef EFFECTS_DUMP
	fclose(el0);
	fclose(el1);
#endif
	if(WaveLog && wavedump_ok) wavedump_close();

	DMALogClose();
	free(spu2regs);
	free(_spu2mem);
#ifdef SPU2_LOG
	if(!AccessLog) return;
	FileLog("[%10d] SPU2shutdown\n",Cycles);
	if(spu2Log) fclose(spu2Log);
#endif
}

void CALLBACK SPU2setClockPtr(u32 *ptr)
{
	cPtr=ptr;
	hasPtr=(cPtr!=NULL);
}

int FillRectangle(HDC dc, int left, int top, int width, int height)
{
	RECT r = { left, top, left+width, top+height };

	return FillRect(dc, &r, (HBRUSH)GetStockObject(DC_BRUSH));
}

BOOL DrawRectangle(HDC dc, int left, int top, int width, int height)
{
	RECT r = { left, top, left+width, top+height };

	POINT p[5] = {
		{ r.left, r.top },
		{ r.right, r.top },
		{ r.right, r.bottom },
		{ r.left, r.bottom },
		{ r.left, r.top },
	};

	return Polyline(dc, p, 5);
}

HFONT hf = NULL;

int lCount=0;
void UpdateDebugDialog()
{
	if(!debugDialogOpen) return;

	lCount++;
	if(lCount>=(SampleRate/10))
	{
		HDC hdc = GetDC(hDebugDialog);

		if(!hf)
		{
			hf = CreateFont( 8, 0, 0, 0, 0, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Lucida Console");
		}

		SelectObject(hdc,hf);
		SelectObject(hdc,GetStockObject(DC_BRUSH));
		SelectObject(hdc,GetStockObject(DC_PEN));

		for(int c=0;c<2;c++)
		{
			for(int v=0;v<24;v++)
			{
				int IX = 8+256*c;
				int IY = 8+ 32*v;
				V_Voice& vc(Cores[c].Voices[v]);

				SetDCBrushColor(hdc,RGB(  0,  0,  0));
				if((vc.ADSR.Phase>0)&&(vc.ADSR.Phase<6))
				{
					SetDCBrushColor(hdc,RGB(  0,  0,128));
				}
				else
				{
					if(vc.lastStopReason==1)
					{
						SetDCBrushColor(hdc,RGB(128,  0,  0));
					}
					if(vc.lastStopReason==2)
					{
						SetDCBrushColor(hdc,RGB(  0,128,  0));
					}
				}

				FillRectangle(hdc,IX,IY,252,30);

				SetDCPenColor(hdc,RGB(  255,  128,  32));

				DrawRectangle(hdc,IX,IY,252,30);

				SetDCBrushColor  (hdc,RGB(  0,255,  0));

				int vl = abs(vc.VolumeL.Value * 24 / 32768);
				int vr = abs(vc.VolumeR.Value * 24 / 32768);

				FillRectangle(hdc,IX+38,IY+26 - vl, 4, vl);
				FillRectangle(hdc,IX+42,IY+26 - vr, 4, vr);

				int adsr = (vc.ADSR.Value>>16) * 24 / 32768;

				FillRectangle(hdc,IX+48,IY+26 - adsr, 4, adsr);

				int peak = vc.displayPeak * 24 / 32768;

				FillRectangle(hdc,IX+56,IY+26 - peak, 4, peak);

				SetTextColor(hdc,RGB(  0,255,  0));
				SetBkColor  (hdc,RGB(  0,  0,  0));

				static char t[1024];

				sprintf(t,"%06x",vc.StartA);
				TextOut(hdc,IX+4,IY+3,t,6);

				sprintf(t,"%06x",vc.NextA);
				TextOut(hdc,IX+4,IY+12,t,6);

				sprintf(t,"%06x",vc.LoopStartA);
				TextOut(hdc,IX+4,IY+21,t,6);

				vc.displayPeak = 0;

				if(vc.lastSetStartA != vc.StartA)
				{
					printf(" *** Warning! Core %d Voice %d: StartA should be %06x, and is %06x.\n",
						c,v,vc.lastSetStartA,vc.StartA);
					vc.lastSetStartA = vc.lastSetStartA;
				}
			}
		}
		ReleaseDC(hDebugDialog,hdc);
		lCount=0;
	}

	MSG msg;
	while(PeekMessage(&msg,hDebugDialog,0,0,PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

//SHOULD be 768, but 751/752 seems to get better results
#define TickInterval 768

u32 TicksCore=0;
u32 TicksThread=0;

DWORD CALLBACK TimeThread(PVOID /* unused param */)
{
	while(spu2open)
	{
		if(TicksThread>=(TicksCore+320))
		{
			Sleep(1);
		}
		else if(TicksThread>=TicksCore)
		{
			Sleep(0);
		}
		else
		{
			Mix();
			TicksThread++;
		}
	}
	return 0;
}

void CALLBACK TimeUpdate(u32 cClocks, u32 syncType)
{
	u32 dClocks = cClocks-lClocks;

	// HACKY but should work anyway.
	if(lClocks==0) lClocks = cClocks;

	if(dClocks>=TickInterval)
	{
		//Update Mixing Progress
		while(dClocks>=TickInterval)
		{
			
			//UpdateDebugDialog();

			if(has_to_call_irq)
			{
				ConLog(" * SPU2: Irq Called (%04x).\n",Spdif.Info);
				has_to_call_irq=false;
				if(_irqcallback) _irqcallback();
			}

			if(Cores[0].InitDelay>0)
			{
				Cores[0].InitDelay--;
				if(Cores[0].InitDelay==0)
				{
					CoreReset(0);
				}
			}

			if(Cores[1].InitDelay>0)
			{
				Cores[1].InitDelay--;
				if(Cores[1].InitDelay==0)
				{
					CoreReset(1);
				}
			}

			//Update DMA4 interrupt delay counter
			if(Cores[0].DMAICounter>0) 
			{
				Cores[0].DMAICounter-=TickInterval;
				if(Cores[0].DMAICounter<=0)
				{
					Cores[0].MADR=Cores[0].TADR;
					Cores[0].DMAICounter=0;
					if(dma4callback) dma4callback();
				}
				else {
					Cores[0].MADR+=TickInterval<<1;
				}
			}

			//Update DMA7 interrupt delay counter
			if(Cores[1].DMAICounter>0) 
			{
				Cores[1].DMAICounter-=TickInterval;
				if(Cores[1].DMAICounter<=0)
				{
					Cores[1].MADR=Cores[1].TADR;
					Cores[1].DMAICounter=0;
					if(dma7callback) dma7callback();
				}
				else {
					Cores[1].MADR+=TickInterval<<1;
				}
			}

			dClocks-=TickInterval;
			lClocks+=TickInterval;
			Cycles++;

			Mix();
		}
	}
}

bool numpad_minus_old=false;
bool numpad_minus = false;
u32 timer=0,time1=0,time2=0;
void CALLBACK SPU2async(u32 cycles) 
{
	u32 oldClocks = lClocks;
	timer++;

	if (timer == 1){
		time1=timeGetTime();
	}
	if (timer == 3000){
		time2 = timeGetTime()-time1 ;
		timer=0;
	}
	DspUpdate();

	if(LimiterToggleEnabled)
	{
		numpad_minus = (GetAsyncKeyState(VK_SUBTRACT)&0x8000)!=0;

		if(numpad_minus && !numpad_minus_old)
		{
			if(LimitMode) LimitMode=0;
			else		  LimitMode=1;
			SndUpdateLimitMode();
		}
		numpad_minus_old = numpad_minus;
	}

	if(hasPtr)
	{
		TimeUpdate(*cPtr,0); 
	}
	else
	{
		pClocks+=cycles;
		TimeUpdate(pClocks,0);
	}
}

void CALLBACK SPU2irqCallback(void (*SPU2callback)(),void (*DMA4callback)(),void (*DMA7callback)())
{
	_irqcallback=SPU2callback;
	dma4callback=DMA4callback;
	dma7callback=DMA7callback;
}

u16 mask = 0xFFFF;

void UpdateSpdifMode()
{
	int OPM=PlayMode;
	u16 last = 0;

	if(mask&Spdif.Out)
	{
		last = mask & Spdif.Out;
		mask=mask&(~Spdif.Out);
	}

	if(Spdif.Out&0x4) // use 24/32bit PCM data streaming
	{
		PlayMode=8;
		ConLog(" * SPU2: WARNING: Possibly CDDA mode set!\n");
		return;
	}

	if(Spdif.Out&SPDIF_OUT_BYPASS)
	{
		PlayMode=2;
		if(Spdif.Mode&SPDIF_MODE_BYPASS_BITSTREAM)
			PlayMode=4; //bitstream bypass
	}
	else
	{
		PlayMode=0; //normal processing
		if(Spdif.Out&SPDIF_OUT_PCM)
		{
			PlayMode=1;
		}
	}
	if(OPM!=PlayMode)
	{
		ConLog(" * SPU2: Play Mode Set to %s (%d).\n",(PlayMode==0)?"Normal":((PlayMode==1)?"PCM Clone":((PlayMode==2)?"PCM Bypass":"BitStream Bypass")),PlayMode);
	}
}

__forceinline void RegLog(int level, char *RName,u32 mem,u32 core,u16 value) 
{
	if(level>1)
		FileLog("[%10d] SPU2 write mem %08x (core %d, register %s) value %04x\n",Cycles,mem,core,RName,value);
}

void CALLBACK SPU_ps1_write(u32 mem, u16 value) 
{
	bool show=true;

	u32 reg = mem&0xffff;

	if((reg>=0x1c00)&&(reg<0x1d80))
	{
		//voice values
		u8 voice = ((reg-0x1c00)>>4);
		u8 vval = reg&0xf;
		switch(vval)
		{
			case 0: //VOLL (Volume L)
				Cores[0].Voices[voice].VolumeL.Mode=0;
				Cores[0].Voices[voice].VolumeL.Value=value<<1;
				Cores[0].Voices[voice].VolumeL.Reg_VOL = value;	break;
			case 1: //VOLR (Volume R)
				Cores[0].Voices[voice].VolumeR.Mode=0;
				Cores[0].Voices[voice].VolumeR.Value=value<<1;
				Cores[0].Voices[voice].VolumeR.Reg_VOL = value;	break;
			case 2:	Cores[0].Voices[voice].Pitch=value;			break;
			case 3:	Cores[0].Voices[voice].StartA=(u32)value<<8;	break;
			case 4: // ADSR1 (Envelope)
				Cores[0].Voices[voice].ADSR.Am=(value & 0x8000)>>15;
				Cores[0].Voices[voice].ADSR.Ar=(value & 0x7F00)>>8;
				Cores[0].Voices[voice].ADSR.Dr=(value & 0xF0)>>4;
				Cores[0].Voices[voice].ADSR.Sl=(value & 0xF);
				Cores[0].Voices[voice].ADSR.Reg_ADSR1 = value;	break;
			case 5: // ADSR2 (Envelope)
				Cores[0].Voices[voice].ADSR.Sm=(value & 0xE000)>>13;
				Cores[0].Voices[voice].ADSR.Sr=(value & 0x1FC0)>>6;
				Cores[0].Voices[voice].ADSR.Rm=(value & 0x20)>>5;
				Cores[0].Voices[voice].ADSR.Rr=(value & 0x1F);
				Cores[0].Voices[voice].ADSR.Reg_ADSR2 = value;	break;
			case 6:	Cores[0].Voices[voice].ADSR.Value=value;	break;
			case 7:	Cores[0].Voices[voice].LoopStartA=(u32)value <<8;	break;
		}
	}
	else switch(reg)
	{
		case 0x1d80://         Mainvolume left
			Cores[0].MasterL.Mode=0;
			Cores[0].MasterL.Value=value;
			break;
		case 0x1d82://         Mainvolume right
			Cores[0].MasterL.Mode=0;
			Cores[0].MasterR.Value=value;
			break;
		case 0x1d84://         Reverberation depth left
			Cores[0].FxL=value;
			break;
		case 0x1d86://         Reverberation depth right
			Cores[0].FxR=value;
			break;

		case 0x1d88://         Voice ON  (0-15)
			SPU2write(REG_S_KON,value);
			break;
		case 0x1d8a://         Voice ON  (16-23)
			SPU2write(REG_S_KON+2,value);
			break;

		case 0x1d8c://         Voice OFF (0-15)
			SPU2write(REG_S_KOFF,value);
			break;
		case 0x1d8e://         Voice OFF (16-23)
			SPU2write(REG_S_KOFF+2,value);
			break;

		case 0x1d90://         Channel FM (pitch lfo) mode (0-15)
			SPU2write(REG_S_PMON,value);
			break;
		case 0x1d92://         Channel FM (pitch lfo) mode (16-23)
			SPU2write(REG_S_PMON+2,value);
			break;


		case 0x1d94://         Channel Noise mode (0-15)
			SPU2write(REG_S_NON,value);
			break;
		case 0x1d96://         Channel Noise mode (16-23)
			SPU2write(REG_S_NON+2,value);
			break;

		case 0x1d98://         Channel Reverb mode (0-15)
			SPU2write(REG_S_VMIXEL,value);
			SPU2write(REG_S_VMIXER,value);
			break;
		case 0x1d9a://         Channel Reverb mode (16-23)
			SPU2write(REG_S_VMIXEL+2,value);
			SPU2write(REG_S_VMIXER+2,value);
			break;
		case 0x1d9c://         Channel Reverb mode (0-15)
			SPU2write(REG_S_VMIXL,value);
			SPU2write(REG_S_VMIXR,value);
			break;
		case 0x1d9e://         Channel Reverb mode (16-23)
			SPU2write(REG_S_VMIXL+2,value);
			SPU2write(REG_S_VMIXR+2,value);
			break;

		case 0x1da2://         Reverb work area start
			{
				u32 val=(u32)value <<8;

				SPU2write(REG_A_ESA,  val&0xFFFF);
				SPU2write(REG_A_ESA+2,val>>16);
			}
			break;
		case 0x1da4:
			Cores[0].IRQA=(u32)value<<8;
			break;
		case 0x1da6:
			Cores[0].TSA=(u32)value<<8;
			break;

		case 0x1daa:
			SPU2write(REG_C_ATTR,value);
			break;
		case 0x1dae:
			SPU2write(REG_P_STATX,value);
			break;
		case 0x1da8:// Spu Write to Memory
			DmaWrite(0,value);
			show=false;
			break;
	}

	if(show) FileLog("[%10d] (!) SPU write mem %08x value %04x\n",Cycles,mem,value);

	spu2Ru16(mem)=value;
}

u16 CALLBACK SPU_ps1_read(u32 mem) 
{
	bool show=true;
	u16 value = spu2Ru16(mem);

	u32 reg = mem&0xffff;

	if((reg>=0x1c00)&&(reg<0x1d80))
	{
		//voice values
		u8 voice = ((reg-0x1c00)>>4);
		u8 vval = reg&0xf;
		switch(vval)
		{
			case 0: //VOLL (Volume L)
				value=Cores[0].Voices[voice].VolumeL.Mode;
				value=Cores[0].Voices[voice].VolumeL.Value;
				value=Cores[0].Voices[voice].VolumeL.Reg_VOL;	break;
			case 1: //VOLR (Volume R)
				value=Cores[0].Voices[voice].VolumeR.Mode;
				value=Cores[0].Voices[voice].VolumeR.Value;
				value=Cores[0].Voices[voice].VolumeR.Reg_VOL;	break;
			case 2:	value=Cores[0].Voices[voice].Pitch;			break;
			case 3:	value=Cores[0].Voices[voice].StartA;	break;
			case 4: value=Cores[0].Voices[voice].ADSR.Reg_ADSR1;	break;
			case 5: value=Cores[0].Voices[voice].ADSR.Reg_ADSR2;	break;
			case 6:	value=Cores[0].Voices[voice].ADSR.Value;	break;
			case 7:	value=Cores[0].Voices[voice].LoopStartA;	break;
		}
	}
	else switch(reg)
	{
		case 0x1d80: value = Cores[0].MasterL.Value; break;
		case 0x1d82: value = Cores[0].MasterR.Value; break;
		case 0x1d84: value = Cores[0].FxL;           break;
		case 0x1d86: value = Cores[0].FxR;           break;

		case 0x1d88: value = 0; break;
		case 0x1d8a: value = 0; break;
		case 0x1d8c: value = 0; break;
		case 0x1d8e: value = 0; break;

		case 0x1d90: value = Cores[0].Regs.PMON&0xFFFF;   break;
		case 0x1d92: value = Cores[0].Regs.PMON>>16;      break;

		case 0x1d94: value = Cores[0].Regs.NON&0xFFFF;    break;
		case 0x1d96: value = Cores[0].Regs.NON>>16;       break;

		case 0x1d98: value = Cores[0].Regs.VMIXEL&0xFFFF; break;
		case 0x1d9a: value = Cores[0].Regs.VMIXEL>>16;    break;
		case 0x1d9c: value = Cores[0].Regs.VMIXL&0xFFFF;  break;
		case 0x1d9e: value = Cores[0].Regs.VMIXL>>16;     break;

		case 0x1da2: value = Cores[0].EffectsStartA>>3;   break;
		case 0x1da4: value = Cores[0].IRQA>>3;            break;
		case 0x1da6: value = Cores[0].TSA>>3;             break;

		case 0x1daa:
			value = SPU2read(REG_C_ATTR);
			break;
		case 0x1dae:
			value = 0; //SPU2read(REG_P_STATX)<<3;
			break;
		case 0x1da8:
			value = DmaRead(0);
			show=false;
			break;
	}

	if(show) FileLog("[%10d] (!) SPU read mem %08x value %04x\n",Cycles,mem,value);
	return value;
}

void RegWriteLog(u32 core,u16 value);

void CALLBACK SPU2writeLog(u32 rmem, u16 value) 
{
	u32 vx=0, vc=0, core=0, omem=rmem, mem=rmem&0x7FF;
	omem=mem=mem&0x7FF; //FFFF;
	if (mem & 0x400) { omem^=0x400; core=1; }

	/*
	if ((omem >= 0x0000) && (omem < 0x0180)) { // Voice Params
		u32 voice=(omem & 0x1F0) >> 4;
		u32 param=(omem & 0xF)>>1;
		FileLog("[%10d] SPU2 write mem %08x (Core %d Voice %d Param %s) value %x\n",Cycles,rmem,core,voice,ParamNames[param],value);
	}
	else if ((omem >= 0x01C0) && (omem < 0x02DE)) {
		u32 voice   =((omem-0x01C0) / 12);
		u32 address =((omem-0x01C0) % 12)>>1;
		FileLog("[%10d] SPU2 write mem %08x (Core %d Voice %d Address %s) value %x\n",Cycles,rmem,core,voice,AddressNames[address],value);
	}
	*/
	else if ((mem >= 0x0760) && (mem < 0x07b0)) {
		omem=mem; core=0;
		if (mem >= 0x0788) {omem-=0x28; core=1;}
		switch(omem) {
		    case REG_P_EVOLL:	RegLog(2,"EVOLL",rmem,core,value);	break;
			case REG_P_EVOLR:	RegLog(2,"EVOLR",rmem,core,value);	break;
			case REG_P_AVOLL:	if (core) { RegLog(2,"AVOLL",rmem,core,value); }	break;
			case REG_P_AVOLR:	if (core) { RegLog(2,"AVOLR",rmem,core,value); }	break;
			case REG_P_BVOLL:	RegLog(2,"BVOLL",rmem,core,value);	break;
			case REG_P_BVOLR:	RegLog(2,"BVOLR",rmem,core,value);	break;
			case REG_P_MVOLXL:	RegLog(2,"MVOLXL",rmem,core,value);	break;
			case REG_P_MVOLXR:	RegLog(2,"MVOLXR",rmem,core,value);	break;
			case R_IIR_ALPHA:	RegLog(2,"IIR_ALPHA",rmem,core,value);	break;
			case R_ACC_COEF_A:	RegLog(2,"ACC_COEF_A",rmem,core,value);	break;
			case R_ACC_COEF_B:	RegLog(2,"ACC_COEF_B",rmem,core,value);	break;
			case R_ACC_COEF_C:	RegLog(2,"ACC_COEF_C",rmem,core,value);	break;
			case R_ACC_COEF_D:	RegLog(2,"ACC_COEF_D",rmem,core,value);	break;
			case R_IIR_COEF:	RegLog(2,"IIR_COEF",rmem,core,value);	break;
			case R_FB_ALPHA:	RegLog(2,"FB_ALPHA",rmem,core,value);	break;
			case R_FB_X:	  	RegLog(2,"FB_X",rmem,core,value);	break;
			case R_IN_COEF_L:	RegLog(2,"IN_COEF_L",rmem,core,value);	break;
			case R_IN_COEF_R:	RegLog(2,"IN_COEF_R",rmem,core,value);	break;

		}
	}
	else if ((mem>=0x07C0) && (mem<0x07CE)) {
		switch(mem) {
			case SPDIF_OUT:
				RegLog(2,"SPDIF_OUT",rmem,-1,value);
				break;
			case IRQINFO:
				RegLog(2,"IRQINFO",rmem,-1,value);
				break;
			case 0x7c4:
				if(Spdif.Unknown1 != value) ConLog(" * SPU2: SPDIF Unknown Register 1 set to %04x\n",value);
				RegLog(2,"SPDIF_UNKNOWN1",rmem,-1,value);
				break;
			case SPDIF_MODE:
				if(Spdif.Mode != value) ConLog(" * SPU2: SPDIF Mode set to %04x\n",value);
				RegLog(2,"SPDIF_MODE",rmem,-1,value);
				break;
			case SPDIF_MEDIA:
				if(Spdif.Media != value) ConLog(" * SPU2: SPDIF Media set to %04x\n",value);
				RegLog(2,"SPDIF_MEDIA",rmem,-1,value);
				break;
			case 0x7ca:
				if(Spdif.Unknown2 != value) ConLog(" * SPU2: SPDIF Unknown Register 2 set to %04x\n",value);
				RegLog(2,"SPDIF_UNKNOWN2",rmem,-1,value);
				break;
			case SPDIF_COPY:
				if(Spdif.Protection != value) ConLog(" * SPU2: SPDIF Copy set to %04x\n",value);
				RegLog(2,"SPDIF_COPY",rmem,-1,value);
				break;
		}
		UpdateSpdifMode();
	}
	else
		switch(omem) {
		case REG_C_ATTR:
			RegLog(4,"ATTR",rmem,core,value);
			break;
		case REG_S_PMON:
			RegLog(1,"PMON0",rmem,core,value);
			break;
		case (REG_S_PMON + 2):
			RegLog(1,"PMON1",rmem,core,value);
			break;
		case REG_S_NON:
			RegLog(1,"NON0",rmem,core,value);
			break;
		case (REG_S_NON + 2):
			RegLog(1,"NON1",rmem,core,value);
			break;
		case REG_S_VMIXL:
			RegLog(1,"VMIXL0",rmem,core,value);
		case (REG_S_VMIXL + 2):
			RegLog(1,"VMIXL1",rmem,core,value);
			break;
		case REG_S_VMIXEL:
			RegLog(1,"VMIXEL0",rmem,core,value);
			break;
		case (REG_S_VMIXEL + 2):
			RegLog(1,"VMIXEL1",rmem,core,value);
			break;
		case REG_S_VMIXR:
			RegLog(1,"VMIXR0",rmem,core,value);
			break;
		case (REG_S_VMIXR + 2):
			RegLog(1,"VMIXR1",rmem,core,value);
			break;
		case REG_S_VMIXER:
			RegLog(1,"VMIXER0",rmem,core,value);
			break;
		case (REG_S_VMIXER + 2):
			RegLog(1,"VMIXER1",rmem,core,value);
			break;
		case REG_P_MMIX:
			RegLog(1,"MMIX",rmem,core,value);
			break;
		case REG_A_IRQA:
			RegLog(2,"IRQAH",rmem,core,value);
			break;
		case (REG_A_IRQA + 2):
			RegLog(2,"IRQAL",rmem,core,value);
			break;
		case (REG_S_KON + 2):
			RegLog(2,"KON1",rmem,core,value);
			break;
		case REG_S_KON:
			RegLog(2,"KON0",rmem,core,value);
			break;
		case (REG_S_KOFF + 2):
			RegLog(2,"KOFF1",rmem,core,value);
			break;
		case REG_S_KOFF:
			RegLog(2,"KOFF0",rmem,core,value);
			break;
		case REG_A_TSA:
			RegLog(2,"TSAH",rmem,core,value);
			break;
		case (REG_A_TSA + 2):
			RegLog(2,"TSAL",rmem,core,value);
			break;
		case REG_S_ENDX:
			//ConLog(" * SPU2: Core %d ENDX cleared!\n",core);
			RegLog(2,"ENDX0",rmem,core,value);
			break;
		case (REG_S_ENDX + 2):	
			//ConLog(" * SPU2: Core %d ENDX cleared!\n",core);
			RegLog(2,"ENDX1",rmem,core,value);
			break;
		case REG_P_MVOLL:
			RegLog(1,"MVOLL",rmem,core,value);
			break;
		case REG_P_MVOLR:
			RegLog(1,"MVOLR",rmem,core,value);
			break;
		case REG_S_ADMAS:
			RegLog(3,"ADMAS",rmem,core,value);
			ConLog(" * SPU2: Core %d AutoDMAControl set to %d\n",core,value);
			break;
		case REG_P_STATX:
			RegLog(3,"STATX",rmem,core,value);
			break;
		case REG_A_ESA:
			RegLog(1,"ESAH",rmem,core,value);
			break;
		case (REG_A_ESA + 2):
			RegLog(1,"ESAL",rmem,core,value);
			break;
		case REG_A_EEA:
			RegLog(1,"EEAH",rmem,core,value);
			break;

#define LOG_REVB_REG(n,t) \
		case R_##n: \
			RegLog(2,t "H",mem,core,value); \
			break; \
		case (R_##n + 2): \
			RegLog(2,t "L",mem,core,value); \
			break;

	LOG_REVB_REG(FB_SRC_A,"FB_SRC_A")
	LOG_REVB_REG(FB_SRC_B,"FB_SRC_B")
	LOG_REVB_REG(IIR_SRC_A0,"IIR_SRC_A0")
	LOG_REVB_REG(IIR_SRC_A1,"IIR_SRC_A1")
	LOG_REVB_REG(IIR_SRC_B1,"IIR_SRC_B1")
	LOG_REVB_REG(IIR_SRC_B0,"IIR_SRC_B0")
	LOG_REVB_REG(IIR_DEST_A0,"IIR_DEST_A0")
	LOG_REVB_REG(IIR_DEST_A1,"IIR_DEST_A1")
	LOG_REVB_REG(IIR_DEST_B0,"IIR_DEST_B0")
	LOG_REVB_REG(IIR_DEST_B1,"IIR_DEST_B1")
	LOG_REVB_REG(ACC_SRC_A0,"ACC_SRC_A0")
	LOG_REVB_REG(ACC_SRC_A1,"ACC_SRC_A1")
	LOG_REVB_REG(ACC_SRC_B0,"ACC_SRC_B0")
	LOG_REVB_REG(ACC_SRC_B1,"ACC_SRC_B1")
	LOG_REVB_REG(ACC_SRC_C0,"ACC_SRC_C0")
	LOG_REVB_REG(ACC_SRC_C1,"ACC_SRC_C1")
	LOG_REVB_REG(ACC_SRC_D0,"ACC_SRC_D0")
	LOG_REVB_REG(ACC_SRC_D1,"ACC_SRC_D1")
	LOG_REVB_REG(MIX_DEST_A0,"MIX_DEST_A0")
	LOG_REVB_REG(MIX_DEST_A1,"MIX_DEST_A1")
	LOG_REVB_REG(MIX_DEST_B0,"MIX_DEST_B0")
	LOG_REVB_REG(MIX_DEST_B1,"MIX_DEST_B1")

		default:			RegLog(2,"UNKNOWN",rmem,core,value); spu2Ru16(mem) = value;
	}
}


void CALLBACK SPU2write(u32 rmem, u16 value) 
{
#ifdef S2R_ENABLE
	if(!replay_mode)
		s2r_writereg(Cycles,rmem,value);
#endif

	if(rmem==0x1f9001ac)
	{
		//RegWriteLog(0,value);
		if((Cores[0].IRQEnable)&&(Cores[0].TSA==Cores[0].IRQA))
		{
			Spdif.Info=4;
			SetIrqCall();
		}
		spu2Mu16(Cores[0].TSA++)=value;
		Cores[0].TSA&=0xfffff;

		return;
	}
	else if(rmem==0x1f9005ac)
	{
		//RegWriteLog(1,value);
		if((Cores[0].IRQEnable)&&(Cores[0].TSA==Cores[0].IRQA))
		{
			Spdif.Info=4;
			SetIrqCall();
		}
		spu2Mu16(Cores[1].TSA++)=value;
		Cores[1].TSA&=0xfffff;

		return;
	}

	if(hasPtr) TimeUpdate(*cPtr,0);

	u32 vx=0, vc=0, core=0, omem=rmem, mem=rmem&0x7FF;
	omem=mem=mem&0x7FF; //FFFF;
	if (mem & 0x400) { omem^=0x400; core=1; }

	if (rmem>>16 == 0x1f80)
	{
		SPU_ps1_write(rmem,value);
	}
	else if ((mem&0xFFFFF)>=0x800)
	{
		ConLog (" * SPU2: Write to reg>=0x800: %08x value %x\n",rmem,value);
		spu2Ru16(mem)=value;
	}
	//else if ((omem >= 0x0000) && (omem < 0x0180)) { // Voice Params
	else if (omem < 0x0180) { // Voice Params
		u32 voice=(omem & 0x1F0) >> 4;
		u32 param=(omem & 0xF)>>1;
		//FileLog("[%10d] SPU2 write mem %08x (Core %d Voice %d Param %s) value %x\n",Cycles,rmem,core,voice,ParamNames[param],value);
		switch (param) { 
			case 0: //VOLL (Volume L)
				if (value & 0x8000) {  // +Lin/-Lin/+Exp/-Exp
					Cores[core].Voices[voice].VolumeL.Mode=(value & 0xF000)>>12;
					Cores[core].Voices[voice].VolumeL.Increment=(value & 0x3F);
				}
				else {
					Cores[core].Voices[voice].VolumeL.Mode=0;
					Cores[core].Voices[voice].VolumeL.Increment=0;
					if(value&0x4000)
						value=0x3fff - (value&0x3fff);
					Cores[core].Voices[voice].VolumeL.Value=value<<1;
				}
				Cores[core].Voices[voice].VolumeL.Reg_VOL = value;	break;
			case 1: //VOLR (Volume R)
				if (value & 0x8000) {
					Cores[core].Voices[voice].VolumeR.Mode=(value & 0xF000)>>12;
					Cores[core].Voices[voice].VolumeR.Increment=(value & 0x3F);
				}
				else {
					Cores[core].Voices[voice].VolumeR.Mode=0;
					Cores[core].Voices[voice].VolumeR.Increment=0;
					Cores[core].Voices[voice].VolumeR.Value=value<<1;
				}
				Cores[core].Voices[voice].VolumeR.Reg_VOL = value;	break;
			case 2:	Cores[core].Voices[voice].Pitch=value;			break;
			case 3: // ADSR1 (Envelope)
				Cores[core].Voices[voice].ADSR.Am=(value & 0x8000)>>15;
				Cores[core].Voices[voice].ADSR.Ar=(value & 0x7F00)>>8;
				Cores[core].Voices[voice].ADSR.Dr=(value & 0xF0)>>4;
				Cores[core].Voices[voice].ADSR.Sl=(value & 0xF);
				Cores[core].Voices[voice].ADSR.Reg_ADSR1 = value;	break;
			case 4: // ADSR2 (Envelope)
				Cores[core].Voices[voice].ADSR.Sm=(value & 0xE000)>>13;
				Cores[core].Voices[voice].ADSR.Sr=(value & 0x1FC0)>>6;
				Cores[core].Voices[voice].ADSR.Rm=(value & 0x20)>>5;
				Cores[core].Voices[voice].ADSR.Rr=(value & 0x1F);
				Cores[core].Voices[voice].ADSR.Reg_ADSR2 = value;	break;
			case 5:	Cores[core].Voices[voice].ADSR.Value=value;		break;
			case 6:	Cores[core].Voices[voice].VolumeL.Value=value;	break;
			case 7:	Cores[core].Voices[voice].VolumeR.Value=value;	break;
		}
	}
	else if ((omem >= 0x01C0) && (omem < 0x02DE)) {
		u32 voice   =((omem-0x01C0) / 12);
		u32 address =((omem-0x01C0) % 12)>>1;
		//FileLog("[%10d] SPU2 write mem %08x (Core %d Voice %d Address %s) value %x\n",Cycles,rmem,core,voice,AddressNames[address],value);
		
		switch (address) {
			case 0:	Cores[core].Voices[voice].StartA=((value & 0x0F) << 16) | (Cores[core].Voices[voice].StartA & 0xFFF8); 
					Cores[core].Voices[voice].lastSetStartA = Cores[core].Voices[voice].StartA; 
					break;
			case 1:	Cores[core].Voices[voice].StartA=(Cores[core].Voices[voice].StartA & 0x0F0000) | (value & 0xFFF8); 
					Cores[core].Voices[voice].lastSetStartA = Cores[core].Voices[voice].StartA; 
					//if(core==1) printf(" *** StartA for C%dV%02d set to 0x%05x\n",core,voice,Cores[core].Voices[voice].StartA);
					break;
			case 2:	Cores[core].Voices[voice].LoopStartA=((value & 0x0F) << 16) | (Cores[core].Voices[voice].LoopStartA & 0xFFF8);
					Cores[core].Voices[voice].LoopMode=3; break;
			case 3:	Cores[core].Voices[voice].LoopStartA=(Cores[core].Voices[voice].LoopStartA & 0x0F0000) | (value & 0xFFF8);break;
					Cores[core].Voices[voice].LoopMode=3; break;
			case 4:	Cores[core].Voices[voice].NextA=((value & 0x0F) << 16) | (Cores[core].Voices[voice].NextA & 0xFFF8);
					//printf(" *** Warning: C%dV%02d NextA MODIFIED EXTERNALLY!\n",core,voice);
					break;
			case 5:	Cores[core].Voices[voice].NextA=(Cores[core].Voices[voice].NextA & 0x0F0000) | (value & 0xFFF8);
					//printf(" *** Warning: C%dV%02d NextA MODIFIED EXTERNALLY!\n",core,voice);
					break;
		}
	}
	else
		switch(omem) {
		case REG_C_ATTR:
			RegLog(4,"ATTR",rmem,core,value);
			{
				int irqe=Cores[core].IRQEnable;
				int bit0=Cores[core].AttrBit0;
				int bit4=Cores[core].AttrBit4;

				if(((value>>15)&1)&&(!Cores[core].CoreEnabled)&&(Cores[core].InitDelay==0)) // on init/reset
				{
					if(hasPtr)
					{
						Cores[core].InitDelay=1;
						Cores[core].Regs.STATX=0;	
					}
					else
					{
						CoreReset(core);
					}
				}

				Cores[core].AttrBit0   =(value>> 0) & 0x01; //1 bit
				Cores[core].DMABits	   =(value>> 1) & 0x07; //3 bits
				Cores[core].AttrBit4   =(value>> 4) & 0x01; //1 bit
				Cores[core].AttrBit5   =(value>> 5) & 0x01; //1 bit
				Cores[core].IRQEnable  =(value>> 6) & 0x01; //1 bit
				Cores[core].FxEnable   =(value>> 7) & 0x01; //1 bit
				Cores[core].NoiseClk   =(value>> 8) & 0x3f; //6 bits
				//Cores[core].Mute	   =(value>>14) & 0x01; //1 bit
				Cores[core].Mute=0;
				Cores[core].CoreEnabled=(value>>15) & 0x01; //1 bit
				Cores[core].Regs.ATTR  =value&0x7fff;

				if(value&0x000E)
				{
					ConLog(" * SPU2: Core %d ATTR unknown bits SET! value=%04x\n",core,value);
				}

				if(Cores[core].AttrBit0!=bit0)
				{
					ConLog(" * SPU2: ATTR bit 0 set to %d\n",Cores[core].AttrBit0);
				}
				if(Cores[core].IRQEnable!=irqe)
				{
					ConLog(" * SPU2: IRQ %s\n",((Cores[core].IRQEnable==0)?"disabled":"enabled"));
					if(!Cores[core].IRQEnable)
						Spdif.Info=0;
				}

			}
			break;
		case REG_S_PMON:
			RegLog(1,"PMON0",rmem,core,value);
			vx=2; for (vc=1;vc<16;vc++) { Cores[core].Voices[vc].Modulated=(s8)((value & vx)/vx); vx<<=1; }
			Cores[core].Regs.PMON = (Cores[core].Regs.PMON & 0xFFFF0000) | value;
			break;
		case (REG_S_PMON + 2):
			RegLog(1,"PMON1",rmem,core,value);
			vx=1; for (vc=16;vc<24;vc++) { Cores[core].Voices[vc].Modulated=(s8)((value & vx)/vx); vx<<=1; }
			Cores[core].Regs.PMON = (Cores[core].Regs.PMON & 0xFFFF) | (value << 16);
			break;
		case REG_S_NON:
			RegLog(1,"NON0",rmem,core,value);
			vx=1; for (vc=0;vc<16;vc++) { Cores[core].Voices[vc].Noise=(s8)((value & vx)/vx); vx<<=1; }
			Cores[core].Regs.NON = (Cores[core].Regs.NON & 0xFFFF0000) | value;
			break;
		case (REG_S_NON + 2):
			RegLog(1,"NON1",rmem,core,value);
			vx=1; for (vc=16;vc<24;vc++) { Cores[core].Voices[vc].Noise=(s8)((value & vx)/vx); vx<<=1; }
			Cores[core].Regs.NON = (Cores[core].Regs.NON & 0xFFFF) | (value << 16);
			break;
		case REG_S_VMIXL:
			RegLog(1,"VMIXL0",rmem,core,value);
			vx=1; for (vc=0;vc<16;vc++) { Cores[core].Voices[vc].DryL=(s8)((value & vx)/vx); vx<<=1; }
			Cores[core].Regs.VMIXL = (Cores[core].Regs.VMIXL & 0xFFFF0000) | value;
		case (REG_S_VMIXL + 2):
			RegLog(1,"VMIXL1",rmem,core,value);
			vx=1; for (vc=16;vc<24;vc++) { Cores[core].Voices[vc].DryL=(s8)((value & vx)/vx); vx<<=1; }
			Cores[core].Regs.VMIXL = (Cores[core].Regs.VMIXL & 0xFFFF) | (value << 16);
		case REG_S_VMIXEL:
			RegLog(1,"VMIXEL0",rmem,core,value);
			vx=1; for (vc=0;vc<16;vc++) { Cores[core].Voices[vc].WetL=(s8)((value & vx)/vx); vx<<=1; }
			Cores[core].Regs.VMIXEL = (Cores[core].Regs.VMIXEL & 0xFFFF0000) | value;
			break;
		case (REG_S_VMIXEL + 2):
			RegLog(1,"VMIXEL1",rmem,core,value);
			vx=1; for (vc=16;vc<24;vc++) { Cores[core].Voices[vc].WetL=(s8)((value & vx)/vx); vx<<=1; }
			Cores[core].Regs.VMIXEL = (Cores[core].Regs.VMIXEL & 0xFFFF) | (value << 16);
			break;
		case REG_S_VMIXR:
			RegLog(1,"VMIXR0",rmem,core,value);
			vx=1; for (vc=0;vc<16;vc++) { Cores[core].Voices[vc].DryR=(s8)((value & vx)/vx); vx<<=1; }
			Cores[core].Regs.VMIXR = (Cores[core].Regs.VMIXR & 0xFFFF0000) | value;
			break;
		case (REG_S_VMIXR + 2):
			RegLog(1,"VMIXR1",rmem,core,value);
			vx=1; for (vc=16;vc<24;vc++) { Cores[core].Voices[vc].DryR=(s8)((value & vx)/vx); vx<<=1; }
			Cores[core].Regs.VMIXR = (Cores[core].Regs.VMIXR & 0xFFFF) | (value << 16);
			break;
		case REG_S_VMIXER:
			RegLog(1,"VMIXER0",rmem,core,value);
			vx=1; for (vc=0;vc<16;vc++) { Cores[core].Voices[vc].WetR=(s8)((value & vx)/vx); vx<<=1; }
			Cores[core].Regs.VMIXER = (Cores[core].Regs.VMIXER & 0xFFFF0000) | value;
			break;
		case (REG_S_VMIXER + 2):
			RegLog(1,"VMIXER1",rmem,core,value);
			vx=1; for (vc=16;vc<24;vc++) { Cores[core].Voices[vc].WetR=(s8)((value & vx)/vx); vx<<=1; }
			Cores[core].Regs.VMIXER = (Cores[core].Regs.VMIXER & 0xFFFF) | (value << 16);
			break;
		case REG_P_MMIX:
			RegLog(1,"MMIX",rmem,core,value);
			vx=value;
			if (core == 0) vx&=0xFF0;
			Cores[core].ExtWetR=(vx & 0x001);
			Cores[core].ExtWetL=(vx & 0x002)>>1;
			Cores[core].ExtDryR=(vx & 0x004)>>2;
			Cores[core].ExtDryL=(vx & 0x008)>>3;
			Cores[core].InpWetR=(vx & 0x010)>>4;
			Cores[core].InpWetL=(vx & 0x020)>>5;
			Cores[core].InpDryR=(vx & 0x040)>>6;
			Cores[core].InpDryL=(vx & 0x080)>>7;
			Cores[core].SndWetR=(vx & 0x100)>>8;
			Cores[core].SndWetL=(vx & 0x200)>>9;
			Cores[core].SndDryR=(vx & 0x400)>>10;
			Cores[core].SndDryL=(vx & 0x800)>>11;
			Cores[core].Regs.MMIX = value;
			break;
		case (REG_S_KON + 2):
			RegLog(2,"KON1",rmem,core,value);
			StartVoices(core,((u32)value)<<16);
			break;
		case REG_S_KON:
			RegLog(2,"KON0",rmem,core,value);
			StartVoices(core,((u32)value));
			break;
		case (REG_S_KOFF + 2):
			RegLog(2,"KOFF1",rmem,core,value);
			StopVoices(core,((u32)value)<<16);
			break;
		case REG_S_KOFF:
			RegLog(2,"KOFF0",rmem,core,value);
			StopVoices(core,((u32)value));
			break;
		case REG_S_ENDX:
			//ConLog(" * SPU2: Core %d ENDX cleared!\n",core);
			RegLog(2,"ENDX0",rmem,core,value);
			Cores[core].Regs.ENDX&=0x00FF0000; break;
		case (REG_S_ENDX + 2):	
			//ConLog(" * SPU2: Core %d ENDX cleared!\n",core);
			RegLog(2,"ENDX1",rmem,core,value);
			Cores[core].Regs.ENDX&=0xFFFF; break;
		case REG_P_MVOLL:
			RegLog(1,"MVOLL",rmem,core,value);
			if (value & 0x8000) {  // +Lin/-Lin/+Exp/-Exp
				Cores[core].MasterL.Mode=(value & 0xE000)/0x2000;
				Cores[core].MasterL.Increment=(value & 0x3F) | ((value & 0x800)/0x10);
			}
			else {
				Cores[core].MasterL.Mode=0;
				Cores[core].MasterL.Increment=0;
				Cores[core].MasterL.Value=value;
			}
			Cores[core].MasterL.Reg_VOL=value;
			break;
		case REG_P_MVOLR:
			RegLog(1,"MVOLR",rmem,core,value);
			if (value & 0x8000) {  // +Lin/-Lin/+Exp/-Exp
				Cores[core].MasterR.Mode=(value & 0xE000)/0x2000;
				Cores[core].MasterR.Increment=(value & 0x3F) | ((value & 0x800)/0x10);
			}
			else {
				Cores[core].MasterR.Mode=0;
				Cores[core].MasterR.Increment=0;
				Cores[core].MasterR.Value=value;
			}
			Cores[core].MasterR.Reg_VOL=value;
			break;
		case REG_S_ADMAS:
			RegLog(3,"ADMAS",rmem,core,value);
			ConLog(" * SPU2: Core %d AutoDMAControl set to %d\n",core,value);
			Cores[core].AutoDMACtrl=value;

			if(value==0)
			{
				Cores[core].AdmaInProgress=0;
			}
			break;

		default:
			SPU2writeLog(mem,value);
			
			*(regtable[mem>>1])=value;
			break;
	}

	if ((mem>=0x07C0) && (mem<0x07CE)) 
	{
		UpdateSpdifMode();
	}
}

u16  CALLBACK SPU2read(u32 rmem) 
{

//	if(!replay_mode)
//		s2r_readreg(Cycles,rmem);

	if(hasPtr) TimeUpdate(*cPtr,1);

	u16 ret=0xDEAD; u32 core=0, mem=rmem&0xFFFF, omem=mem;
	if (mem & 0x400) { omem^=0x400; core=1; }

	if(rmem==0x1f9001AC)
	{
		ret =  DmaRead(core);
	}
	else if (rmem>>16 == 0x1f80)
	{
		ret = SPU_ps1_read(rmem);
	}
	else if ((mem&0xFFFF)>=0x800)
	{
		ret=spu2Ru16(mem);
		ConLog(" * SPU2: Read from reg>=0x800: %x value %x\n",mem,ret);
		FileLog(" * SPU2: Read from reg>=0x800: %x value %x\n",mem,ret);
	}
	else 
	{
		ret = *(regtable[(mem>>1)]);

		FileLog("[%10d] SPU2 read mem %x (core %d, register %x): %x\n",Cycles, mem, core, (omem & 0x7ff), ret);
	}

	return ret;
}

void CALLBACK SPU2configure() {
	configure();
}

void CALLBACK SPU2about() {
	SysMessage("%s %d.%d", libraryName, revision, build);
}

s32 CALLBACK SPU2test() {
	return SndTest();
}

typedef struct 
{
	// compatibility with zerospu2
    u32 version;
	u8 unkregs[0x10000];
	u8 mem[0x200000];
    u16 interrupt;
    int nSpuIrq[2];
    u32 dwNewChannel2[2], dwEndChannel2[2];
    u32 dwNoiseVal;
    int iFMod[48];
    u32 MemAddr[2];

	struct ADMA
	{
		unsigned short * MemAddr;
		int			  Index;
		int			  AmountLeft;
		int			  Enabled;
	} adma[2];
    u32 Adma4MemAddr, Adma7MemAddr;

    int SPUCycles, SPUWorkerCycles;
    int SPUStartCycle[2];
    int SPUTargetCycle[2];

    int voicesize;

	// compatibility with zerospu2
	u32 id;
	V_Core Cores[2];
	V_SPDIF Spdif;
	s16 OutPos;
	s16 InputPos;
	u8 InpBuff;
	u32 Cycles;
	s32 uTicks;
	double srate_pv;
	double opitch;
	int osps;
	int PlayMode;

	int lClocks;

} SPU2freezeData;

#define ZEROSPU_VERSION 0x70000001
#define SAVE_ID 0x73326701

s32  CALLBACK SPU2freeze(int mode, freezeData *data){

	SPU2freezeData *spud;

	if (mode == FREEZE_LOAD) {

		spud = (SPU2freezeData*)data->data;


		if((spud->id!=SAVE_ID)&&(spud->version!=ZEROSPU_VERSION))
		{
			printf("Warning: The savestate not compatible with this plugin. Trying to load.\n");
		}
		else
		{
			// base stuff
			memcpy(spu2regs, spud->unkregs, 0x010000);
			memcpy(_spu2mem, spud->mem,     0x200000);

			if(spud->id==SAVE_ID)
			{
				memcpy(Cores, spud->Cores, sizeof(Cores));
				memcpy(&Spdif, &spud->Spdif, sizeof(Spdif));
				OutPos=spud->OutPos;
				InputPos=spud->InputPos;
				InpBuff=spud->InpBuff;
				Cycles=spud->Cycles;
				uTicks=spud->uTicks;
				srate_pv=spud->srate_pv;
				opitch=spud->opitch;
				osps=spud->osps;
				PlayMode=spud->PlayMode;
				lClocks = spud->lClocks;

			}
			else
			{

				Spdif.Info = spud->interrupt;

				Cores[0].MADR = spud->MemAddr[0];
				Cores[1].MADR = spud->MemAddr[1];

				SPU2write(0x1f900000+REG_S_KON+0,(spud->dwNewChannel2[0]));
				SPU2write(0x1f900000+REG_S_KON+2,(spud->dwNewChannel2[0])>>16);

				SPU2write(0x1f900000+REG_S_KON+0x400,(spud->dwNewChannel2[1]));
				SPU2write(0x1f900000+REG_S_KON+0x402,(spud->dwNewChannel2[1])>>16);

				Cores[0].Regs.ENDX = spud->dwEndChannel2[0];
				Cores[1].Regs.ENDX = spud->dwEndChannel2[1];

				for(int i=0;i<24;i++)
				{
					Cores[0].Voices[i].Modulated = spud->iFMod[i];
				}
				for(int i=0;i<24;i++)
				{
					Cores[1].Voices[i].Modulated = spud->iFMod[i+24];
				}
				
				// Zerospu compatibility mode
				for(int i=0;i<0x200;i++)
				{
					int reg = i<<1;
					
					if((reg!=0x1f900000+REG_S_KON)&&(((reg-1)!=0x1f900000+REG_S_KON)))
					{
						SPU2write(reg,spu2regs[i]);
					}
				}

				Cores[0].AdmaInProgress = spud->adma[0].Enabled;
				Cores[0].InputDataLeft = spud->adma[0].AmountLeft;
				Cores[0].InputPos = spud->adma[0].Index&0x1ff;
				Cores[1].AdmaInProgress = spud->adma[1].Enabled;
				Cores[1].InputDataLeft = spud->adma[1].AmountLeft;
				Cores[1].InputPos = spud->adma[1].Index&0x1ff;

				lClocks = 0;

			}
		}

	} else if (mode == FREEZE_SAVE) {

		data->size = sizeof(SPU2freezeData);

		data->data = (s8*)malloc(data->size);

		if (data->data == NULL) return -1;

		spud = (SPU2freezeData*)data->data;

		spud->id=SAVE_ID;
		spud->version=ZEROSPU_VERSION;

		memcpy(spud->unkregs, spu2regs, 0x010000);
		memcpy(spud->mem,     _spu2mem, 0x200000);
		memcpy(spud->Cores, Cores, sizeof(Cores));
		memcpy(&spud->Spdif, &Spdif, sizeof(Spdif));
		spud->OutPos=OutPos;
		spud->InputPos=InputPos;
		spud->InpBuff=InpBuff;
		spud->Cycles=Cycles;
		spud->uTicks=uTicks;
		spud->srate_pv=srate_pv;
		spud->opitch=opitch;
		spud->osps=osps;
		spud->PlayMode=PlayMode;
		spud->lClocks = lClocks;

	} else if (mode == FREEZE_SIZE) {
		data->size = sizeof(SPU2freezeData);
	}

	return 0;

}

void VoiceStart(int core,int vc)
{
	if((Cycles-Cores[core].Voices[vc].PlayCycle)>=4)
	{
		if(Cores[core].Voices[vc].StartA&7)
		{
			printf(" *** Missaligned StartA %05x!\n",Cores[core].Voices[vc].StartA);
			Cores[core].Voices[vc].StartA=(Cores[core].Voices[vc].StartA+0xFFFF8)+0x8;
		}

		Cores[core].Voices[vc].ADSR.Releasing=0;
		Cores[core].Voices[vc].ADSR.Value=1;
		Cores[core].Voices[vc].ADSR.Phase=1;
		Cores[core].Voices[vc].PlayCycle=Cycles;
		Cores[core].Voices[vc].SCurrent=28;
		Cores[core].Voices[vc].LoopMode=0;
		Cores[core].Voices[vc].Loop=0;
		Cores[core].Voices[vc].LoopStart=0;
		Cores[core].Voices[vc].LoopEnd=0;
		Cores[core].Voices[vc].LoopStartA=Cores[core].Voices[vc].StartA;
		Cores[core].Voices[vc].NextA=Cores[core].Voices[vc].StartA;
		Cores[core].Voices[vc].FirstBlock=1;
		Cores[core].Voices[vc].Prev1=0;
		Cores[core].Voices[vc].Prev2=0;

		Cores[core].Voices[vc].PV1=Cores[core].Voices[vc].PV2=0;
		Cores[core].Voices[vc].PV3=Cores[core].Voices[vc].PV4=0;

		Cores[core].Regs.ENDX&=~(1<<vc);


		if(core==1)
		{
			if(MsgKeyOnOff) ConLog(" * SPU2: KeyOn: C%dV%02d: SSA: %8x; M: %s%s%s%s; H: %02x%02x; P: %04x V: %04x/%04x; ADSR: %04x%04x\n",
						core,vc,Cores[core].Voices[vc].StartA,
						(Cores[core].Voices[vc].DryL)?"+":"-",(Cores[core].Voices[vc].DryR)?"+":"-",
						(Cores[core].Voices[vc].WetL)?"+":"-",(Cores[core].Voices[vc].WetR)?"+":"-",
						*(u8*)GetMemPtr(Cores[core].Voices[vc].StartA),*(u8 *)GetMemPtr((Cores[core].Voices[vc].StartA)+1),
						Cores[core].Voices[vc].Pitch,
						Cores[core].Voices[vc].VolumeL.Value,Cores[core].Voices[vc].VolumeR.Value,
						Cores[core].Voices[vc].ADSR.Reg_ADSR1,Cores[core].Voices[vc].ADSR.Reg_ADSR2);
		}
	}
	else
	{
		printf(" *** KeyOn after less than 4 T disregarded.\n");
	}
}

void VoiceStop(int core,int vc)
{
	Cores[core].Voices[vc].ADSR.Value=0;
	Cores[core].Voices[vc].ADSR.Phase=0;
	//Cores[core].Regs.ENDX|=(1<<vc);
}

void StartVoices(int core, u32 value)
{
	int vx=1,vc=0;
	for (vc=0;vc<24;vc++) {
		if ((value>>vc) & 1) {
			VoiceStart(core,vc);
		}
	}
	Cores[core].Regs.ENDX &= ~(value);
	//Cores[core].Regs.ENDX = 0;
}

void StopVoices(int core, u32 value)
{
	u32 vx=1,vc=0;
	for (vc=0;vc<24;vc++) {
		if ((value>>vc) & 1) {
			Cores[core].Voices[vc].ADSR.Releasing=1;
			//if(MsgKeyOnOff) ConLog(" * SPU2: KeyOff: Core %d; Voice %d.\n",core,vc);
		}
	}
}


// if start is 1, starts recording spu2 data, else stops
// returns a non zero value if successful
// for now, pData is not used
int CALLBACK SPU2setupRecording(int start, void* pData)
{
	if(start==0)
	{
		//stop recording
		RecordStop();
		if(recording==0)
			return 1;
	}
	else if(start==1)
	{
		//start recording
		RecordStart();
		if(recording!=0)
			return 1;
	}
	return 0;
}