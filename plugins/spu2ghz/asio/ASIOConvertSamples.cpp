#include "ginclude.h"
#include "ASIOConvertSamples.h"
#include <math.h>

#if MAC
#define TRUNCATE 0

#elif ASIO_CPU_X86 || ASIO_CPU_SPARC || ASIO_CPU_MIPS
#define TRUNCATE 1
#undef MAXFLOAT
#define MAXFLOAT 0x7fffff00L
#endif

ASIOConvertSamples::ASIOConvertSamples()
{
}


//-------------------------------------------------------------------------------------------
// mono

void ASIOConvertSamples::convertMono8Unsigned(long *source, char *dest, long frames)
{
	unsigned char *c = (unsigned char *)source;
	unsigned char a;

	dest--;
	while(--frames >= 0)
	{
#if ASIO_LITTLE_ENDIAN
		a = c[3];
#else
		a = c[0];
#endif
		c += 4;
		a -= 0x80U;
		*++dest = a;
	}
}

void ASIOConvertSamples::convertMono8(long *source, char *dest, long frames)
{
	char *c = (char *)source;
	char a;

	dest--;
	while(--frames >= 0)
	{
#if ASIO_LITTLE_ENDIAN
		a = c[3];
#else
		a = c[0];
#endif
		c += 4;
		*++dest = a;
	}
}

void ASIOConvertSamples::convertMono16(long *source, short *dest, long frames)
{
#if ASIO_LITTLE_ENDIAN
	char* s = (char*)source;
	char* d = (char*)dest;
	while(--frames >= 0)
	{
		*d++ = s[3];	// dest big endian, msb first
		*d++ = s[2];
		s += 4;
	}
#else
	long l;

	source--;
	dest--;
	while(--frames >= 0)
	{
		l = *++source;
		*++dest = (short)(l >> 16);
	}	
#endif
}

void ASIOConvertSamples::convertMono24(long *source, char *dest, long frames)
{
	// work with chars in order to prevent misalignments
	char *s = (char *)source;
	char a, b, c;

	dest--;
	while(--frames >= 0)
	{
#if ASIO_LITTLE_ENDIAN
		a = s[3];	// msb
		b = s[2];
		c = s[1];	// lsb
#else
		a = s[0];
		b = s[1];
		c = s[2];
#endif
		s += 4;
		*++dest = a;	// big endian, msb first
		*++dest = b;
		*++dest = c;
	}
}

// small endian

void ASIOConvertSamples::convertMono16SmallEndian(long *source, short *dest, long frames)
{
	char *s = (char *)source;
	char *d = (char *)dest;
	char a, b;

	d--;
	while(--frames >= 0)
	{
#if ASIO_LITTLE_ENDIAN
		a = s[3];
		b = s[2];
#else
		a = s[0];
		b = s[1];
#endif
		s += 4;
		*++d = b;	// dest small endian, lsb first
		*++d = a;
	}
}

void ASIOConvertSamples::convertMono24SmallEndian(long *source, char *dest, long frames)
{
	// work with chars in order to prevent misalignments
	char *s = (char *)source;
	char a, b, c;

	dest--;
	while(--frames >= 0)
	{
#if ASIO_LITTLE_ENDIAN
		a = s[3];
		b = s[2];
		c = s[1];
#else
		a = s[0];
		b = s[1];
		c = s[2];
#endif
		s += 4;
		*++dest = c;	// lsb first
		*++dest = b;
		*++dest = a;
	}
}


//-------------------------------------------------------------------------------------------
// stereo interleaved

void ASIOConvertSamples::convertStereo8InterleavedUnsigned(long *left, long *right, char *dest, long frames)
{
	unsigned char *cl = (unsigned char *)left;
	unsigned char *cr = (unsigned char *)right;
	unsigned char a, b;

	dest--;
	while(--frames >= 0)
	{
#if ASIO_LITTLE_ENDIAN
		a = cl[3];
		b = cr[3];
#else
		a = cl[0];
		b = cr[0];
#endif
		cl += 4;
		cr += 4;
		a -= 0x80U;
		b -= 0x80U;
		*++dest = a;
		*++dest = b;
	}	
}

