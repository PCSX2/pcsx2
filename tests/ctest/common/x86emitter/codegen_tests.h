// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/Pcsx2Defs.h"

void runCodegenTest(void (*exec)(void *base), const char* description, const char* expected);

#define CODEGEN_TEST(command, expected) runCodegenTest([](void *base){ command; }, #command, expected)
