////////////////////////////////////////////////////////////////////////////////
///
/// Beats-per-minute (BPM) detection routine.
///
/// The beat detection algorithm works as follows:
/// - Use function 'inputSamples' to input a chunks of samples to the class for
///   analysis. It's a good idea to enter a large sound file or stream in smallish
///   chunks of around few kilosamples in order not to extinguish too much RAM memory.
/// - Inputted sound data is decimated to approx 500 Hz to reduce calculation burden,
///   which is basically ok as low (bass) frequencies mostly determine the beat rate.
///   Simple averaging is used for anti-alias filtering because the resulting signal
///   quality isn't of that high importance.
/// - Decimated sound data is enveloped, i.e. the amplitude shape is detected by
///   taking absolute value that's smoothed by sliding average. Signal levels that
///   are below a couple of times the general RMS amplitude level are cut away to
///   leave only notable peaks there.
/// - Repeating sound patterns (e.g. beats) are detected by calculating short-term 
///   autocorrelation function of the enveloped signal.
/// - After whole sound data file has been analyzed as above, the bpm level is 
///   detected by function 'getBpm' that finds the highest peak of the autocorrelation 
///   function, calculates it's precise location and converts this reading to bpm's.
///
/// Author        : Copyright (c) Olli Parviainen
/// Author e-mail : oparviai 'at' iki.fi
/// SoundTouch WWW: http://www.surina.net/soundtouch
///
////////////////////////////////////////////////////////////////////////////////
//
// Last changed  : $Date: 2016-01-05 22:59:57 +0200 (ti, 05 tammi 2016) $
// File revision : $Revision: 4 $
//
// $Id: BPMDetect.cpp 237 2016-01-05 20:59:57Z oparviai $
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

#include <math.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "FIFOSampleBuffer.h"
#include "PeakFinder.h"
#include "BPMDetect.h"

using namespace soundtouch;

#define INPUT_BLOCK_SAMPLES       2048
#define DECIMATED_BLOCK_SAMPLES   256

/// Target sample rate after decimation
const int target_srate = 1000;

/// XCorr update sequence size, update in about 200msec chunks
const int xcorr_update_sequence = 200;

/// XCorr decay time constant, decay to half in 30 seconds
/// If it's desired to have the system adapt quicker to beat rate 
/// changes within a continuing music stream, then the 
/// 'xcorr_decay_time_constant' value can be reduced, yet that
/// can increase possibility of glitches in bpm detection.
const double xcorr_decay_time_constant = 30.0;

////////////////////////////////////////////////////////////////////////////////

// Enable following define to create bpm analysis file:

// #define _CREATE_BPM_DEBUG_FILE

#ifdef _CREATE_BPM_DEBUG_FILE

    #define DEBUGFILE_NAME  "c:\\temp\\soundtouch-bpm-debug.txt"

    static void _SaveDebugData(const float *data, int minpos, int maxpos, double coeff)
    {
        FILE *fptr = fopen(DEBUGFILE_NAME, "wt");
        int i;

        if (fptr)
        {
            printf("\n\nWriting BPM debug data into file " DEBUGFILE_NAME "\n\n");
            for (i = minpos; i < maxpos; i ++)
            {
                fprintf(fptr, "%d\t%.1lf\t%f\n", i, coeff / (double)i, data[i]);
            }
            fclose(fptr);
        }
    }
#else
    #define _SaveDebugData(a,b,c,d)
#endif

////////////////////////////////////////////////////////////////////////////////


BPMDetect::BPMDetect(int numChannels, int aSampleRate)
{
    this->sampleRate = aSampleRate;
    this->channels = numChannels;

    decimateSum = 0;
    decimateCount = 0;

    // choose decimation factor so that result is approx. 1000 Hz
    decimateBy = sampleRate / target_srate;
    assert(decimateBy > 0);
    assert(INPUT_BLOCK_SAMPLES < decimateBy * DECIMATED_BLOCK_SAMPLES);

    // Calculate window length & starting item according to desired min & max bpms
    windowLen = (60 * sampleRate) / (decimateBy * MIN_BPM);
    windowStart = (60 * sampleRate) / (decimateBy * MAX_BPM);

    assert(windowLen > windowStart);

    // allocate new working objects
    xcorr = new float[windowLen];
    memset(xcorr, 0, windowLen * sizeof(float));

    // allocate processing buffer
    buffer = new FIFOSampleBuffer();
    // we do processing in mono mode
    buffer->setChannels(1);
    buffer->clear();
}



BPMDetect::~BPMDetect()
{
    delete[] xcorr;
    delete buffer;
}



