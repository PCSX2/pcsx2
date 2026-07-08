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

#ifndef ANDROID_FXLAB_DELAYLINE_H
#define ANDROID_FXLAB_DELAYLINE_H

#include <vector>


template<class T>
class DelayLine {
public:
    DelayLine(std::size_t size): N(size), mArr(N, 0) { }
    void push(const T& value) {
        mArr[mfront++] = value;
        if (mfront == N) mfront = 0;
    }
    // indexed from last value written backwards
    // i.e T-1 to T-N
    const T& operator[](int i) {
        int index = mfront - i;
        if (index < 0) index += N;
        return mArr[index];
    }

private:
    const int N;
    int mfront = 0;
    std::vector<T> mArr;
};
#endif //ANDROID_FXLAB_DELAYLINE_H
