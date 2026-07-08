/*
 * Copyright 2019 The Android Open Source Project
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

#ifndef OBOE_AAUDIO_EXTENSIONS_H
#define OBOE_AAUDIO_EXTENSIONS_H

#include <algorithm>
#include <dlfcn.h>
#include <set>
#include <stdint.h>

#include <sys/system_properties.h>

#include "common/OboeDebug.h"
#include "oboe/Oboe.h"
#include "AAudioLoader.h"

namespace oboe {

#define LIB_AAUDIO_NAME          "libaaudio.so"
#define FUNCTION_IS_MMAP         "AAudioStream_isMMapUsed"
#define FUNCTION_SET_MMAP_POLICY "AAudio_setMMapPolicy"
#define FUNCTION_GET_MMAP_POLICY "AAudio_getMMapPolicy"

#define AAUDIO_ERROR_UNAVAILABLE  static_cast<aaudio_result_t>(Result::ErrorUnavailable)

typedef struct AAudioStreamStruct         AAudioStream;

// The output device type collection must be updated if there is any new added output device type
const static std::set<DeviceType> ALL_OUTPUT_DEVICE_TYPES = {
        DeviceType::BuiltinEarpiece,
        DeviceType::BuiltinSpeaker,
        DeviceType::WiredHeadset,
        DeviceType::WiredHeadphones,
        DeviceType::LineAnalog,
        DeviceType::LineDigital,
        DeviceType::BluetoothSco,
        DeviceType::BluetoothA2dp,
        DeviceType::Hdmi,
        DeviceType::HdmiArc,
        DeviceType::HdmiEarc,
        DeviceType::UsbDevice,
        DeviceType::UsbHeadset,
        DeviceType::UsbAccessory,
        DeviceType::Dock,
        DeviceType::DockAnalog,
        DeviceType::FM,
        DeviceType::Telephony,
        DeviceType::AuxLine,
        DeviceType::IP,
        DeviceType::Bus,
        DeviceType::HearingAid,
        DeviceType::BuiltinSpeakerSafe,
        DeviceType::RemoteSubmix,
        DeviceType::BleHeadset,
        DeviceType::BleSpeaker,
        DeviceType::BleBroadcast,
};

// The input device type collection must be updated if there is any new added input device type
const static std::set<DeviceType> ALL_INPUT_DEVICE_TYPES = {
        DeviceType::BuiltinMic,
        DeviceType::BluetoothSco,
        DeviceType::WiredHeadset,
        DeviceType::Hdmi,
        DeviceType::Telephony,
        DeviceType::Dock,
        DeviceType::DockAnalog,
        DeviceType::UsbAccessory,
        DeviceType::UsbDevice,
        DeviceType::UsbHeadset,
        DeviceType::FMTuner,
        DeviceType::TVTuner,
        DeviceType::LineAnalog,
        DeviceType::LineDigital,
        DeviceType::BluetoothA2dp,
        DeviceType::IP,
        DeviceType::Bus,
        DeviceType::RemoteSubmix,
        DeviceType::BleHeadset,
        DeviceType::HdmiArc,
        DeviceType::HdmiEarc,
};

/**
 * Call some AAudio test routines that are not part of the normal API.
 */
class AAudioExtensions {
private: // Because it is a singleton. Call getInstance() instead.
    AAudioExtensions() {
        mLibLoader = AAudioLoader::getInstance();
        if (!initMMapPolicy()) {
            int32_t policy = getIntegerProperty("aaudio.mmap_policy", 0);
            mMMapSupported = isPolicyEnabled(policy);

            policy = getIntegerProperty("aaudio.mmap_exclusive_policy", 0);
            mMMapExclusiveSupported = isPolicyEnabled(policy);
        }
    }

public:
    static bool isPolicyEnabled(int32_t policy) {
        const MMapPolicy mmapPolicy = static_cast<MMapPolicy>(policy);
        return (mmapPolicy == MMapPolicy::Auto || mmapPolicy == MMapPolicy::Always);
    }

    static AAudioExtensions &getInstance() {
        static AAudioExtensions instance;
        return instance;
    }

