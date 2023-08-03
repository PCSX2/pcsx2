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

#include "PrecompiledHeader.h"

#include "QtHost.h"
#include "Settings/ControllerSettingsDialog.h"
#include "Settings/ControllerGlobalSettingsWidget.h"
#include "Settings/ControllerBindingWidgets.h"
#include "Settings/HotkeySettingsWidget.h"

#include "pcsx2/INISettingsInterface.h"
#include "pcsx2/SIO/Pad/Pad.h"
#include "pcsx2/SIO/Sio.h"
#include "pcsx2/VMManager.h"

#include "common/Assertions.h"
#include "common/FileSystem.h"

#include <array>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QTextEdit>

static constexpr const std::array<char, 4> s_mtap_slot_names = {{'A', 'B', 'C', 'D'}};

ControllerSettingsDialog::ControllerSettingsDialog(QWidget* parent /* = nullptr */)
	: QDialog(parent)
{
	m_ui.setupUi(this);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	refreshProfileList();
	createWidgets();

	m_ui.settingsCategory->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	connect(m_ui.settingsCategory, &QListWidget::currentRowChanged, this, &ControllerSettingsDialog::onCategoryCurrentRowChanged);
	connect(m_ui.currentProfile, &QComboBox::currentIndexChanged, this, &ControllerSettingsDialog::onCurrentProfileChanged);
	connect(m_ui.buttonBox, &QDialogButtonBox::rejected, this, &ControllerSettingsDialog::close);
	connect(m_ui.newProfile, &QPushButton::clicked, this, &ControllerSettingsDialog::onNewProfileClicked);
	connect(m_ui.loadProfile, &QPushButton::clicked, this, &ControllerSettingsDialog::onLoadProfileClicked);
	connect(m_ui.deleteProfile, &QPushButton::clicked, this, &ControllerSettingsDialog::onDeleteProfileClicked);
	connect(m_ui.restoreDefaults, &QPushButton::clicked, this, &ControllerSettingsDialog::onRestoreDefaultsClicked);

	connect(g_emu_thread, &EmuThread::onInputDevicesEnumerated, this, &ControllerSettingsDialog::onInputDevicesEnumerated);
	connect(g_emu_thread, &EmuThread::onInputDeviceConnected, this, &ControllerSettingsDialog::onInputDeviceConnected);
	connect(g_emu_thread, &EmuThread::onInputDeviceDisconnected, this, &ControllerSettingsDialog::onInputDeviceDisconnected);
	connect(g_emu_thread, &EmuThread::onVibrationMotorsEnumerated, this, &ControllerSettingsDialog::onVibrationMotorsEnumerated);

	// trigger a device enumeration to populate the device list
	g_emu_thread->enumerateInputDevices();
	g_emu_thread->enumerateVibrationMotors();
}

ControllerSettingsDialog::~ControllerSettingsDialog() = default;

void ControllerSettingsDialog::setCategory(Category category)
{
	switch (category)
	{
		case Category::GlobalSettings:
			m_ui.settingsCategory->setCurrentRow(0);
			break;

			// TODO: These will need to take multitap into consideration in the future.
		case Category::FirstControllerSettings:
			m_ui.settingsCategory->setCurrentRow(1);
			break;

		case Category::HotkeySettings:
			m_ui.settingsCategory->setCurrentRow(5);
			break;

		default:
			break;
	}
}

void ControllerSettingsDialog::onCategoryCurrentRowChanged(int row)
{
	m_ui.settingsContainer->setCurrentIndex(row);
}

void ControllerSettingsDialog::onCurrentProfileChanged(int index)
{
	switchProfile((index == 0) ? 0 : m_ui.currentProfile->itemText(index));
}

