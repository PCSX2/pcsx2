/*
 * Copyright  2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_FXLAB_EFFECTS_H
#define ANDROID_FXLAB_EFFECTS_H


// The Sample Rate for effects
static int SAMPLE_RATE = 48000;

// This header should include the various effect descriptions
#include "descrip/EffectDescription.h"
#include "descrip/PassthroughDescription.h"
#include "descrip/VibratoDescription.h"
#include "descrip/TremoloDescription.h"
#include "descrip/GainDescription.h"
#include "descrip/FlangerDescription.h"
#include "descrip/WhiteChorusDescription.h"
#include "descrip/FIRDescription.h"
#include "descrip/IIRDescription.h"
#include "descrip/AllPassDescription.h"
#include "descrip/DoublingDescription.h"
#include "descrip/OverdriveDescription.h"
#include "descrip/DistortionDescription.h"
#include "descrip/EchoDescription.h"
#include "descrip/SlapbackDescription.h"

constexpr std::tuple<
        Effect::PassthroughDescription,
        Effect::TremoloDescription,
        Effect::VibratoDescription,
        Effect::GainDescription,
        Effect::FlangerDescription,
        Effect::WhiteChorusDescription,
        Effect::FIRDescription,
        Effect::IIRDescription,
        Effect::AllPassDescription,
        Effect::DoublingDescription,
        Effect::OverdriveDescription,
        Effect::DistortionDescription,
        Effect::EchoDescription,
        Effect::SlapbackDescription
> EffectsTuple{};

constexpr size_t numEffects = std::tuple_size<decltype(EffectsTuple)>::value;


#endif //ANDROID_FXLAB_EFFECTS_H
