/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#pragma once

//#define ENABLE_VTUNE
//#define ENABLE_PCRTC_DEBUG
//#define ENABLE_ACCURATE_BUFFER_EMULATION
#define ENABLE_JIT_RASTERIZER

//#define DISABLE_HW_TEXTURE_CACHE // Slow but fixes a lot of bugs

//#define DISABLE_BITMASKING

//#define DISABLE_COLCLAMP

//#define DISABLE_DATE


#if !defined(NDEBUG) || defined(_DEBUG) || defined(_DEVEL)
#define ENABLE_OGL_DEBUG // Create a debug context and check opengl command status. Allow also to dump various textures/states.
//#define ENABLE_OGL_DEBUG_FENCE
//#define ENABLE_OGL_DEBUG_MEM_BW // compute the quantity of data transfered (debug purpose)
//#define ENABLE_TRACE_REG // print GS reg write
//#define ENABLE_EXTRA_LOG // print extra log
#endif

#if (defined(__unix__) || defined(__APPLE__)) && !(defined(_DEBUG) || defined(_DEVEL))
#define DISABLE_PERF_MON // Burn cycle for nothing in release mode
#endif
