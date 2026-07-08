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
#ifndef ANDROID_FXLAB_WHITENOISE_H
#define ANDROID_FXLAB_WHITENOISE_H
class WhiteNoise  {
    const int kScale;
public:
    WhiteNoise(int scale): kScale(scale)  {}
    float operator() () {
        static int counter = 0;
        static float r_0, r_1 = 0;
        if (counter == 0) {
            r_0 = r_1;
            r_1 = (static_cast <float> (rand()) / static_cast <float> (RAND_MAX)) * 2 - 1;
        }
        float ret = r_0 + counter * (r_1 - r_0) / kScale;
        if (++counter == kScale) counter = 0;
        if (ret > 0.99) return 0.99f;
        if (ret < -0.99) return -0.99f;
        return ret;
    }
};
#endif //ANDROID_FXLAB_WHITENOISE_H
