// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "ui_ControllerSettingsWindow.h"

#include "pcsx2/Input/InputManager.h"
#include "pcsx2/USB/USB.h"

#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtWidgets/QWidget>
#include <array>
#include <string>

class ControllerGlobalSettingsWidget;
class ControllerBindingWidget;
class HotkeySettingsWidget;
class USBDeviceWidget;

class SettingsInterface;

class ControllerSettingsWindow final : public QWidget
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
		MAX_PORTS = 8
	};

	ControllerSettingsWindow();
	~ControllerSettingsWindow();

	__fi HotkeySettingsWidget* getHotkeySettingsWidget() const { return m_hotkey_settings; }

	__fi const QList<QPair<QString, QString>>& getDeviceList() const { return m_device_list; }
	__fi const QStringList& getVibrationMotors() const { return m_vibration_motors; }

	__fi bool isEditingGlobalSettings() const { return m_profile_name.isEmpty(); }
	__fi bool isEditingProfile() const { return !m_profile_name.isEmpty(); }
	__fi SettingsInterface* getProfileSettingsInterface() { return m_profile_interface.get(); }

	void updateListDescription(u32 global_slot, ControllerBindingWidget* widget);
	void updateListDescription(u32 port, USBDeviceWidget* widget);

	// Helper functions for updating setting values globally or in the profile.
	bool getBoolValue(const char* section, const char* key, bool default_value) const;
	s32 getIntValue(const char* section, const char* key, s32 default_value) const;
	std::string getStringValue(const char* section, const char* key, const char* default_value) const;
	void setBoolValue(const char* section, const char* key, bool value);
	void setIntValue(const char* section, const char* key, s32 value);
	void setStringValue(const char* section, const char* key, const char* value);
	void clearSettingValue(const char* section, const char* key);

Q_SIGNALS:
	void inputProfileSwitched();

public Q_SLOTS:
	void setCategory(Category category);

private Q_SLOTS:
	void onCategoryCurrentRowChanged(int row);
	void onCurrentProfileChanged(int index);
	void onNewProfileClicked();
	void onLoadProfileClicked();
	void onDeleteProfileClicked();
	void onMappingSettingsClicked();
	void onRestoreDefaultsClicked();

	void onInputDevicesEnumerated(const QList<QPair<QString, QString>>& devices);
	void onInputDeviceConnected(const QString& identifier, const QString& device_name);
	void onInputDeviceDisconnected(const QString& identifier);
	void onVibrationMotorsEnumerated(const QList<InputBindingKey>& motors);

	void createWidgets();

private:
	void refreshProfileList();
	void switchProfile(const QString& name);

	Ui::ControllerSettingsWindow m_ui;

	ControllerGlobalSettingsWidget* m_global_settings = nullptr;
	std::array<ControllerBindingWidget*, MAX_PORTS> m_port_bindings{};
	std::array<USBDeviceWidget*, USB::NUM_PORTS> m_usb_bindings{};
	HotkeySettingsWidget* m_hotkey_settings = nullptr;

	QList<QPair<QString, QString>> m_device_list;
	QStringList m_vibration_motors;

	QString m_profile_name;
	std::unique_ptr<SettingsInterface> m_profile_interface;
};
