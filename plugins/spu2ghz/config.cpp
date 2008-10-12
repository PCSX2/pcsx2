///GiGaHeRz's SPU2 Driver
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
#include "dialogs.h"
// Config Vars

// DEBUG

bool DebugEnabled=false;
bool MsgToConsole=false;
bool MsgKeyOnOff=false;
bool MsgVoiceOff=false;
bool MsgDMA=false;
bool MsgAutoDMA=false;

bool AccessLog=false;
bool DMALog=false;
bool WaveLog=false;

bool CoresDump=false;
bool MemDump=false;
bool RegDump=false;

char AccessLogFileName[255];
char WaveLogFileName[255];

char DMA4LogFileName[255];
char DMA7LogFileName[255];

char CoresDumpFileName[255];
char MemDumpFileName[255];
char RegDumpFileName[255];

int WaveDumpFormat;

int AutoDMAPlayRate[2]={0,0};

// MIXING
int Interpolation=1;
/* values:
		0: no interpolation (use nearest)
		1. linear interpolation
		2. cubic interpolation
*/

// EFFECTS
bool EffectsEnabled=false;

// OUTPUT
int SampleRate=48000;
int CurBufferSize=1024;
int MaxBufferCount=8;
int CurBufferCount=MaxBufferCount;

int OutputModule=OUTPUT_DSOUND;

int VolumeMultiplier=1;
int VolumeDivisor=1;

int LimitMode=0;
/* values:
	0. No limiter
	1. Soft limiter -- less cpu-intensive, but can cause problems
	2. Hard limiter -- more cpu-intensive while limiting, but should give better (constant) speeds
*/

u32 GainL  =256;
u32 GainR  =256;
u32 GainSL =200;
u32 GainSR =200;
u32 GainC  =200;
u32 GainLFE=256;
u32 AddCLR = 56;
u32 LowpassLFE=80;

// MISC
bool LimiterToggleEnabled=true;
int  LimiterToggle=VK_SUBTRACT;

// DSP
bool dspPluginEnabled=false;
char dspPlugin[256];
int  dspPluginModule=0;

bool timeStretchEnabled=false;

// OUTPUT MODULES
char AsioDriver[129]="";

/// module-specific settings

char DSoundDevice[255];


//////

char CfgFile[]="inis\\SPU2Ghz.ini";


 /*| Config File Format: |¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
+--+---------------------+------------------------+
|												  |
| Option=Value									  |
|												  |
|												  |
| Boolean Values: TRUE,YES,1,T,Y mean 'true',	  |
|                 everything else means 'false'.  |
|												  |
| All Values are limited to 255 chars.			  |
|												  |
+-------------------------------------------------+
 \*_____________________________________________*/


void CfgWriteBool(char *Section, char*Name, bool Value) {
	char *Data=Value?"TRUE":"FALSE";

	WritePrivateProfileString(Section,Name,Data,CfgFile);
}

void CfgWriteInt(char *Section, char*Name, int Value) {
	char Data[255];
	_itoa(Value,Data,10);

	WritePrivateProfileString(Section,Name,Data,CfgFile);
}

void CfgWriteStr(char *Section, char*Name,char *Data) {
	WritePrivateProfileString(Section,Name,Data,CfgFile);
}

/*****************************************************************************/

bool CfgReadBool(char *Section,char *Name,bool Default) {
	char Data[255]="";
	GetPrivateProfileString(Section,Name,"",Data,255,CfgFile);
	Data[254]=0;
	if(strlen(Data)==0) {
		CfgWriteBool(Section,Name,Default);
		return Default;
	}

	if(strcmp(Data,"1")==0) return true;
	if(strcmp(Data,"Y")==0) return true;
	if(strcmp(Data,"T")==0) return true;
	if(strcmp(Data,"YES")==0) return true;
	if(strcmp(Data,"TRUE")==0) return true;
	return false;
}

int CfgReadInt(char *Section, char*Name,int Default) {
	char Data[255]="";
	GetPrivateProfileString(Section,Name,"",Data,255,CfgFile);
	Data[254]=0;

	if(strlen(Data)==0) {
		CfgWriteInt(Section,Name,Default);
		return Default;
	}
	
	return atoi(Data);
}

