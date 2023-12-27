// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "pcsx2/PrecompiledHeader.h"

// Needed because of moc shenanigans with pch.
#include <memory>

// Silence these warnings in the PCH, that way it happens before any of our headers are included.
// warning : variable 's_hrErrorLast' set but not used [-Wunused-but-set-variable]
#if defined(_WIN32) && defined(__clang__)
#pragma clang diagnostic push
// warning : known but unsupported action 'shared' for '#pragma section' - ignored [-Wignored-pragmas]
#pragma clang diagnostic ignored "-Wignored-pragmas"
// warning : dynamic_cast will not work since RTTI data is disabled by /GR- [-Wrtti]
#pragma clang diagnostic ignored "-Wrtti"
#endif

#include <QtCore/QtCore>

#if defined(_WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif
