// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+
#pragma once

/// Report test-suite results back to NativeApp.onTestResults(label, pass, total) via JNI.
/// Defined in native-lib.cpp.  Safe to call from any thread that has a JNI env available
/// through SDL_GetAndroidJNIEnv().
void ReportTestResults(const char* label, int pass, int total);