void ASIOConvertSamples::convertStereo8Interleaved(long *left, long *right, char *dest, long frames)
{
	unsigned char *cl = (unsigned char *)left;
	unsigned char *cr = (unsigned char *)right;
	unsigned char a, b;

	dest--;
	while(--frames >= 0)
	{
#if ASIO_LITTLE_ENDIAN
		a = cl[3];
		b = cr[3];
#else
		a = cl[0];
		b = cr[0];
#endif
		cl += 4;
		cr += 4;
		*++dest = a;
		*++dest = b;
	}	
}

void ASIOConvertSamples::convertStereo16Interleaved(long *left, long *right, short *dest, long frames)
{
#if ASIO_LITTLE_ENDIAN
	char* sl = (char*)left;
	char* sr = (char*)right;
	char* d = (char*)dest;
	while(--frames >= 0)
	{
		*d++ = sl[3];	// msb first
		*d++ = sl[2];
		*d++ = sr[3];
		*d++ = sr[2];
		sl += 4;
		sr += 4;
	}
#else
	long l, r;

	left--;
	right--;
	dest--;
	while(--frames >= 0)
	{
		l = *++left;
		r = *++right;
		*++dest = (short)(l >> 16);
		*++dest = (short)(r >> 16);
	}
#endif	
}

void ASIOConvertSamples::convertStereo24Interleaved(long *left, long *right, char *dest, long frames)
{
	// work with chars in order to prevent misalignments
	char *sl = (char *)left;
	char *sr = (char *)right;
	char al, bl, cl, ar, br, cr;

	dest--;
	while(--frames >= 0)
	{
#if ASIO_LITTLE_ENDIAN
		al = sl[3];
		bl = sl[2];
		cl = sl[1];
		ar = sr[3];
		br = sr[2];
		cr = sr[1];
#else
		al = sl[0];
		bl = sl[1];
		cl = sl[2];
		ar = sr[0];
		br = sr[1];
		cr = sr[2];
#endif
		sl += 4;
		sr += 4;
		*++dest = al;
		*++dest = bl;
		*++dest = cl;
		*++dest = ar;
		*++dest = br;
		*++dest = cr;
	}
}

void ASIOConvertSamples::convertStereo16InterleavedSmallEndian(long *left, long *right, short *dest, long frames)
{
	char *sl = (char *)left;
	char *sr = (char *)right;
	char *d =  (char *)dest;
	char al, bl, ar, br;

	d--;
	while(--frames >= 0)
	{
#if ASIO_LITTLE_ENDIAN
		al = sl[3];
		bl = sl[2];
		ar = sr[3];
		br = sr[2];
#else
		al = sl[0];
		bl = sl[1];
		ar = sr[0];
		br = sr[1];
#endif
		sl += 4;
		sr += 4;
		*++d = bl;	// lsb first
		*++d = al;
		*++d = br;
		*++d = ar;
	}
}

void ASIOConvertSamples::convertStereo24InterleavedSmallEndian(long *left, long *right, char *dest, long frames)
{
	// work with chars in order to prevent misalignments
	char *sl = (char *)left;
	char *sr = (char *)right;
	char al, bl, cl, ar, br, cr;

	dest--;
	while(--frames >= 0)
	{
#if ASIO_LITTLE_ENDIAN
		al = sl[3];
		bl = sl[2];
		cl = sl[1];
		ar = sr[3];
		br = sr[2];
		cr = sr[1];
#else
		al = sl[0];
		bl = sl[1];
		cl = sl[2];
		ar = sr[0];
		br = sr[1];
		cr = sr[2];
#endif
		sl += 4;
		sr += 4;
		*++dest = cl;
		*++dest = bl;
		*++dest = al;
		*++dest = cr;
		*++dest = br;
		*++dest = ar;
	}
}


//-------------------------------------------------------------------------------------------
// stereo split

void ASIOConvertSamples::convertStereo8Unsigned(long *left, long *right, char *dLeft, char *dRight, long frames)
{
	unsigned char *cl = (unsigned char *)left;
	unsigned char *cr = (unsigned char *)right;
	unsigned char a, b;

	dLeft--;
	dRight--;
	while(--frames >= 0)
	{
#if ASIO_LITTLE_ENDIAN
		a = cl[3];
		b = cr[3];
#else
		a = cl[0];
		b = cr[0];
#endif
		cl += 4;
		cr += 4;
		a -= 0x80U;
		b -= 0x80U;
		*++dLeft = a;
		*++dRight = b;
	}	
}

