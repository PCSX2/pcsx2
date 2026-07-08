/*
 * Copyright 2017 The Android Open Source Project
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
 */

#include <dlfcn.h>

#include "common/OboeDebug.h"
#include "EngineOpenSLES.h"
#include "OpenSLESUtilities.h"

using namespace oboe;

// OpenSL ES is deprecated in SDK 30.
// So we use custom dynamic linking to access the library.
#define LIB_OPENSLES_NAME "libOpenSLES.so"

EngineOpenSLES &EngineOpenSLES::getInstance() {
    static EngineOpenSLES sInstance;
    return sInstance;
}

// Satisfy extern in OpenSLES.h
// These are required because of b/337360630, which was causing
// Oboe to have link failures if libOpenSLES.so was not available.
// If you are statically linking Oboe and libOpenSLES.so in a shared library
// and you observe crashes, you can pass DO_NOT_DEFINE_OPENSL_ES_CONSTANTS to cmake.
#ifndef DO_NOT_DEFINE_OPENSL_ES_CONSTANTS
SL_API const SLInterfaceID SL_IID_ENGINE = nullptr;
SL_API const SLInterfaceID SL_IID_ANDROIDSIMPLEBUFFERQUEUE = nullptr;
SL_API const SLInterfaceID SL_IID_ANDROIDCONFIGURATION = nullptr;
SL_API const SLInterfaceID SL_IID_RECORD = nullptr;
SL_API const SLInterfaceID SL_IID_BUFFERQUEUE = nullptr;
SL_API const SLInterfaceID SL_IID_VOLUME = nullptr;
SL_API const SLInterfaceID SL_IID_PLAY = nullptr;
#endif

static const char *getSafeDlerror() {
    static const char *defaultMessage = "not found?";
    char *errorMessage = dlerror();
    return (errorMessage == nullptr) ? defaultMessage : errorMessage;
}

// Load the OpenSL ES library and the one primary entry point.
// @return true if linked OK
bool EngineOpenSLES::linkOpenSLES() {
    if (mDynamicLinkState == kLinkStateBad) {
        LOGE("%s(), OpenSL ES not available, based on previous link failure.", __func__);
    } else if (mDynamicLinkState == kLinkStateUninitialized) {
        // Set to BAD now in case we return because of an error.
        // This is safe form race conditions because this function is always called
        // under mLock amd the state is only accessed from this function.
        mDynamicLinkState = kLinkStateBad;
        // Use RTLD_NOW to avoid the unpredictable behavior that RTLD_LAZY can cause.
        // Also resolving all the links now will prevent a run-time penalty later.
        mLibOpenSlesLibraryHandle = dlopen(LIB_OPENSLES_NAME, RTLD_NOW);
        if (mLibOpenSlesLibraryHandle == nullptr) {
            LOGE("%s() could not dlopen(%s), %s", __func__, LIB_OPENSLES_NAME, getSafeDlerror());
            return false;
        } else {
            mFunction_slCreateEngine = (prototype_slCreateEngine) dlsym(
                    mLibOpenSlesLibraryHandle,
                    "slCreateEngine");
            LOGD("%s(): dlsym(%s) returned %p", __func__,
                 "slCreateEngine", mFunction_slCreateEngine);
            if (mFunction_slCreateEngine == nullptr) {
                LOGE("%s(): dlsym(slCreateEngine) returned null, %s", __func__, getSafeDlerror());
                return false;
            }

            // Load IID interfaces.
            LOCAL_SL_IID_ENGINE = getIidPointer("SL_IID_ENGINE");
            if (LOCAL_SL_IID_ENGINE == nullptr) return false;
            LOCAL_SL_IID_ANDROIDSIMPLEBUFFERQUEUE = getIidPointer(
                    "SL_IID_ANDROIDSIMPLEBUFFERQUEUE");
            if (LOCAL_SL_IID_ANDROIDSIMPLEBUFFERQUEUE == nullptr) return false;
            LOCAL_SL_IID_ANDROIDCONFIGURATION = getIidPointer(
                    "SL_IID_ANDROIDCONFIGURATION");
            if (LOCAL_SL_IID_ANDROIDCONFIGURATION == nullptr) return false;
            LOCAL_SL_IID_RECORD = getIidPointer("SL_IID_RECORD");
            if (LOCAL_SL_IID_RECORD == nullptr) return false;
            LOCAL_SL_IID_BUFFERQUEUE = getIidPointer("SL_IID_BUFFERQUEUE");
            if (LOCAL_SL_IID_BUFFERQUEUE == nullptr) return false;
            LOCAL_SL_IID_VOLUME = getIidPointer("SL_IID_VOLUME");
            if (LOCAL_SL_IID_VOLUME == nullptr) return false;
            LOCAL_SL_IID_PLAY = getIidPointer("SL_IID_PLAY");
            if (LOCAL_SL_IID_PLAY == nullptr) return false;

            mDynamicLinkState = kLinkStateGood;
        }
    }
    return (mDynamicLinkState == kLinkStateGood);
}

