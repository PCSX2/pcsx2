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
#ifndef SNDOUT_H_INCLUDE
#define SNDOUT_H_INCLUDE

#define OUTPUT_NULL		0
#define OUTPUT_WAVEOUT	1
#define OUTPUT_DSOUND	2
#define OUTPUT_DSOUND51	3
#define OUTPUT_ASIO		4
#define OUTPUT_XAUDIO2	5

#define pcmlog
extern FILE *wavelog;

s32  SndInit();
void SndClose();
s32  SndWrite(s32 ValL, s32 ValR);
s32  SndTest();
void SndConfigure(HWND parent);
bool SndGetStats(u32 *written, u32 *played);

class SndBuffer
{
public:
	virtual ~SndBuffer() {}

	virtual void WriteSamples(s32 *buffer, s32 nSamples)=0;
	virtual void ReadSamples (s32 *buffer, s32 nSamples)=0;

	virtual void PauseOnWrite(bool doPause)=0;

	virtual s32  GetBufferUsage()=0;
	virtual s32  GetBufferSize()=0;
};

class SndOutModule
{
public:
	virtual s32  Init(SndBuffer *buffer)=0;
	virtual void Close()=0;
	virtual s32  Test()=0;
	virtual void Configure(HWND parent)=0;

	virtual bool Is51Out()=0;
};

//internal
extern SndOutModule *WaveOut;
extern SndOutModule *DSoundOut;
extern SndOutModule *FModOut;
extern SndOutModule *ASIOOut;
extern SndOutModule *XAudio2Out;
extern SndOutModule *DSound51Out;

extern SndOutModule* mods[];
extern const u32 mods_count;

#endif // SNDOUT_H_INCLUDE
