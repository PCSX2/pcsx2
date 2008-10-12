#ifndef __ASIOConvertSamples__
#define __ASIOConvertSamples__

class ASIOConvertSamples
{
public:
	ASIOConvertSamples();
	~ASIOConvertSamples() {}

	// format converters, input 32 bit integer
	// mono
	void convertMono8(long *source, char *dest, long frames);
	void convertMono8Unsigned(long *source, char *dest, long frames);
	void convertMono16(long *source, short *dest, long frames);
	void convertMono16SmallEndian(long *source, short *dest, long frames);
	void convertMono24(long *source, char *dest, long frames);
	void convertMono24SmallEndian(long *source, char *dest, long frames);

	// stereo interleaved
	void convertStereo8Interleaved(long *left, long *right, char *dest, long frames);
	void convertStereo8InterleavedUnsigned(long *left, long *right, char *dest, long frames);
	void convertStereo16Interleaved(long *left, long *right, short *dest, long frames);
	void convertStereo16InterleavedSmallEndian(long *left, long *right, short *dest, long frames);
	void convertStereo24Interleaved(long *left, long *right, char *dest, long frames);
	void convertStereo24InterleavedSmallEndian(long *left, long *right, char *dest, long frames);

	// stereo split
	void convertStereo8(long *left, long *right, char *dLeft, char *dRight, long frames);
	void convertStereo8Unsigned(long *left, long *right, char *dLeft, char *dRight, long frames);
	void convertStereo16(long *left, long *right, short *dLeft, short *dRight, long frames);
	void convertStereo16SmallEndian(long *left, long *right, short *dLeft, short *dRight, long frames);
	void convertStereo24(long *left, long *right, char *dLeft, char *dRight, long frames);
	void convertStereo24SmallEndian(long *left, long *right, char *dLeft, char *dRight, long frames);

	// integer in place conversions

	void int32msb16to16inPlace(long *in, long frames);
	void int32lsb16to16inPlace(long *in, long frames);
	void int32msb16shiftedTo16inPlace(long *in1, long frames, long shift);
	void int24msbto16inPlace(unsigned char *in, long frames);

	// integer to integer

	void shift32(void* buffer, long shiftAmount, long targetByteWidth,
		bool reverseEndian,	long frames);
	void reverseEndian(void* buffer, long byteWidth, long frames);

	void int32to16inPlace(void* buffer, long frames);
	void int24to16inPlace(void* buffer, long frames);
	void int32to24inPlace(void* buffer, long frames);
	void int16to24inPlace(void* buffer, long frames);
	void int24to32inPlace(void* buffer, long frames);
	void int16to32inPlace(void* buffer, long frames);

	// float to integer
	
	void float32toInt16inPlace(float* buffer, long frames);
	void float32toInt24inPlace(float* buffer, long frames);
	void float32toInt32inPlace(float* buffer, long frames);
};

#endif
