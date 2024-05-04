// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <optional>
#include <type_traits>

#include <QtCore/QtCore>
#include <QtGui/QAction>
#include <QtGui/QFont>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>

#include "common/FileSystem.h"
#include "common/Path.h"

#include "pcsx2/Config.h"
#include "pcsx2/Host.h"

#include "QtHost.h"
#include "QtUtils.h"
#include "Settings/SettingsWindow.h"

namespace SettingWidgetBinder
{
	static constexpr const char* NULLABLE_PROPERTY = "SettingWidgetBinder_isNullable";
	static constexpr const char* IS_NULL_PROPERTY = "SettingWidgetBinder_isNull";
	static constexpr const char* GLOBAL_VALUE_PROPERTY = "SettingWidgetBinder_globalValue";

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
		static void setNullableStringValue(QLineEdit* widget, std::optional<QString> value)
		{
			setStringValue(widget, value.value_or(QString()));
		}

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
			//: THIS STRING IS SHARED ACROSS MULTIPLE OPTIONS. Be wary about gender/number. Also, ignore Crowdin's warning regarding [Enabled]: the text must be translated.
			widget->insertItem(0,
				globalValue ?
					qApp->translate("SettingsDialog", "Use Global Setting [Enabled]") :
					//: THIS STRING IS SHARED ACROSS MULTIPLE OPTIONS. Be wary about gender/number. Also, ignore Crowdin's warning regarding [Disabled]: the text must be translated.
					qApp->translate("SettingsDialog", "Use Global Setting [Disabled]"));
		}

		static int getIntValue(const QComboBox* widget) { return widget->currentIndex(); }
		static void setIntValue(QComboBox* widget, int value) { widget->setCurrentIndex(value); }
		static void makeNullableInt(QComboBox* widget, int globalValue)
		{
			widget->insertItem(
				0, qApp->translate("SettingsDialog", "Use Global Setting [%1]")
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
			widget->insertItem(0, qApp->translate("SettingsDialog", "Use Global Setting [%1]")
									  .arg((globalValue >= 0.0f && static_cast<int>(globalValue) < widget->count()) ?
											   widget->itemText(static_cast<int>(globalValue)) :
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
		static void makeNullableString(QComboBox* widget, const QString& globalValue)
		{
			makeNullableInt(widget, widget->findData(globalValue));
		}
		static std::optional<QString> getNullableStringValue(const QComboBox* widget)
		{
			return isNullValue(widget) ? std::nullopt : std::optional<QString>(getStringValue(widget));
		}
		static void setNullableStringValue(QComboBox* widget, std::optional<QString> value)
		{
			value.has_value() ? setStringValue(widget, value.value()) : widget->setCurrentIndex(0);
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
			widget->connect(widget, &QCheckBox::checkStateChanged, func);
		}
	};

	template <>
	struct SettingAccessor<QSlider>
	{
		static bool isNullable(const QSlider* widget) { return widget->property(NULLABLE_PROPERTY).toBool(); }

		static bool getBoolValue(const QSlider* widget) { return widget->value() > 0; }
		static void setBoolValue(QSlider* widget, bool value) { widget->setValue(value ? 1 : 0); }
		static void makeNullableBool(QSlider* widget, bool globalSetting)
		{
			widget->setProperty(NULLABLE_PROPERTY, QVariant(true));
			widget->setProperty(GLOBAL_VALUE_PROPERTY, QVariant(globalSetting));
		}
		static std::optional<bool> getNullableBoolValue(const QSlider* widget)
		{
			if (widget->property(IS_NULL_PROPERTY).toBool())
				return std::nullopt;

			return getBoolValue(widget);
		}
		static void setNullableBoolValue(QSlider* widget, std::optional<bool> value)
		{
			widget->setProperty(IS_NULL_PROPERTY, QVariant(!value.has_value()));
			setBoolValue(widget, value.has_value() ? value.value() : widget->property(GLOBAL_VALUE_PROPERTY).toBool());
		}

		static int getIntValue(const QSlider* widget) { return widget->value(); }
		static void setIntValue(QSlider* widget, int value) { widget->setValue(value); }
		static void makeNullableInt(QSlider* widget, int globalValue)
		{
			widget->setProperty(NULLABLE_PROPERTY, QVariant(true));
			widget->setProperty(GLOBAL_VALUE_PROPERTY, QVariant(globalValue));
		}
		static std::optional<int> getNullableIntValue(const QSlider* widget)
		{
			if (widget->property(IS_NULL_PROPERTY).toBool())
				return std::nullopt;

			return getIntValue(widget);
		}
		static void setNullableIntValue(QSlider* widget, std::optional<int> value)
		{
			widget->setProperty(IS_NULL_PROPERTY, QVariant(!value.has_value()));
			setIntValue(widget, value.has_value() ? value.value() : widget->property(GLOBAL_VALUE_PROPERTY).toInt());
		}

		static float getFloatValue(const QSlider* widget) { return static_cast<float>(widget->value()); }
		static void setFloatValue(QSlider* widget, float value) { widget->setValue(static_cast<int>(value)); }
		static void makeNullableFloat(QSlider* widget, float globalValue) { widget->setEnabled(false); }
		static std::optional<float> getNullableFloatValue(const QSlider* widget)
		{
			if (widget->property(IS_NULL_PROPERTY).toBool())
				return std::nullopt;

			return getFloatValue(widget);
		}
		static void setNullableFloatValue(QSlider* widget, std::optional<float> value)
		{
			widget->setProperty(IS_NULL_PROPERTY, QVariant(!value.has_value()));
			setFloatValue(widget, value.has_value() ? value.value() : widget->property(GLOBAL_VALUE_PROPERTY).toFloat());
		}

		static QString getStringValue(const QSlider* widget) { return QString::number(widget->value()); }
		static void setStringValue(QSlider* widget, const QString& value) { widget->setValue(value.toInt()); }
		static void makeNullableString(QSlider* widget, const QString& globalValue) { widget->setEnabled(false); }
		static std::optional<QString> getNullableStringValue(const QSlider* widget)
		{
			if (widget->property(IS_NULL_PROPERTY).toBool())
				return std::nullopt;

			return getStringValue(widget);
		}
		static void setNullableStringValue(QSlider* widget, std::optional<QString> value)
		{
			widget->setProperty(IS_NULL_PROPERTY, QVariant(!value.has_value()));
			setStringValue(widget, value.has_value() ? value.value() : widget->property(GLOBAL_VALUE_PROPERTY).toString());
		}

		template <typename F>
		static void connectValueChanged(QSlider* widget, F func)
		{
			if (!isNullable(widget))
			{
				widget->connect(widget, &QSlider::valueChanged, func);
			}
			else
			{
				widget->setContextMenuPolicy(Qt::CustomContextMenu);
				widget->connect(widget, &QSlider::customContextMenuRequested, widget, [widget, func](const QPoint& pt) {
					QMenu menu(widget);
					widget->connect(menu.addAction(qApp->translate("SettingWidgetBinder", "Reset")), &QAction::triggered, widget,
						[widget, func = std::move(func)]() {
							const bool old = widget->blockSignals(true);
							setNullableIntValue(widget, std::nullopt);
							widget->blockSignals(old);
							func();
						});
					menu.exec(widget->mapToGlobal(pt));
				});
				widget->connect(widget, &QSlider::valueChanged, widget, [widget, func = std::move(func)]() {
					if (widget->property(IS_NULL_PROPERTY).toBool())
						widget->setProperty(IS_NULL_PROPERTY, QVariant(false));
					func();
				});
			}
		}
	};

	template <>
	struct SettingAccessor<QSpinBox>
	{
		static bool isNullable(const QSpinBox* widget) { return widget->property(NULLABLE_PROPERTY).toBool(); }

		static void updateNullState(QSpinBox* widget, bool isNull)
		{
			widget->setPrefix(isNull ? qApp->translate("SettingWidgetBinder", "Default: ") : QString());
		}

		static bool getBoolValue(const QSpinBox* widget) { return widget->value() > 0; }
		static void setBoolValue(QSpinBox* widget, bool value) { widget->setValue(value ? 1 : 0); }
		static void makeNullableBool(QSpinBox* widget, bool globalSetting)
		{
			widget->setProperty(NULLABLE_PROPERTY, QVariant(true));
			widget->setProperty(GLOBAL_VALUE_PROPERTY, QVariant(globalSetting));
		}
		static std::optional<bool> getNullableBoolValue(const QSpinBox* widget)
		{
			if (widget->property(IS_NULL_PROPERTY).toBool())
				return std::nullopt;

			return getBoolValue(widget);
		}
		static void setNullableBoolValue(QSpinBox* widget, std::optional<bool> value)
		{
			widget->setProperty(IS_NULL_PROPERTY, QVariant(!value.has_value()));
			setBoolValue(widget, value.has_value() ? value.value() : widget->property(GLOBAL_VALUE_PROPERTY).toBool());
			updateNullState(widget, !value.has_value());
		}

		static int getIntValue(const QSpinBox* widget) { return widget->value(); }
		static void setIntValue(QSpinBox* widget, int value) { widget->setValue(value); }
		static void makeNullableInt(QSpinBox* widget, int globalValue)
		{
			widget->setProperty(NULLABLE_PROPERTY, QVariant(true));
			widget->setProperty(GLOBAL_VALUE_PROPERTY, QVariant(globalValue));
		}
		static std::optional<int> getNullableIntValue(const QSpinBox* widget)
		{
			if (widget->property(IS_NULL_PROPERTY).toBool())
				return std::nullopt;

			return getIntValue(widget);
		}
		static void setNullableIntValue(QSpinBox* widget, std::optional<int> value)
		{
			widget->setProperty(IS_NULL_PROPERTY, QVariant(!value.has_value()));
			setIntValue(widget, value.has_value() ? value.value() : widget->property(GLOBAL_VALUE_PROPERTY).toInt());
			updateNullState(widget, !value.has_value());
		}

		static float getFloatValue(const QSpinBox* widget) { return static_cast<float>(widget->value()); }
		static void setFloatValue(QSpinBox* widget, float value) { widget->setValue(static_cast<int>(value)); }
		static void makeNullableFloat(QSpinBox* widget, float globalValue)
		{
			widget->setProperty(NULLABLE_PROPERTY, QVariant(true));
			widget->setProperty(GLOBAL_VALUE_PROPERTY, QVariant(globalValue));
		}
		static std::optional<float> getNullableFloatValue(const QSpinBox* widget)
		{
			if (widget->property(IS_NULL_PROPERTY).toBool())
				return std::nullopt;

			return getFloatValue(widget);
		}
		static void setNullableFloatValue(QSpinBox* widget, std::optional<float> value)
		{
			widget->setProperty(IS_NULL_PROPERTY, QVariant(!value.has_value()));
			setFloatValue(widget, value.has_value() ? value.value() : widget->property(GLOBAL_VALUE_PROPERTY).toFloat());
			updateNullState(widget, !value.has_value());
		}

		static QString getStringValue(const QSpinBox* widget) { return QString::number(widget->value()); }
		static void setStringValue(QSpinBox* widget, const QString& value) { widget->setValue(value.toInt()); }
		static void makeNullableString(QSpinBox* widget, const QString& globalValue)
		{
			widget->setProperty(NULLABLE_PROPERTY, QVariant(true));
			widget->setProperty(GLOBAL_VALUE_PROPERTY, QVariant(globalValue));
		}
		static std::optional<QString> getNullableStringValue(const QSpinBox* widget)
		{
			if (widget->property(IS_NULL_PROPERTY).toBool())
				return std::nullopt;

			return getStringValue(widget);
		}
		static void setNullableStringValue(QSpinBox* widget, std::optional<QString> value)
		{
			widget->setProperty(IS_NULL_PROPERTY, QVariant(!value.has_value()));
			setStringValue(widget, value.has_value() ? value.value() : widget->property(GLOBAL_VALUE_PROPERTY).toString());
			updateNullState(widget, !value.has_value());
		}

		template <typename F>
		static void connectValueChanged(QSpinBox* widget, F func)
		{
			if (!isNullable(widget))
			{
				widget->connect(widget, QOverload<int>::of(&QSpinBox::valueChanged), func);
			}
			else
			{
				widget->setContextMenuPolicy(Qt::CustomContextMenu);
				widget->connect(widget, &QSpinBox::customContextMenuRequested, widget, [widget, func](const QPoint& pt) {
					QMenu menu(widget);
					widget->connect(menu.addAction(qApp->translate("SettingWidgetBinder", "Reset")), &QAction::triggered, widget,
						[widget, func = std::move(func)]() {
							const bool old = widget->blockSignals(true);
							setNullableIntValue(widget, std::nullopt);
							widget->blockSignals(old);
							updateNullState(widget, true);
							func();
						});
					menu.exec(widget->mapToGlobal(pt));
				});
				widget->connect(widget, &QSpinBox::valueChanged, widget, [widget, func = std::move(func)]() {
					if (widget->property(IS_NULL_PROPERTY).toBool())
					{
						widget->setProperty(IS_NULL_PROPERTY, QVariant(false));
						updateNullState(widget, false);
					}
					func();
				});
			}
		}
	};

	template <>
	struct SettingAccessor<QDoubleSpinBox>
	{
		static bool isNullable(const QDoubleSpinBox* widget) { return widget->property(NULLABLE_PROPERTY).toBool(); }

		static void updateNullState(QDoubleSpinBox* widget, bool isNull)
		{
			widget->setPrefix(isNull ? qApp->translate("SettingWidgetBinder", "Default: ") : QString());
		}

		static bool getBoolValue(const QDoubleSpinBox* widget) { return widget->value() > 0.0; }
		static void setBoolValue(QDoubleSpinBox* widget, bool value) { widget->setValue(value ? 1.0 : 0.0); }
		static void makeNullableBool(QDoubleSpinBox* widget, bool globalSetting)
		{
			widget->setProperty(NULLABLE_PROPERTY, QVariant(true));
			widget->setProperty(GLOBAL_VALUE_PROPERTY, QVariant(globalSetting));
		}
		static std::optional<bool> getNullableBoolValue(const QDoubleSpinBox* widget)
		{
			if (widget->property(IS_NULL_PROPERTY).toBool())
				return std::nullopt;

			return getBoolValue(widget);
		}
		static void setNullableBoolValue(QDoubleSpinBox* widget, std::optional<bool> value)
		{
			widget->setProperty(IS_NULL_PROPERTY, QVariant(!value.has_value()));
			setBoolValue(widget, value.has_value() ? value.value() : widget->property(GLOBAL_VALUE_PROPERTY).toBool());
			updateNullState(widget, !value.has_value());
		}

		static int getIntValue(const QDoubleSpinBox* widget) { return static_cast<int>(widget->value()); }
		static void setIntValue(QDoubleSpinBox* widget, int value) { widget->setValue(static_cast<double>(value)); }
		static void makeNullableInt(QDoubleSpinBox* widget, int globalValue)
		{
			widget->setProperty(NULLABLE_PROPERTY, QVariant(true));
			widget->setProperty(GLOBAL_VALUE_PROPERTY, QVariant(globalValue));
		}
		static std::optional<int> getNullableIntValue(const QDoubleSpinBox* widget)
		{
			if (widget->property(IS_NULL_PROPERTY).toBool())
				return std::nullopt;

			return getIntValue(widget);
		}
		static void setNullableIntValue(QDoubleSpinBox* widget, std::optional<int> value)
		{
			widget->setProperty(IS_NULL_PROPERTY, QVariant(!value.has_value()));
			setIntValue(widget, value.has_value() ? value.value() : widget->property(GLOBAL_VALUE_PROPERTY).toInt());
			updateNullState(widget, !value.has_value());
		}

		static float getFloatValue(const QDoubleSpinBox* widget) { return static_cast<float>(widget->value()); }
		static void setFloatValue(QDoubleSpinBox* widget, float value) { widget->setValue(static_cast<double>(value)); }
		static void makeNullableFloat(QDoubleSpinBox* widget, float globalValue)
		{
			widget->setProperty(NULLABLE_PROPERTY, QVariant(true));
			widget->setProperty(GLOBAL_VALUE_PROPERTY, QVariant(globalValue));
		}
		static std::optional<float> getNullableFloatValue(const QDoubleSpinBox* widget)
		{
			if (widget->property(IS_NULL_PROPERTY).toBool())
				return std::nullopt;

			return getFloatValue(widget);
		}
		static void setNullableFloatValue(QDoubleSpinBox* widget, std::optional<float> value)
		{
			widget->setProperty(IS_NULL_PROPERTY, QVariant(!value.has_value()));
			setFloatValue(widget, value.has_value() ? value.value() : widget->property(GLOBAL_VALUE_PROPERTY).toFloat());
			updateNullState(widget, !value.has_value());
		}

		static QString getStringValue(const QDoubleSpinBox* widget) { return QString::number(widget->value()); }
		static void setStringValue(QDoubleSpinBox* widget, const QString& value) { widget->setValue(value.toDouble()); }
		static void makeNullableString(QDoubleSpinBox* widget, const QString& globalValue)
		{
			widget->setProperty(NULLABLE_PROPERTY, QVariant(true));
			widget->setProperty(GLOBAL_VALUE_PROPERTY, QVariant(globalValue));
		}
		static std::optional<QString> getNullableStringValue(const QDoubleSpinBox* widget)
		{
			if (widget->property(IS_NULL_PROPERTY).toBool())
				return std::nullopt;

			return getStringValue(widget);
		}
		static void setNullableStringValue(QDoubleSpinBox* widget, std::optional<QString> value)
		{
			widget->setProperty(IS_NULL_PROPERTY, QVariant(!value.has_value()));
			setStringValue(widget, value.has_value() ? value.value() : widget->property(GLOBAL_VALUE_PROPERTY).toString());
			updateNullState(widget, !value.has_value());
		}

		template <typename F>
		static void connectValueChanged(QDoubleSpinBox* widget, F func)
		{
			if (!isNullable(widget))
			{
				widget->connect(widget, QOverload<double>::of(&QDoubleSpinBox::valueChanged), func);
			}
			else
			{
				widget->setContextMenuPolicy(Qt::CustomContextMenu);
				widget->connect(widget, &QDoubleSpinBox::customContextMenuRequested, widget, [widget, func](const QPoint& pt) {
					QMenu menu(widget);
					widget->connect(menu.addAction(qApp->translate("SettingWidgetBinder", "Reset")), &QAction::triggered, widget,
						[widget, func = std::move(func)]() {
							const bool old = widget->blockSignals(true);
							setNullableFloatValue(widget, std::nullopt);
							widget->blockSignals(old);
							updateNullState(widget, true);
							func();
						});
					menu.exec(widget->mapToGlobal(pt));
				});
				widget->connect(widget, QOverload<double>::of(&QDoubleSpinBox::valueChanged), widget, [widget, func = std::move(func)]() {
					if (widget->property(IS_NULL_PROPERTY).toBool())
					{
						widget->setProperty(IS_NULL_PROPERTY, QVariant(false));
						updateNullState(widget, false);
					}
					func();
				});
			}
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
		static void setNullableStringValue(QAction* widget, std::optional<QString> value)
		{
			setStringValue(widget, value.value_or(QString()));
		}

		template <typename F>
		static void connectValueChanged(QAction* widget, F func)
		{
			widget->connect(widget, &QAction::toggled, func);
		}
	};

	/// Binds a widget's value to a setting, updating it when the value changes.

	template <typename WidgetType>
	static inline void BindWidgetToBoolSetting(
		SettingsInterface* sif, WidgetType* widget, std::string section, std::string key, bool default_value)
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

				QtHost::SaveGameSettings(sif, true);
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
			Accessor::setBoolValue(widget, value);

			Accessor::connectValueChanged(widget, [widget, section = std::move(section), key = std::move(key)]() {
				const bool new_value = Accessor::getBoolValue(widget);
				Host::SetBaseBoolSettingValue(section.c_str(), key.c_str(), new_value);
				Host::CommitBaseSettingChanges();
				g_emu_thread->applySettings();
			});
		}
	}

	template <typename WidgetType>
	static inline void BindWidgetToIntSetting(
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

				QtHost::SaveGameSettings(sif, true);
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
			Accessor::setIntValue(widget, static_cast<int>(value));

			Accessor::connectValueChanged(widget, [widget, section = std::move(section), key = std::move(key), option_offset]() {
				const int new_value = Accessor::getIntValue(widget);
				Host::SetBaseIntSettingValue(section.c_str(), key.c_str(), new_value + option_offset);
				Host::CommitBaseSettingChanges();
				g_emu_thread->applySettings();
			});
		}
	}

	template <typename WidgetType>
	static inline void BindWidgetAndLabelToIntSetting(SettingsInterface* sif, WidgetType* widget, QLabel* label,
		const QString& label_suffix, std::string section, std::string key,
		int default_value, int option_offset = 0)
	{
		using Accessor = SettingAccessor<WidgetType>;

		const s32 global_value =
			Host::GetBaseIntSettingValue(section.c_str(), key.c_str(), static_cast<s32>(default_value)) - option_offset;

		if (sif)
		{
			QFont orig_font(label->font());
			QFont bold_font(orig_font);
			bold_font.setBold(true);

			Accessor::makeNullableInt(widget, global_value);

			int sif_value;
			if (sif->GetIntValue(section.c_str(), key.c_str(), &sif_value))
			{
				Accessor::setNullableIntValue(widget, sif_value - option_offset);
				if (label)
				{
					label->setText(QStringLiteral("%1%2").arg(sif_value).arg(label_suffix));
					label->setFont(bold_font);
				}
			}
			else
			{
				Accessor::setNullableIntValue(widget, std::nullopt);
				if (label)
					label->setText(QStringLiteral("%1%2").arg(global_value).arg(label_suffix));
			}

			Accessor::connectValueChanged(widget, [sif, widget, label, label_suffix, section = std::move(section),
													  key = std::move(key), option_offset, global_value,
													  bold_font = std::move(bold_font), orig_font = std::move(orig_font)]() {
				if (std::optional<int> new_value = Accessor::getNullableIntValue(widget); new_value.has_value())
				{
					sif->SetIntValue(section.c_str(), key.c_str(), new_value.value() + option_offset);
					if (label)
					{
						label->setFont(bold_font);
						label->setText(QStringLiteral("%1%2").arg(new_value.value()).arg(label_suffix));
					}
				}
				else
				{
					sif->DeleteValue(section.c_str(), key.c_str());
					if (label)
					{
						label->setFont(orig_font);
						label->setText(QStringLiteral("%1%2").arg(global_value).arg(label_suffix));
					}
				}

				QtHost::SaveGameSettings(sif, true);
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
			Accessor::setIntValue(widget, static_cast<int>(global_value));

			if (label)
				label->setText(QStringLiteral("%1%2").arg(global_value).arg(label_suffix));

			Accessor::connectValueChanged(
				widget, [widget, label, label_suffix, section = std::move(section), key = std::move(key), option_offset]() {
					const int new_value = Accessor::getIntValue(widget);
					Host::SetBaseIntSettingValue(section.c_str(), key.c_str(), new_value + option_offset);
					Host::CommitBaseSettingChanges();
					g_emu_thread->applySettings();

					if (label)
						label->setText(QStringLiteral("%1%2").arg(new_value).arg(label_suffix));
				});
		}
	}

	template <typename WidgetType>
	static inline void BindWidgetToFloatSetting(
		SettingsInterface* sif, WidgetType* widget, std::string section, std::string key, float default_value)
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

				QtHost::SaveGameSettings(sif, true);
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
			Accessor::setFloatValue(widget, value);

			Accessor::connectValueChanged(widget, [widget, section = std::move(section), key = std::move(key)]() {
				const float new_value = Accessor::getFloatValue(widget);
				Host::SetBaseFloatSettingValue(section.c_str(), key.c_str(), new_value);
				Host::CommitBaseSettingChanges();
				g_emu_thread->applySettings();
			});
		}
	}

	template <typename WidgetType>
	static inline void BindWidgetToNormalizedSetting(
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

				QtHost::SaveGameSettings(sif, true);
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
			Accessor::setIntValue(widget, static_cast<int>(value * range));

			Accessor::connectValueChanged(widget, [widget, section = std::move(section), key = std::move(key), range]() {
				const float new_value = (static_cast<float>(Accessor::getIntValue(widget)) / range);
				Host::SetBaseFloatSettingValue(section.c_str(), key.c_str(), new_value);
				Host::CommitBaseSettingChanges();
				g_emu_thread->applySettings();
			});
		}
	}

	template <typename WidgetType>
	static inline void BindWidgetToStringSetting(
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

				QtHost::SaveGameSettings(sif, true);
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
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

	template <typename WidgetType, typename DataType>
	static inline void BindWidgetToEnumSetting(SettingsInterface* sif, WidgetType* widget, std::string section, std::string key,
		std::optional<DataType> (*from_string_function)(const char* str), const char* (*to_string_function)(DataType value),
		DataType default_value)
	{
		using Accessor = SettingAccessor<WidgetType>;
		using UnderlyingType = std::underlying_type_t<DataType>;

		const std::string value(Host::GetBaseStringSettingValue(section.c_str(), key.c_str(), to_string_function(default_value)));
		const std::optional<DataType> typed_value = from_string_function(value.c_str());

		if (sif)
		{
			Accessor::makeNullableInt(
				widget, typed_value.has_value() ? static_cast<int>(static_cast<UnderlyingType>(typed_value.value())) : 0);

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

				QtHost::SaveGameSettings(sif, true);
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
				Host::SetBaseStringSettingValue(section.c_str(), key.c_str(), string_value);
				Host::CommitBaseSettingChanges();
				g_emu_thread->applySettings();
			});
		}
	}

	template <typename WidgetType, typename DataType>
	static inline void BindWidgetToEnumSetting(
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

				QtHost::SaveGameSettings(sif, true);
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
			Accessor::setIntValue(widget, static_cast<int>(enum_index));

			Accessor::connectValueChanged(widget, [widget, section = std::move(section), key = std::move(key), enum_names]() {
				const UnderlyingType value = static_cast<UnderlyingType>(Accessor::getIntValue(widget));
				Host::SetBaseStringSettingValue(section.c_str(), key.c_str(), enum_names[value]);
				Host::CommitBaseSettingChanges();
				g_emu_thread->applySettings();
			});
		}
	}

	template <typename WidgetType>
	static inline void BindWidgetToEnumSetting(SettingsInterface* sif, WidgetType* widget, std::string section, std::string key,
		const char** enum_names, const char** enum_values, const char* default_value, const char* translation_ctx = nullptr)
	{
		using Accessor = SettingAccessor<WidgetType>;

		const std::string value = Host::GetBaseStringSettingValue(section.c_str(), key.c_str(), default_value);

		for (int i = 0; enum_names[i] != nullptr; i++)
		{
			widget->addItem(translation_ctx ? qApp->translate(translation_ctx, enum_names[i]) : QString::fromUtf8(enum_names[i]));
		}

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

				QtHost::SaveGameSettings(sif, true);
				g_emu_thread->reloadGameSettings();
			});
		}
		else
		{
			if (enum_index >= 0)
				Accessor::setIntValue(widget, enum_index);

			Accessor::connectValueChanged(widget, [widget, section = std::move(section), key = std::move(key), enum_values]() {
				const int value = Accessor::getIntValue(widget);
				Host::SetBaseStringSettingValue(section.c_str(), key.c_str(), enum_values[value]);
				Host::CommitBaseSettingChanges();
				g_emu_thread->applySettings();
			});
		}
	}

	static inline void BindWidgetToFolderSetting(SettingsInterface* sif, QLineEdit* widget, QAbstractButton* browse_button,
		QAbstractButton* open_button, QAbstractButton* reset_button, std::string section, std::string key, std::string default_value,
		bool use_relative = true)
	{
		using Accessor = SettingAccessor<QLineEdit>;

		std::string current_path(Host::GetBaseStringSettingValue(section.c_str(), key.c_str(), default_value.c_str()));
		if (current_path.empty())
			current_path = default_value;
		else if (use_relative && !Path::IsAbsolute(current_path))
			current_path = Path::Canonicalize(Path::Combine(EmuFolders::DataRoot, current_path));

		const QString value(QString::fromStdString(current_path));
		Accessor::setStringValue(widget, value);

		// if we're doing per-game settings, disable the widget, we only allow folder changes in the base config
		if (sif)
		{
			widget->setEnabled(false);
			if (browse_button)
				browse_button->setEnabled(false);
			if (reset_button)
				reset_button->setEnabled(false);
			return;
		}


		auto value_changed = [widget, section = std::move(section), key = std::move(key), default_value, use_relative]() {
			const std::string new_value(widget->text().toStdString());
			if (!new_value.empty())
			{
				if (FileSystem::DirectoryExists(new_value.c_str()) ||
					QMessageBox::question(QtUtils::GetRootWidget(widget), qApp->translate("SettingWidgetBinder", "Confirm Folder"),
						qApp->translate("SettingWidgetBinder",
								"The chosen directory does not currently exist:\n\n%1\n\nDo you want to create this directory?")
							.arg(QString::fromStdString(new_value)),
						QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
				{
					if (use_relative)
					{
						const std::string relative_path(Path::MakeRelative(new_value, EmuFolders::DataRoot));
						Host::SetBaseStringSettingValue(section.c_str(), key.c_str(), relative_path.c_str());
					}
					else
					{
						Host::SetBaseStringSettingValue(section.c_str(), key.c_str(), new_value.c_str());
					}

					Host::CommitBaseSettingChanges();
					g_emu_thread->updateEmuFolders();
					return;
				}
			}
			else
			{
				QMessageBox::critical(QtUtils::GetRootWidget(widget), qApp->translate("SettingWidgetBinder", "Error"),
					qApp->translate("SettingWidgetBinder", "Folder path cannot be empty."));
			}

			// reset to old value
			std::string current_path(Host::GetBaseStringSettingValue(section.c_str(), key.c_str(), default_value.c_str()));
			if (current_path.empty())
				current_path = default_value;
			else if (use_relative && !Path::IsAbsolute(current_path))
				current_path = Path::Canonicalize(Path::Combine(EmuFolders::DataRoot, current_path));

			widget->setText(QString::fromStdString(current_path));
		};

		if (browse_button)
		{
			QObject::connect(browse_button, &QAbstractButton::clicked, browse_button, [widget, key, value_changed]() {
				const QString path(QDir::toNativeSeparators(QFileDialog::getExistingDirectory(QtUtils::GetRootWidget(widget),
					//It seems that the latter half should show the types of folders that can be selected within Settings -> Folders, but right now it's broken. It would be best for localization purposes to duplicate this into multiple lines, each per type of folder.
					qApp->translate("SettingWidgetBinder", "Select folder for %1").arg(QString::fromStdString(key)))));
				if (path.isEmpty())
					return;

				widget->setText(path);
				value_changed();
			});
		}
		if (open_button)
		{
			QObject::connect(open_button, &QAbstractButton::clicked, open_button, [widget]() {
				QString path(Accessor::getStringValue(widget));
				if (!path.isEmpty())
					QtUtils::OpenURL(QtUtils::GetRootWidget(widget), QUrl::fromLocalFile(path));
			});
		}
		if (reset_button)
		{
			QObject::connect(
				reset_button, &QAbstractButton::clicked, reset_button, [widget, default_value = std::move(default_value), value_changed]() {
					widget->setText(QString::fromStdString(default_value));
					value_changed();
				});
		}

		widget->connect(widget, &QLineEdit::editingFinished, widget, std::move(value_changed));
	}
} // namespace SettingWidgetBinder