void CfgReadStr(char *Section, char*Name,char *Data,int DataSize,char *Default) {
	int sl;
	GetPrivateProfileString(Section,Name,"",Data,DataSize,CfgFile);

	if(strlen(Data)==0) { 
		sl=(int)strlen(Default); 
		strncpy(Data,Default,sl>255?255:sl);
		CfgWriteStr(Section,Name,Data);
	}
}

/*****************************************************************************/

void ReadSettings()
{
	
	DebugEnabled=CfgReadBool("DEBUG","Global_Debug_Enabled",0);
	MsgToConsole=DebugEnabled&CfgReadBool("DEBUG","Show_Messages",0);
	MsgKeyOnOff =DebugEnabled&MsgToConsole&CfgReadBool("DEBUG","Show_Messages_Key_On_Off",0);
	MsgVoiceOff =DebugEnabled&MsgToConsole&CfgReadBool("DEBUG","Show_Messages_Voice_Off",0);
	MsgDMA      =DebugEnabled&MsgToConsole&CfgReadBool("DEBUG","Show_Messages_DMA_Transfer",0);
	MsgAutoDMA  =DebugEnabled&MsgToConsole&CfgReadBool("DEBUG","Show_Messages_AutoDMA",0);
	AccessLog   =DebugEnabled&CfgReadBool("DEBUG","Log_Register_Access",0);
	DMALog      =DebugEnabled&CfgReadBool("DEBUG","Log_DMA_Transfers",0);
	WaveLog     =DebugEnabled&CfgReadBool("DEBUG","Log_WAVE_Output",0);

	CoresDump   =DebugEnabled&CfgReadBool("DEBUG","Dump_Info",0);
	MemDump     =DebugEnabled&CfgReadBool("DEBUG","Dump_Memory",0);
	RegDump     =DebugEnabled&CfgReadBool("DEBUG","Dump_Regs",0);

	CfgReadStr("DEBUG","Access_Log_Filename",AccessLogFileName,255,"logs\\SPU2Log.txt");
	CfgReadStr("DEBUG","WaveLog_Filename",   WaveLogFileName,  255,"logs\\SPU2log.wav");
	CfgReadStr("DEBUG","DMA4Log_Filename",   DMA4LogFileName,  255,"logs\\SPU2dma4.dat");
	CfgReadStr("DEBUG","DMA7Log_Filename",   DMA7LogFileName,  255,"logs\\SPU2dma7.dat");

	CfgReadStr("DEBUG","Info_Dump_Filename",CoresDumpFileName,255,"logs\\SPU2Cores.txt");
	CfgReadStr("DEBUG","Mem_Dump_Filename", MemDumpFileName,  255,"logs\\SPU2mem.dat");
	CfgReadStr("DEBUG","Reg_Dump_Filename", RegDumpFileName,  255,"logs\\SPU2regs.dat");

	WaveDumpFormat=CfgReadInt("DEBUG","Wave_Log_Format",0);

		
	Interpolation=CfgReadInt("MIXING","Interpolation",1);

	AutoDMAPlayRate[0]=CfgReadInt("MIXING","AutoDMA_Play_Rate_0",0);
	AutoDMAPlayRate[1]=CfgReadInt("MIXING","AutoDMA_Play_Rate_1",0);

	EffectsEnabled=CfgReadBool("EFFECTS","Enable_Effects",0);

	SampleRate=CfgReadInt("OUTPUT","Sample_Rate",48000);

	CurBufferSize=CfgReadInt("OUTPUT","Buffer_Size",1024);
	MaxBufferCount=CfgReadInt("OUTPUT","Buffer_Count",8);
	if(MaxBufferCount<3)	MaxBufferCount=3;
	CurBufferCount=MaxBufferCount;

	OutputModule=CfgReadInt("OUTPUT","Output_Module",OUTPUT_DSOUND);

	VolumeMultiplier=CfgReadInt("OUTPUT","Volume_Multiplier",1);
	VolumeDivisor   =CfgReadInt("OUTPUT","Volume_Divisor",1);

	GainL  =CfgReadInt("OUTPUT","Channel_Gain_L",  256);
	GainR  =CfgReadInt("OUTPUT","Channel_Gain_R",  256);
	GainC  =CfgReadInt("OUTPUT","Channel_Gain_C",  256);
	GainLFE=CfgReadInt("OUTPUT","Channel_Gain_LFE",256);
	GainSL =CfgReadInt("OUTPUT","Channel_Gain_SL", 200);
	GainSR =CfgReadInt("OUTPUT","Channel_Gain_SR", 200);
	AddCLR =CfgReadInt("OUTPUT","Channel_Center_In_LR", 56);
	LowpassLFE = CfgReadInt("OUTPUT","LFE_Lowpass_Frequency", 80);

	LimitMode=CfgReadInt("OUTPUT","Speed_Limit_Mode",0);

	CfgReadStr("OUTPUT","Asio_Driver_Name",AsioDriver,128,"");

	CfgReadStr("DSP PLUGIN","Filename",dspPlugin,255,"");
	dspPluginModule = CfgReadInt("DSP PLUGIN","ModuleNum",0);
	dspPluginEnabled= CfgReadBool("DSP PLUGIN","Enabled",false);

	timeStretchEnabled = CfgReadBool("DSP","Timestretch_Enable",false);

	LimiterToggleEnabled = CfgReadBool("KEYS","Limiter_Toggle_Enabled",true);
	LimiterToggle        = CfgReadInt ("KEYS","Limiter_Toggle",VK_SUBTRACT);

	CfgReadStr("DirectSound Output (Stereo)","Device",DSoundDevice,255,"");

	if(VolumeMultiplier<0)       VolumeMultiplier=-VolumeMultiplier;
	else if(VolumeMultiplier==0) VolumeMultiplier=1;

	if(VolumeDivisor<0)       VolumeDivisor=-VolumeDivisor;
	else if(VolumeDivisor==0) VolumeDivisor=1;

}