void ASIOConvertSamples::convertStereo8(long *left, long *right, char *dLeft, char *dRight, long frames)
{
	unsigned char *cl = (unsigned char *)left;
	unsigned char *cr = (unsigned char *)right;
	unsigned char a, b;

	dLeft--;
	dRight--;
	while(--frames >= 0)
	{
#if ASIO_LITTLE_ENDIAN
		a = cl[3];
		b = cr[3];
#else
		a = cl[0];
		b = cr[0];
#endif
		cl += 4;
		cr += 4;
		*++dLeft = a;
		*++dRight = b;
	}	
}

void ASIOConvertSamples::convertStereo16(long *left, long *right, short *dLeft, short *dRight, long frames)
{
#if ASIO_LITTLE_ENDIAN
	char* sl = (char*)left;
	char* sr = (char*)right;
	char* dl = (char*)dLeft;
	char* dr = (char*)dRight;
	while(--frames >= 0)
	{
		*dl++ = sl[3];	// msb first
		*dl++ = sl[2];
		*dr++ = sr[3];
		*dr++ = sr[2];
		sl += 4;
		sr += 4;
	}
#else
	long l, r;

	left--;
	right--;
	dLeft--;
	dRight--;
	while(--frames >= 0)
	{
		l = *++left;
		r = *++right;
		*++dLeft = (short)(l >> 16);
		*++dRight = (short)(r >> 16);
	}
#endif	
}

void ASIOConvertSamples::convertStereo24(long *left, long *right, char *dLeft, char *dRight, long frames)
{
	// work with chars in order to prevent misalignments
	char *sl = (char *)left;
	char *sr = (char *)right;
	char al, bl, cl, ar, br, cr;

	dLeft--;
	dRight--;
	while(--frames >= 0)
	{
#if ASIO_LITTLE_ENDIAN
		al = sl[3];
		bl = sl[2];
		cl = sl[1];
		ar = sr[3];
		br = sr[2];
		cr = sr[1];
#else
		al = sl[0];
		bl = sl[1];
		cl = sl[2];
		ar = sr[0];
		br = sr[1];
		cr = sr[2];
#endif
		sl += 4;
		sr += 4;
		*++dLeft = al;
		*++dLeft = bl;
		*++dLeft = cl;
		*++dRight = ar;
		*++dRight = br;
		*++dRight = cr;
	}
}

// small endian
void ASIOConvertSamples::convertStereo16SmallEndian(long *left, long *right, short *dLeft, short *dRight, long frames)
{
	char *sl = (char *)left;
	char *sr = (char *)right;
	char *dl = (char *)dLeft;
	char *dr = (char *)dRight;
	char al, bl, ar, br;

	dl--;
	dr--;
	while(--frames >= 0)
	{
#if ASIO_LITTLE_ENDIAN
		al = sl[3];
		bl = sl[2];
		ar = sr[3];
		br = sr[2];
#else
		al = sl[0];
		bl = sl[1];
		ar = sr[0];
		br = sr[1];
#endif
		sl += 4;
		sr += 4;
		*++dl = bl;
		*++dl = al;
		*++dr = br;
		*++dr = ar;
	}
}

void ASIOConvertSamples::convertStereo24SmallEndian(long *left, long *right, char *dLeft, char *dRight, long frames)
{
	// work with chars in order to prevent misalignments
	char *sl = (char *)left;
	char *sr = (char *)right;
	char al, bl, cl, ar, br, cr;

	dLeft--;
	dRight--;
	while(--frames >= 0)
	{
#if ASIO_LITTLE_ENDIAN
		al = sl[3];
		bl = sl[2];
		cl = sl[1];
		ar = sr[3];
		br = sr[2];
		cr = sr[1];
#else
		al = sl[0];
		bl = sl[1];
		cl = sl[2];
		ar = sr[0];
		br = sr[1];
		cr = sr[2];
#endif
		sl += 4;
		sr += 4;
		*++dLeft = cl;
		*++dLeft = bl;
		*++dLeft = al;
		*++dRight = cr;
		*++dRight = br;
		*++dRight = ar;
	}
}

