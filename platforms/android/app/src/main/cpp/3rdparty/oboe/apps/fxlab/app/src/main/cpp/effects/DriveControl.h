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
#ifndef ANDROID_FXLAB_DRIVECONTROL_H
#define ANDROID_FXLAB_DRIVECONTROL_H


#include <functional>

template <class iter_type>

class DriveControl {
public:

    DriveControl(std::function<void(iter_type, iter_type)> function, double scale):
        mFunction(function), kScale(scale) {}

    void operator() (iter_type beg, iter_type end) {
        std::for_each(beg, end, [this](auto &x){x *= kScale;});
        mFunction(beg, end);
        std::for_each(beg, end, [this](auto &x){x *= recip;});
    }

private:
    std::function<void(iter_type, iter_type)> mFunction;
    const double kScale;
    const double recip = 1 / kScale;
};
#endif //ANDROID_FXLAB_DRIVECONTROL_H
