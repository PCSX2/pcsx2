/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "common/Pcsx2Defs.h"
#include "common/WindowInfo.h"

#include <array>
#include <memory>
#include <vector>

namespace GL {
class Context
{
public:
  Context(const WindowInfo& wi);
  virtual ~Context();

  enum class Profile
  {
    NoProfile,
    Core,
    ES
  };

  struct Version
  {
    Profile profile;
    int major_version;
    int minor_version;
  };

  struct FullscreenModeInfo
  {
    u32 width;
    u32 height;
    float refresh_rate;
  };

  __fi const WindowInfo& GetWindowInfo() const { return m_wi; }
  __fi bool IsGLES() const { return (m_version.profile == Profile::ES); }
  __fi u32 GetSurfaceWidth() const { return m_wi.surface_width; }
  __fi u32 GetSurfaceHeight() const { return m_wi.surface_height; }

  virtual void* GetProcAddress(const char* name) = 0;
  virtual bool ChangeSurface(const WindowInfo& new_wi) = 0;
  virtual void ResizeSurface(u32 new_surface_width = 0, u32 new_surface_height = 0) = 0;
  virtual bool SwapBuffers() = 0;
  virtual bool MakeCurrent() = 0;
  virtual bool DoneCurrent() = 0;
  virtual bool SetSwapInterval(s32 interval) = 0;
  virtual std::unique_ptr<Context> CreateSharedContext(const WindowInfo& wi) = 0;

  virtual std::vector<FullscreenModeInfo> EnumerateFullscreenModes();

  static std::unique_ptr<Context> Create(const WindowInfo& wi, const Version* versions_to_try,
                                         size_t num_versions_to_try);

  template<size_t N>
  static std::unique_ptr<Context> Create(const WindowInfo& wi, const std::array<Version, N>& versions_to_try)
  {
    return Create(wi, versions_to_try.data(), versions_to_try.size());
  }

  static std::unique_ptr<Context> Create(const WindowInfo& wi) { return Create(wi, GetAllVersionsList()); }

  static const std::array<Version, 16>& GetAllVersionsList();

protected:
  WindowInfo m_wi;
  Version m_version = {};
};
} // namespace GL
