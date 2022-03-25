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
#include "ui_ControllerSettingsDialog.h"
#include "Frontend/InputManager.h"
#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtWidgets/QDialog>
#include <array>

class ControllerGlobalSettingsWidget;
class ControllerBindingWidget;
class HotkeySettingsWidget;

class ControllerSettingsDialog final : public QDialog
{
	Q_OBJECT

public:
	enum class Category
	{
		GlobalSettings,
		FirstControllerSettings,
		HotkeySettings,
		Count
	};

	enum : u32
	{
		MAX_PORTS = 2
	};

	ControllerSettingsDialog(QWidget* parent = nullptr);
	~ControllerSettingsDialog();

	HotkeySettingsWidget* getHotkeySettingsWidget() const { return m_hotkey_settings; }

	__fi const QList<QPair<QString, QString>>& getDeviceList() const { return m_device_list; }
	__fi const QStringList& getVibrationMotors() const { return m_vibration_motors; }

public Q_SLOTS:
	void setCategory(Category category);

private Q_SLOTS:
	void onCategoryCurrentRowChanged(int row);

	void onInputDevicesEnumerated(const QList<QPair<QString, QString>>& devices);
	void onInputDeviceConnected(const QString& identifier, const QString& device_name);
	void onInputDeviceDisconnected(const QString& identifier);
	void onVibrationMotorsEnumerated(const QList<InputBindingKey>& motors);

private:
	Ui::ControllerSettingsDialog m_ui;

	ControllerGlobalSettingsWidget* m_global_settings = nullptr;
	std::array<ControllerBindingWidget*, MAX_PORTS> m_port_bindings{};
	HotkeySettingsWidget* m_hotkey_settings = nullptr;

	QList<QPair<QString, QString>> m_device_list;
	QStringList m_vibration_motors;
};
