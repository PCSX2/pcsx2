////////////////////////////////////////////////////////////////////////////////
/// 
/// Cubic interpolation routine.
///
/// Author        : Copyright (c) Olli Parviainen
/// Author e-mail : oparviai 'at' iki.fi
/// SoundTouch WWW: http://www.surina.net/soundtouch
///
////////////////////////////////////////////////////////////////////////////////
//
// $Id: InterpolateCubic.cpp 179 2014-01-06 18:41:42Z oparviai $
//
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

#include <stddef.h>
#include <math.h>
#include "InterpolateCubic.h"
#include "STTypes.h"

using namespace soundtouch;

// cubic interpolation coefficients
static const float _coeffs[]= 
{ -0.5f,  1.0f, -0.5f, 0.0f,
   1.5f, -2.5f,  0.0f, 1.0f,
  -1.5f,  2.0f,  0.5f, 0.0f,
   0.5f, -0.5f,  0.0f, 0.0f};


InterpolateCubic::InterpolateCubic()
{
    fract = 0;
}


void InterpolateCubic::resetRegisters()
{
    fract = 0;
}


/// Transpose mono audio. Returns number of produced output samples, and 
/// updates "srcSamples" to amount of consumed source samples
int InterpolateCubic::transposeMono(SAMPLETYPE *pdest, 
                    const SAMPLETYPE *psrc, 
                    int &srcSamples)
{
    int i;
    int srcSampleEnd = srcSamples - 4;
    int srcCount = 0;

    i = 0;
    while (srcCount < srcSampleEnd)
    {
        float out;
        const float x3 = 1.0f;
        const float x2 = (float)fract;    // x
        const float x1 = x2*x2;           // x^2
        const float x0 = x1*x2;           // x^3
        float y0, y1, y2, y3;

        assert(fract < 1.0);

        y0 =  _coeffs[0] * x0 +  _coeffs[1] * x1 +  _coeffs[2] * x2 +  _coeffs[3] * x3;
        y1 =  _coeffs[4] * x0 +  _coeffs[5] * x1 +  _coeffs[6] * x2 +  _coeffs[7] * x3;
        y2 =  _coeffs[8] * x0 +  _coeffs[9] * x1 + _coeffs[10] * x2 + _coeffs[11] * x3;
        y3 = _coeffs[12] * x0 + _coeffs[13] * x1 + _coeffs[14] * x2 + _coeffs[15] * x3;

        out = y0 * psrc[0] + y1 * psrc[1] + y2 * psrc[2] + y3 * psrc[3];

        pdest[i] = (SAMPLETYPE)out;
        i ++;

        // update position fraction
        fract += rate;
        // update whole positions
        int whole = (int)fract;
        fract -= whole;
        psrc += whole;
        srcCount += whole;
    }
    srcSamples = srcCount;
    return i;
}


/// Transpose stereo audio. Returns number of produced output samples, and 
/// updates "srcSamples" to amount of consumed source samples
int InterpolateCubic::transposeStereo(SAMPLETYPE *pdest, 
                    const SAMPLETYPE *psrc, 
                    int &srcSamples)
{
    int i;
    int srcSampleEnd = srcSamples - 4;
    int srcCount = 0;

    i = 0;
    while (srcCount < srcSampleEnd)
    {
        const float x3 = 1.0f;
        const float x2 = (float)fract;    // x
        const float x1 = x2*x2;           // x^2
        const float x0 = x1*x2;           // x^3
        float y0, y1, y2, y3;
        float out0, out1;

        assert(fract < 1.0);

        y0 =  _coeffs[0] * x0 +  _coeffs[1] * x1 +  _coeffs[2] * x2 +  _coeffs[3] * x3;
        y1 =  _coeffs[4] * x0 +  _coeffs[5] * x1 +  _coeffs[6] * x2 +  _coeffs[7] * x3;
        y2 =  _coeffs[8] * x0 +  _coeffs[9] * x1 + _coeffs[10] * x2 + _coeffs[11] * x3;
        y3 = _coeffs[12] * x0 + _coeffs[13] * x1 + _coeffs[14] * x2 + _coeffs[15] * x3;

        out0 = y0 * psrc[0] + y1 * psrc[2] + y2 * psrc[4] + y3 * psrc[6];
        out1 = y0 * psrc[1] + y1 * psrc[3] + y2 * psrc[5] + y3 * psrc[7];

        pdest[2*i]   = (SAMPLETYPE)out0;
        pdest[2*i+1] = (SAMPLETYPE)out1;
        i ++;

        // update position fraction
        fract += rate;
        // update whole positions
        int whole = (int)fract;
        fract -= whole;
        psrc += 2*whole;
        srcCount += whole;
    }
    srcSamples = srcCount;
    return i;
}


/// Transpose multi-channel audio. Returns number of produced output samples, and 
/// updates "srcSamples" to amount of consumed source samples
int InterpolateCubic::transposeMulti(SAMPLETYPE *pdest, 
                    const SAMPLETYPE *psrc, 
                    int &srcSamples)
{
    int i;
    int srcSampleEnd = srcSamples - 4;
    int srcCount = 0;

    i = 0;
    while (srcCount < srcSampleEnd)
    {
        const float x3 = 1.0f;
        const float x2 = (float)fract;    // x
        const float x1 = x2*x2;           // x^2
        const float x0 = x1*x2;           // x^3
        float y0, y1, y2, y3;

        assert(fract < 1.0);

        y0 =  _coeffs[0] * x0 +  _coeffs[1] * x1 +  _coeffs[2] * x2 +  _coeffs[3] * x3;
        y1 =  _coeffs[4] * x0 +  _coeffs[5] * x1 +  _coeffs[6] * x2 +  _coeffs[7] * x3;
        y2 =  _coeffs[8] * x0 +  _coeffs[9] * x1 + _coeffs[10] * x2 + _coeffs[11] * x3;
        y3 = _coeffs[12] * x0 + _coeffs[13] * x1 + _coeffs[14] * x2 + _coeffs[15] * x3;

        for (int c = 0; c < numChannels; c ++)
        {
            float out;
            out = y0 * psrc[c] + y1 * psrc[c + numChannels] + y2 * psrc[c + 2 * numChannels] + y3 * psrc[c + 3 * numChannels];
            pdest[0] = (SAMPLETYPE)out;
            pdest ++;
        }
        i ++;

        // update position fraction
        fract += rate;
        // update whole positions
        int whole = (int)fract;
        fract -= whole;
        psrc += numChannels*whole;
        srcCount += whole;
    }
    srcSamples = srcCount;
    return i;
}
