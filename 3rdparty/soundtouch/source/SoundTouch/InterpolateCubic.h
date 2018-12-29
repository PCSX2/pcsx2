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

#ifndef _InterpolateCubic_H_
#define _InterpolateCubic_H_

#include "RateTransposer.h"
#include "STTypes.h"

namespace soundtouch
{

class InterpolateCubic : public TransposerBase
{
protected:
    virtual void resetRegisters();
    virtual int transposeMono(SAMPLETYPE *dest, 
                        const SAMPLETYPE *src, 
                        int &srcSamples);
    virtual int transposeStereo(SAMPLETYPE *dest, 
                        const SAMPLETYPE *src, 
                        int &srcSamples);
    virtual int transposeMulti(SAMPLETYPE *dest, 
                        const SAMPLETYPE *src, 
                        int &srcSamples);

    double fract;

public:
    InterpolateCubic();
};

}

#endif
