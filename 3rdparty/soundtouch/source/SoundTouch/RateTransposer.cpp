////////////////////////////////////////////////////////////////////////////////
/// 
/// Sample rate transposer. Changes sample rate by using linear interpolation 
/// together with anti-alias filtering (first order interpolation with anti-
/// alias filtering should be quite adequate for this application)
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
// $Id: RateTransposer.cpp 243 2016-10-15 19:34:59Z oparviai $
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

#include <memory.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "RateTransposer.h"
#include "InterpolateLinear.h"
#include "InterpolateCubic.h"
#include "InterpolateShannon.h"
#include "AAFilter.h"

using namespace soundtouch;

// Define default interpolation algorithm here
TransposerBase::ALGORITHM TransposerBase::algorithm = TransposerBase::CUBIC;


// Constructor
RateTransposer::RateTransposer() : FIFOProcessor(&outputBuffer)
{
    bUseAAFilter = true;

    // Instantiates the anti-alias filter
    pAAFilter = new AAFilter(64);
    pTransposer = TransposerBase::newInstance();
}



RateTransposer::~RateTransposer()
{
    delete pAAFilter;
    delete pTransposer;
}



/// Enables/disables the anti-alias filter. Zero to disable, nonzero to enable
void RateTransposer::enableAAFilter(bool newMode)
{
    bUseAAFilter = newMode;
}


/// Returns nonzero if anti-alias filter is enabled.
bool RateTransposer::isAAFilterEnabled() const
{
    return bUseAAFilter;
}


AAFilter *RateTransposer::getAAFilter()
{
    return pAAFilter;
}



// Sets new target iRate. Normal iRate = 1.0, smaller values represent slower 
// iRate, larger faster iRates.
void RateTransposer::setRate(double newRate)
{
    double fCutoff;

    pTransposer->setRate(newRate);

    // design a new anti-alias filter
    if (newRate > 1.0) 
    {
        fCutoff = 0.5 / newRate;
    } 
    else 
    {
        fCutoff = 0.5 * newRate;
    }
    pAAFilter->setCutoffFreq(fCutoff);
}


// Adds 'nSamples' pcs of samples from the 'samples' memory position into
// the input of the object.
void RateTransposer::putSamples(const SAMPLETYPE *samples, uint nSamples)
{
    processSamples(samples, nSamples);
}


// Transposes sample rate by applying anti-alias filter to prevent folding. 
// Returns amount of samples returned in the "dest" buffer.
// The maximum amount of samples that can be returned at a time is set by
// the 'set_returnBuffer_size' function.
void RateTransposer::processSamples(const SAMPLETYPE *src, uint nSamples)
{
    uint count;

    if (nSamples == 0) return;

    // Store samples to input buffer
    inputBuffer.putSamples(src, nSamples);

    // If anti-alias filter is turned off, simply transpose without applying
    // the filter
    if (bUseAAFilter == false) 
    {
        count = pTransposer->transpose(outputBuffer, inputBuffer);
        return;
    }

    assert(pAAFilter);

    // Transpose with anti-alias filter
    if (pTransposer->rate < 1.0f) 
    {
        // If the parameter 'Rate' value is smaller than 1, first transpose
        // the samples and then apply the anti-alias filter to remove aliasing.

        // Transpose the samples, store the result to end of "midBuffer"
        pTransposer->transpose(midBuffer, inputBuffer);

        // Apply the anti-alias filter for transposed samples in midBuffer
        pAAFilter->evaluate(outputBuffer, midBuffer);
    } 
    else  
    {
        // If the parameter 'Rate' value is larger than 1, first apply the
        // anti-alias filter to remove high frequencies (prevent them from folding
        // over the lover frequencies), then transpose.

        // Apply the anti-alias filter for samples in inputBuffer
        pAAFilter->evaluate(midBuffer, inputBuffer);

        // Transpose the AA-filtered samples in "midBuffer"
        pTransposer->transpose(outputBuffer, midBuffer);
    }
}


// Sets the number of channels, 1 = mono, 2 = stereo
void RateTransposer::setChannels(int nChannels)
{
    assert(nChannels > 0);

    if (pTransposer->numChannels == nChannels) return;
    pTransposer->setChannels(nChannels);

    inputBuffer.setChannels(nChannels);
    midBuffer.setChannels(nChannels);
    outputBuffer.setChannels(nChannels);
}


// Clears all the samples in the object
void RateTransposer::clear()
{
    outputBuffer.clear();
    midBuffer.clear();
    inputBuffer.clear();
}


// Returns nonzero if there aren't any samples available for outputting.
int RateTransposer::isEmpty() const
{
    int res;

    res = FIFOProcessor::isEmpty();
    if (res == 0) return 0;
    return inputBuffer.isEmpty();
}


/// Return approximate initial input-output latency
int RateTransposer::getLatency() const
{
    return (bUseAAFilter) ? pAAFilter->getLength() : 0;
}


//////////////////////////////////////////////////////////////////////////////
//
// TransposerBase - Base class for interpolation
// 

// static function to set interpolation algorithm
void TransposerBase::setAlgorithm(TransposerBase::ALGORITHM a)
{
    TransposerBase::algorithm = a;
}


// Transposes the sample rate of the given samples using linear interpolation. 
// Returns the number of samples returned in the "dest" buffer
int TransposerBase::transpose(FIFOSampleBuffer &dest, FIFOSampleBuffer &src)
{
    int numSrcSamples = src.numSamples();
    int sizeDemand = (int)((double)numSrcSamples / rate) + 8;
    int numOutput;
    SAMPLETYPE *psrc = src.ptrBegin();
    SAMPLETYPE *pdest = dest.ptrEnd(sizeDemand);

#ifndef USE_MULTICH_ALWAYS
    if (numChannels == 1)
    {
        numOutput = transposeMono(pdest, psrc, numSrcSamples);
    }
    else if (numChannels == 2) 
    {
        numOutput = transposeStereo(pdest, psrc, numSrcSamples);
    } 
    else 
#endif // USE_MULTICH_ALWAYS
    {
        assert(numChannels > 0);
        numOutput = transposeMulti(pdest, psrc, numSrcSamples);
    }
    dest.putSamples(numOutput);
    src.receiveSamples(numSrcSamples);
    return numOutput;
}


TransposerBase::TransposerBase()
{
    numChannels = 0;
    rate = 1.0f;
}


TransposerBase::~TransposerBase()
{
}


void TransposerBase::setChannels(int channels)
{
    numChannels = channels;
    resetRegisters();
}


void TransposerBase::setRate(double newRate)
{
    rate = newRate;
}


// static factory function
TransposerBase *TransposerBase::newInstance()
{
#ifdef SOUNDTOUCH_INTEGER_SAMPLES
    // Notice: For integer arithmetics support only linear algorithm (due to simplest calculus)
    return ::new InterpolateLinearInteger;
#else
    switch (algorithm)
    {
        case LINEAR:
            return new InterpolateLinearFloat;

        case CUBIC:
            return new InterpolateCubic;

        case SHANNON:
            return new InterpolateShannon;

        default:
            assert(false);
            return NULL;
    }
#endif
}
