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

#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

extern bool DebugEnabled;

extern bool MsgToConsole;
extern bool MsgKeyOnOff;
extern bool MsgVoiceOff;
extern bool MsgDMA;
extern bool MsgAutoDMA;

extern bool AccessLog;
extern bool DMALog;
extern bool WaveLog;

extern bool CoresDump;
extern bool MemDump;
extern bool RegDump;

extern char AccessLogFileName[255];
extern char WaveLogFileName[255];

extern char DMA4LogFileName[255];
extern char DMA7LogFileName[255];

extern char CoresDumpFileName[255];
extern char MemDumpFileName[255];
extern char RegDumpFileName[255];

extern int Interpolation;

extern int WaveDumpFormat;

extern int SampleRate;

extern bool EffectsEnabled;

extern int AutoDMAPlayRate[2];

extern int OutputModule;
extern int CurBufferSize;
extern int CurBufferCount;
extern int MaxBufferCount;

extern int VolumeMultiplier;
extern int VolumeDivisor;

extern int LimitMode;

extern char AsioDriver[129];

extern u32 GainL;
extern u32 GainR;
extern u32 GainC;
extern u32 GainLFE;
extern u32 GainSL;
extern u32 GainSR;
extern u32 AddCLR;
extern u32 LowpassLFE;

extern char dspPlugin[];
extern int  dspPluginModule;

extern bool	dspPluginEnabled;
extern bool timeStretchEnabled;

extern bool LimiterToggleEnabled;
extern int  LimiterToggle;

/// module-specific settings

extern char DSoundDevice[];


//////

void ReadSettings();
void WriteSettings();
void configure();

#endif // CONFIG_H_INCLUDED