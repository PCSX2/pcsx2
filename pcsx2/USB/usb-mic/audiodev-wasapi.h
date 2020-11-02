// Used OBS as example

#include "audiodeviceproxy.h"
#include "../libsamplerate/samplerate.h"
#include "shared/ringbuffer.h"

#include <mmdeviceapi.h>
#include <audioclient.h>

namespace usb_mic { namespace audiodev_wasapi {

static const char *APINAME = "wasapi";

class MMAudioDevice : public AudioDevice
{
public:
	MMAudioDevice(int port, const char* dev_type, int device, AudioDir dir): AudioDevice(port, dev_type, device, dir)
	, mmCapture(NULL)
	, mmRender(NULL)
	, mmClient(NULL)
	, mmDevice(NULL)
	, mmClock(NULL)
	, mmEnumerator(NULL)
	, mResampler(NULL)
	, mDeviceLost(false)
	, mResample(false)
	, mFirstSamples(true)
	, mSamplesPerSec(48000)
	, mResampleRatio(1.0)
	, mTimeAdjust(1.0)
	, mThread(INVALID_HANDLE_VALUE)
	, mQuit(false)
	, mPaused(true)
	, mLastGetBufferMS(0)
	, mBuffering(50)
	{
		mMutex = CreateMutex(NULL, FALSE, TEXT("ResampledQueueMutex"));
		if(!Init())
			throw AudioDeviceError("MMAudioDevice:: device name is empty, skipping");
		if(!Reinitialize())
			throw AudioDeviceError("MMAudioDevice:: WASAPI init failed!");
	}

	~MMAudioDevice();
	void FreeData();
	bool Init();
	bool Reinitialize();
	void Start();
	void Stop();
	void ResetBuffers();
	//TODO or just return samples count in mOutBuffer?
	virtual bool GetFrames(uint32_t *size);

	static unsigned WINAPI CaptureThread(LPVOID ptr);
	static unsigned WINAPI RenderThread(LPVOID ptr);

	virtual uint32_t GetBuffer(int16_t *outBuf, uint32_t outFrames);
	virtual uint32_t SetBuffer(int16_t *inBuf, uint32_t inFrames);
	/*
		Returns read frame count.
	*/
	uint32_t GetMMBuffer();
	virtual void SetResampling(int samplerate);
	virtual uint32_t GetChannels()
	{
		return mDeviceChannels;
	}

	virtual bool Compare(AudioDevice* compare)
	{
		if (compare)
		{
			MMAudioDevice *src = static_cast<MMAudioDevice *>(compare);
			if (src && mDevID == src->mDevID)
				return true;
		}
		return false;
	}

	static const char* TypeName()
	{
		return APINAME;
	}

	static const TCHAR* Name()
	{
		return TEXT("WASAPI");
	}

	static bool AudioInit();

	static void AudioDeinit()
	{
	}

	static void AudioDevices(std::vector<AudioDeviceInfo> &devices, AudioDir dir);
	static int Configure(int port, const char* dev_type, void *data);

private:
	IMMDeviceEnumerator *mmEnumerator;

	IMMDevice           *mmDevice;
	IAudioClient        *mmClient;
	IAudioCaptureClient *mmCapture;
	IAudioRenderClient  *mmRender;
	IAudioClock         *mmClock;

	bool  mResample;
	bool  mFloat;
	bool  mFirstSamples; //On the first call, empty the buffer to lower latency
	UINT  mDeviceChannels;
	UINT  mDeviceSamplesPerSec;
	UINT  mSamplesPerSec;
	UINT  mDeviceBitsPerSample;
	UINT  mDeviceBlockSize;
	DWORD mInputChannelMask;

	std::wstring mDevID;
	bool mDeviceLost;
	std::wstring mDeviceName;
	LONGLONG mBuffering;

	SRC_STATE *mResampler;
	double mResampleRatio;
	// Speed up or slow down audio
	double mTimeAdjust;
	RingBuffer mInBuffer;
	RingBuffer mOutBuffer;
	HANDLE mThread;
	HANDLE mMutex;
	bool mQuit;
	bool mPaused;
	LONGLONG mLastGetBufferMS;

	LONGLONG mTime = 0;
	int mSamples = 0;
	LONGLONG mLastTimeMS = 0;
	LONGLONG mLastTimeNS = 0;
};

}} // namespace