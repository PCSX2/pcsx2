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

#include "EmuThread.h"
#include "QtHost.h"
#include "Settings/SettingsDialog.h"

namespace SettingWidgetBinder
{
	template <typename T>
	struct SettingAccessor
	{
		static bool getBoolValue(const T* widget);
		static void setBoolValue(T* widget, bool value);
		static void makeNullableBool(T* widget, bool globalValue);
		static std::optional<bool> getNullableBoolValue(const T* widget);
		static void setNullableBoolValue(T* widget, std::optional<bool> value);

		static int getIntValue(const T* widget);
		static void setIntValue(T* widget, bool nullable, int value);
		static void makeNullableInt(T* widget, int globalValue);
		static std::optional<int> getNullableIntValue(const T* widget);
		static void setNullableIntValue(T* widget, std::optional<int> value);

		static int getFloatValue(const T* widget);
		static void setFloatValue(T* widget, int value);
		static void makeNullableFloat(T* widget, float globalValue);
		static std::optional<float> getNullableFloatValue(const T* widget);
		static void setNullableFloatValue(T* widget, std::optional<float> value);

		static QString getStringValue(const T* widget);
		static void setStringValue(T* widget, const QString& value);
		static void makeNullableString(T* widget, const QString& globalValue);
		static std::optional<QString> getNullableStringValue(const T* widget);
		static void setNullableFloatValue(T* widget, std::optional<QString> value);

		template <typename F>
		static void connectValueChanged(T* widget, F func);
	};

	template <>
	struct SettingAccessor<QLineEdit>
	{
		static bool getBoolValue(const QLineEdit* widget) { return widget->text().toInt() != 0; }
		static void setBoolValue(QLineEdit* widget, bool value) { widget->setText(value ? QStringLiteral("1") : QStringLiteral("0")); }
		static void makeNullableBool(QLineEdit* widget, bool globalValue) { widget->setEnabled(false); }
		static std::optional<bool> getNullableBoolValue(const QLineEdit* widget) { return getBoolValue(widget); }
		static void setNullableBoolValue(QLineEdit* widget, std::optional<bool> value) { setBoolValue(widget, value.value_or(false)); }

		static int getIntValue(const QLineEdit* widget) { return widget->text().toInt(); }
		static void setIntValue(QLineEdit* widget, int value) { widget->setText(QString::number(value)); }
		static void makeNullableInt(QLineEdit* widget, int globalValue) { widget->setEnabled(false); }
		static std::optional<int> getNullableIntValue(const QLineEdit* widget) { return getIntValue(widget); }
		static void setNullableIntValue(QLineEdit* widget, std::optional<int> value) { setIntValue(widget, value.value_or(0)); }

		static float getFloatValue(const QLineEdit* widget) { return widget->text().toFloat(); }
		static void setFloatValue(QLineEdit* widget, float value) { widget->setText(QString::number(value)); }
		static void makeNullableFloat(QLineEdit* widget, float globalValue) { widget->setEnabled(false); }
		static std::optional<float> getNullableFloatValue(const QLineEdit* widget) { return getFloatValue(widget); }
		static void setNullableFloatValue(QLineEdit* widget, std::optional<float> value) { setFloatValue(widget, value.value_or(0.0f)); }

		static QString getStringValue(const QLineEdit* widget) { return widget->text(); }
		static void setStringValue(QLineEdit* widget, const QString& value) { widget->setText(value); }
		static void makeNullableString(QLineEdit* widget, const QString& globalValue) { widget->setEnabled(false); }
		static std::optional<QString> getNullableStringValue(const QLineEdit* widget) { return getStringValue(widget); }
		static void setNullableStringValue(QLineEdit* widget, std::optional<QString> value) { setStringValue(widget, value.value_or(QString())); }

		template <typename F>
		static void connectValueChanged(QLineEdit* widget, F func)
		{
			widget->connect(widget, &QLineEdit::textChanged, func);
		}
	};

	template <>
	struct SettingAccessor<QComboBox>
	{
		static bool isNullValue(const QComboBox* widget) { return (widget->currentIndex() == 0); }

		static bool getBoolValue(const QComboBox* widget) { return widget->currentIndex() > 0; }
		static void setBoolValue(QComboBox* widget, bool value) { widget->setCurrentIndex(value ? 1 : 0); }
		static void makeNullableBool(QComboBox* widget, bool globalValue)
		{
			widget->insertItem(0, globalValue ? qApp->translate("SettingsDialog", "Use Global Setting [Enabled]") :
                                                qApp->translate("SettingsDialog", "Use Global Setting [Disabled]"));
		}

