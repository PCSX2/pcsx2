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
#include <stdio.h>
#include <string.h>
#include "asio/asiosys.h"
#include "asio/asio.h"
#include "asio/asioDrivers.h"
#include "asio/ASIOConvertSamples.h"

extern double pow_2_31;

class ASIOOutModule: public SndOutModule
{
private:
	bool showBufferInfo;
	bool bufferInfoReady;
	char *bufferSampleType;

	int OutputSamples;

	#ifndef __WIN64__

	// [Air] : This needs fixed.
	static const int BufferSize = SndOutPacketSize;
	static const int BufferSizeBytes = BufferSize << 2;

	s32* asio_lbuffer;

	AsioDrivers *asioDrivers;

	SndBuffer *buff;

	enum
	 {
		kMaxInputChannels = 0,
		kMaxOutputChannels = 2
	};

	// internal data storage
	typedef struct DriverInfo
	{
		// ASIOInit()
		ASIODriverInfo driverInfo;

		// ASIOGetChannels()
		long           inputChannels;
		long           outputChannels;

		// ASIOGeasio_tbufferSize()
		long           minSize;
		long           maxSize;
		long           preferredSize;
		long           granularity;

		// ASIOGetSampleRate()
		ASIOSampleRate sampleRate;

		// ASIOOutputReady()
		bool           postOutput;

		// ASIOGetLatencies ()
		long           inputLatency;
		long           outputLatency;

		// ASIOCreateBuffers ()
		long inputBuffers;	// becomes number of actual created input buffers
		long outputBuffers;	// becomes number of actual created output buffers
		ASIOBufferInfo bufferInfos[kMaxInputChannels + kMaxOutputChannels]; // buffer info's

		// ASIOGetChannelInfo()
		ASIOChannelInfo channelInfos[kMaxInputChannels + kMaxOutputChannels]; // channel info's
		// The above two arrays share the same indexing, as the data in them are linked together

		// Information from ASIOGetSamplePosition()
		// data is converted to double floats for easier use, however 64 bit integer can be used, too
		double         nanoSeconds;
		double         samples;
		double         tcSamples;	// time code samples

		// bufferSwitchTimeInfo()
		ASIOTime       tInfo;			// time info state
		unsigned long  sysRefTime;      // system reference time, when bufferSwitch() was called

		// Signal the end of processing in this example
		bool           stopped;
	} DriverInfo;


	DriverInfo asioDriverInfo;
	ASIOCallbacks asioCallbacks;

	//----------------------------------------------------------------------------------
	long init_asio_static_data ()//DriverInfo *asioDriverInfo)
	{	// collect the informational data of the driver
		// get the number of available channels
		if(ASIOGetChannels(&asioDriverInfo.inputChannels, &asioDriverInfo.outputChannels) == ASE_OK)
		{
			// get the usable buffer sizes
			if(ASIOGetBufferSize(&asioDriverInfo.minSize, &asioDriverInfo.maxSize, &asioDriverInfo.preferredSize, &asioDriverInfo.granularity) == ASE_OK)
			{
				if(ASIOCanSampleRate(SampleRate) != ASE_OK)
				{
					ConLog(" * SPU2: ERROR: Sample rate not supported!\n");
					return -7;
				}

				if(ASIOSetSampleRate(SampleRate) == ASE_OK)
				{
					if(ASIOGetSampleRate(&asioDriverInfo.sampleRate) != ASE_OK)
						return -6;

					if(asioDriverInfo.sampleRate != SampleRate)
					{
						ConLog(" * SPU2: ERROR: Sample rate couldn't be set to the specified value!\n");
						return -8;
					}
				}
				else
					return -5;
			}

			// check wether the driver requires the ASIOOutputReady() optimization
			// (can be used by the driver to reduce output latency by one block)
			if(ASIOOutputReady() == ASE_OK)
				asioDriverInfo.postOutput = true;
			else
				asioDriverInfo.postOutput = false;

			return 0;
		}
		return -1;
	}


	//----------------------------------------------------------------------------------
	// conversion from 64 bit ASIOSample/ASIOTimeStamp to double float
	#if NATIVE_INT64
		#define ASIO64toDouble(a)  (a)
	#else
		#define ASIO64toDouble(a)  ((a).lo + (a).hi * pow_2_31*2)
	#endif