/// convert to mono, low-pass filter & decimate to about 500 Hz. 
/// return number of outputted samples.
///
/// Decimation is used to remove the unnecessary frequencies and thus to reduce 
/// the amount of data needed to be processed as calculating autocorrelation 
/// function is a very-very heavy operation.
///
/// Anti-alias filtering is done simply by averaging the samples. This is really a 
/// poor-man's anti-alias filtering, but it's not so critical in this kind of application
/// (it'd also be difficult to design a high-quality filter with steep cut-off at very 
/// narrow band)
int BPMDetect::decimate(SAMPLETYPE *dest, const SAMPLETYPE *src, int numsamples)
{
    int count, outcount;
    LONG_SAMPLETYPE out;

    assert(channels > 0);
    assert(decimateBy > 0);
    outcount = 0;
    for (count = 0; count < numsamples; count ++) 
    {
        int j;

        // convert to mono and accumulate
        for (j = 0; j < channels; j ++)
        {
            decimateSum += src[j];
        }
        src += j;

        decimateCount ++;
        if (decimateCount >= decimateBy) 
        {
            // Store every Nth sample only
            out = (LONG_SAMPLETYPE)(decimateSum / (decimateBy * channels));
            decimateSum = 0;
            decimateCount = 0;
#ifdef SOUNDTOUCH_INTEGER_SAMPLES
            // check ranges for sure (shouldn't actually be necessary)
            if (out > 32767) 
            {
                out = 32767;
            } 
            else if (out < -32768) 
            {
                out = -32768;
            }
#endif // SOUNDTOUCH_INTEGER_SAMPLES
            dest[outcount] = (SAMPLETYPE)out;
            outcount ++;
        }
    }
    return outcount;
}



// Calculates autocorrelation function of the sample history buffer
void BPMDetect::updateXCorr(int process_samples)
{
    int offs;
    SAMPLETYPE *pBuffer;
    
    assert(buffer->numSamples() >= (uint)(process_samples + windowLen));

    pBuffer = buffer->ptrBegin();

    // calculate decay factor for xcorr filtering
    float xcorr_decay = (float)pow(0.5, 1.0 / (xcorr_decay_time_constant * target_srate / process_samples));

    #pragma omp parallel for
    for (offs = windowStart; offs < windowLen; offs ++) 
    {
        LONG_SAMPLETYPE sum;
        int i;

        sum = 0;
        for (i = 0; i < process_samples; i ++) 
        {
            sum += pBuffer[i] * pBuffer[i + offs];    // scaling the sub-result shouldn't be necessary
        }
        xcorr[offs] *= xcorr_decay;   // decay 'xcorr' here with suitable time constant.

        xcorr[offs] += (float)fabs(sum);
    }
}



void BPMDetect::inputSamples(const SAMPLETYPE *samples, int numSamples)
{
    SAMPLETYPE decimated[DECIMATED_BLOCK_SAMPLES];

    // iterate so that max INPUT_BLOCK_SAMPLES processed per iteration
    while (numSamples > 0)
    {
        int block;
        int decSamples;

        block = (numSamples > INPUT_BLOCK_SAMPLES) ? INPUT_BLOCK_SAMPLES : numSamples;

        // decimate. note that converts to mono at the same time
        decSamples = decimate(decimated, samples, block);
        samples += block * channels;
        numSamples -= block;

        buffer->putSamples(decimated, decSamples);
    }

    // when the buffer has enought samples for processing...
    while ((int)buffer->numSamples() >= windowLen + xcorr_update_sequence) 
    {
        // ... calculate autocorrelations for oldest samples...
        updateXCorr(xcorr_update_sequence);
        // ... and remove these from the buffer
        buffer->receiveSamples(xcorr_update_sequence);
    }
}



void BPMDetect::removeBias()
{
    int i;
    float minval = 1e12f;   // arbitrary large number

    for (i = windowStart; i < windowLen; i ++)
    {
        if (xcorr[i] < minval)
        {
            minval = xcorr[i];
        }
    }

    for (i = windowStart; i < windowLen; i ++)
    {
        xcorr[i] -= minval;
    }
}


float BPMDetect::getBpm()
{
    double peakPos;
    double coeff;
    PeakFinder peakFinder;

    coeff = 60.0 * ((double)sampleRate / (double)decimateBy);

    // save bpm debug analysis data if debug data enabled
    _SaveDebugData(xcorr, windowStart, windowLen, coeff);

    // remove bias from xcorr data
    removeBias();

    // find peak position
    peakPos = peakFinder.detectPeak(xcorr, windowStart, windowLen);

    assert(decimateBy != 0);
    if (peakPos < 1e-9) return 0.0; // detection failed.

    // calculate BPM
    return (float) (coeff / peakPos);
}