		static int getIntValue(const QComboBox* widget) { return widget->currentIndex(); }
		static void setIntValue(QComboBox* widget, int value) { widget->setCurrentIndex(value); }
		static void makeNullableInt(QComboBox* widget, int globalValue)
		{
			widget->insertItem(0, qApp->translate("SettingsDialog", "Use Global Setting [%1]")
									  .arg((globalValue >= 0 && globalValue < widget->count()) ? widget->itemText(globalValue) : QString()));
		}
		static std::optional<int> getNullableIntValue(const QComboBox* widget)
		{
			return isNullValue(widget) ? std::nullopt : std::optional<int>(widget->currentIndex() - 1);
		}
		static void setNullableIntValue(QComboBox* widget, std::optional<int> value)
		{
			widget->setCurrentIndex(value.has_value() ? (value.value() + 1) : 0);
		}

		static float getFloatValue(const QComboBox* widget) { return static_cast<float>(widget->currentIndex()); }
		static void setFloatValue(QComboBox* widget, float value) { widget->setCurrentIndex(static_cast<int>(value)); }
		static void makeNullableFloat(QComboBox* widget, float globalValue)
		{
			widget->insertItem(0,
				qApp->translate("SettingsDialog", "Use Global Setting [%1]")
					.arg((globalValue >= 0.0f && static_cast<int>(globalValue) < widget->count()) ? widget->itemText(static_cast<int>(globalValue)) :
                                                                                                    QString()));
		}
		static std::optional<float> getNullableFloatValue(const QComboBox* widget)
		{
			return isNullValue(widget) ? std::nullopt : std::optional<float>(static_cast<float>(widget->currentIndex() + 1));
		}
		static void setNullableFloatValue(QComboBox* widget, std::optional<float> value)
		{
			widget->setCurrentIndex(value.has_value() ? static_cast<int>(value.value() + 1.0f) : 0);
		}

		static QString getStringValue(const QComboBox* widget)
		{
			const QVariant currentData(widget->currentData());
			if (currentData.metaType().id() == QMetaType::QString)
				return currentData.toString();

			return widget->currentText();
		}
		static void setStringValue(QComboBox* widget, const QString& value)
		{
			const int index = widget->findData(value);
			if (index >= 0)
			{
				widget->setCurrentIndex(index);
				return;
			}

			widget->setCurrentText(value);
		}
		static void makeNullableString(QComboBox* widget, const QString& globalValue) { makeNullableInt(widget, widget->findData(globalValue)); }
		static std::optional<QString> getNullableStringValue(const QComboBox* widget)
		{
			return isNullValue(widget) ? std::nullopt : std::optional<QString>(getStringValue(widget));
		}
		static void setNullableStringValue(QComboBox* widget, std::optional<QString> value)
		{
			isNullValue(widget) ? widget->setCurrentIndex(0) : setStringValue(widget, value.value());
		}