	static ASIOTime *bufferSwitchTimeInfo(ASIOTime *timeInfo, long index, ASIOBool processNow)
	{
		return ASIOMod.TbufferSwitchTimeInfo(timeInfo,index,processNow);
	}

	ASIOTime *TbufferSwitchTimeInfo(ASIOTime *timeInfo, long index, ASIOBool processNow)
	{	// the actual processing callback.
		// Beware that this is normally in a seperate thread, hence be sure that you take care
		// about thread synchronization. This is omitted here for simplicity.
		static int processedSamples = 0;

		// buffer size in samples
		long buffSize = asioDriverInfo.preferredSize;
		static long oldBuffSize=0;

		ASIOConvertSamples converter;

	#define DBL(t) ((t*)(asioDriverInfo.bufferInfos[0].buffers[index]))
	#define DBR(t) ((t*)(asioDriverInfo.bufferInfos[1].buffers[index]))

		int BLen=BufferSize*Config_Asio.NumBuffers;
		int ssize=2;

		if(showBufferInfo)
		{
			switch (asioDriverInfo.channelInfos[0].type)
			{
			case ASIOSTInt16LSB:
				bufferSampleType = "16bit Integer (LSB)";
				break;
			case ASIOSTInt24LSB:		// used for 20 bits as well
				bufferSampleType = "24bit Integer (LSB)";
				break;
			case ASIOSTInt32LSB:
				bufferSampleType = "32bit Integer (LSB)";
				break;
			case ASIOSTInt32LSB16:		// 32 bit data with 16 bit alignment
				bufferSampleType = "32bit Integer with 16bit alignment (LSB)";
				break;
			case ASIOSTInt32LSB18:		// 32 bit data with 18 bit alignment
				bufferSampleType = "32bit Integer with 18bit alignment (LSB)";
				break;
			case ASIOSTInt32LSB20:		// 32 bit data with 20 bit alignment
				bufferSampleType = "32bit Integer with 20bit alignment (LSB)";
				break;
			case ASIOSTInt32LSB24:		// 32 bit data with 24 bit alignment
				bufferSampleType = "32bit Integer with 24bit alignment (LSB)";
				break;
			case ASIOSTFloat32LSB:		// IEEE 754 32 bit float, as found on Intel x86 architecture
				bufferSampleType = "32bit Float (LSB)";
				break;
			case ASIOSTFloat64LSB: 		// IEEE 754 64 bit double float, as found on Intel x86 architecture
				bufferSampleType = "64bit Float (LSB)";
				break;
			case ASIOSTInt16MSB:
				bufferSampleType = "16bit Integer (MSB)";
				break;
			case ASIOSTInt24MSB:		// used for 20 bits as well
				bufferSampleType = "24bit Integer (MSB)";
				break;
			case ASIOSTInt32MSB:
				bufferSampleType = "32bit Integer (MSB)";
				break;
			case ASIOSTInt32MSB16:		// 32 bit data with 16 bit alignment
				bufferSampleType = "32bit Integer with 16bit alignment (MSB)";
				break;
			case ASIOSTInt32MSB18:		// 32 bit data with 18 bit alignment
				bufferSampleType = "32bit Integer with 18bit alignment (MSB)";
				break;
			case ASIOSTInt32MSB20:		// 32 bit data with 20 bit alignment
				bufferSampleType = "32bit Integer with 20bit alignment (MSB)";
				break;
			case ASIOSTInt32MSB24:		// 32 bit data with 24 bit alignment
				bufferSampleType = "32bit Integer with 24bit alignment (MSB)";
				break;
			case ASIOSTFloat32MSB:		// IEEE 754 32 bit float, as found on Intel x86 architecture
				bufferSampleType = "32bit Float (MSB)";
				break;
			case ASIOSTFloat64MSB: 		// IEEE 754 64 bit double float, as found on Intel x86 architecture
				bufferSampleType = "64bit Float (MSB)";
				break;
			}

			bufferInfoReady=true;
		}

		// [Air] : Dunno if this is right...
		//   Maybe there shouldn't be 2 packets? (doesn't make sense for low
		//   latency drivers, but then again using ASIO at all doesn't make sense).
		buff->ReadSamples(asio_lbuffer);
		buff->ReadSamples(&asio_lbuffer[SndOutPacketSize]);
		s32 asio_read_num = 0;

		// perform the processing
		switch (asioDriverInfo.channelInfos[0].type)
		{
		case ASIOSTInt16LSB:
			for(int i=0;i<buffSize;i++)
			{
				DBL(__int16)[i]=asio_lbuffer[asio_read_num++]>>8;
				DBR(__int16)[i]=asio_lbuffer[asio_read_num++]>>8;
			}
			ssize=2;
			break;
		case ASIOSTInt24LSB:		// used for 20 bits as well
			for(int i=0;i<buffSize;i++)
			{
				DBL(__int32)[i]=(s32)(asio_lbuffer[asio_read_num++])<<8;
				DBR(__int32)[i]=(s32)(asio_lbuffer[asio_read_num++])<<8;
			}
			converter.int32to24inPlace(DBL(__int16),buffSize);
			converter.int32to24inPlace(DBR(__int16),buffSize);
			ssize=3;
			break;

		case ASIOSTInt32LSB:
		case ASIOSTInt32LSB16:		// 32 bit data with 16 bit alignment
		case ASIOSTInt32LSB18:		// 32 bit data with 18 bit alignment
		case ASIOSTInt32LSB20:		// 32 bit data with 20 bit alignment
		case ASIOSTInt32LSB24:		// 32 bit data with 24 bit alignment
			for(int i=0;i<buffSize;i++)
			{
				DBL(__int32)[i]=(s32)(asio_lbuffer[asio_read_num++])<<8;
				DBR(__int32)[i]=(s32)(asio_lbuffer[asio_read_num++])<<8;
			}
			ssize=4;
			break;

		case ASIOSTFloat32LSB:		// IEEE 754 32 bit float, as found on Intel x86 architecture
			for(int i=0;i<buffSize;i++)
			{
				DBL(float)[i]=asio_lbuffer[asio_read_num++]/16777216.0f;
				DBR(float)[i]=asio_lbuffer[asio_read_num++]/16777216.0f;
			}
			ssize=4;
			break;

		case ASIOSTFloat64LSB: 		// IEEE 754 64 bit double float, as found on Intel x86 architecture
			for(int i=0;i<buffSize;i++)
			{
				DBL(double)[i]=asio_lbuffer[asio_read_num++]/16777216.0;
				DBR(double)[i]=asio_lbuffer[asio_read_num++]/16777216.0;
			}
			ssize=8;
			break;

		case ASIOSTInt16MSB:
			for(int i=0;i<buffSize;i++)
			{
				DBL(__int16)[i]=asio_lbuffer[asio_read_num++]>>8;
				DBR(__int16)[i]=asio_lbuffer[asio_read_num++]>>8;
			}
			converter.reverseEndian(DBL(__int16),2,buffSize);
			converter.reverseEndian(DBR(__int16),2,buffSize);
			ssize=2;
			break;

		case ASIOSTInt24MSB:		// used for 20 bits as well
			for(int i=0;i<buffSize;i++)
			{
				DBL(__int32)[i]=asio_lbuffer[asio_read_num++]<<8;
				DBR(__int32)[i]=asio_lbuffer[asio_read_num++]<<8;
			}
			converter.int32to24inPlace(DBL(__int16),buffSize);
			converter.int32to24inPlace(DBR(__int16),buffSize);
			converter.reverseEndian(DBL(__int16),3,buffSize);
			converter.reverseEndian(DBR(__int16),3,buffSize);
			ssize=3;
			break;

		case ASIOSTInt32MSB:
		case ASIOSTInt32MSB16:		// 32 bit data with 16 bit alignment
		case ASIOSTInt32MSB18:		// 32 bit data with 18 bit alignment
		case ASIOSTInt32MSB20:		// 32 bit data with 20 bit alignment
		case ASIOSTInt32MSB24:		// 32 bit data with 24 bit alignment
			for(int i=0;i<buffSize;i++)
			{
				DBL(__int32)[i]=(s32)(asio_lbuffer[asio_read_num++])<<8;
				DBR(__int32)[i]=(s32)(asio_lbuffer[asio_read_num++])<<8;
			}
			converter.reverseEndian(DBL(__int16),4,buffSize);
			converter.reverseEndian(DBR(__int16),4,buffSize);
			ssize=4;
			break;

		case ASIOSTFloat32MSB:		// IEEE 754 32 bit float, as found on Intel x86 architecture
			for(int i=0;i<buffSize;i++)
			{
				DBL(float)[i]=asio_lbuffer[asio_read_num++]/16777216.0f;
				DBR(float)[i]=asio_lbuffer[asio_read_num++]/16777216.0f;
			}
			converter.reverseEndian(DBL(__int16),4,buffSize);
			converter.reverseEndian(DBR(__int16),4,buffSize);
			ssize=4;
			break;

		case ASIOSTFloat64MSB: 		// IEEE 754 64 bit double float, as found on Intel x86 architecture
			for(int i=0;i<buffSize;i++)
			{
				DBL(double)[i]=asio_lbuffer[asio_read_num++]/16777216.0;
				DBR(double)[i]=asio_lbuffer[asio_read_num++]/16777216.0;
			}
			converter.reverseEndian(DBL(__int16),8,buffSize);
			converter.reverseEndian(DBR(__int16),8,buffSize);
			ssize=8;
			break;
		}

		// finally if the driver supports the ASIOOutputReady() optimization, do it here, all data are in place
		if (asioDriverInfo.postOutput)
			ASIOOutputReady();

		OutputSamples+=buffSize;

		return 0L;
	}

