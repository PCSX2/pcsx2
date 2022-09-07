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

#pragma once
#include "pcsx2/Host.h"
#include "pcsx2/HostDisplay.h"
#include "pcsx2/HostSettings.h"
#include "pcsx2/Frontend/InputManager.h"
#include "pcsx2/VMManager.h"
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <functional>
#include <memory>
#include <optional>

class SettingsInterface;

class EmuThread;

enum class CDVD_SourceType : uint8_t;

Q_DECLARE_METATYPE(std::shared_ptr<VMBootParameters>);
Q_DECLARE_METATYPE(std::optional<bool>);
Q_DECLARE_METATYPE(std::function<void()>);
Q_DECLARE_METATYPE(GSRendererType);
Q_DECLARE_METATYPE(InputBindingKey);
Q_DECLARE_METATYPE(CDVD_SourceType);

namespace QtHost
{
	/// Sets batch mode (exit after game shutdown).
	bool InBatchMode();

	/// Sets NoGUI mode (implys batch mode, does not display main window, exits on shutdown).
	bool InNoGUIMode();

	/// Returns true if the calling thread is the UI thread.
	bool IsOnUIThread();

	/// Executes a function on the UI thread.
	void RunOnUIThread(const std::function<void()>& func, bool block = false);

	/// Returns the application name and version, optionally including debug/devel config indicator.
	QString GetAppNameAndVersion();

	/// Returns the debug/devel config indicator.
	QString GetAppConfigSuffix();

	/// Returns the base path for resources. This may be : prefixed, if we're using embedded resources.
	QString GetResourcesBasePath();

	/// VM state, safe to access on UI thread.
	bool IsVMValid();
	bool IsVMPaused();
} // namespace QtHost