/*****************************************************************************/

void WriteSettings()
{
	CfgWriteBool("DEBUG","Global_Debug_Enabled",DebugEnabled);

	if(DebugEnabled) {
		CfgWriteBool("DEBUG","Show_Messages",             MsgToConsole);
		CfgWriteBool("DEBUG","Show_Messages_Key_On_Off",  MsgKeyOnOff);
		CfgWriteBool("DEBUG","Show_Messages_Voice_Off",   MsgVoiceOff);
		CfgWriteBool("DEBUG","Show_Messages_DMA_Transfer",MsgDMA);
		CfgWriteBool("DEBUG","Show_Messages_AutoDMA",     MsgAutoDMA);

		CfgWriteBool("DEBUG","Log_Register_Access",AccessLog);
		CfgWriteBool("DEBUG","Log_DMA_Transfers",  DMALog);
		CfgWriteBool("DEBUG","Log_WAVE_Output",    WaveLog);

		CfgWriteBool("DEBUG","Dump_Info",  CoresDump);
		CfgWriteBool("DEBUG","Dump_Memory",MemDump);
		CfgWriteBool("DEBUG","Dump_Regs",  RegDump);

		CfgWriteStr("DEBUG","Access_Log_Filename",AccessLogFileName);
		CfgWriteStr("DEBUG","WaveLog_Filename",   WaveLogFileName);
		CfgWriteStr("DEBUG","DMA4Log_Filename",   DMA4LogFileName);
		CfgWriteStr("DEBUG","DMA7Log_Filename",   DMA7LogFileName);

		CfgWriteStr("DEBUG","Info_Dump_Filename",CoresDumpFileName);
		CfgWriteStr("DEBUG","Mem_Dump_Filename", MemDumpFileName);
		CfgWriteStr("DEBUG","Reg_Dump_Filename", RegDumpFileName);

		CfgWriteInt("DEBUG","Wave_Log_Format",   WaveDumpFormat);
	}

	CfgWriteInt("MIXING","Interpolation",Interpolation);

	CfgWriteInt("MIXING","AutoDMA_Play_Rate_0",AutoDMAPlayRate[0]);
	CfgWriteInt("MIXING","AutoDMA_Play_Rate_1",AutoDMAPlayRate[1]);

	CfgWriteBool("EFFECTS","Enable_Effects",EffectsEnabled);

	CfgWriteInt("OUTPUT","Output_Module",OutputModule);
	CfgWriteInt("OUTPUT","Sample_Rate",SampleRate);
	CfgWriteInt("OUTPUT","Buffer_Size",CurBufferSize);
	CfgWriteInt("OUTPUT","Buffer_Count",MaxBufferCount);

	CfgWriteInt("OUTPUT","Volume_Multiplier",VolumeMultiplier);
	CfgWriteInt("OUTPUT","Volume_Divisor",VolumeDivisor);

	CfgWriteInt("OUTPUT","Channel_Gain_L",  GainL);
	CfgWriteInt("OUTPUT","Channel_Gain_R",  GainR);
	CfgWriteInt("OUTPUT","Channel_Gain_C",  GainC);
	CfgWriteInt("OUTPUT","Channel_Gain_LFE",GainLFE);
	CfgWriteInt("OUTPUT","Channel_Gain_SL", GainSL);
	CfgWriteInt("OUTPUT","Channel_Gain_SR", GainSR);
	CfgWriteInt("OUTPUT","Channel_Center_In_LR", AddCLR);
	CfgWriteInt("OUTPUT","LFE_Lowpass_Frequency", LowpassLFE);

	CfgWriteInt("OUTPUT","Speed_Limit_Mode",LimitMode);

	CfgWriteStr("OUTPUT","Asio_Driver_Name",AsioDriver);

	CfgWriteStr("DSP PLUGIN","Filename",dspPlugin);
	CfgWriteInt("DSP PLUGIN","ModuleNum",dspPluginModule);
	CfgWriteBool("DSP PLUGIN","Enabled",dspPluginEnabled);

	CfgWriteBool("DSP","Timestretch_Enable",timeStretchEnabled);

	CfgWriteBool("KEYS","Limiter_Toggle_Enabled",LimiterToggleEnabled);
	CfgWriteInt ("KEYS","Limiter_Toggle",LimiterToggle);

	CfgWriteStr("DirectSound Output (Stereo)","Device",DSoundDevice);
}