	//----------------------------------------------------------------------------------
	static void bufferSwitch(long index, ASIOBool processNow)
	{	// the actual processing callback.
		// Beware that this is normally in a seperate thread, hence be sure that you take care
		// about thread synchronization. This is omitted here for simplicity.

		// as this is a "back door" into the bufferSwitchTimeInfo a timeInfo needs to be created
		// though it will only set the timeInfo.samplePosition and timeInfo.systemTime fields and the according flags
		ASIOTime  timeInfo;
		memset (&timeInfo, 0, sizeof (timeInfo));

		// get the time stamp of the buffer, not necessary if no
		// synchronization to other media is required
		//if(ASIOGetSamplePosition(&timeInfo.timeInfo.samplePosition, &timeInfo.timeInfo.systemTime) == ASE_OK)
		//	timeInfo.timeInfo.flags = kSystemTimeValid | kSamplePositionValid;

		bufferSwitchTimeInfo (&timeInfo, index, processNow);
	}


	//----------------------------------------------------------------------------------
	static void sampleRateChanged(ASIOSampleRate sRate)
	{
		// do whatever you need to do if the sample rate changed
		// usually this only happens during external sync.
		// Audio processing is not stopped by the driver, actual sample rate
		// might not have even changed, maybe only the sample rate status of an
		// AES/EBU or S/PDIF digital input at the audio device.
		// You might have to update time/sample related conversion routines, etc.
		ConLog(" * SPU2: ASIO sample rate changed to %f\n",sRate);
	}

