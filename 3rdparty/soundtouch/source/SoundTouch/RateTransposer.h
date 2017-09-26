////////////////////////////////////////////////////////////////////////////////
/// 
/// Sample rate transposer. Changes sample rate by using linear interpolation 
/// together with anti-alias filtering (first order interpolation with anti-
/// alias filtering should be quite adequate for this application).
///
/// Use either of the derived classes of 'RateTransposerInteger' or 
/// 'RateTransposerFloat' for corresponding integer/floating point tranposing
/// algorithm implementation.
///
/// Author        : Copyright (c) Olli Parviainen
/// Author e-mail : oparviai 'at' iki.fi
/// SoundTouch WWW: http://www.surina.net/soundtouch
///
////////////////////////////////////////////////////////////////////////////////
//
// Last changed  : $Date: 2016-10-15 22:34:59 +0300 (la, 15 loka 2016) $
// File revision : $Revision: 4 $
//
// $Id: RateTransposer.h 243 2016-10-15 19:34:59Z oparviai $
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

#ifndef RateTransposer_H
#define RateTransposer_H

#include <stddef.h>
#include "AAFilter.h"
#include "FIFOSamplePipe.h"
#include "FIFOSampleBuffer.h"

#include "STTypes.h"

namespace soundtouch
{

/// Abstract base class for transposer implementations (linear, advanced vs integer, float etc)
class TransposerBase
{
public:
        enum ALGORITHM {
        LINEAR = 0,
        CUBIC,
        SHANNON
    };

protected:
    virtual void resetRegisters() = 0;

    virtual int transposeMono(SAMPLETYPE *dest, 
                        const SAMPLETYPE *src, 
                        int &srcSamples)  = 0;
    virtual int transposeStereo(SAMPLETYPE *dest, 
                        const SAMPLETYPE *src, 
                        int &srcSamples) = 0;
    virtual int transposeMulti(SAMPLETYPE *dest, 
                        const SAMPLETYPE *src, 
                        int &srcSamples) = 0;

    static ALGORITHM algorithm;

public:
    double rate;
    int numChannels;

    TransposerBase();
    virtual ~TransposerBase();

    virtual int transpose(FIFOSampleBuffer &dest, FIFOSampleBuffer &src);
    virtual void setRate(double newRate);
    virtual void setChannels(int channels);

    // static factory function
    static TransposerBase *newInstance();

    // static function to set interpolation algorithm
    static void setAlgorithm(ALGORITHM a);
};


/// A common linear samplerate transposer class.
///
class RateTransposer : public FIFOProcessor
{
protected:
    /// Anti-alias filter object
    AAFilter *pAAFilter;
    TransposerBase *pTransposer;

    /// Buffer for collecting samples to feed the anti-alias filter between
    /// two batches
    FIFOSampleBuffer inputBuffer;

    /// Buffer for keeping samples between transposing & anti-alias filter
    FIFOSampleBuffer midBuffer;

    /// Output sample buffer
    FIFOSampleBuffer outputBuffer;

    bool bUseAAFilter;


    /// Transposes sample rate by applying anti-alias filter to prevent folding. 
    /// Returns amount of samples returned in the "dest" buffer.
    /// The maximum amount of samples that can be returned at a time is set by
    /// the 'set_returnBuffer_size' function.
    void processSamples(const SAMPLETYPE *src, 
                        uint numSamples);

public:
    RateTransposer();
    virtual ~RateTransposer();

    /// Operator 'new' is overloaded so that it automatically creates a suitable instance 
    /// depending on if we're to use integer or floating point arithmetics.
//    static void *operator new(size_t s);

    /// Use this function instead of "new" operator to create a new instance of this class. 
    /// This function automatically chooses a correct implementation, depending on if 
    /// integer ot floating point arithmetics are to be used.
//    static RateTransposer *newInstance();

    /// Returns the output buffer object
    FIFOSamplePipe *getOutput() { return &outputBuffer; };

    /// Returns the store buffer object
//    FIFOSamplePipe *getStore() { return &storeBuffer; };

    /// Return anti-alias filter object
    AAFilter *getAAFilter();

    /// Enables/disables the anti-alias filter. Zero to disable, nonzero to enable
    void enableAAFilter(bool newMode);

    /// Returns nonzero if anti-alias filter is enabled.
    bool isAAFilterEnabled() const;

    /// Sets new target rate. Normal rate = 1.0, smaller values represent slower 
    /// rate, larger faster rates.
    virtual void setRate(double newRate);

    /// Sets the number of channels, 1 = mono, 2 = stereo
    void setChannels(int channels);

    /// Adds 'numSamples' pcs of samples from the 'samples' memory position into
    /// the input of the object.
    void putSamples(const SAMPLETYPE *samples, uint numSamples);

    /// Clears all the samples in the object
    void clear();

    /// Returns nonzero if there aren't any samples available for outputting.
    int isEmpty() const;

    /// Return approximate initial input-output latency
    int getLatency() const;
};

}

#endif