BOOL CALLBACK ConfigProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	int wmId,wmEvent;
	char temp[20]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

	switch(uMsg)
	{

		case WM_PAINT:
			SET_CHECK(IDC_EFFECTS, EffectsEnabled);
			SET_CHECK(IDC_DEBUG,   DebugEnabled);
			SET_CHECK(IDC_MSGKEY,  DebugEnabled&MsgToConsole);
			SET_CHECK(IDC_MSGKEY,  DebugEnabled&MsgKeyOnOff);
			SET_CHECK(IDC_MSGVOICE,DebugEnabled&MsgVoiceOff);
			SET_CHECK(IDC_MSGDMA,  DebugEnabled&MsgDMA);
			SET_CHECK(IDC_MSGADMA, DebugEnabled&MsgDMA&MsgAutoDMA);
			SET_CHECK(IDC_LOGREGS, AccessLog);
			SET_CHECK(IDC_LOGDMA,  DMALog);
			SET_CHECK(IDC_LOGWAVE, WaveLog);
			SET_CHECK(IDC_DUMPCORE,CoresDump);
			SET_CHECK(IDC_DUMPMEM, MemDump);
			SET_CHECK(IDC_DUMPREGS,RegDump);
			SET_CHECK(IDC_SPEEDLIMIT,LimitMode);
			return FALSE;
		case WM_INITDIALOG:

			SendMessage(GetDlgItem(hWnd,IDC_SRATE),CB_RESETCONTENT,0,0); 
			SendMessage(GetDlgItem(hWnd,IDC_SRATE),CB_ADDSTRING,0,(LPARAM)"16000");
			SendMessage(GetDlgItem(hWnd,IDC_SRATE),CB_ADDSTRING,0,(LPARAM)"22050");
			SendMessage(GetDlgItem(hWnd,IDC_SRATE),CB_ADDSTRING,0,(LPARAM)"24000");
			SendMessage(GetDlgItem(hWnd,IDC_SRATE),CB_ADDSTRING,0,(LPARAM)"32000");
			SendMessage(GetDlgItem(hWnd,IDC_SRATE),CB_ADDSTRING,0,(LPARAM)"44100");
			SendMessage(GetDlgItem(hWnd,IDC_SRATE),CB_ADDSTRING,0,(LPARAM)"48000");

			sprintf(temp,"%d",SampleRate);
			SetDlgItemText(hWnd,IDC_SRATE,temp);

			SendMessage(GetDlgItem(hWnd,IDC_INTERPOLATE),CB_RESETCONTENT,0,0); 
			SendMessage(GetDlgItem(hWnd,IDC_INTERPOLATE),CB_ADDSTRING,0,(LPARAM)"0 - Nearest (none)");
			SendMessage(GetDlgItem(hWnd,IDC_INTERPOLATE),CB_ADDSTRING,0,(LPARAM)"1 - Linear (mid)");
			SendMessage(GetDlgItem(hWnd,IDC_INTERPOLATE),CB_ADDSTRING,0,(LPARAM)"2 - Cubic (good)");
			SendMessage(GetDlgItem(hWnd,IDC_INTERPOLATE),CB_SETCURSEL,Interpolation,0); 

			SendMessage(GetDlgItem(hWnd,IDC_OUTPUT),CB_RESETCONTENT,0,0); 
			SendMessage(GetDlgItem(hWnd,IDC_OUTPUT),CB_ADDSTRING,0,(LPARAM)"0 - Disabled (Emulate only)");
			SendMessage(GetDlgItem(hWnd,IDC_OUTPUT),CB_ADDSTRING,0,(LPARAM)"1 - waveOut (Slow/Laggy)");
			SendMessage(GetDlgItem(hWnd,IDC_OUTPUT),CB_ADDSTRING,0,(LPARAM)"2 - DSound (Typical)");
			SendMessage(GetDlgItem(hWnd,IDC_OUTPUT),CB_ADDSTRING,0,(LPARAM)"3 - DSound 5.1 (Experimental)");
			SendMessage(GetDlgItem(hWnd,IDC_OUTPUT),CB_ADDSTRING,0,(LPARAM)"4 - ASIO (Low Latency, BROKEN)");
			SendMessage(GetDlgItem(hWnd,IDC_OUTPUT),CB_ADDSTRING,0,(LPARAM)"5 - XAudio2 (Experimental)");
			SendMessage(GetDlgItem(hWnd,IDC_OUTPUT),CB_SETCURSEL,OutputModule,0); 

			INIT_SLIDER(IDC_BUFFER,512,16384,4096,2048,512);

			SendMessage(GetDlgItem(hWnd,IDC_BUFFER),TBM_SETPOS,TRUE,CurBufferSize); 
			sprintf(temp,"%d",CurBufferSize);
			SetWindowText(GetDlgItem(hWnd,IDC_BSIZE),temp);

			sprintf(temp,"%d",MaxBufferCount);
			SetWindowText(GetDlgItem(hWnd,IDC_BCOUNT),temp);

			ENABLE_CONTROL(IDC_MSGSHOW, DebugEnabled);
			ENABLE_CONTROL(IDC_MSGKEY,  DebugEnabled&MsgToConsole);
			ENABLE_CONTROL(IDC_MSGVOICE,DebugEnabled&MsgToConsole);
			ENABLE_CONTROL(IDC_MSGDMA,  DebugEnabled&MsgToConsole);
			ENABLE_CONTROL(IDC_MSGADMA, DebugEnabled&MsgToConsole&MsgDMA);
			ENABLE_CONTROL(IDC_LOGREGS, DebugEnabled);
			ENABLE_CONTROL(IDC_LOGDMA,  DebugEnabled);
			ENABLE_CONTROL(IDC_LOGWAVE, DebugEnabled);
			ENABLE_CONTROL(IDC_DUMPCORE,DebugEnabled);
			ENABLE_CONTROL(IDC_DUMPMEM, DebugEnabled);
			ENABLE_CONTROL(IDC_DUMPREGS,DebugEnabled);

			SET_CHECK(IDC_EFFECTS, EffectsEnabled);
			SET_CHECK(IDC_DEBUG,   DebugEnabled);
			SET_CHECK(IDC_MSGSHOW, MsgToConsole);
			SET_CHECK(IDC_MSGKEY,  MsgKeyOnOff);
			SET_CHECK(IDC_MSGVOICE,MsgVoiceOff);
			SET_CHECK(IDC_MSGDMA,  MsgDMA);
			SET_CHECK(IDC_MSGADMA, MsgAutoDMA);
			SET_CHECK(IDC_LOGREGS, AccessLog);
			SET_CHECK(IDC_LOGDMA,  DMALog);
			SET_CHECK(IDC_LOGWAVE, WaveLog);
			SET_CHECK(IDC_DUMPCORE,CoresDump);
			SET_CHECK(IDC_DUMPMEM, MemDump);
			SET_CHECK(IDC_DUMPREGS,RegDump);
			SET_CHECK(IDC_SPEEDLIMIT,LimitMode);
			SET_CHECK(IDC_DSP_ENABLE,dspPluginEnabled);
			SET_CHECK(IDC_TS_ENABLE,timeStretchEnabled);
			break;
		case WM_COMMAND:
			wmId    = LOWORD(wParam); 
			wmEvent = HIWORD(wParam); 
			// Parse the menu selections:
			switch (wmId)
			{
				case IDOK:
					GetDlgItemText(hWnd,IDC_SRATE,temp,20);
					temp[19]=0;
					SampleRate=atoi(temp);

					GetDlgItemText(hWnd,IDC_BSIZE,temp,20);
					temp[19]=0;
					CurBufferSize=atoi(temp);

					if(CurBufferSize<512)   CurBufferSize=512;
					if(CurBufferSize>16384) CurBufferSize=16384;

					GetDlgItemText(hWnd,IDC_BCOUNT,temp,20);
					temp[19]=0;
					MaxBufferCount=atoi(temp);

					if(MaxBufferCount<4)  MaxBufferCount=4;
					if(MaxBufferCount>50) MaxBufferCount=50;

					Interpolation=(int)SendMessage(GetDlgItem(hWnd,IDC_INTERPOLATE),CB_GETCURSEL,0,0);
					OutputModule=(int)SendMessage(GetDlgItem(hWnd,IDC_OUTPUT),CB_GETCURSEL,0,0);

					/*
					if((BufferSize*CurBufferCount)<9600)
					{
						int ret=MessageBoxEx(hWnd,"The total buffer space is too small and might cause problems.\n"
												  "Press [Cancel] to go back and increase the buffer size or number.",
												  "WARNING",MB_OKCANCEL | MB_ICONEXCLAMATION, 0);
						if(ret==IDCANCEL)
							break;
					}
					*/

					DebugEnabled=DebugEnabled;
					MsgToConsole=DebugEnabled&MsgToConsole;
					MsgKeyOnOff =DebugEnabled&MsgToConsole&MsgKeyOnOff;
					MsgVoiceOff =DebugEnabled&MsgToConsole&MsgVoiceOff;
					MsgDMA      =DebugEnabled&MsgToConsole&MsgDMA;
					MsgAutoDMA  =DebugEnabled&MsgToConsole&MsgAutoDMA;
					AccessLog   =DebugEnabled&AccessLog;
					DMALog      =DebugEnabled&DMALog;
					WaveLog     =DebugEnabled&WaveLog;

					CoresDump   =DebugEnabled&CoresDump;
					MemDump     =DebugEnabled&MemDump;
					RegDump     =DebugEnabled&RegDump;


					WriteSettings();
					EndDialog(hWnd,0);
					break;
				case IDCANCEL:
					EndDialog(hWnd,0);
					break;

				case IDC_OUTCONF:
					SndConfigure(hWnd);
					break;
				

				HANDLE_CHECK(IDC_EFFECTS,EffectsEnabled);
				HANDLE_CHECKNB(IDC_DEBUG,DebugEnabled);
					ENABLE_CONTROL(IDC_MSGSHOW, DebugEnabled);
					ENABLE_CONTROL(IDC_MSGKEY,  DebugEnabled&MsgToConsole);
					ENABLE_CONTROL(IDC_MSGVOICE,DebugEnabled&MsgToConsole);
					ENABLE_CONTROL(IDC_MSGDMA,  DebugEnabled&MsgToConsole);
					ENABLE_CONTROL(IDC_MSGADMA, DebugEnabled&MsgToConsole&MsgDMA);
					ENABLE_CONTROL(IDC_LOGREGS, DebugEnabled);
					ENABLE_CONTROL(IDC_LOGDMA,  DebugEnabled);
					ENABLE_CONTROL(IDC_LOGWAVE, DebugEnabled);
					ENABLE_CONTROL(IDC_DUMPCORE,DebugEnabled);
					ENABLE_CONTROL(IDC_DUMPMEM, DebugEnabled);
					ENABLE_CONTROL(IDC_DUMPREGS,DebugEnabled);
					break;
				HANDLE_CHECKNB(IDC_MSGSHOW,MsgToConsole);
					ENABLE_CONTROL(IDC_MSGKEY,  DebugEnabled&MsgToConsole);
					ENABLE_CONTROL(IDC_MSGVOICE,DebugEnabled&MsgToConsole);
					ENABLE_CONTROL(IDC_MSGDMA,  DebugEnabled&MsgToConsole);
					ENABLE_CONTROL(IDC_MSGADMA, DebugEnabled&MsgToConsole&MsgDMA);
					break;
				HANDLE_CHECK(IDC_MSGKEY,MsgKeyOnOff);
				HANDLE_CHECK(IDC_MSGVOICE,MsgVoiceOff);
				HANDLE_CHECKNB(IDC_MSGDMA,MsgDMA);
					ENABLE_CONTROL(IDC_MSGADMA, DebugEnabled&MsgToConsole&MsgDMA);
					break;
				HANDLE_CHECK(IDC_MSGADMA,MsgAutoDMA);
				HANDLE_CHECK(IDC_LOGREGS,AccessLog);
				HANDLE_CHECK(IDC_LOGDMA, DMALog);
				HANDLE_CHECK(IDC_LOGWAVE,WaveLog);
				HANDLE_CHECK(IDC_DUMPCORE,CoresDump);
				HANDLE_CHECK(IDC_DUMPMEM, MemDump);
				HANDLE_CHECK(IDC_DUMPREGS,RegDump);
				HANDLE_CHECK(IDC_SPEEDLIMIT,LimitMode);
				HANDLE_CHECK(IDC_DSP_ENABLE,dspPluginEnabled);
				HANDLE_CHECK(IDC_TS_ENABLE,timeStretchEnabled);
				default:
					return FALSE;
			}
			break;
		case WM_HSCROLL:
			wmId    = LOWORD(wParam); 
			wmEvent = HIWORD(wParam); 
			switch(wmId) {
				//case TB_ENDTRACK:
				//case TB_THUMBPOSITION:
				case TB_LINEUP:
				case TB_LINEDOWN:
				case TB_PAGEUP:
				case TB_PAGEDOWN:
					wmEvent=(int)SendMessage((HWND)lParam,TBM_GETPOS,0,0);
				case TB_THUMBTRACK:
					if(wmEvent<512) wmEvent=512;
					if(wmEvent>24000) wmEvent=24000;
					SendMessage((HWND)lParam,TBM_SETPOS,TRUE,wmEvent);
					sprintf(temp,"%d",wmEvent);
					SetDlgItemText(hWnd,IDC_BSIZE,temp);
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
 
void configure()
{
	INT_PTR ret;
	ReadSettings();
	ret=DialogBoxParam(hInstance,MAKEINTRESOURCE(IDD_CONFIG),GetActiveWindow(),(DLGPROC)ConfigProc,1);
	if(ret==-1)
	{
		MessageBoxEx(GetActiveWindow(),"Error Opening the config dialog.","OMG ERROR!",MB_OK,0);
		return;
	}
	ReadSettings();
}
