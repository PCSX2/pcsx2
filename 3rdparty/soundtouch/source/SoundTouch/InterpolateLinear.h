////////////////////////////////////////////////////////////////////////////////
/// 
/// Linear interpolation routine.
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

#ifndef _InterpolateLinear_H_
#define _InterpolateLinear_H_

#include "RateTransposer.h"
#include "STTypes.h"

namespace soundtouch
{

/// Linear transposer class that uses integer arithmetic
class InterpolateLinearInteger : public TransposerBase
{
protected:
    int iFract;
    int iRate;

    virtual void resetRegisters();

    virtual int transposeMono(SAMPLETYPE *dest, 
                       const SAMPLETYPE *src, 
                       int &srcSamples);
    virtual int transposeStereo(SAMPLETYPE *dest, 
                         const SAMPLETYPE *src, 
                         int &srcSamples);
    virtual int transposeMulti(SAMPLETYPE *dest, const SAMPLETYPE *src, int &srcSamples);
public:
    InterpolateLinearInteger();

    /// Sets new target rate. Normal rate = 1.0, smaller values represent slower 
    /// rate, larger faster rates.
    virtual void setRate(double newRate);
};


/// Linear transposer class that uses floating point arithmetic
class InterpolateLinearFloat : public TransposerBase
{
protected:
    double fract;

    virtual void resetRegisters();

    virtual int transposeMono(SAMPLETYPE *dest, 
                       const SAMPLETYPE *src, 
                       int &srcSamples);
    virtual int transposeStereo(SAMPLETYPE *dest, 
                         const SAMPLETYPE *src, 
                         int &srcSamples);
    virtual int transposeMulti(SAMPLETYPE *dest, const SAMPLETYPE *src, int &srcSamples);

public:
    InterpolateLinearFloat();
};

}

#endif
