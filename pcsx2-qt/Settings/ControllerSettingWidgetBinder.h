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

#include <optional>
#include <type_traits>

#include <QtCore/QtCore>
#include <QtGui/QAction>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>

#include "pcsx2/HostSettings.h"

#include "QtHost.h"
#include "SettingWidgetBinder.h"

/// This nastyness is required because input profiles aren't overlaid settings like the rest of them, it's
/// input profile *or* global, not both.
namespace ControllerSettingWidgetBinder
{
	/// Interface specific method of BindWidgetToBoolSetting().
	template <typename WidgetType>
	static inline void BindWidgetToInputProfileBool(
		SettingsInterface* sif, WidgetType* widget, std::string section, std::string key, bool default_value)
	{
		using Accessor = SettingWidgetBinder::SettingAccessor<WidgetType>;

		if (sif)
		{
			const bool value = sif->GetBoolValue(section.c_str(), key.c_str(), default_value);
			Accessor::setBoolValue(widget, value);

			Accessor::connectValueChanged(widget, [sif, widget, section = std::move(section), key = std::move(key)]() {
				const bool new_value = Accessor::getBoolValue(widget);
				sif->SetBoolValue(section.c_str(), key.c_str(), new_value);
				sif->Save();
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
			const bool value = Host::GetBaseBoolSettingValue(section.c_str(), key.c_str(), default_value);
			Accessor::setBoolValue(widget, value);

			Accessor::connectValueChanged(widget, [widget, section = std::move(section), key = std::move(key)]() {
				const bool new_value = Accessor::getBoolValue(widget);
				Host::SetBaseBoolSettingValue(section.c_str(), key.c_str(), new_value);
				Host::CommitBaseSettingChanges();
				g_emu_thread->applySettings();
			});
		}
	}

	/// Interface specific method of BindWidgetToIntSetting().
	template <typename WidgetType>
	static inline void BindWidgetToInputProfileInt(
		SettingsInterface* sif, WidgetType* widget, std::string section, std::string key, s32 default_value, s32 option_offset = 0)
	{
		using Accessor = SettingWidgetBinder::SettingAccessor<WidgetType>;

		if (sif)
		{
			const s32 value = sif->GetIntValue(section.c_str(), key.c_str(), default_value);
			Accessor::setIntValue(widget, value - option_offset);

			Accessor::connectValueChanged(widget, [sif, widget, section = std::move(section), key = std::move(key), option_offset]() {
				const float new_value = Accessor::getIntValue(widget);
				sif->SetIntValue(section.c_str(), key.c_str(), new_value + option_offset);
				sif->Save();
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
			const s32 value = Host::GetBaseIntSettingValue(section.c_str(), key.c_str(), default_value);
			Accessor::setIntValue(widget, value - option_offset);

			Accessor::connectValueChanged(widget, [widget, section = std::move(section), key = std::move(key), option_offset]() {
				const s32 new_value = Accessor::getIntValue(widget);
				Host::SetBaseIntSettingValue(section.c_str(), key.c_str(), new_value + option_offset);
				Host::CommitBaseSettingChanges();
				g_emu_thread->applySettings();
			});
		}
	}

	/// Interface specific method of BindWidgetToFloatSetting().
	template <typename WidgetType>
	static inline void BindWidgetToInputProfileFloat(
		SettingsInterface* sif, WidgetType* widget, std::string section, std::string key, float default_value, float multiplier = 1.0f)
	{
		using Accessor = SettingWidgetBinder::SettingAccessor<WidgetType>;

		if (sif)
		{
			const float value = sif->GetFloatValue(section.c_str(), key.c_str(), default_value);
			Accessor::setFloatValue(widget, value * multiplier);

			Accessor::connectValueChanged(widget, [sif, widget, section = std::move(section), key = std::move(key), multiplier]() {
				const float new_value = Accessor::getFloatValue(widget) / multiplier;
				sif->SetFloatValue(section.c_str(), key.c_str(), new_value);
				sif->Save();
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
			const float value = Host::GetBaseFloatSettingValue(section.c_str(), key.c_str(), default_value);
			Accessor::setFloatValue(widget, value * multiplier);

			Accessor::connectValueChanged(widget, [widget, section = std::move(section), key = std::move(key), multiplier]() {
				const float new_value = Accessor::getFloatValue(widget) / multiplier;
				Host::SetBaseFloatSettingValue(section.c_str(), key.c_str(), new_value);
				Host::CommitBaseSettingChanges();
				g_emu_thread->applySettings();
			});
		}
	}

	/// Interface specific method of BindWidgetToNormalizedSetting().
	template <typename WidgetType>
	static inline void BindWidgetToInputProfileNormalized(
		SettingsInterface* sif, WidgetType* widget, std::string section, std::string key, float range, float default_value)
	{
		using Accessor = SettingWidgetBinder::SettingAccessor<WidgetType>;

		if (sif)
		{
			const float value = sif->GetFloatValue(section.c_str(), key.c_str(), default_value);
			Accessor::setIntValue(widget, static_cast<int>(value * range));

			Accessor::connectValueChanged(widget, [sif, widget, section = std::move(section), key = std::move(key), range]() {
				const int new_value = Accessor::getIntValue(widget);
				sif->SetFloatValue(section.c_str(), key.c_str(), static_cast<float>(new_value) / range);
				sif->Save();
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
			const float value = Host::GetBaseFloatSettingValue(section.c_str(), key.c_str(), default_value);
			Accessor::setIntValue(widget, static_cast<int>(value * range));

			Accessor::connectValueChanged(widget, [widget, section = std::move(section), key = std::move(key), range]() {
				const float new_value = (static_cast<float>(Accessor::getIntValue(widget)) / range);
				Host::SetBaseFloatSettingValue(section.c_str(), key.c_str(), new_value);
				Host::CommitBaseSettingChanges();
				g_emu_thread->applySettings();
			});
		}
	}

	/// Interface specific method of BindWidgetToStringSetting().
	template <typename WidgetType>
	static inline void BindWidgetToInputProfileString(
		SettingsInterface* sif, WidgetType* widget, std::string section, std::string key, std::string default_value = std::string())
	{
		using Accessor = SettingWidgetBinder::SettingAccessor<WidgetType>;

		if (sif)
		{
			const QString value(QString::fromStdString(sif->GetStringValue(section.c_str(), key.c_str(), default_value.c_str())));

			Accessor::setStringValue(widget, value);

			Accessor::connectValueChanged(widget, [widget, sif, section = std::move(section), key = std::move(key)]() {
				const QString new_value = Accessor::getStringValue(widget);
				if (!new_value.isEmpty())
					sif->SetStringValue(section.c_str(), key.c_str(), new_value.toUtf8().constData());
				else
					sif->DeleteValue(section.c_str(), key.c_str());

				sif->Save();
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
			const QString value(
				QString::fromStdString(Host::GetBaseStringSettingValue(section.c_str(), key.c_str(), default_value.c_str())));

			Accessor::setStringValue(widget, value);

			Accessor::connectValueChanged(widget, [widget, section = std::move(section), key = std::move(key)]() {
				const QString new_value = Accessor::getStringValue(widget);
				if (!new_value.isEmpty())
					Host::SetBaseStringSettingValue(section.c_str(), key.c_str(), new_value.toUtf8().constData());
				else
					Host::RemoveBaseSettingValue(section.c_str(), key.c_str());

				Host::CommitBaseSettingChanges();
				g_emu_thread->applySettings();
			});
		}
	}
} // namespace ControllerSettingWidgetBinder