// A symbol like SL_IID_PLAY is a pointer to a structure.
// The dlsym() function returns the address of the pointer, not the structure.
// To get the address of the structure we have to dereference the pointer.
SLInterfaceID EngineOpenSLES::getIidPointer(const char *symbolName) {
    SLInterfaceID *iid_address = (SLInterfaceID *) dlsym(
            mLibOpenSlesLibraryHandle,
            symbolName);
    if (iid_address == nullptr) {
        LOGE("%s(): dlsym(%s) returned null, %s", __func__, symbolName, getSafeDlerror());
        return (SLInterfaceID) nullptr;
    }
    return *iid_address; // Get address of the structure.
}

SLresult EngineOpenSLES::open() {
    std::lock_guard<std::mutex> lock(mLock);

    SLresult result = SL_RESULT_SUCCESS;
    if (mOpenCount++ == 0) {
        // load the library and link to it
        if (!linkOpenSLES()) {
            result = SL_RESULT_FEATURE_UNSUPPORTED;
            goto error;
        };

        // create engine
        result = (*mFunction_slCreateEngine)(&mEngineObject, 0, NULL, 0, NULL, NULL);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("EngineOpenSLES - slCreateEngine() result:%s", getSLErrStr(result));
            goto error;
        }

        // realize the engine
        result = (*mEngineObject)->Realize(mEngineObject, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("EngineOpenSLES - Realize() engine result:%s", getSLErrStr(result));
            goto error;
        }

        // get the engine interface, which is needed in order to create other objects
        result = (*mEngineObject)->GetInterface(mEngineObject,
                                                EngineOpenSLES::getInstance().getIidEngine(),
                                                &mEngineInterface);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("EngineOpenSLES - GetInterface() engine result:%s", getSLErrStr(result));
            goto error;
        }
    }

    return result;

error:
    close_l();
    return result;
}

void EngineOpenSLES::close() {
    std::lock_guard<std::mutex> lock(mLock);
    close_l();
}

// This must be called under mLock
void EngineOpenSLES::close_l() {
    if (--mOpenCount == 0) {
        if (mEngineObject != nullptr) {
            (*mEngineObject)->Destroy(mEngineObject);
            mEngineObject = nullptr;
            mEngineInterface = nullptr;
        }
    }
}

SLresult EngineOpenSLES::createOutputMix(SLObjectItf *objectItf) {
    return (*mEngineInterface)->CreateOutputMix(mEngineInterface, objectItf, 0, 0, 0);
}

SLresult EngineOpenSLES::createAudioPlayer(SLObjectItf *objectItf,
                                           SLDataSource *audioSource,
                                           SLDataSink *audioSink) {

    SLInterfaceID ids[] = {LOCAL_SL_IID_BUFFERQUEUE, LOCAL_SL_IID_ANDROIDCONFIGURATION};
    SLboolean reqs[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    return (*mEngineInterface)->CreateAudioPlayer(mEngineInterface, objectItf, audioSource,
                                                  audioSink,
                                                  sizeof(ids) / sizeof(ids[0]), ids, reqs);
}

SLresult EngineOpenSLES::createAudioRecorder(SLObjectItf *objectItf,
                                             SLDataSource *audioSource,
                                             SLDataSink *audioSink) {

    SLInterfaceID ids[] = {LOCAL_SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                           LOCAL_SL_IID_ANDROIDCONFIGURATION };
    SLboolean reqs[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    return (*mEngineInterface)->CreateAudioRecorder(mEngineInterface, objectItf, audioSource,
                                                    audioSink,
                                                    sizeof(ids) / sizeof(ids[0]), ids, reqs);
}

