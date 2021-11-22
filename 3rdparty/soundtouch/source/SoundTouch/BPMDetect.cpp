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

#define _USE_MATH_DEFINES

#include <math.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <cfloat>
#include "FIFOSampleBuffer.h"
#include "PeakFinder.h"
#include "BPMDetect.h"

using namespace soundtouch;

// algorithm input sample block size
static const int INPUT_BLOCK_SIZE = 2048;

// decimated sample block size
static const int DECIMATED_BLOCK_SIZE = 256;

/// Target sample rate after decimation
static const int TARGET_SRATE = 1000;

/// XCorr update sequence size, update in about 200msec chunks
static const int XCORR_UPDATE_SEQUENCE = (int)(TARGET_SRATE / 5);

/// Moving average N size
static const int MOVING_AVERAGE_N = 15;

/// XCorr decay time constant, decay to half in 30 seconds
/// If it's desired to have the system adapt quicker to beat rate 
/// changes within a continuing music stream, then the 
/// 'xcorr_decay_time_constant' value can be reduced, yet that
/// can increase possibility of glitches in bpm detection.
static const double XCORR_DECAY_TIME_CONSTANT = 30.0;

/// Data overlap factor for beat detection algorithm
static const int OVERLAP_FACTOR = 4;

static const double TWOPI = (2 * M_PI);

////////////////////////////////////////////////////////////////////////////////

// Enable following define to create bpm analysis file:

//#define _CREATE_BPM_DEBUG_FILE

#ifdef _CREATE_BPM_DEBUG_FILE

    static void _SaveDebugData(const char *name, const float *data, int minpos, int maxpos, double coeff)
    {
        FILE *fptr = fopen(name, "wt");
        int i;

        if (fptr)
        {
            printf("\nWriting BPM debug data into file %s\n", name);
            for (i = minpos; i < maxpos; i ++)
            {
                fprintf(fptr, "%d\t%.1lf\t%f\n", i, coeff / (double)i, data[i]);
            }
            fclose(fptr);
        }
    }

    void _SaveDebugBeatPos(const char *name, const std::vector<BEAT> &beats)
    {
        printf("\nWriting beat detections data into file %s\n", name);

        FILE *fptr = fopen(name, "wt");
        if (fptr)
        {
            for (uint i = 0; i < beats.size(); i++)
            {
                BEAT b = beats[i];
                fprintf(fptr, "%lf\t%lf\n", b.pos, b.strength);
            }
            fclose(fptr);
        }
    }
#else
    #define _SaveDebugData(name, a,b,c,d)
    #define _SaveDebugBeatPos(name, b)
#endif

// Hamming window
void hamming(float *w, int N)
{
    for (int i = 0; i < N; i++)
    {
        w[i] = (float)(0.54 - 0.46 * cos(TWOPI * i / (N - 1)));
    }

}

////////////////////////////////////////////////////////////////////////////////
//
// IIR2_filter - 2nd order IIR filter

IIR2_filter::IIR2_filter(const double *lpf_coeffs)
{
    memcpy(coeffs, lpf_coeffs, 5 * sizeof(double));
    memset(prev, 0, sizeof(prev));
}


float IIR2_filter::update(float x)
{
    prev[0] = x;
    double y = x * coeffs[0];

    for (int i = 4; i >= 1; i--)
    {
        y += coeffs[i] * prev[i];
        prev[i] = prev[i - 1];
    }

    prev[3] = y;
    return (float)y;
}


// IIR low-pass filter coefficients, calculated with matlab/octave cheby2(2,40,0.05)
const double _LPF_coeffs[5] = { 0.00996655391939, -0.01944529148401, 0.00996655391939, 1.96867605796247, -0.96916387431724 };

////////////////////////////////////////////////////////////////////////////////

BPMDetect::BPMDetect(int numChannels, int aSampleRate) :
    beat_lpf(_LPF_coeffs)
{
    beats.reserve(250); // initial reservation to prevent frequent reallocation

    this->sampleRate = aSampleRate;
    this->channels = numChannels;

    decimateSum = 0;
    decimateCount = 0;

    // choose decimation factor so that result is approx. 1000 Hz
    decimateBy = sampleRate / TARGET_SRATE;
    if ((decimateBy <= 0) || (decimateBy * DECIMATED_BLOCK_SIZE < INPUT_BLOCK_SIZE))
    {
        ST_THROW_RT_ERROR("Too small samplerate");
    }

    // Calculate window length & starting item according to desired min & max bpms
    windowLen = (60 * sampleRate) / (decimateBy * MIN_BPM);
    windowStart = (60 * sampleRate) / (decimateBy * MAX_BPM_RANGE);

    assert(windowLen > windowStart);

    // allocate new working objects
    xcorr = new float[windowLen];
    memset(xcorr, 0, windowLen * sizeof(float));

    pos = 0;
    peakPos = 0;
    peakVal = 0;
    init_scaler = 1;
    beatcorr_ringbuffpos = 0;
    beatcorr_ringbuff = new float[windowLen];
    memset(beatcorr_ringbuff, 0, windowLen * sizeof(float));

    // allocate processing buffer
    buffer = new FIFOSampleBuffer();
    // we do processing in mono mode
    buffer->setChannels(1);
    buffer->clear();

    // calculate hamming windows
    hamw = new float[XCORR_UPDATE_SEQUENCE];
    hamming(hamw, XCORR_UPDATE_SEQUENCE);
    hamw2 = new float[XCORR_UPDATE_SEQUENCE / 2];
    hamming(hamw2, XCORR_UPDATE_SEQUENCE / 2);
}