	//----------------------------------------------------------------------------------
	static long asioMessages(long selector, long value, void* message, double* opt)
	{
		return ASIOMod.TasioMessages(selector,value,message,opt);
	}
	long TasioMessages(long selector, long value, void* message, double* opt)
	{
		// currently the parameters "value", "message" and "opt" are not used.
		long ret = 0;
		switch(selector)
		{
			case kAsioSelectorSupported:
				if(value == kAsioResetRequest
				|| value == kAsioEngineVersion
				|| value == kAsioResyncRequest
				|| value == kAsioLatenciesChanged
				// the following three were added for ASIO 2.0, you don't necessarily have to support them
				|| value == kAsioSupportsTimeInfo
				|| value == kAsioSupportsTimeCode
				|| value == kAsioSupportsInputMonitor)
					ret = 1L;
				break;
			case kAsioResetRequest:
				// defer the task and perform the reset of the driver during the next "safe" situation
				// You cannot reset the driver right now, as this code is called from the driver.
				// Reset the driver is done by completely destruct is. I.e. ASIOStop(), ASIODisposeBuffers(), Destruction
				// Afterwards you initialize the driver again.
				asioDriverInfo.stopped;  // In this sample the processing will just stop
				ret = 1L;
				break;
			case kAsioResyncRequest:
				// This informs the application, that the driver encountered some non fatal data loss.
				// It is used for synchronization purposes of different media.
				// Added mainly to work around the Win16Mutex problems in Windows 95/98 with the
				// Windows Multimedia system, which could loose data because the Mutex was hold too long
				// by another thread.
				// However a driver can issue it in other situations, too.
				ret = 1L;
				break;
			case kAsioLatenciesChanged:
				// This will inform the host application that the drivers were latencies changed.
				// Beware, it this does not mean that the buffer sizes have changed!
				// You might need to update internal delay data.
				ret = 1L;
				break;
			case kAsioEngineVersion:
				// return the supported ASIO version of the host application
				// If a host applications does not implement this selector, ASIO 1.0 is assumed
				// by the driver
				ret = 2L;
				break;
			case kAsioSupportsTimeInfo:
				// informs the driver wether the asioCallbacks.bufferSwitchTimeInfo() callback
				// is supported.
				// For compatibility with ASIO 1.0 drivers the host application should always support
				// the "old" bufferSwitch method, too.
				ret = 1;
				break;
			case kAsioSupportsTimeCode:
				// informs the driver wether application is interested in time code info.
				// If an application does not need to know about time code, the driver has less work
				// to do.
				ret = 0;
				break;
		}
		return ret;
	}


