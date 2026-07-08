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

#ifndef EFFECTS_APP_DUPLEXSTREAM_H
#define EFFECTS_APP_DUPLEXSTREAM_H

#include <array>
#include <algorithm>
#include <variant>

#include <oboe/Oboe.h>
#include "FunctionList.h"
#include "DuplexCallback.h"


class DuplexEngine {
public:
    DuplexEngine();

    void beginStreams();

    virtual ~DuplexEngine() = default;

    oboe::Result startStreams();

    oboe::Result stopStreams();


    std::variant<FunctionList<int16_t *>, FunctionList<float *>> functionList{
            std::in_place_type<FunctionList<int16_t *>>};

private:

    void openInStream();

    void openOutStream();

    static oboe::AudioStreamBuilder defaultBuilder();

    template<class numeric>
    void createCallback();

    oboe::ManagedStream inStream;
    std::unique_ptr<oboe::AudioStreamCallback> mCallback;
    oboe::ManagedStream outStream;


};


#endif //EFFECTS_APP_DUPLEXSTREAM_H