BPMDetect::~BPMDetect()
{
    delete[] xcorr;
    delete[] beatcorr_ringbuff;
    delete[] hamw;
    delete[] hamw2;
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
    assert(process_samples == XCORR_UPDATE_SEQUENCE);

    pBuffer = buffer->ptrBegin();

    // calculate decay factor for xcorr filtering
    float xcorr_decay = (float)pow(0.5, 1.0 / (XCORR_DECAY_TIME_CONSTANT * TARGET_SRATE / process_samples));

    // prescale pbuffer
    float tmp[XCORR_UPDATE_SEQUENCE];
    for (int i = 0; i < process_samples; i++)
    {
        tmp[i] = hamw[i] * hamw[i] * pBuffer[i];
    }

    #pragma omp parallel for
    for (offs = windowStart; offs < windowLen; offs ++) 
    {
        float sum;
        int i;

        sum = 0;
        for (i = 0; i < process_samples; i ++) 
        {
            sum += tmp[i] * pBuffer[i + offs];  // scaling the sub-result shouldn't be necessary
        }
        xcorr[offs] *= xcorr_decay;   // decay 'xcorr' here with suitable time constant.

        xcorr[offs] += (float)fabs(sum);
    }
}


// Detect individual beat positions
void BPMDetect::updateBeatPos(int process_samples)
{
    SAMPLETYPE *pBuffer;

    assert(buffer->numSamples() >= (uint)(process_samples + windowLen));

    pBuffer = buffer->ptrBegin();
    assert(process_samples == XCORR_UPDATE_SEQUENCE / 2);

    //    static double thr = 0.0003;
    double posScale = (double)this->decimateBy / (double)this->sampleRate;
    int resetDur = (int)(0.12 / posScale + 0.5);

    // prescale pbuffer
    float tmp[XCORR_UPDATE_SEQUENCE / 2];
    for (int i = 0; i < process_samples; i++)
    {
        tmp[i] = hamw2[i] * hamw2[i] * pBuffer[i];
    }

    #pragma omp parallel for
    for (int offs = windowStart; offs < windowLen; offs++)
    {
        float sum = 0;
        for (int i = 0; i < process_samples; i++)
        {
            sum += tmp[i] * pBuffer[offs + i];
        }
        beatcorr_ringbuff[(beatcorr_ringbuffpos + offs) % windowLen] += (float)((sum > 0) ? sum : 0); // accumulate only positive correlations
    }

    int skipstep = XCORR_UPDATE_SEQUENCE / OVERLAP_FACTOR;

    // compensate empty buffer at beginning by scaling coefficient
    float scale = (float)windowLen / (float)(skipstep * init_scaler);
    if (scale > 1.0f)
    {
        init_scaler++;
    }
    else
    {
        scale = 1.0f;
    }

    // detect beats
    for (int i = 0; i < skipstep; i++)
    {
        LONG_SAMPLETYPE max = 0;

        float sum = beatcorr_ringbuff[beatcorr_ringbuffpos];
        sum -= beat_lpf.update(sum);

        if (sum > peakVal)
        {
            // found new local largest value
            peakVal = sum;
            peakPos = pos;
        }
        if (pos > peakPos + resetDur)
        {
            // largest value not updated for 200msec => accept as beat
            peakPos += skipstep;
            if (peakVal > 0)
            {
                // add detected beat to end of "beats" vector
                BEAT temp = { (float)(peakPos * posScale), (float)(peakVal * scale) };
                beats.push_back(temp);
            }

            peakVal = 0;
            peakPos = pos;
        }

        beatcorr_ringbuff[beatcorr_ringbuffpos] = 0;
        pos++;
        beatcorr_ringbuffpos = (beatcorr_ringbuffpos + 1) % windowLen;
    }
}


#define max(x,y) ((x) > (y) ? (x) : (y))