	//----------------------------------------------------------------------------------
	ASIOError create_asio_buffers () //DriverInfo *asioDriverInfo)
	{	// create buffers for all inputs and outputs of the card with the 
		// preferredSize from ASIOGeasio_tbufferSize() as buffer size
		long i;
		ASIOError result;

		// fill the bufferInfos from the start without a gap
		ASIOBufferInfo *info = asioDriverInfo.bufferInfos;

		// prepare inputs (Though this is not necessaily required, no opened inputs will work, too
		if (asioDriverInfo.inputChannels > kMaxInputChannels)
			asioDriverInfo.inputBuffers = kMaxInputChannels;
		else
			asioDriverInfo.inputBuffers = asioDriverInfo.inputChannels;
		for(i = 0; i < asioDriverInfo.inputBuffers; i++, info++)
		{
			info->isInput = ASIOTrue;
			info->channelNum = i;
			info->buffers[0] = info->buffers[1] = 0;
		}

		// prepare outputs
		if (asioDriverInfo.outputChannels > kMaxOutputChannels)
			asioDriverInfo.outputBuffers = kMaxOutputChannels;
		else
			asioDriverInfo.outputBuffers = asioDriverInfo.outputChannels;
		for(i = 0; i < asioDriverInfo.outputBuffers; i++, info++)
		{
			info->isInput = ASIOFalse;
			info->channelNum = i;
			info->buffers[0] = info->buffers[1] = 0;
		}

		// create and activate buffers
		result = ASIOCreateBuffers(asioDriverInfo.bufferInfos,
			asioDriverInfo.inputBuffers + asioDriverInfo.outputBuffers,
			asioDriverInfo.preferredSize, &asioCallbacks);
		if (result == ASE_OK)
		{
			// now get all the buffer details, sample word length, name, word clock group and activation
			for (i = 0; i < asioDriverInfo.inputBuffers + asioDriverInfo.outputBuffers; i++)
			{
				asioDriverInfo.channelInfos[i].channel = asioDriverInfo.bufferInfos[i].channelNum;
				asioDriverInfo.channelInfos[i].isInput = asioDriverInfo.bufferInfos[i].isInput;
				result = ASIOGetChannelInfo(&asioDriverInfo.channelInfos[i]);
				if (result != ASE_OK)
					break;
			}

			if (result == ASE_OK)
			{
				// get the input and output latencies
				// Latencies often are only valid after ASIOCreateBuffers()
				// (input latency is the age of the first sample in the currently returned audio block)
				// (output latency is the time the first sample in the currently returned audio block requires to get to the output)
				result = ASIOGetLatencies(&asioDriverInfo.inputLatency, &asioDriverInfo.outputLatency);
				if (result == ASE_OK)
					ConLog(" * SPU2: ASIOGetLatencies (input: %d, output: %d);\n", asioDriverInfo.inputLatency, asioDriverInfo.outputLatency);
			}
		}
		return result;
	}

	unsigned long get_sys_reference_time()
	{	// get the system reference time
	#if WINDOWS
		return timeGetTime();
	#elif MAC
	static const double twoRaisedTo32 = 4294967296.;
		UnsignedWide ys;
		Microseconds(&ys);
		double r = ((double)ys.hi * twoRaisedTo32 + (double)ys.lo);
		return (unsigned long)(r / 1000.);
	#endif
	}

