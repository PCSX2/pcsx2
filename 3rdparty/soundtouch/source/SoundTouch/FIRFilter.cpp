////////////////////////////////////////////////////////////////////////////////
///
/// General FIR digital filter routines with MMX optimization. 
///
/// Notes : MMX optimized functions reside in a separate, platform-specific file, 
/// e.g. 'mmx_win.cpp' or 'mmx_gcc.cpp'
///
/// This source file contains OpenMP optimizations that allow speeding up the
/// corss-correlation algorithm by executing it in several threads / CPU cores 
/// in parallel. See the following article link for more detailed discussion 
/// about SoundTouch OpenMP optimizations:
/// http://www.softwarecoven.com/parallel-computing-in-embedded-mobile-devices
///
/// Author        : Copyright (c) Olli Parviainen
/// Author e-mail : oparviai 'at' iki.fi
/// SoundTouch WWW: http://www.surina.net/soundtouch
///
////////////////////////////////////////////////////////////////////////////////
//
// License :
//
//  SoundTouch audio processing library
//  Copyright (c) Olli Parviainen
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include "FIRFilter.h"
#include "cpu_detect.h"

using namespace soundtouch;

/*****************************************************************************
 *
 * Implementation of the class 'FIRFilter'
 *
 *****************************************************************************/

FIRFilter::FIRFilter()
{
    resultDivFactor = 0;
    resultDivider = 0;
    length = 0;
    lengthDiv8 = 0;
    filterCoeffs = NULL;
    filterCoeffsStereo = NULL;
}


FIRFilter::~FIRFilter()
{
    delete[] filterCoeffs;
    delete[] filterCoeffsStereo;
}


// Usual C-version of the filter routine for stereo sound
uint FIRFilter::evaluateFilterStereo(SAMPLETYPE *dest, const SAMPLETYPE *src, uint numSamples) const
{
    int j, end;
#ifdef SOUNDTOUCH_FLOAT_SAMPLES
    // when using floating point samples, use a scaler instead of a divider
    // because division is much slower operation than multiplying.
    double dScaler = 1.0 / (double)resultDivider;
#endif
    // hint compiler autovectorization that loop length is divisible by 8
    int ilength = length & -8;

    assert((length != 0) && (length == ilength) && (src != NULL) && (dest != NULL) && (filterCoeffs != NULL));

    end = 2 * (numSamples - ilength);

    #pragma omp parallel for
    for (j = 0; j < end; j += 2) 
    {
        const SAMPLETYPE *ptr;
        LONG_SAMPLETYPE suml, sumr;

        suml = sumr = 0;
        ptr = src + j;

        for (int i = 0; i < ilength; i ++)
        {
            suml += ptr[2 * i] * filterCoeffsStereo[2 * i];
            sumr += ptr[2 * i + 1] * filterCoeffsStereo[2 * i + 1];
        }

#ifdef SOUNDTOUCH_INTEGER_SAMPLES
        suml >>= resultDivFactor;
        sumr >>= resultDivFactor;
        // saturate to 16 bit integer limits
        suml = (suml < -32768) ? -32768 : (suml > 32767) ? 32767 : suml;
        // saturate to 16 bit integer limits
        sumr = (sumr < -32768) ? -32768 : (sumr > 32767) ? 32767 : sumr;
#endif // SOUNDTOUCH_INTEGER_SAMPLES
        dest[j] = (SAMPLETYPE)suml;
        dest[j + 1] = (SAMPLETYPE)sumr;
    }
    return numSamples - ilength;
}


// Usual C-version of the filter routine for mono sound
uint FIRFilter::evaluateFilterMono(SAMPLETYPE *dest, const SAMPLETYPE *src, uint numSamples) const
{
    int j, end;
#ifdef SOUNDTOUCH_FLOAT_SAMPLES
    // when using floating point samples, use a scaler instead of a divider
    // because division is much slower operation than multiplying.
    double dScaler = 1.0 / (double)resultDivider;
#endif

    // hint compiler autovectorization that loop length is divisible by 8
    int ilength = length & -8;

    assert(ilength != 0);

    end = numSamples - ilength;
    #pragma omp parallel for
    for (j = 0; j < end; j ++)
    {
        const SAMPLETYPE *pSrc = src + j;
        LONG_SAMPLETYPE sum;
        int i;

        sum = 0;
        for (i = 0; i < ilength; i ++)
        {
            sum += pSrc[i] * filterCoeffs[i];
        }
#ifdef SOUNDTOUCH_INTEGER_SAMPLES
        sum >>= resultDivFactor;
        // saturate to 16 bit integer limits
        sum = (sum < -32768) ? -32768 : (sum > 32767) ? 32767 : sum;
#endif // SOUNDTOUCH_INTEGER_SAMPLES
        dest[j] = (SAMPLETYPE)sum;
    }
    return end;
}