//------------------------------------------------------------------------------------------
// in place integer conversions

void ASIOConvertSamples::int32msb16to16inPlace(long *in, long frames)
{
	short *d1 = (short *)in;
	short* out = d1;
#if ASIO_LITTLE_ENDIAN
	d1++;
#endif
	while(--frames >= 0)
	{
		*out++ = *d1;
		d1 += 2;
	}
}

void ASIOConvertSamples::int32lsb16to16inPlace(long *in, long frames)
{
	short *d1 = (short *)in;
	short* out = d1;
#if !ASIO_LITTLE_ENDIAN
	d1++;
#endif
	while(--frames >= 0)
	{
		*out++ = *d1;
		d1 += 2;
	}
}

void ASIOConvertSamples::int32msb16shiftedTo16inPlace(long *in, long frames, long shift)
{
	short* out = (short*)in;
	while(--frames >= 0)
		*out++ = (short)(*in++ >> shift);
}

void ASIOConvertSamples::int24msbto16inPlace(unsigned char *in, long frames)
{
	short a;
	short* out = (short*)in;
	while(--frames >= 0)
	{
#if ASIO_LITTLE_ENDIAN
		a = (short)in[2];
		a <<= 8;
		a |= (in[1] & 0xff);
#else
		a = (short)in[0];
		a <<= 8;
		a |= (in[1] & 0xff);
#endif
		*out++ = a;
		in += 3;
	}
}

//-----------------------------------------------------------------------------------------

void ASIOConvertSamples::shift32(void* buffer, long shiftAmount, long targetByteWidth,
	bool revertEndian,	long sampleFrames)
{
	long a;
	long frames = sampleFrames;
	long* source = (long*)buffer;
	if(revertEndian)
	{
		reverseEndian(buffer, 4, sampleFrames);
		revertEndian = false;
	}

	if(targetByteWidth == 2)
	{
		short* dest = (short*)buffer;
		short* al = (short*)&a;
#if ASIO_LITTLE_ENDIAN
		al++;
#endif
		while(--frames >= 0)
		{
			a = *source++;
			a <<= shiftAmount;
			*dest++ = *al;
		}
	}
	
	else if(targetByteWidth == 3)
	{
		char* dest = (char*)buffer;
		source = (long*)buffer;
		char* aa = (char*)&a;
		while(--frames >= 0)
		{
			a = *source++;
			a <<= shiftAmount;
#if ASIO_LITTLE_ENDIAN
			dest[0] = aa[1];	// lsb
			dest[1] = aa[2];
			dest[2] = aa[3];	// msb
#else
			dest[0] = aa[0];	// msb
			dest[1] = aa[1];
			dest[2] = aa[2];	// lsb
#endif
			dest += 3;
		}
	}
	
	else if(targetByteWidth == 4)
	{
		long* dest = source;
		while(--frames >= 0)
			*dest++ = *source++ << shiftAmount;
	}
}

void ASIOConvertSamples::reverseEndian(void* buffer, long byteWidth, long frames)
{
	char* a = (char*)buffer;
	char* b = a;
	char c; 
	if(byteWidth == 2)
	{
		while(--frames >= 0)
		{
			c = a[0];
			a[0] = a[1];
			a[1] = c;
			a += 2;
		}
	}
	else if(byteWidth == 3)
	{
		while(--frames >= 0)
		{
			c = a[0];
			a[0] = a[2];
			a[2] = c;
			a += 3;
		}
	}
	else if(byteWidth == 4)
	{
		while(--frames >= 0)
		{
			c = a[0];
			a[0] = a[3];
			a[3] = c;
			c = a[1];
			a[1] = a[2];
			a[2] = c;
			a += 4;
		}
	}
	else if(byteWidth == 8)
	{
		while(--frames >= 0)
		{
			c = a[0];
			a[0] = a[7];
			a[7] = c;
			c = a[1];
			a[1] = a[6];
			a[6] = c;
			c = a[2];
			a[2] = a[5];
			a[5] = c;
			c = a[3];
			a[3] = a[4];
			a[4] = c;
			a += 4;
		}
	}
}

//-------------------------------------------------------------------------------------------------

