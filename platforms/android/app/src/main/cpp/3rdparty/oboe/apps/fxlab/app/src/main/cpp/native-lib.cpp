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

#include <jni.h>

#include <cassert>
#include <string>
#include <functional>
#include <utility>
#include <vector>

#include "DuplexEngine.h"
#include "effects/Effects.h"
#include "FunctionList.h"


// JNI Utility functions and globals
static DuplexEngine *enginePtr = nullptr;

// Actual JNI interface
extern "C" {

JNIEXPORT void JNICALL
Java_com_mobileer_androidfxlab_NativeInterface_createAudioEngine(
        JNIEnv,
        jobject /* this */) {
    enginePtr = new DuplexEngine();
}
JNIEXPORT void JNICALL
Java_com_mobileer_androidfxlab_NativeInterface_destroyAudioEngine(
        JNIEnv,
        jobject /* this */) {
    if (!enginePtr) return;
    delete enginePtr;
    enginePtr = nullptr;
}

JNIEXPORT jobjectArray JNICALL
Java_com_mobileer_androidfxlab_NativeInterface_getEffects(JNIEnv *env, jobject) {
    jclass jcl = env->FindClass("com/mobileer/androidfxlab/datatype/EffectDescription");
    jclass jparamcl = env->FindClass("com/mobileer/androidfxlab/datatype/ParamDescription");
    assert (jcl != nullptr && jparamcl != nullptr);

    auto jparamMethodId = env->GetMethodID(jparamcl, "<init>", "(Ljava/lang/String;FFF)V");
    auto jMethodId = env->GetMethodID(jcl, "<init>",
                                      "(Ljava/lang/String;Ljava/lang/String;I[Lcom/mobileer/androidfxlab/datatype/ParamDescription;)V");

    auto arr = env->NewObjectArray(numEffects, jcl, nullptr);
    auto lambda = [&](auto &arg, int i) {
        const auto &paramArr = arg.getParams();
        auto jparamArr = env->NewObjectArray(paramArr.size(), jparamcl, nullptr);
        int c = 0;
        for (auto const &elem: paramArr) {
            jobject j = env->NewObject(jparamcl, jparamMethodId,
                                       env->NewStringUTF(std::string(elem.kName).c_str()),
                                       elem.kMinVal, elem.kMaxVal, elem.kDefVal);
            assert(j != nullptr);
            env->SetObjectArrayElement(jparamArr, c++, j);
        }
        jobject j = env->NewObject(jcl, jMethodId,
                                   env->NewStringUTF(std::string(arg.getName()).c_str()),
                                   env->NewStringUTF(std::string(arg.getCategory()).c_str()),
                                   i, jparamArr);
        assert(j != nullptr);
        env->SetObjectArrayElement(arr, i, j);
    };
    int i = 0;
    std::apply([&i, &lambda](auto &&... args) mutable { ((lambda(args, i++)), ...); },
               EffectsTuple);
    return arr;
}

JNIEXPORT void JNICALL
Java_com_mobileer_androidfxlab_NativeInterface_addDefaultEffectNative(JNIEnv *, jobject, jint jid) {
    if (!enginePtr) return;
    auto id = static_cast<int>(jid);

    std::visit([id](auto &&stack) {
        std::function<void(decltype(stack.getType()), decltype(stack.getType()))> f;
        int i = 0;
        std::apply([id, &f, &i](auto &&... args) mutable {
            ((f = (i++ == id) ?
                  args.template buildDefaultEffect<decltype(stack.getType())>() : f), ...);
        }, EffectsTuple);
        stack.addEffect(std::move(f));
    }, enginePtr->functionList);
}
JNIEXPORT void JNICALL
Java_com_mobileer_androidfxlab_NativeInterface_removeEffectNative(JNIEnv *, jobject, jint jind) {
    if (!enginePtr) return;
    auto ind = static_cast<size_t>(jind);
    std::visit([ind](auto &&arg) {
        arg.removeEffectAt(ind);
    }, enginePtr->functionList);
}
JNIEXPORT void JNICALL
Java_com_mobileer_androidfxlab_NativeInterface_rotateEffectNative(JNIEnv *, jobject,
                                                                jint jfrom, jint jto) {
    if (!enginePtr) return;
    auto from = static_cast<size_t>(jfrom);
    auto to = static_cast<size_t>(jto);

    std::visit([from, to](auto &&arg) {
        arg.rotateEffectAt(from, to);
    }, enginePtr->functionList);
}

JNIEXPORT void JNICALL
Java_com_mobileer_androidfxlab_NativeInterface_modifyEffectNative(
        JNIEnv *env, jobject, jint jid, jint jindex, jfloatArray params) {
    if (!enginePtr) return;
    int id = static_cast<int>(jid);
    int index = static_cast<size_t>(jindex);

    jfloat *data = env->GetFloatArrayElements(params, nullptr);
    std::vector<float> arr{data, data + env->GetArrayLength(params)};
    env->ReleaseFloatArrayElements(params, data, 0);
    std::visit([&arr, &id, &index](auto &&stack) {
        std::function<void(decltype(stack.getType()), decltype(stack.getType()))> ef;
        int i = 0;
        std::apply([&](auto &&... args) mutable {
            ((ef = (i++ == id) ?
                   args.modifyEffectVec(ef, arr) : ef), ...);
        }, EffectsTuple);
        stack.modifyEffectAt(index, std::move(ef));
    }, enginePtr->functionList);
}
JNIEXPORT void JNICALL
Java_com_mobileer_androidfxlab_NativeInterface_enableEffectNative(
        JNIEnv *, jobject, jint jindex, jboolean jenable) {
    if (!enginePtr) return;
    auto ind = static_cast<size_t>(jindex);
    auto enable = static_cast<bool>(jenable);
    std::visit([ind, enable](auto &&args) {
        args.enableEffectAt(ind, enable);
    }, enginePtr->functionList);
}
JNIEXPORT void JNICALL
Java_com_mobileer_androidfxlab_NativeInterface_enablePassthroughNative(
        JNIEnv *, jobject, jboolean jenable) {
    if (!enginePtr) return;
    std::visit([enable = static_cast<bool>(jenable)](auto &&args) {
        args.mute(!enable);
    }, enginePtr->functionList);
}
} //extern C