uint FIRFilter::evaluateFilterMulti(SAMPLETYPE *dest, const SAMPLETYPE *src, uint numSamples, uint numChannels)
{
    int j, end;

#ifdef SOUNDTOUCH_FLOAT_SAMPLES
    // when using floating point samples, use a scaler instead of a divider
    // because division is much slower operation than multiplying.
    double dScaler = 1.0 / (double)resultDivider;
#endif

    assert(length != 0);
    assert(src != NULL);
    assert(dest != NULL);
    assert(filterCoeffs != NULL);
    assert(numChannels < 16);

    // hint compiler autovectorization that loop length is divisible by 8
    int ilength = length & -8;

    end = numChannels * (numSamples - ilength);

    #pragma omp parallel for
    for (j = 0; j < end; j += numChannels)
    {
        const SAMPLETYPE *ptr;
        LONG_SAMPLETYPE sums[16];
        uint c;
        int i;

        for (c = 0; c < numChannels; c ++)
        {
            sums[c] = 0;
        }

        ptr = src + j;

        for (i = 0; i < ilength; i ++)
        {
            SAMPLETYPE coef=filterCoeffs[i];
            for (c = 0; c < numChannels; c ++)
            {
                sums[c] += ptr[0] * coef;
                ptr ++;
            }
        }
        
        for (c = 0; c < numChannels; c ++)
        {
#ifdef SOUNDTOUCH_INTEGER_SAMPLES
            sums[c] >>= resultDivFactor;
#endif // SOUNDTOUCH_INTEGER_SAMPLES
            dest[j+c] = (SAMPLETYPE)sums[c];
        }
    }
    return numSamples - ilength;
}


// Set filter coeffiecients and length.
//
// Throws an exception if filter length isn't divisible by 8
void FIRFilter::setCoefficients(const SAMPLETYPE *coeffs, uint newLength, uint uResultDivFactor)
{
    assert(newLength > 0);
    if (newLength % 8) ST_THROW_RT_ERROR("FIR filter length not divisible by 8");

    #ifdef SOUNDTOUCH_FLOAT_SAMPLES
        // scale coefficients already here if using floating samples
        double scale = 1.0 / resultDivider;
    #else
        short scale = 1;
    #endif

    lengthDiv8 = newLength / 8;
    length = lengthDiv8 * 8;
    assert(length == newLength);

    resultDivFactor = uResultDivFactor;
    resultDivider = (SAMPLETYPE)::pow(2.0, (int)resultDivFactor);

    delete[] filterCoeffs;
    filterCoeffs = new SAMPLETYPE[length];
    delete[] filterCoeffsStereo;
    filterCoeffsStereo = new SAMPLETYPE[length*2];
    for (uint i = 0; i < length; i ++)
    {
        filterCoeffs[i] = (SAMPLETYPE)(coeffs[i] * scale);
        // create also stereo set of filter coefficients: this allows compiler
        // to autovectorize filter evaluation much more efficiently
        filterCoeffsStereo[2 * i] = (SAMPLETYPE)(coeffs[i] * scale);
        filterCoeffsStereo[2 * i + 1] = (SAMPLETYPE)(coeffs[i] * scale);
    }
}


uint FIRFilter::getLength() const
{
    return length;
}


// Applies the filter to the given sequence of samples. 
//
// Note : The amount of outputted samples is by value of 'filter_length' 
// smaller than the amount of input samples.
uint FIRFilter::evaluate(SAMPLETYPE *dest, const SAMPLETYPE *src, uint numSamples, uint numChannels) 
{
    assert(length > 0);
    assert(lengthDiv8 * 8 == length);

    if (numSamples < length) return 0;

#ifndef USE_MULTICH_ALWAYS
    if (numChannels == 1)
    {
        return evaluateFilterMono(dest, src, numSamples);
    } 
    else if (numChannels == 2)
    {
        return evaluateFilterStereo(dest, src, numSamples);
    }
    else
#endif // USE_MULTICH_ALWAYS
    {
        assert(numChannels > 0);
        return evaluateFilterMulti(dest, src, numSamples, numChannels);
    }
}


// Operator 'new' is overloaded so that it automatically creates a suitable instance 
// depending on if we've a MMX-capable CPU available or not.
void * FIRFilter::operator new(size_t s)
{
    // Notice! don't use "new FIRFilter" directly, use "newInstance" to create a new instance instead!
    ST_THROW_RT_ERROR("Error in FIRFilter::new: Don't use 'new FIRFilter', use 'newInstance' member instead!");
    return newInstance();
}


FIRFilter * FIRFilter::newInstance()
{
    uint uExtensions;

    uExtensions = detectCPUextensions();

    // Check if MMX/SSE instruction set extensions supported by CPU

#ifdef SOUNDTOUCH_ALLOW_MMX
    // MMX routines available only with integer sample types
    if (uExtensions & SUPPORT_MMX)
    {
        return ::new FIRFilterMMX;
    }
    else
#endif // SOUNDTOUCH_ALLOW_MMX

#ifdef SOUNDTOUCH_ALLOW_SSE
    if (uExtensions & SUPPORT_SSE)
    {
        // SSE support
        return ::new FIRFilterSSE;
    }
    else
#endif // SOUNDTOUCH_ALLOW_SSE

    {
        // ISA optimizations not supported, use plain C version
        return ::new FIRFilter;
    }
}