void ASIOConvertSamples::int32to16inPlace(void* buffer, long frames)
{
	short* in = (short*)buffer;
	short* out = in;
#if ASIO_LITTLE_ENDIAN
	in++;
#endif
	while(--frames >= 0)
	{
		*out++ = *in;
		in += 2;
	}
}

void ASIOConvertSamples::int24to16inPlace(void* buffer, long frames)
{
	char* from = (char*)buffer;
	char* to = from;
	while(--frames >= 0)
	{
#if ASIO_LITTLE_ENDIAN
		to[0] = from[1];
		to[1] = from[2];
#else
		to[0] = from[0];
		to[1] = from[1];
#endif
		from += 3;
		to += 2;
	}
}

void ASIOConvertSamples::int32to24inPlace(void* buffer, long frames)
{
	long* in = (long*)buffer;
	char* out = (char*)buffer;
	long a;
	while(--frames >= 0)
	{
		a = *in++;
		a >>= 8;			// 32->24
#if ASIO_LITTLE_ENDIAN
		out[0] = (char)a;	// lsb
		a >>= 8;
		out[1] = (char)a;
		a >>= 8;
		out[2] = (char)a;
#else
		out[2] = (char)a;	// lsb
		a >>= 8;
		out[1] = (char)a;
		a >>= 8;
		out[0] = (char)a;
#endif
		out += 3;
	}
}

void ASIOConvertSamples::int16to24inPlace(void* buffer, long frames)
{
	char* in = (char*)buffer;
	char* out = (char*)buffer;
	in += frames * 2;
	out += frames * 3;
	while(--frames >= 0)
	{
		out -= 3;
		in -= 2;
#if ASIO_LITTLE_ENDIAN
		out[2] = in[1];	// msb
		out[1] = in[0];	// lsb
		out[0] = 0;
#else
		out[2] = 0;
		out[1] = in[1];	// lsb
		out[0] = in[0];	// msb
#endif
	}
}

void ASIOConvertSamples::int24to32inPlace(void* buffer, long frames)
{
	long a, b, c;
	char* in = (char*)buffer;
	long* out = (long*)buffer;
	in += (frames * 3);
	out += frames;
	while(--frames >= 0)
	{
#if ASIO_LITTLE_ENDIAN
		a = (long)in[-1];	// msb
		b = (long)in[-2];
		c = (long)in[-3];
#else
		a = (long)in[-3];	// msb
		b = (long)in[-2];
		c = (long)in[-1];
#endif
		a <<= 24;
		b <<= 16;
		b &= 0x00ff0000;
		a |= b;
		c <<= 8;
		c &= 0x0000ff00;
		a |= c;
		*--out = a;
		in -= 3;
	}
}

void ASIOConvertSamples::int16to32inPlace(void* buffer, long frames)
{
	short* in = (short*)buffer;
	long* out = (long*)buffer;
	in += frames;
	out += frames;
	while(--frames >= 0)
		*--out = ((long)(*--in)) << 16;
}

//------------------------------------------------------------------------------------------
// float to int

const double fScaler16 = (double)0x7fffL;
const double fScaler24 = (double)0x7fffffL;
const double fScaler32 = (double)0x7fffffffL;

void ASIOConvertSamples::float32toInt16inPlace(float* buffer, long frames)
{
	double sc = fScaler16 + .49999;
	short* b = (short*)buffer;
	while(--frames >= 0)
		*b++ = (short)((double)(*buffer++) * sc);
}

void ASIOConvertSamples::float32toInt24inPlace(float* buffer, long frames)
{
	double sc = fScaler24 + .49999;
	long a;
	char* b = (char*)buffer;
	char* aa = (char*)&a;
	
	while(--frames >= 0)
	{
		a = (long)((double)(*buffer++) * sc);
#if ASIO_LITTLE_ENDIAN
		*b++ = aa[3];
		*b++ = aa[2];
		*b++ = aa[1];
#else
		*b++ = aa[1];
		*b++ = aa[2];
		*b++ = aa[3];
#endif
	}
}

void ASIOConvertSamples::float32toInt32inPlace(float* buffer, long frames)
{
	double sc = fScaler32 + .49999;
	long* b = (long*)buffer;
	while(--frames >= 0)
		*b++ = (long)((double)(*buffer++) * sc);
}