		template <typename F>
		static void connectValueChanged(QComboBox* widget, F func)
		{
			widget->connect(widget, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), func);
		}
	};

	template <>
	struct SettingAccessor<QCheckBox>
	{
		static bool getBoolValue(const QCheckBox* widget) { return widget->isChecked(); }
		static void setBoolValue(QCheckBox* widget, bool value) { widget->setChecked(value); }
		static void makeNullableBool(QCheckBox* widget, bool globalValue) { widget->setTristate(true); }
		static std::optional<bool> getNullableBoolValue(const QCheckBox* widget)
		{
			return (widget->checkState() == Qt::PartiallyChecked) ? std::nullopt : std::optional<bool>(widget->isChecked());
		}
		static void setNullableBoolValue(QCheckBox* widget, std::optional<bool> value)
		{
			widget->setCheckState(value.has_value() ? (value.value() ? Qt::Checked : Qt::Unchecked) : Qt::PartiallyChecked);
		}

		static int getIntValue(const QCheckBox* widget) { return widget->isChecked() ? 1 : 0; }
		static void setIntValue(QCheckBox* widget, int value) { widget->setChecked(value != 0); }
		static void makeNullableInt(QCheckBox* widget, int globalValue) { widget->setTristate(true); }
		static std::optional<int> getNullableIntValue(const QCheckBox* widget)
		{
			return (widget->checkState() == Qt::PartiallyChecked) ? std::nullopt : std::optional<int>(widget->isChecked() ? 1 : 0);
		}
		static void setNullableIntValue(QCheckBox* widget, std::optional<int> value)
		{
			widget->setCheckState(value.has_value() ? ((value.value() != 0) ? Qt::Checked : Qt::Unchecked) : Qt::PartiallyChecked);
		}

		static float getFloatValue(const QCheckBox* widget) { return widget->isChecked() ? 1.0f : 0.0f; }
		static void setFloatValue(QCheckBox* widget, float value) { widget->setChecked(value != 0.0f); }
		static void makeNullableFloat(QCheckBox* widget, float globalValue) { widget->setTristate(true); }
		static std::optional<float> getNullableFloatValue(const QCheckBox* widget)
		{
			return (widget->checkState() == Qt::PartiallyChecked) ? std::nullopt : std::optional<float>(widget->isChecked() ? 1.0f : 0.0f);
		}
		static void setNullableFloatValue(QCheckBox* widget, std::optional<float> value)
		{
			widget->setCheckState(value.has_value() ? ((value.value() != 0.0f) ? Qt::Checked : Qt::Unchecked) : Qt::PartiallyChecked);
		}

		static QString getStringValue(const QCheckBox* widget) { return widget->isChecked() ? QStringLiteral("1") : QStringLiteral("0"); }
		static void setStringValue(QCheckBox* widget, const QString& value) { widget->setChecked(value.toInt() != 0); }
		static void makeNullableString(QCheckBox* widget, const QString& globalValue) { widget->setTristate(true); }
		static std::optional<QString> getNullableStringValue(const QCheckBox* widget)
		{
			return (widget->checkState() == Qt::PartiallyChecked) ?
                       std::nullopt :
                       std::optional<QString>(widget->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
		}
		static void setNullableStringValue(QCheckBox* widget, std::optional<QString> value)
		{
			widget->setCheckState(value.has_value() ? ((value->toInt() != 0) ? Qt::Checked : Qt::Unchecked) : Qt::PartiallyChecked);
		}

		template <typename F>
		static void connectValueChanged(QCheckBox* widget, F func)
		{
			widget->connect(widget, &QCheckBox::stateChanged, func);
		}
	};

	template <>
	struct SettingAccessor<QSlider>
	{
		static bool getBoolValue(const QSlider* widget) { return widget->value() > 0; }
		static void setBoolValue(QSlider* widget, bool value) { widget->setValue(value ? 1 : 0); }
		static void makeNullableBool(QSlider* widget, bool globalSetting) { widget->setEnabled(false); }
		static std::optional<bool> getNullableBoolValue(const QSlider* widget) { return getBoolValue(widget); }
		static void setNullableBoolValue(QSlider* widget, std::optional<bool> value) { setBoolValue(widget, value.value_or(false)); }

		static int getIntValue(const QSlider* widget) { return widget->value(); }
		static void setIntValue(QSlider* widget, int value) { widget->setValue(value); }
		static void makeNullableInt(QSlider* widget, int globalValue) { widget->setEnabled(false); }
		static std::optional<int> getNullableIntValue(const QSlider* widget) { return getIntValue(widget); }
		static void setNullableIntValue(QSlider* widget, std::optional<int> value) { setIntValue(widget, value.value_or(0)); }

		static float getFloatValue(const QSlider* widget) { return static_cast<float>(widget->value()); }
		static void setFloatValue(QSlider* widget, float value) { widget->setValue(static_cast<int>(value)); }
		static void makeNullableFloat(QSlider* widget, float globalValue) { widget->setEnabled(false); }
		static std::optional<float> getNullableFloatValue(const QSlider* widget) { return getFloatValue(widget); }
		static void setNullableFloatValue(QSlider* widget, std::optional<float> value) { setFloatValue(widget, value.value_or(0.0f)); }

		static QString getStringValue(const QSlider* widget) { return QString::number(widget->value()); }
		static void setStringValue(QSlider* widget, const QString& value) { widget->setValue(value.toInt()); }
		static void makeNullableString(QSlider* widget, const QString& globalValue) { widget->setEnabled(false); }
		static std::optional<QString> getNullableStringValue(const QSlider* widget) { return getStringValue(widget); }
		static void setNullableStringValue(QSlider* widget, std::optional<QString> value) { setStringValue(widget, value.value_or(QString())); }

		template <typename F>
		static void connectValueChanged(QSlider* widget, F func)
		{
			widget->connect(widget, &QSlider::valueChanged, func);
		}
	};

	template <>
	struct SettingAccessor<QSpinBox>
	{
		static bool getBoolValue(const QSpinBox* widget) { return widget->value() > 0; }
		static void setBoolValue(QSpinBox* widget, bool value) { widget->setValue(value ? 1 : 0); }
		static void makeNullableBool(QSpinBox* widget, bool globalSetting) { widget->setEnabled(false); }
		static std::optional<bool> getNullableBoolValue(const QSpinBox* widget) { return getBoolValue(widget); }
		static void setNullableBoolValue(QSpinBox* widget, std::optional<bool> value) { setBoolValue(widget, value.value_or(false)); }

		static int getIntValue(const QSpinBox* widget) { return widget->value(); }
		static void setIntValue(QSpinBox* widget, int value) { widget->setValue(value); }
		static void makeNullableInt(QSpinBox* widget, int globalValue) { widget->setEnabled(false); }
		static std::optional<int> getNullableIntValue(const QSpinBox* widget) { return getIntValue(widget); }
		static void setNullableIntValue(QSpinBox* widget, std::optional<int> value) { setIntValue(widget, value.value_or(0)); }

		static float getFloatValue(const QSpinBox* widget) { return static_cast<float>(widget->value()); }
		static void setFloatValue(QSpinBox* widget, float value) { widget->setValue(static_cast<int>(value)); }
		static void makeNullableFloat(QSpinBox* widget, float globalValue) { widget->setEnabled(false); }
		static std::optional<float> getNullableFloatValue(const QSpinBox* widget) { return getFloatValue(widget); }
		static void setNullableFloatValue(QSpinBox* widget, std::optional<float> value) { setFloatValue(widget, value.value_or(0.0f)); }

		static QString getStringValue(const QSpinBox* widget) { return QString::number(widget->value()); }
		static void setStringValue(QSpinBox* widget, const QString& value) { widget->setValue(value.toInt()); }
		static void makeNullableString(QSpinBox* widget, const QString& globalValue) { widget->setEnabled(false); }
		static std::optional<QString> getNullableStringValue(const QSpinBox* widget) { return getStringValue(widget); }
		static void setNullableStringValue(QSpinBox* widget, std::optional<QString> value) { setStringValue(widget, value.value_or(QString())); }

		template <typename F>
		static void connectValueChanged(QSpinBox* widget, F func)
		{
			widget->connect(widget, QOverload<int>::of(&QSpinBox::valueChanged), func);
		}
	};

	template <>
	struct SettingAccessor<QDoubleSpinBox>
	{
		static bool getBoolValue(const QDoubleSpinBox* widget) { return widget->value() > 0.0; }
		static void setBoolValue(QDoubleSpinBox* widget, bool value) { widget->setValue(value ? 1.0 : 0.0); }
		static void makeNullableBool(QDoubleSpinBox* widget, bool globalSetting) { widget->setEnabled(false); }
		static std::optional<bool> getNullableBoolValue(const QDoubleSpinBox* widget) { return getBoolValue(widget); }
		static void setNullableBoolValue(QDoubleSpinBox* widget, std::optional<bool> value) { setBoolValue(widget, value.value_or(false)); }

		static int getIntValue(const QDoubleSpinBox* widget) { return static_cast<int>(widget->value()); }
		static void setIntValue(QDoubleSpinBox* widget, int value) { widget->setValue(static_cast<double>(value)); }
		static void makeNullableInt(QDoubleSpinBox* widget, int globalValue) { widget->setEnabled(false); }
		static std::optional<int> getNullableIntValue(const QDoubleSpinBox* widget) { return getIntValue(widget); }
		static void setNullableIntValue(QDoubleSpinBox* widget, std::optional<int> value) { setIntValue(widget, value.value_or(0)); }

		static float getFloatValue(const QDoubleSpinBox* widget) { return static_cast<float>(widget->value()); }
		static void setFloatValue(QDoubleSpinBox* widget, float value) { widget->setValue(static_cast<double>(value)); }
		static void makeNullableFloat(QDoubleSpinBox* widget, float globalValue) { widget->setEnabled(false); }
		static std::optional<float> getNullableFloatValue(const QDoubleSpinBox* widget) { return getFloatValue(widget); }
		static void setNullableFloatValue(QDoubleSpinBox* widget, std::optional<float> value) { setFloatValue(widget, value.value_or(0.0f)); }

		static QString getStringValue(const QDoubleSpinBox* widget) { return QString::number(widget->value()); }
		static void setStringValue(QDoubleSpinBox* widget, const QString& value) { widget->setValue(value.toDouble()); }
		static void makeNullableString(QDoubleSpinBox* widget, const QString& globalValue) { widget->setEnabled(false); }
		static std::optional<QString> getNullableStringValue(const QDoubleSpinBox* widget) { return getStringValue(widget); }
		static void setNullableStringValue(QDoubleSpinBox* widget, std::optional<QString> value)
		{
			setStringValue(widget, value.value_or(QString()));
		}

		template <typename F>
		static void connectValueChanged(QDoubleSpinBox* widget, F func)
		{
			widget->connect(widget, QOverload<double>::of(&QDoubleSpinBox::valueChanged), func);
		}
	};

	template <>
	struct SettingAccessor<QAction>
	{
		static bool getBoolValue(const QAction* widget) { return widget->isChecked(); }
		static void setBoolValue(QAction* widget, bool value) { widget->setChecked(value); }
		static void makeNullableBool(QAction* widget, bool globalSetting) { widget->setEnabled(false); }
		static std::optional<bool> getNullableBoolValue(const QAction* widget) { return getBoolValue(widget); }
		static void setNullableBoolValue(QAction* widget, std::optional<bool> value) { setBoolValue(widget, value.value_or(false)); }

		static int getIntValue(const QAction* widget) { return widget->isChecked() ? 1 : 0; }
		static void setIntValue(QAction* widget, int value) { widget->setChecked(value != 0); }
		static void makeNullableInt(QAction* widget, int globalValue) { widget->setEnabled(false); }
		static std::optional<int> getNullableIntValue(const QAction* widget) { return getIntValue(widget); }
		static void setNullableIntValue(QAction* widget, std::optional<int> value) { setIntValue(widget, value.value_or(0)); }

		static float getFloatValue(const QAction* widget) { return widget->isChecked() ? 1.0f : 0.0f; }
		static void setFloatValue(QAction* widget, float value) { widget->setChecked(value != 0.0f); }
		static void makeNullableFloat(QAction* widget, float globalValue) { widget->setEnabled(false); }
		static std::optional<float> getNullableFloatValue(const QAction* widget) { return getFloatValue(widget); }
		static void setNullableFloatValue(QAction* widget, std::optional<float> value) { setFloatValue(widget, value.value_or(0.0f)); }

		static QString getStringValue(const QAction* widget) { return widget->isChecked() ? QStringLiteral("1") : QStringLiteral("0"); }
		static void setStringValue(QAction* widget, const QString& value) { widget->setChecked(value.toInt() != 0); }
		static void makeNullableString(QAction* widget, const QString& globalValue) { widget->setEnabled(false); }
		static std::optional<QString> getNullableStringValue(const QAction* widget) { return getStringValue(widget); }
		static void setNullableStringValue(QAction* widget, std::optional<QString> value) { setStringValue(widget, value.value_or(QString())); }

		template <typename F>
		static void connectValueChanged(QAction* widget, F func)
		{
			widget->connect(widget, &QAction::toggled, func);
		}
	};

	/// Binds a widget's value to a setting, updating it when the value changes.

	template <typename WidgetType>
	static void BindWidgetToBoolSetting(SettingsInterface* sif, WidgetType* widget, std::string section, std::string key, bool default_value)
	{
		using Accessor = SettingAccessor<WidgetType>;

		const bool value = Host::GetBaseBoolSettingValue(section.c_str(), key.c_str(), default_value);

		if (sif)
		{
			Accessor::makeNullableBool(widget, value);

			bool sif_value;
			if (sif->GetBoolValue(section.c_str(), key.c_str(), &sif_value))
				Accessor::setNullableBoolValue(widget, sif_value);
			else
				Accessor::setNullableBoolValue(widget, std::nullopt);

			Accessor::connectValueChanged(widget, [sif, widget, section = std::move(section), key = std::move(key)]() {
				if (std::optional<bool> new_value = Accessor::getNullableBoolValue(widget); new_value.has_value())
					sif->SetBoolValue(section.c_str(), key.c_str(), new_value.value());
				else
					sif->DeleteValue(section.c_str(), key.c_str());

				sif->Save();
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
			Accessor::setBoolValue(widget, value);

			Accessor::connectValueChanged(widget, [widget, section = std::move(section), key = std::move(key)]() {
				const bool new_value = Accessor::getBoolValue(widget);
				QtHost::SetBaseBoolSettingValue(section.c_str(), key.c_str(), new_value);
				g_emu_thread->applySettings();
			});
		}
	}

	template <typename WidgetType>
	static void BindWidgetToIntSetting(
		SettingsInterface* sif, WidgetType* widget, std::string section, std::string key, int default_value, int option_offset = 0)
	{
		using Accessor = SettingAccessor<WidgetType>;

		const s32 value = Host::GetBaseIntSettingValue(section.c_str(), key.c_str(), static_cast<s32>(default_value)) - option_offset;

		if (sif)
		{
			Accessor::makeNullableInt(widget, value);

			int sif_value;
			if (sif->GetIntValue(section.c_str(), key.c_str(), &sif_value))
				Accessor::setNullableIntValue(widget, sif_value - option_offset);
			else
				Accessor::setNullableIntValue(widget, std::nullopt);

			Accessor::connectValueChanged(widget, [sif, widget, section = std::move(section), key = std::move(key), option_offset]() {
				if (std::optional<int> new_value = Accessor::getNullableIntValue(widget); new_value.has_value())
					sif->SetIntValue(section.c_str(), key.c_str(), new_value.value() + option_offset);
				else
					sif->DeleteValue(section.c_str(), key.c_str());

				sif->Save();
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
			Accessor::setIntValue(widget, static_cast<int>(value));

			Accessor::connectValueChanged(widget, [widget, section = std::move(section), key = std::move(key), option_offset]() {
				const int new_value = Accessor::getIntValue(widget);
				QtHost::SetBaseIntSettingValue(section.c_str(), key.c_str(), new_value + option_offset);
				g_emu_thread->applySettings();
			});
		}
	}

	template <typename WidgetType>
	static void BindWidgetToFloatSetting(SettingsInterface* sif, WidgetType* widget, std::string section, std::string key, float default_value)
	{
		using Accessor = SettingAccessor<WidgetType>;

		const float value = Host::GetBaseFloatSettingValue(section.c_str(), key.c_str(), default_value);

		if (sif)
		{
			Accessor::makeNullableFloat(widget, value);

			float sif_value;
			if (sif->GetFloatValue(section.c_str(), key.c_str(), &sif_value))
				Accessor::setNullableFloatValue(widget, sif_value);
			else
				Accessor::setNullableFloatValue(widget, std::nullopt);

			Accessor::connectValueChanged(widget, [sif, widget, section = std::move(section), key = std::move(key)]() {
				if (std::optional<float> new_value = Accessor::getNullableFloatValue(widget); new_value.has_value())
					sif->SetFloatValue(section.c_str(), key.c_str(), new_value.value());
				else
					sif->DeleteValue(section.c_str(), key.c_str());

				sif->Save();
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
			Accessor::setFloatValue(widget, value);

			Accessor::connectValueChanged(widget, [widget, section = std::move(section), key = std::move(key)]() {
				const float new_value = Accessor::getFloatValue(widget);
				QtHost::SetBaseFloatSettingValue(section.c_str(), key.c_str(), new_value);
				g_emu_thread->applySettings();
			});
		}
	}

	template <typename WidgetType>
	static void BindWidgetToNormalizedSetting(
		SettingsInterface* sif, WidgetType* widget, std::string section, std::string key, float range, float default_value)
	{
		using Accessor = SettingAccessor<WidgetType>;

		const float value = Host::GetBaseFloatSettingValue(section.c_str(), key.c_str(), default_value);

		if (sif)
		{
			Accessor::makeNullableInt(widget, static_cast<int>(value * range));

			float sif_value;
			if (sif->GetFloatValue(section.c_str(), key.c_str(), &sif_value))
				Accessor::setNullableIntValue(widget, static_cast<int>(sif_value * range));
			else
				Accessor::setNullableIntValue(widget, std::nullopt);

			Accessor::connectValueChanged(widget, [sif, widget, section = std::move(section), key = std::move(key), range]() {
				if (std::optional<int> new_value = Accessor::getNullableIntValue(widget); new_value.has_value())
					sif->SetFloatValue(section.c_str(), key.c_str(), static_cast<float>(new_value.value()) / range);
				else
					sif->DeleteValue(section.c_str(), key.c_str());

				sif->Save();
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
			Accessor::setIntValue(widget, static_cast<int>(value * range));

			Accessor::connectValueChanged(widget, [widget, section = std::move(section), key = std::move(key), range]() {
				const float new_value = (static_cast<float>(Accessor::getIntValue(widget)) / range);
				QtHost::SetBaseFloatSettingValue(section.c_str(), key.c_str(), new_value);
				g_emu_thread->applySettings();
			});
		}
	}

	template <typename WidgetType>
	static void BindWidgetToStringSetting(
		SettingsInterface* sif, WidgetType* widget, std::string section, std::string key, std::string default_value = std::string())
	{
		using Accessor = SettingAccessor<WidgetType>;

		const QString value(QString::fromStdString(Host::GetBaseStringSettingValue(section.c_str(), key.c_str(), default_value.c_str())));

		if (sif)
		{
			Accessor::makeNullableString(widget, value);

			std::string sif_value;
			if (sif->GetStringValue(section.c_str(), key.c_str(), &sif_value))
				Accessor::setNullableStringValue(widget, QString::fromStdString(sif_value));
			else
				Accessor::setNullableStringValue(widget, std::nullopt);

			Accessor::connectValueChanged(widget, [widget, sif, section = std::move(section), key = std::move(key)]() {
				if (std::optional<QString> new_value = Accessor::getNullableStringValue(widget); new_value.has_value())
					sif->SetStringValue(section.c_str(), key.c_str(), new_value->toUtf8().constData());
				else
					sif->DeleteValue(section.c_str(), key.c_str());

				sif->Save();
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
			Accessor::setStringValue(widget, value);

			Accessor::connectValueChanged(widget, [widget, section = std::move(section), key = std::move(key)]() {
				const QString new_value = Accessor::getStringValue(widget);
				if (!new_value.isEmpty())
					QtHost::SetBaseStringSettingValue(section.c_str(), key.c_str(), new_value.toUtf8().constData());
				else
					QtHost::RemoveBaseSettingValue(section.c_str(), key.c_str());

				g_emu_thread->applySettings();
			});
		}
	}

	template <typename WidgetType, typename DataType>
	static void BindWidgetToEnumSetting(SettingsInterface* sif, WidgetType* widget, std::string section, std::string key,
		std::optional<DataType> (*from_string_function)(const char* str), const char* (*to_string_function)(DataType value), DataType default_value)
	{
		using Accessor = SettingAccessor<WidgetType>;
		using UnderlyingType = std::underlying_type_t<DataType>;

		const std::string value(Host::GetBaseStringSettingValue(section.c_str(), key.c_str(), to_string_function(default_value)));
		const std::optional<DataType> typed_value = from_string_function(value.c_str());

		if (sif)
		{
			Accessor::makeNullableInt(widget, typed_value.has_value() ? static_cast<int>(static_cast<UnderlyingType>(typed_value.value())) : 0);

			std::string sif_value;
			if (sif->GetStringValue(section.c_str(), key.c_str(), &sif_value))
			{
				const std::optional<DataType> old_setting_value = from_string_function(sif_value.c_str());
				if (old_setting_value.has_value())
					Accessor::setNullableIntValue(widget, static_cast<int>(static_cast<UnderlyingType>(old_setting_value.value())));
				else
					Accessor::setNullableIntValue(widget, std::nullopt);
			}
			else
			{
				Accessor::setNullableIntValue(widget, std::nullopt);
			}

			Accessor::connectValueChanged(widget, [sif, widget, section = std::move(section), key = std::move(key), to_string_function]() {
				if (std::optional<int> new_value = Accessor::getNullableIntValue(widget); new_value.has_value())
				{
					const char* string_value = to_string_function(static_cast<DataType>(static_cast<UnderlyingType>(new_value.value())));
					sif->SetStringValue(section.c_str(), key.c_str(), string_value);
				}
				else
				{
					sif->DeleteValue(section.c_str(), key.c_str());
				}

				sif->Save();
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
			if (typed_value.has_value())
				Accessor::setIntValue(widget, static_cast<int>(static_cast<UnderlyingType>(typed_value.value())));
			else
				Accessor::setIntValue(widget, static_cast<int>(static_cast<UnderlyingType>(default_value)));

			Accessor::connectValueChanged(widget, [widget, section = std::move(section), key = std::move(key), to_string_function]() {
				const DataType value = static_cast<DataType>(static_cast<UnderlyingType>(Accessor::getIntValue(widget)));
				const char* string_value = to_string_function(value);
				QtHost::SetBaseStringSettingValue(section.c_str(), key.c_str(), string_value);
				g_emu_thread->applySettings();
			});
		}
	}

	template <typename WidgetType, typename DataType>
	static void BindWidgetToEnumSetting(
		SettingsInterface* sif, WidgetType* widget, std::string section, std::string key, const char** enum_names, DataType default_value)
	{
		using Accessor = SettingAccessor<WidgetType>;
		using UnderlyingType = std::underlying_type_t<DataType>;

		const std::string value(
			Host::GetBaseStringSettingValue(section.c_str(), key.c_str(), enum_names[static_cast<UnderlyingType>(default_value)]));

		UnderlyingType enum_index = static_cast<UnderlyingType>(default_value);
		for (UnderlyingType i = 0; enum_names[i] != nullptr; i++)
		{
			if (value == enum_names[i])
			{
				enum_index = i;
				break;
			}
		}

		if (sif)
		{
			Accessor::makeNullableInt(widget, static_cast<int>(enum_index));

			std::string sif_value;
			std::optional<int> sif_int_value;
			if (sif->GetStringValue(section.c_str(), key.c_str(), &sif_value))
			{
				for (UnderlyingType i = 0; enum_names[i] != nullptr; i++)
				{
					if (sif_value == enum_names[i])
					{
						sif_int_value = static_cast<int>(i);
						break;
					}
				}
			}
			Accessor::setNullableIntValue(widget, sif_int_value);

			Accessor::connectValueChanged(widget, [sif, widget, section = std::move(section), key = std::move(key), enum_names]() {
				if (std::optional<int> new_value = Accessor::getNullableIntValue(widget); new_value.has_value())
					sif->SetStringValue(section.c_str(), key.c_str(), enum_names[new_value.value()]);
				else
					sif->DeleteValue(section.c_str(), key.c_str());

				sif->Save();
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
			Accessor::setIntValue(widget, static_cast<int>(enum_index));

			Accessor::connectValueChanged(widget, [widget, section = std::move(section), key = std::move(key), enum_names]() {
				const UnderlyingType value = static_cast<UnderlyingType>(Accessor::getIntValue(widget));
				QtHost::SetBaseStringSettingValue(section.c_str(), key.c_str(), enum_names[value]);
				g_emu_thread->applySettings();
			});
		}
	}

	template <typename WidgetType>
	static void BindWidgetToEnumSetting(SettingsInterface* sif, WidgetType* widget, std::string section, std::string key, const char** enum_names,
		const char** enum_values, const char* default_value)
	{
		using Accessor = SettingAccessor<WidgetType>;

		const std::string value = Host::GetBaseStringSettingValue(section.c_str(), key.c_str(), default_value);

		for (int i = 0; enum_names[i] != nullptr; i++)
			widget->addItem(QString::fromUtf8(enum_names[i]));

		int enum_index = -1;
		for (int i = 0; enum_values[i] != nullptr; i++)
		{
			if (value == enum_values[i])
			{
				enum_index = i;
				break;
			}
		}

		if (sif)
		{
			Accessor::makeNullableInt(widget, enum_index);

			std::string sif_value;
			std::optional<int> sif_int_value;
			if (sif->GetStringValue(section.c_str(), key.c_str(), &sif_value))
			{
				for (int i = 0; enum_values[i] != nullptr; i++)
				{
					if (sif_value == enum_values[i])
					{
						sif_int_value = i;
						break;
					}
				}
			}
			Accessor::setNullableIntValue(widget, sif_int_value);

			Accessor::connectValueChanged(widget, [sif, widget, section = std::move(section), key = std::move(key), enum_values]() {
				if (std::optional<int> new_value = Accessor::getNullableIntValue(widget); new_value.has_value())
					sif->SetStringValue(section.c_str(), key.c_str(), enum_values[new_value.value()]);
				else
					sif->DeleteValue(section.c_str(), key.c_str());

				sif->Save();
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
			if (enum_index >= 0)
				Accessor::setIntValue(widget, enum_index);

			Accessor::connectValueChanged(widget, [widget, section = std::move(section), key = std::move(key), enum_values]() {
				const int value = Accessor::getIntValue(widget);
				QtHost::SetBaseStringSettingValue(section.c_str(), key.c_str(), enum_values[value]);
				g_emu_thread->applySettings();
			});
		}
	}

} // namespace SettingWidgetBinder