void BPMDetect::inputSamples(const SAMPLETYPE *samples, int numSamples)
{
    SAMPLETYPE decimated[DECIMATED_BLOCK_SIZE];

    // iterate so that max INPUT_BLOCK_SAMPLES processed per iteration
    while (numSamples > 0)
    {
        int block;
        int decSamples;

        block = (numSamples > INPUT_BLOCK_SIZE) ? INPUT_BLOCK_SIZE : numSamples;

        // decimate. note that converts to mono at the same time
        decSamples = decimate(decimated, samples, block);
        samples += block * channels;
        numSamples -= block;

        buffer->putSamples(decimated, decSamples);
    }

    // when the buffer has enough samples for processing...
    int req = max(windowLen + XCORR_UPDATE_SEQUENCE, 2 * XCORR_UPDATE_SEQUENCE);
    while ((int)buffer->numSamples() >= req) 
    {
        // ... update autocorrelations...
        updateXCorr(XCORR_UPDATE_SEQUENCE);
        // ...update beat position calculation...
        updateBeatPos(XCORR_UPDATE_SEQUENCE / 2);
        // ... and remove proceessed samples from the buffer
        int n = XCORR_UPDATE_SEQUENCE / OVERLAP_FACTOR;
        buffer->receiveSamples(n);
    }
}


void BPMDetect::removeBias()
{
    int i;

    // Remove linear bias: calculate linear regression coefficient
    // 1. calc mean of 'xcorr' and 'i'
    double mean_i = 0;
    double mean_x = 0;
    for (i = windowStart; i < windowLen; i++)
    {
        mean_x += xcorr[i];
    }
    mean_x /= (windowLen - windowStart);
    mean_i = 0.5 * (windowLen - 1 + windowStart);

    // 2. calculate linear regression coefficient
    double b = 0;
    double div = 0;
    for (i = windowStart; i < windowLen; i++)
    {
        double xt = xcorr[i] - mean_x;
        double xi = i - mean_i;
        b += xt * xi;
        div += xi * xi;
    }
    b /= div;

    // subtract linear regression and resolve min. value bias
    float minval = FLT_MAX;   // arbitrary large number
    for (i = windowStart; i < windowLen; i ++)
    {
        xcorr[i] -= (float)(b * i);
        if (xcorr[i] < minval)
        {
            minval = xcorr[i];
        }
    }

    // subtract min.value
    for (i = windowStart; i < windowLen; i ++)
    {
        xcorr[i] -= minval;
    }
}


// Calculate N-point moving average for "source" values
void MAFilter(float *dest, const float *source, int start, int end, int N)
{
    for (int i = start; i < end; i++)
    {
        int i1 = i - N / 2;
        int i2 = i + N / 2 + 1;
        if (i1 < start) i1 = start;
        if (i2 > end)   i2 = end;

        double sum = 0;
        for (int j = i1; j < i2; j ++)
        { 
            sum += source[j];
        }
        dest[i] = (float)(sum / (i2 - i1));
    }
}


float BPMDetect::getBpm()
{
    double peakPos;
    double coeff;
    PeakFinder peakFinder;

    // remove bias from xcorr data
    removeBias();

    coeff = 60.0 * ((double)sampleRate / (double)decimateBy);

    // save bpm debug data if debug data writing enabled
    _SaveDebugData("soundtouch-bpm-xcorr.txt", xcorr, windowStart, windowLen, coeff);

    // Smoothen by N-point moving-average
    float *data = new float[windowLen];
    memset(data, 0, sizeof(float) * windowLen);
    MAFilter(data, xcorr, windowStart, windowLen, MOVING_AVERAGE_N);

    // find peak position
    peakPos = peakFinder.detectPeak(data, windowStart, windowLen);

    // save bpm debug data if debug data writing enabled
    _SaveDebugData("soundtouch-bpm-smoothed.txt", data, windowStart, windowLen, coeff);

    delete[] data;

    assert(decimateBy != 0);
    if (peakPos < 1e-9) return 0.0; // detection failed.

    _SaveDebugBeatPos("soundtouch-detected-beats.txt", beats);

    // calculate BPM
    float bpm = (float)(coeff / peakPos);
    return (bpm >= MIN_BPM && bpm <= MAX_BPM_VALID) ? bpm : 0;
}


/// Get beat position arrays. Note: The array includes also really low beat detection values 
/// in absence of clear strong beats. Consumer may wish to filter low values away.
/// - "pos" receive array of beat positions
/// - "values" receive array of beat detection strengths
/// - max_num indicates max.size of "pos" and "values" array.  
///
/// You can query a suitable array sized by calling this with NULL in "pos" & "values".
///
/// \return number of beats in the arrays.
int BPMDetect::getBeats(float *pos, float *values, int max_num)
{
    int num = (int)beats.size();
    if ((!pos) || (!values)) return num;    // pos or values NULL, return just size

    for (int i = 0; (i < num) && (i < max_num); i++)
    {
        pos[i] = beats[i].pos;
        values[i] = beats[i].strength;
    }
    return num;
}
