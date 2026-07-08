/*
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_OGL_H
#define ANDROID_OGL_H

#include <assert.h>

struct ScreenColor {
    float red;
    float green;
    float blue;
    float alpha;
};

constexpr ScreenColor RED { 1.0f, 0.0f, 0.0f, 1.0f };
constexpr ScreenColor GREEN { 0.0f, 1.0f, 0.0f, 1.0f };
constexpr ScreenColor BLUE { 0.0f, 0.0f, 1.0f, 1.0f };
constexpr ScreenColor PURPLE { 1.0f, 0.0f, 1.0f, 1.0f };
constexpr ScreenColor ORANGE { 1.0f, 0.5f, 0.0f, 1.0f };
constexpr ScreenColor GREY { 0.3f, 0.3f, 0.3f, 0.3f };
constexpr ScreenColor YELLOW { 1.0f, 1.0f, 0.0f, 1.0f };

#ifdef GL3
#include <GLES3/gl3.h>
#elif GL3_2
#include <GLES3/gl32.h>
#else
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

void CheckOpenGLError(const char* stmt, const char* fname, int line);

#ifndef NDEBUG
#define GL_CHECK(stmt) \
            stmt;\
            CheckOpenGLError(#stmt, __FILE__, __LINE__);
#else
#define GL_CHECK(stmt) stmt
#endif

void SetGLScreenColor(ScreenColor color);

#endif //ANDROID_OGL_H