    bool isMMapUsed(oboe::AudioStream *oboeStream) {
        AAudioStream *aaudioStream = (AAudioStream *) oboeStream->getUnderlyingStream();
        return isMMapUsed(aaudioStream);
    }

    bool isMMapUsed(AAudioStream *aaudioStream) {
        if (mLibLoader != nullptr && mLibLoader->stream_isMMapUsed != nullptr) {
            return mLibLoader->stream_isMMapUsed(aaudioStream);
        }
        if (loadSymbols()) return false;
        if (mAAudioStream_isMMap == nullptr) return false;
        return mAAudioStream_isMMap(aaudioStream);
    }

    /**
     * Controls whether the MMAP data path can be selected when opening a stream.
     * It has no effect after the stream has been opened.
     * It only affects the application that calls it. Other apps are not affected.
     *
     * @param enabled
     * @return 0 or a negative error code
     */
    int32_t setMMapEnabled(bool enabled) {
        // The API for setting mmap policy is public after API level 36.
        if (mLibLoader != nullptr && mLibLoader->aaudio_setMMapPolicy != nullptr) {
            return mLibLoader->aaudio_setMMapPolicy(
                    static_cast<aaudio_policy_t>(enabled ? MMapPolicy::Auto : MMapPolicy::Never));
        }
        // When there is no public API, fallback to loading the symbol from hidden API.
        if (loadSymbols()) return AAUDIO_ERROR_UNAVAILABLE;
        if (mAAudio_setMMapPolicy == nullptr) return false;
        return mAAudio_setMMapPolicy(
                static_cast<int32_t>(enabled ? MMapPolicy::Auto : MMapPolicy::Never));
    }

    bool isMMapEnabled() {
        // The API for getting mmap policy is public after API level 36.
        // Use it when it is available.
        if (mLibLoader != nullptr && mLibLoader->aaudio_getMMapPolicy != nullptr) {
            MMapPolicy policy = static_cast<MMapPolicy>(mLibLoader->aaudio_getMMapPolicy());
            return policy == MMapPolicy::Unspecified
                    ? mMMapSupported : isPolicyEnabled(static_cast<int32_t>(policy));
        }
        // When there is no public API, fallback to loading the symbol from hidden API.
        if (loadSymbols()) return false;
        if (mAAudio_getMMapPolicy == nullptr) return false;
        int32_t policy = mAAudio_getMMapPolicy();
        return (policy == Unspecified) ? mMMapSupported : isPolicyEnabled(policy);
    }

    bool isMMapSupported() {
        return mMMapSupported;
    }

    bool isMMapExclusiveSupported() {
        return mMMapExclusiveSupported;
    }

    MMapPolicy getMMapPolicy(DeviceType deviceType, Direction direction) {
        if (mLibLoader == nullptr ||
            mLibLoader->aaudio_getPlatformMMapPolicy == nullptr) {
            return MMapPolicy::Unspecified;
        }
        return static_cast<MMapPolicy>(mLibLoader->aaudio_getPlatformMMapPolicy(
                static_cast<AAudio_DeviceType>(deviceType),
                static_cast<aaudio_direction_t>(direction)));
    }

    MMapPolicy getMMapExclusivePolicy(DeviceType deviceType, Direction direction) {
        if (mLibLoader == nullptr ||
            mLibLoader->aaudio_getPlatformMMapExclusivePolicy == nullptr) {
            return MMapPolicy::Unspecified;
        }
        return static_cast<MMapPolicy>(mLibLoader->aaudio_getPlatformMMapExclusivePolicy(
                static_cast<AAudio_DeviceType>(deviceType),
                static_cast<aaudio_direction_t>(direction)));
    }

