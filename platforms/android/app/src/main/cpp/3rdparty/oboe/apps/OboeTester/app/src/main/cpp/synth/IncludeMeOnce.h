/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This code was translated from the JSyn Java code.
 * JSyn is Copyright 2009 Phil Burk, Mobileer Inc
 * JSyn is licensed under the Apache License, Version 2.0
 */

#ifndef INCLUDE_ME_ONCE_H
#define INCLUDE_ME_ONCE_H

#include "UnitGenerator.h"
#include "PitchToFrequency.h"

namespace marksynth {

//synth statics
int32_t UnitGenerator::mSampleRate = kSynthmarkSampleRate;
synth_float_t UnitGenerator::mSamplePeriod = 1.0f / kSynthmarkSampleRate;

PowerOfTwoTable PitchToFrequency::mPowerTable(64);
};

#endif //INCLUDE_ME_ONCE_H
