////////////////////////////////////////////////////////////////////////////////
/// 
/// Sample interpolation routine using 8-tap band-limited Shannon interpolation 
/// with kaiser window.
///
/// Notice. This algorithm is remarkably much heavier than linear or cubic
/// interpolation, and not remarkably better than cubic algorithm. Thus mostly
/// for experimental purposes
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

#include <math.h>
#include "InterpolateShannon.h"
#include "STTypes.h"

using namespace soundtouch;


/// Kaiser window with beta = 2.0
/// Values scaled down by 5% to avoid overflows
static const double _kaiser8[8] = 
{
   0.41778693317814,
   0.64888025049173,
   0.83508562409944,
   0.93887857733412,
   0.93887857733412,
   0.83508562409944,
   0.64888025049173,
   0.41778693317814
};


InterpolateShannon::InterpolateShannon()
{
    fract = 0;
}


void InterpolateShannon::resetRegisters()
{
    fract = 0;
}


#define PI 3.1415926536
#define sinc(x) (sin(PI * (x)) / (PI * (x)))

/// Transpose mono audio. Returns number of produced output samples, and 
/// updates "srcSamples" to amount of consumed source samples
int InterpolateShannon::transposeMono(SAMPLETYPE *pdest, 
                    const SAMPLETYPE *psrc, 
                    int &srcSamples)
{
    int i;
    int srcSampleEnd = srcSamples - 8;
    int srcCount = 0;

    i = 0;
    while (srcCount < srcSampleEnd)
    {
        double out;
        assert(fract < 1.0);

        out  = psrc[0] * sinc(-3.0 - fract) * _kaiser8[0];
        out += psrc[1] * sinc(-2.0 - fract) * _kaiser8[1];
        out += psrc[2] * sinc(-1.0 - fract) * _kaiser8[2];
        if (fract < 1e-6)
        {
            out += psrc[3] * _kaiser8[3];     // sinc(0) = 1
        }
        else
        {
            out += psrc[3] * sinc(- fract) * _kaiser8[3];
        }
        out += psrc[4] * sinc( 1.0 - fract) * _kaiser8[4];
        out += psrc[5] * sinc( 2.0 - fract) * _kaiser8[5];
        out += psrc[6] * sinc( 3.0 - fract) * _kaiser8[6];
        out += psrc[7] * sinc( 4.0 - fract) * _kaiser8[7];

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
int InterpolateShannon::transposeStereo(SAMPLETYPE *pdest, 
                    const SAMPLETYPE *psrc, 
                    int &srcSamples)
{
    int i;
    int srcSampleEnd = srcSamples - 8;
    int srcCount = 0;

    i = 0;
    while (srcCount < srcSampleEnd)
    {
        double out0, out1, w;
        assert(fract < 1.0);

        w = sinc(-3.0 - fract) * _kaiser8[0];
        out0 = psrc[0] * w; out1 = psrc[1] * w;
        w = sinc(-2.0 - fract) * _kaiser8[1];
        out0 += psrc[2] * w; out1 += psrc[3] * w;
        w = sinc(-1.0 - fract) * _kaiser8[2];
        out0 += psrc[4] * w; out1 += psrc[5] * w;
        w = _kaiser8[3] * ((fract < 1e-5) ? 1.0 : sinc(- fract));   // sinc(0) = 1
        out0 += psrc[6] * w; out1 += psrc[7] * w;
        w = sinc( 1.0 - fract) * _kaiser8[4];
        out0 += psrc[8] * w; out1 += psrc[9] * w;
        w = sinc( 2.0 - fract) * _kaiser8[5];
        out0 += psrc[10] * w; out1 += psrc[11] * w;
        w = sinc( 3.0 - fract) * _kaiser8[6];
        out0 += psrc[12] * w; out1 += psrc[13] * w;
        w = sinc( 4.0 - fract) * _kaiser8[7];
        out0 += psrc[14] * w; out1 += psrc[15] * w;

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


/// Transpose stereo audio. Returns number of produced output samples, and 
/// updates "srcSamples" to amount of consumed source samples
int InterpolateShannon::transposeMulti(SAMPLETYPE *pdest, 
                    const SAMPLETYPE *psrc, 
                    int &srcSamples)
{
    // not implemented
    assert(false);
    return 0;
}
