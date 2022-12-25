/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

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