    bool isPartialDataCallbackSupported() {
        return mLibLoader != nullptr && mLibLoader->builder_setPartialDataCallback != nullptr;
    }

private:
    bool initMMapPolicy() {
        if (mLibLoader == nullptr || mLibLoader->open() != 0) {
            return false;
        }
        if (mLibLoader->aaudio_getPlatformMMapPolicy == nullptr ||
            mLibLoader->aaudio_getPlatformMMapExclusivePolicy == nullptr) {
            return false;
        }
        mMMapSupported =
                std::any_of(ALL_INPUT_DEVICE_TYPES.begin(), ALL_INPUT_DEVICE_TYPES.end(),
                            [this](DeviceType deviceType) {
                                return  isPolicyEnabled(static_cast<int32_t>(
                                        getMMapPolicy(deviceType, Direction::Input)));
                            }) ||
                std::any_of(ALL_OUTPUT_DEVICE_TYPES.begin(), ALL_OUTPUT_DEVICE_TYPES.end(),
                            [this](DeviceType deviceType) {
                                return  isPolicyEnabled(static_cast<int32_t>(
                                        getMMapPolicy(deviceType, Direction::Output)));
                            });
        mMMapExclusiveSupported =
                std::any_of(ALL_INPUT_DEVICE_TYPES.begin(), ALL_INPUT_DEVICE_TYPES.end(),
                            [this](DeviceType deviceType) {
                                return  isPolicyEnabled(static_cast<int32_t>(
                                        getMMapExclusivePolicy(deviceType, Direction::Input)));
                            }) ||
                std::any_of(ALL_OUTPUT_DEVICE_TYPES.begin(), ALL_OUTPUT_DEVICE_TYPES.end(),
                            [this](DeviceType deviceType) {
                                return  isPolicyEnabled(static_cast<int32_t>(
                                        getMMapExclusivePolicy(deviceType, Direction::Output)));
                            });
        return true;
    }

    int getIntegerProperty(const char *name, int defaultValue) {
        int result = defaultValue;
        char valueText[PROP_VALUE_MAX] = {0};
        if (__system_property_get(name, valueText) != 0) {
            result = atoi(valueText);
        }
        return result;
    }

    /**
     * Load the function pointers.
     * This can be called multiple times.
     * It should only be called from one thread.
     *
     * @return 0 if successful or negative error.
     */
    aaudio_result_t loadSymbols() {
        if (mAAudio_getMMapPolicy != nullptr) {
            return 0;
        }

        if (mLibLoader == nullptr || mLibLoader->open() != 0) {
            LOGD("%s() could not open " LIB_AAUDIO_NAME, __func__);
            return AAUDIO_ERROR_UNAVAILABLE;
        }

        void *libHandle = mLibLoader->getLibHandle();
        if (libHandle == nullptr) {
            LOGE("%s() could not find " LIB_AAUDIO_NAME, __func__);
            return AAUDIO_ERROR_UNAVAILABLE;
        }

        mAAudioStream_isMMap = (bool (*)(AAudioStream *stream))
                dlsym(libHandle, FUNCTION_IS_MMAP);
        if (mAAudioStream_isMMap == nullptr) {
            LOGI("%s() could not find " FUNCTION_IS_MMAP, __func__);
            return AAUDIO_ERROR_UNAVAILABLE;
        }

        mAAudio_setMMapPolicy = (int32_t (*)(aaudio_policy_t policy))
                dlsym(libHandle, FUNCTION_SET_MMAP_POLICY);
        if (mAAudio_setMMapPolicy == nullptr) {
            LOGI("%s() could not find " FUNCTION_SET_MMAP_POLICY, __func__);
            return AAUDIO_ERROR_UNAVAILABLE;
        }

        mAAudio_getMMapPolicy = (aaudio_policy_t (*)())
                dlsym(libHandle, FUNCTION_GET_MMAP_POLICY);
        if (mAAudio_getMMapPolicy == nullptr) {
            LOGI("%s() could not find " FUNCTION_GET_MMAP_POLICY, __func__);
            return AAUDIO_ERROR_UNAVAILABLE;
        }

        return 0;
    }

    bool      mMMapSupported = false;
    bool      mMMapExclusiveSupported = false;

    bool    (*mAAudioStream_isMMap)(AAudioStream *stream) = nullptr;
    int32_t (*mAAudio_setMMapPolicy)(aaudio_policy_t policy) = nullptr;
    aaudio_policy_t (*mAAudio_getMMapPolicy)() = nullptr;

    AAudioLoader *mLibLoader;
};

} // namespace oboe

#endif //OBOE_AAUDIO_EXTENSIONS_H