void ControllerSettingsDialog::onNewProfileClicked()
{
	const QString profile_name(QInputDialog::getText(this, tr("Create Input Profile"), 
	tr("Custom input profiles are used to override the Shared input profile for specific games.\n"
	"To apply a custom input profile to a game, go to its Game Properties, then change the 'Input Profile' on the Summary tab.\n\n"
	"Enter the name for the new input profile:")));
	if (profile_name.isEmpty())
		return;

	std::string profile_path(VMManager::GetInputProfilePath(profile_name.toStdString()));
	if (FileSystem::FileExists(profile_path.c_str()))
	{
		QMessageBox::critical(this, tr("Error"), tr("A profile with the name '%1' already exists.").arg(profile_name));
		return;
	}

	const int res = QMessageBox::question(this, tr("Create Input Profile"),
		tr("Do you want to copy all bindings from the currently-selected profile to the new profile? Selecting No will create a completely "
		   "empty profile."),
		QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
	if (res == QMessageBox::Cancel)
		return;

	INISettingsInterface temp_si(std::move(profile_path));
	if (res == QMessageBox::Yes)
	{
		// copy from global or the current profile
		if (!m_profile_interface)
		{
			// from global
			auto lock = Host::GetSettingsLock();
			Pad::CopyConfiguration(&temp_si, *Host::Internal::GetBaseSettingsLayer(), true, true, false);
			USB::CopyConfiguration(&temp_si, *Host::Internal::GetBaseSettingsLayer(), true, true);
		}
		else
		{
			// from profile
			const bool copy_hotkey_bindings = m_profile_interface->GetBoolValue("Pad", "UseProfileHotkeyBindings", false);
			temp_si.SetBoolValue("Pad", "UseProfileHotkeyBindings", copy_hotkey_bindings);
			Pad::CopyConfiguration(&temp_si, *m_profile_interface, true, true, copy_hotkey_bindings);
			USB::CopyConfiguration(&temp_si, *m_profile_interface, true, true);
		}
	}

	if (!temp_si.Save())
	{
		QMessageBox::critical(
			this, tr("Error"), tr("Failed to save the new profile to '%1'.").arg(QString::fromStdString(temp_si.GetFileName())));
		return;
	}

	refreshProfileList();
	switchProfile(profile_name);
}

void ControllerSettingsDialog::onLoadProfileClicked()
{
	if (QMessageBox::question(this, tr("Load Input Profile"),
			tr("Are you sure you want to load the input profile named '%1'?\n\n"
			   "All current global bindings will be removed, and the profile bindings loaded.\n\n"
			   "You cannot undo this action.")
				.arg(m_profile_name)) != QMessageBox::Yes)
	{
		return;
	}

	{
		auto lock = Host::GetSettingsLock();
		Pad::CopyConfiguration(Host::Internal::GetBaseSettingsLayer(), *m_profile_interface, true, true, false);
		USB::CopyConfiguration(Host::Internal::GetBaseSettingsLayer(), *m_profile_interface, true, true);
	}
	Host::CommitBaseSettingChanges();

	g_emu_thread->applySettings();

	// make it visible
	switchProfile({});
}

void ControllerSettingsDialog::onDeleteProfileClicked()
{
	if (QMessageBox::question(this, tr("Delete Input Profile"),
			tr("Are you sure you want to delete the input profile named '%1'?\n\n"
			   "You cannot undo this action.")
				.arg(m_profile_name)) != QMessageBox::Yes)
	{
		return;
	}

	std::string profile_path(VMManager::GetInputProfilePath(m_profile_name.toStdString()));
	if (!FileSystem::DeleteFilePath(profile_path.c_str()))
	{
		QMessageBox::critical(this, tr("Error"), tr("Failed to delete '%1'.").arg(QString::fromStdString(profile_path)));
		return;
	}

	// switch back to global
	refreshProfileList();
	switchProfile({});
}

void ControllerSettingsDialog::onRestoreDefaultsClicked()
{
	if (QMessageBox::question(this, tr("Restore Defaults"),
			tr("Are you sure you want to restore the default controller configuration?\n\n"
			   "All shared bindings and configuration will be lost, but your input profiles will remain.\n\n"
			   "You cannot undo this action.")) != QMessageBox::Yes)
	{
		return;
	}

	// actually restore it
	{
		auto lock = Host::GetSettingsLock();
		VMManager::SetDefaultSettings(*Host::Internal::GetBaseSettingsLayer(), false, false, true, true, false);
	}
	Host::CommitBaseSettingChanges();

	g_emu_thread->applySettings();

	// reload all settings
	switchProfile({});
}

void ControllerSettingsDialog::onInputDevicesEnumerated(const QList<QPair<QString, QString>>& devices)
{
	m_device_list = devices;
	for (const QPair<QString, QString>& device : devices)
		m_global_settings->addDeviceToList(device.first, device.second);
}

void ControllerSettingsDialog::onInputDeviceConnected(const QString& identifier, const QString& device_name)
{
	m_device_list.emplace_back(identifier, device_name);
	m_global_settings->addDeviceToList(identifier, device_name);
	g_emu_thread->enumerateVibrationMotors();
}

void ControllerSettingsDialog::onInputDeviceDisconnected(const QString& identifier)
{
	for (auto iter = m_device_list.begin(); iter != m_device_list.end(); ++iter)
	{
		if (iter->first == identifier)
		{
			m_device_list.erase(iter);
			break;
		}
	}

	m_global_settings->removeDeviceFromList(identifier);
	g_emu_thread->enumerateVibrationMotors();
}

void ControllerSettingsDialog::onVibrationMotorsEnumerated(const QList<InputBindingKey>& motors)
{
	m_vibration_motors.clear();
	m_vibration_motors.reserve(motors.size());

	for (const InputBindingKey key : motors)
	{
		const std::string key_str(InputManager::ConvertInputBindingKeyToString(InputBindingInfo::Type::Motor, key));
		if (!key_str.empty())
			m_vibration_motors.push_back(QString::fromStdString(key_str));
	}
}

bool ControllerSettingsDialog::getBoolValue(const char* section, const char* key, bool default_value) const
{
	if (m_profile_interface)
		return m_profile_interface->GetBoolValue(section, key, default_value);
	else
		return Host::GetBaseBoolSettingValue(section, key, default_value);
}

s32 ControllerSettingsDialog::getIntValue(const char* section, const char* key, s32 default_value) const
{
	if (m_profile_interface)
		return m_profile_interface->GetIntValue(section, key, default_value);
	else
		return Host::GetBaseIntSettingValue(section, key, default_value);
}

std::string ControllerSettingsDialog::getStringValue(const char* section, const char* key, const char* default_value) const
{
	std::string value;
	if (m_profile_interface)
		value = m_profile_interface->GetStringValue(section, key, default_value);
	else
		value = Host::GetBaseStringSettingValue(section, key, default_value);
	return value;
}

void ControllerSettingsDialog::setBoolValue(const char* section, const char* key, bool value)
{
	if (m_profile_interface)
	{
		m_profile_interface->SetBoolValue(section, key, value);
		m_profile_interface->Save();
		g_emu_thread->reloadGameSettings();
	}
	else
	{
		Host::SetBaseBoolSettingValue(section, key, value);
		Host::CommitBaseSettingChanges();
		g_emu_thread->applySettings();
	}
}

void ControllerSettingsDialog::setIntValue(const char* section, const char* key, s32 value)
{
	if (m_profile_interface)
	{
		m_profile_interface->SetIntValue(section, key, value);
		m_profile_interface->Save();
		g_emu_thread->reloadGameSettings();
	}
	else
	{
		Host::SetBaseIntSettingValue(section, key, value);
		Host::CommitBaseSettingChanges();
		g_emu_thread->applySettings();
	}
}

void ControllerSettingsDialog::setStringValue(const char* section, const char* key, const char* value)
{
	if (m_profile_interface)
	{
		m_profile_interface->SetStringValue(section, key, value);
		m_profile_interface->Save();
		g_emu_thread->reloadGameSettings();
	}
	else
	{
		Host::SetBaseStringSettingValue(section, key, value);
		Host::CommitBaseSettingChanges();
		g_emu_thread->applySettings();
	}
}

void ControllerSettingsDialog::clearSettingValue(const char* section, const char* key)
{
	if (m_profile_interface)
	{
		m_profile_interface->DeleteValue(section, key);
		m_profile_interface->Save();
		g_emu_thread->reloadGameSettings();
	}
	else
	{
		Host::RemoveBaseSettingValue(section, key);
		Host::CommitBaseSettingChanges();
		g_emu_thread->applySettings();
	}
}

void ControllerSettingsDialog::createWidgets()
{
	QSignalBlocker sb(m_ui.settingsContainer);
	QSignalBlocker sb2(m_ui.settingsCategory);

	while (m_ui.settingsContainer->count() > 0)
	{
		QWidget* widget = m_ui.settingsContainer->widget(m_ui.settingsContainer->count() - 1);
		m_ui.settingsContainer->removeWidget(widget);
		widget->deleteLater();
	}

	m_ui.settingsCategory->clear();

	m_global_settings = nullptr;
	m_hotkey_settings = nullptr;

	{
		// global settings
		QListWidgetItem* item = new QListWidgetItem();
		item->setText(tr("Global Settings"));
		item->setIcon(QIcon::fromTheme("settings-3-line"));
		m_ui.settingsCategory->addItem(item);
		m_ui.settingsCategory->setCurrentRow(0);
		m_global_settings = new ControllerGlobalSettingsWidget(m_ui.settingsContainer, this);
		m_ui.settingsContainer->addWidget(m_global_settings);
		connect(m_global_settings, &ControllerGlobalSettingsWidget::bindingSetupChanged, this, &ControllerSettingsDialog::createWidgets);
		for (const QPair<QString, QString>& dev : m_device_list)
			m_global_settings->addDeviceToList(dev.first, dev.second);
	}

	// load mtap settings
	const std::array<bool, 2> mtap_enabled = {{getBoolValue("Pad", "MultitapPort1", false), getBoolValue("Pad", "MultitapPort2", false)}};

	// we reorder things a little to make it look less silly for mtap
	static constexpr const std::array<u32, MAX_PORTS> mtap_port_order = {{0, 2, 3, 4, 1, 5, 6, 7}};

	// create the ports
	for (u32 global_slot : mtap_port_order)
	{
		const bool is_mtap_port = sioPadIsMultitapSlot(global_slot);
		const auto [port, slot] = sioConvertPadToPortAndSlot(global_slot);
		if (is_mtap_port && !mtap_enabled[port])
			continue;

		m_port_bindings[global_slot] = new ControllerBindingWidget(m_ui.settingsContainer, this, global_slot);
		m_ui.settingsContainer->addWidget(m_port_bindings[global_slot]);

		const Pad::ControllerInfo* ci = Pad::GetControllerInfo(m_port_bindings[global_slot]->getControllerType());
		const QString display_name(QString::fromUtf8(ci ? ci->GetLocalizedName() : "Unknown"));

		QListWidgetItem* item = new QListWidgetItem();
		//: Controller Port is an official term from Sony. Find the official translation for your language inside the console's manual.
		item->setText(mtap_enabled[port] ? (tr("Controller Port %1%2\n%3").arg(port + 1).arg(s_mtap_slot_names[slot]).arg(display_name)) :
                                           //: Controller Port is an official term from Sony. Find the official translation for your language inside the console's manual.
                                           tr("Controller Port %1\n%2").arg(port + 1).arg(display_name));
		item->setIcon(m_port_bindings[global_slot]->getIcon());
		item->setData(Qt::UserRole, QVariant(global_slot));
		m_ui.settingsCategory->addItem(item);
	}

	// USB ports
	for (u32 port = 0; port < USB::NUM_PORTS; port++)
	{
		m_usb_bindings[port] = new USBDeviceWidget(m_ui.settingsContainer, this, port);
		m_ui.settingsContainer->addWidget(m_usb_bindings[port]);

		const std::string dtype(getStringValue(fmt::format("USB{}", port + 1).c_str(), "Type", "None"));
		const QString display_name(qApp->translate("USB", USB::GetDeviceName(dtype)));

		QListWidgetItem* item = new QListWidgetItem();
		item->setText(tr("USB Port %1\n%2").arg(port + 1).arg(display_name));
		item->setIcon(m_usb_bindings[port]->getIcon());
		item->setData(Qt::UserRole, QVariant(MAX_PORTS + port));
		m_ui.settingsCategory->addItem(item);
	}

	// only add hotkeys if we're editing global settings
	if (!m_profile_interface || m_profile_interface->GetBoolValue("Pad", "UseProfileHotkeyBindings", false))
	{
		QListWidgetItem* item = new QListWidgetItem();
		item->setText(tr("Hotkeys"));
		item->setIcon(QIcon::fromTheme("keyboard-line"));
		m_ui.settingsCategory->addItem(item);
		m_hotkey_settings = new HotkeySettingsWidget(m_ui.settingsContainer, this);
		m_ui.settingsContainer->addWidget(m_hotkey_settings);
	}

	m_ui.loadProfile->setEnabled(isEditingProfile());
	m_ui.deleteProfile->setEnabled(isEditingProfile());
	m_ui.restoreDefaults->setEnabled(isEditingGlobalSettings());
}

void ControllerSettingsDialog::updateListDescription(u32 global_slot, ControllerBindingWidget* widget)
{
	for (int i = 0; i < m_ui.settingsCategory->count(); i++)
	{
		QListWidgetItem* item = m_ui.settingsCategory->item(i);
		const QVariant data(item->data(Qt::UserRole));
		if (data.metaType().id() == QMetaType::UInt && data.toUInt() == global_slot)
		{
			const auto [port, slot] = sioConvertPadToPortAndSlot(global_slot);
			const bool mtap_enabled = getBoolValue("Pad", (port == 0) ? "MultitapPort1" : "MultitapPort2", false);

			const Pad::ControllerInfo* ci = Pad::GetControllerInfo(widget->getControllerType());
			const QString display_name = QString::fromUtf8(ci ? ci->GetLocalizedName() : "Unknown");

			//: Controller Port is an official term from Sony. Find the official translation for your language inside the console's manual.
			item->setText(mtap_enabled ? (tr("Controller Port %1%2\n%3").arg(port + 1).arg(s_mtap_slot_names[slot]).arg(display_name)) :
                                         //: Controller Port is an official term from Sony. Find the official translation for your language inside the console's manual.
                                         tr("Controller Port %1\n%2").arg(port + 1).arg(display_name));
			item->setIcon(widget->getIcon());
			break;
		}
	}
}

void ControllerSettingsDialog::updateListDescription(u32 port, USBDeviceWidget* widget)
{
	for (int i = 0; i < m_ui.settingsCategory->count(); i++)
	{
		QListWidgetItem* item = m_ui.settingsCategory->item(i);
		const QVariant data(item->data(Qt::UserRole));
		if (data.metaType().id() == QMetaType::UInt && data.toUInt() == (MAX_PORTS + port))
		{
			const std::string dtype(getStringValue(fmt::format("USB{}", port + 1).c_str(), "Type", "None"));
			const QString display_name(qApp->translate("USB", USB::GetDeviceName(dtype)));

			item->setText(tr("USB Port %1\n%2").arg(port + 1).arg(display_name));
			item->setIcon(widget->getIcon());
			break;
		}
	}
}

void ControllerSettingsDialog::refreshProfileList()
{
	const std::vector<std::string> names = Pad::GetInputProfileNames();

	QSignalBlocker sb(m_ui.currentProfile);
	m_ui.currentProfile->clear();
	//: "Shared" refers here to the shared input profile.
	m_ui.currentProfile->addItem(tr("Shared"));
	if (isEditingGlobalSettings())
		m_ui.currentProfile->setCurrentIndex(0);

	for (const std::string& name : names)
	{
		const QString qname(QString::fromStdString(name));
		m_ui.currentProfile->addItem(qname);
		if (qname == m_profile_name)
			m_ui.currentProfile->setCurrentIndex(m_ui.currentProfile->count() - 1);
	}
}

void ControllerSettingsDialog::switchProfile(const QString& name)
{
	QSignalBlocker sb(m_ui.currentProfile);

	if (!name.isEmpty())
	{
		std::string path(VMManager::GetInputProfilePath(name.toStdString()));
		if (!FileSystem::FileExists(path.c_str()))
		{
			QMessageBox::critical(this, tr("Error"), tr("The input profile named '%1' cannot be found.").arg(name));
			return;
		}

		std::unique_ptr<INISettingsInterface> sif(std::make_unique<INISettingsInterface>(std::move(path)));
		sif->Load();
		m_profile_interface = std::move(sif);
		m_ui.currentProfile->setCurrentIndex(m_ui.currentProfile->findText(name));
	}
	else
	{
		m_profile_interface.reset();
		m_ui.currentProfile->setCurrentIndex(0);
	}

	m_profile_name = name;
	createWidgets();
}
