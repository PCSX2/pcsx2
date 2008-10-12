/*
	Steinberg Audio Stream I/O API
	(c) 1996, Steinberg Soft- und Hardware GmbH
	charlie (May 1996)

	asiodrvr.cpp
	c++ superclass to implement asio functionality. from this,
	you can derive whatever required
*/

#include <string.h>
#include "asiosys.h"
#include "asiodrvr.h"

#if WINDOWS
#error do not use this
AsioDriver::AsioDriver (LPUNKNOWN pUnk, HRESULT *phr) : CUnknown("My AsioDriver", pUnk, phr)
{
}

#else

AsioDriver::AsioDriver()
{
}

#endif

AsioDriver::~AsioDriver()
{
}

ASIOBool AsioDriver::init(void *sysRef)
{
	return ASE_NotPresent;
}

void AsioDriver::getDriverName(char *name)
{
	strcpy(name, "No Driver");
}

long AsioDriver::getDriverVersion()
{
	return 0;
}

void AsioDriver::getErrorMessage(char *string)
{
	strcpy(string, "ASIO Driver Implementation Error!");
}

ASIOError AsioDriver::start()
{
	return ASE_NotPresent;
}

ASIOError AsioDriver::stop()
{
	return ASE_NotPresent;
}

ASIOError AsioDriver::getChannels(long *numInputChannels, long *numOutputChannels)
{
	return ASE_NotPresent;
}

ASIOError AsioDriver::getLatencies(long *inputLatency, long *outputLatency)
{
	return ASE_NotPresent;
}

ASIOError AsioDriver::getBufferSize(long *minSize, long *maxSize,
		long *preferredSize, long *granularity)
{
	return ASE_NotPresent;
}

ASIOError AsioDriver::canSampleRate(ASIOSampleRate sampleRate)
{
	return ASE_NotPresent;
}

ASIOError AsioDriver::getSampleRate(ASIOSampleRate *sampleRate)
{
	return ASE_NotPresent;
}

ASIOError AsioDriver::setSampleRate(ASIOSampleRate sampleRate)
{
	return ASE_NotPresent;
}

ASIOError AsioDriver::getClockSources(ASIOClockSource *clocks, long *numSources)
{
	*numSources = 0;
	return ASE_NotPresent;
}

ASIOError AsioDriver::setClockSource(long reference)
{
	return ASE_NotPresent;
}

ASIOError AsioDriver::getSamplePosition(ASIOSamples *sPos, ASIOTimeStamp *tStamp)
{
	return ASE_NotPresent;
}

ASIOError AsioDriver::getChannelInfo(ASIOChannelInfo *info)
{
	return ASE_NotPresent;
}

ASIOError AsioDriver::createBuffers(ASIOBufferInfo *channelInfos, long numChannels,
		long bufferSize, ASIOCallbacks *callbacks)
{
	return ASE_NotPresent;
}

ASIOError AsioDriver::disposeBuffers()
{
	return ASE_NotPresent;
}

ASIOError AsioDriver::controlPanel()
{
	return ASE_NotPresent;
}

ASIOError AsioDriver::future(long selector, void *opt)
{
	return ASE_NotPresent;
}

ASIOError AsioDriver::outputReady()
{
	return ASE_NotPresent;
}