	#endif

public:

	bool handling_exception;
	LPTOP_LEVEL_EXCEPTION_FILTER oldFilter;

	static LONG WINAPI UEH(struct _EXCEPTION_POINTERS* ExceptionInfo)
	{
		ASIOMod.handling_exception=true;
		ConLog(" * SPU2: Exception catched. Closing ASIO...\n");
		ASIOMod.Close();
		ASIOMod.handling_exception=false;
		return EXCEPTION_CONTINUE_SEARCH;
	}

	s32  Init(SndBuffer *sb)
	{
		buff=sb;

		oldFilter = SetUnhandledExceptionFilter(UEH);
	#ifndef __WIN64__
		char  driverNameSpace[100*40];
		char* driverNames[100];

		asio_lbuffer= new s32[BufferSize];

		for(int i=0;i<100;i++)
			driverNames[i]=driverNameSpace+(i*40);

		asioDrivers=new AsioDrivers();

		long driverMax=asioDrivers->getDriverNames(driverNames,100);

		long selected=-1;

		ConLog(" * SPU2: ASIO Output Module: There are %u ASIO drivers available:\n",driverMax);
		for(int i=0;i<driverMax;i++)
		{
			ConLog(" *** %u - %s\n",i+1,driverNames[i]);
			if(_stricmp(driverNames[i],AsioDriver)==0)
			{
				selected=i+1;
				break;
			}
		}

		if(strlen(AsioDriver)==0)
			selected=-2;

		if(selected==-1)
			ConLog(" * SPU2: ASIO Output Module: Driver not found. Using Driver: '%s'.\n",driverNames[0]);
		else if(selected==-2)
			ConLog(" * SPU2: ASIO Output Module: Driver not specified. Using Driver: '%s'.\n",driverNames[0]);
		else
			ConLog(" * SPU2: ASIO Output Module: Using driver '%s'.\n",driverNames[selected-1]);

		if(selected<1) selected=1;

		if(!(asioDrivers->loadDriver(driverNames[selected-1])))
		{
			return -1;
		}
		// initialize the driver
		if (ASIOInit (&asioDriverInfo.driverInfo) != ASE_OK)
		{
			asioDrivers->removeCurrentDriver();
			return -1;
		}

		if (init_asio_static_data () != 0)
		{
			ASIOExit();
			asioDrivers->removeCurrentDriver();
			return -1;
		}

		// ASIOControlPanel(); you might want to check wether the ASIOControlPanel() can open

		// set up the asioCallback structure and create the ASIO data buffer
		asioCallbacks.bufferSwitch = &bufferSwitch;
		asioCallbacks.sampleRateDidChange = &sampleRateChanged;
		asioCallbacks.asioMessage = &asioMessages;
		asioCallbacks.bufferSwitchTimeInfo = &bufferSwitchTimeInfo;
		if (create_asio_buffers () != ASE_OK)
		{
			ASIOExit();
			asioDrivers->removeCurrentDriver();
			return -1;
		}

		if (ASIOStart() != ASE_OK)
		{
			ASIODisposeBuffers();
			ASIOExit();
			asioDrivers->removeCurrentDriver();
			return -1;
		}

		return 0;
	#else
		return -1;
	#endif
	}

	void Close()
	{
		if(!handling_exception)
			SetUnhandledExceptionFilter(oldFilter);
	#ifndef __WIN64__
		ASIOStop();
		Sleep(1);
		ASIODisposeBuffers();
		ASIOExit();
		if(asioDrivers) asioDrivers->removeCurrentDriver();
		delete asio_lbuffer;
	#endif
	}


	virtual void Configure(HWND parent)
	{
	}

	virtual bool Is51Out() const { return false; }

	s32  Test() const
	{
	#ifndef __WIN64__
		if(asioDrivers->asioGetNumDev()>0)
			return 0;
	#endif
		return -1;
	}

	int GetEmptySampleCount() const 
	{
		return 0;
	}

	const char* GetIdent() const
	{
		return "asio";
	}

	const char* GetLongName() const
	{
		return "ASIO (BROKEN)";
	}

} ASIOMod;

SndOutModule *ASIOOut=&ASIOMod;
