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
#ifndef __WAVTYPES_H__
#define __WAVTYPES_H__

namespace parselib {

/*
 * Declarations for various (cross-platform) WAV-specific data types.
 */
typedef unsigned int RiffID;    // A "four character code" (i.e. FOURCC)
typedef int RiffInt32;          // A 32-bit signed integer
typedef short RiffInt16;        // A 16-bit signed integer

/*
 * Packs the specified characters into a 32-bit value in accordance with the Microsoft
 * FOURCC specification.
 */
inline RiffID makeRiffID(char a, char b, char c, char d) {
    return ((RiffID)d << 24) | ((RiffID)c << 16) | ((RiffID)b << 8) | (RiffID)a;
}

} // namespace parselib

#endif /* __WAVTYPES_H__ */
