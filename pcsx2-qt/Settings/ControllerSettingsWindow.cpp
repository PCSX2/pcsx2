// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "QtHost.h"
#include "Settings/ControllerSettingsWindow.h"
#include "Settings/ControllerGlobalSettingsWidget.h"
#include "Settings/ControllerBindingWidget.h"
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

ControllerSettingsWindow::ControllerSettingsWindow()
	: QWidget(nullptr)
{
	m_ui.setupUi(this);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	refreshProfileList();
	createWidgets();

	m_ui.settingsCategory->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	connect(m_ui.settingsCategory, &QListWidget::currentRowChanged, this, &ControllerSettingsWindow::onCategoryCurrentRowChanged);
	connect(m_ui.currentProfile, &QComboBox::currentIndexChanged, this, &ControllerSettingsWindow::onCurrentProfileChanged);
	connect(m_ui.buttonBox, &QDialogButtonBox::rejected, this, &ControllerSettingsWindow::close);
	connect(m_ui.newProfile, &QPushButton::clicked, this, &ControllerSettingsWindow::onNewProfileClicked);
	connect(m_ui.applyProfile, &QPushButton::clicked, this, &ControllerSettingsWindow::onApplyProfileClicked);
	connect(m_ui.renameProfile, &QPushButton::clicked, this, &ControllerSettingsWindow::onRenameProfileClicked);
	connect(m_ui.deleteProfile, &QPushButton::clicked, this, &ControllerSettingsWindow::onDeleteProfileClicked);
	connect(m_ui.mappingSettings, &QPushButton::clicked, this, &ControllerSettingsWindow::onMappingSettingsClicked);
	connect(m_ui.restoreDefaults, &QPushButton::clicked, this, &ControllerSettingsWindow::onRestoreDefaultsClicked);

	connect(g_emu_thread, &EmuThread::onInputDevicesEnumerated, this, &ControllerSettingsWindow::onInputDevicesEnumerated);
	connect(g_emu_thread, &EmuThread::onInputDeviceConnected, this, &ControllerSettingsWindow::onInputDeviceConnected);
	connect(g_emu_thread, &EmuThread::onInputDeviceDisconnected, this, &ControllerSettingsWindow::onInputDeviceDisconnected);
	connect(g_emu_thread, &EmuThread::onVibrationMotorsEnumerated, this, &ControllerSettingsWindow::onVibrationMotorsEnumerated);

	// trigger a device enumeration to populate the device list
	g_emu_thread->enumerateInputDevices();
	g_emu_thread->enumerateVibrationMotors();
}

ControllerSettingsWindow::~ControllerSettingsWindow() = default;

void ControllerSettingsWindow::setCategory(Category category)
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

void ControllerSettingsWindow::onCategoryCurrentRowChanged(int row)
{
	m_ui.settingsContainer->setCurrentIndex(row);
}

void ControllerSettingsWindow::onCurrentProfileChanged(int index)
{
	switchProfile((index == 0) ? 0 : m_ui.currentProfile->itemText(index));
}

void ControllerSettingsWindow::onNewProfileClicked()
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
			const int hkres = QMessageBox::question(this, tr("Create Input Profile"),
				tr("Do you want to copy the current hotkey bindings from global settings to the new input profile?"),
				QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
			if (hkres == QMessageBox::Cancel)
				return;

			const bool copy_hotkey_bindings = (hkres == QMessageBox::Yes);
			if (copy_hotkey_bindings)
				temp_si.SetBoolValue("Pad", "UseProfileHotkeyBindings", true);

			// from global
			auto lock = Host::GetSettingsLock();
			Pad::CopyConfiguration(&temp_si, *Host::Internal::GetBaseSettingsLayer(), true, true, copy_hotkey_bindings);
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

void ControllerSettingsWindow::onApplyProfileClicked()
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
		const bool copy_hotkey_bindings = m_profile_interface->GetBoolValue("Pad", "UseProfileHotkeyBindings", false);
		auto lock = Host::GetSettingsLock();
		Pad::CopyConfiguration(Host::Internal::GetBaseSettingsLayer(), *m_profile_interface, true, true, copy_hotkey_bindings);
		USB::CopyConfiguration(Host::Internal::GetBaseSettingsLayer(), *m_profile_interface, true, true);
	}
	Host::CommitBaseSettingChanges();

	g_emu_thread->applySettings();

	// make it visible
	switchProfile({});
}

void ControllerSettingsWindow::onRenameProfileClicked()
{
	const QString profile_name(QInputDialog::getText(this, tr("Rename Input Profile"),
		tr("Enter the new name for the input profile:").arg(m_profile_name)));

	if (profile_name.isEmpty())
		return;

	std::string old_profile_name(m_profile_name.toStdString());
	std::string old_profile_path(VMManager::GetInputProfilePath(m_profile_name.toStdString()));
	std::string profile_path(VMManager::GetInputProfilePath(profile_name.toStdString()));
	if (FileSystem::FileExists(profile_path.c_str()))
	{
		QMessageBox::critical(this, tr("Error"), tr("A profile with the name '%1' already exists.").arg(profile_name));
		return;
	}

	if (!FileSystem::RenamePath(old_profile_path.c_str(), profile_path.c_str()))
	{
		QMessageBox::critical(this, tr("Error"), tr("Failed to rename '%1'.").arg(QString::fromStdString(old_profile_path)));
		return;
	}

	FileSystem::FindResultsArray files;
	FileSystem::FindFiles(EmuFolders::GameSettings.c_str(), "*", FILESYSTEM_FIND_FILES, &files);
	for (const auto& game_settings : files)
	{
		std::string game_settings_path(game_settings.FileName.c_str());
		std::unique_ptr<INISettingsInterface> update_sif(std::make_unique<INISettingsInterface>(std::move(game_settings_path)));

		update_sif->Load();

		if (!old_profile_name.compare(update_sif->GetStringValue("EmuCore", "InputProfileName")))
		{
			update_sif->SetStringValue("EmuCore", "InputProfileName", profile_name.toUtf8());
		}
	}

	refreshProfileList();
	switchProfile({profile_name});
}

void ControllerSettingsWindow::onDeleteProfileClicked()
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

void ControllerSettingsWindow::onMappingSettingsClicked()
{
	ControllerMappingSettingsDialog dialog(this);
	dialog.exec();
}

void ControllerSettingsWindow::onRestoreDefaultsClicked()
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

void ControllerSettingsWindow::onInputDevicesEnumerated(const QList<QPair<QString, QString>>& devices)
{
	m_device_list = devices;
	for (const QPair<QString, QString>& device : devices)
		m_global_settings->addDeviceToList(device.first, device.second);
}

void ControllerSettingsWindow::onInputDeviceConnected(const QString& identifier, const QString& device_name)
{
	m_device_list.emplace_back(identifier, device_name);
	m_global_settings->addDeviceToList(identifier, device_name);
	g_emu_thread->enumerateVibrationMotors();
}

void ControllerSettingsWindow::onInputDeviceDisconnected(const QString& identifier)
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

void ControllerSettingsWindow::onVibrationMotorsEnumerated(const QList<InputBindingKey>& motors)
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

bool ControllerSettingsWindow::getBoolValue(const char* section, const char* key, bool default_value) const
{
	if (m_profile_interface)
		return m_profile_interface->GetBoolValue(section, key, default_value);
	else
		return Host::GetBaseBoolSettingValue(section, key, default_value);
}

s32 ControllerSettingsWindow::getIntValue(const char* section, const char* key, s32 default_value) const
{
	if (m_profile_interface)
		return m_profile_interface->GetIntValue(section, key, default_value);
	else
		return Host::GetBaseIntSettingValue(section, key, default_value);
}

std::string ControllerSettingsWindow::getStringValue(const char* section, const char* key, const char* default_value) const
{
	std::string value;
	if (m_profile_interface)
		value = m_profile_interface->GetStringValue(section, key, default_value);
	else
		value = Host::GetBaseStringSettingValue(section, key, default_value);
	return value;
}

void ControllerSettingsWindow::setBoolValue(const char* section, const char* key, bool value)
{
	if (m_profile_interface)
	{
		m_profile_interface->SetBoolValue(section, key, value);
		saveAndReloadGameSettings();
	}
	else
	{
		Host::SetBaseBoolSettingValue(section, key, value);
		Host::CommitBaseSettingChanges();
		g_emu_thread->applySettings();
	}
}

void ControllerSettingsWindow::setIntValue(const char* section, const char* key, s32 value)
{
	if (m_profile_interface)
	{
		m_profile_interface->SetIntValue(section, key, value);
		saveAndReloadGameSettings();
	}
	else
	{
		Host::SetBaseIntSettingValue(section, key, value);
		Host::CommitBaseSettingChanges();
		g_emu_thread->applySettings();
	}
}

void ControllerSettingsWindow::setStringValue(const char* section, const char* key, const char* value)
{
	if (m_profile_interface)
	{
		m_profile_interface->SetStringValue(section, key, value);
		saveAndReloadGameSettings();
	}
	else
	{
		Host::SetBaseStringSettingValue(section, key, value);
		Host::CommitBaseSettingChanges();
		g_emu_thread->applySettings();
	}
}

void ControllerSettingsWindow::clearSettingValue(const char* section, const char* key)
{
	if (m_profile_interface)
	{
		m_profile_interface->DeleteValue(section, key);
		saveAndReloadGameSettings();
	}
	else
	{
		Host::RemoveBaseSettingValue(section, key);
		Host::CommitBaseSettingChanges();
		g_emu_thread->applySettings();
	}
}

void ControllerSettingsWindow::saveAndReloadGameSettings()
{
	pxAssert(m_profile_interface);
	QtHost::SaveGameSettings(m_profile_interface.get(), false);
	g_emu_thread->reloadGameSettings();
}

void ControllerSettingsWindow::createWidgets()
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
		connect(m_global_settings, &ControllerGlobalSettingsWidget::bindingSetupChanged, this, &ControllerSettingsWindow::createWidgets);
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
		item->setText(mtap_enabled[port] ? (tr("Controller Port %1%2\n%3").arg(port + 1).arg(s_mtap_slot_names[slot]).arg(display_name))
		//: Controller Port is an official term from Sony. Find the official translation for your language inside the console's manual.
		                                 : tr("Controller Port %1\n%2").arg(port + 1).arg(display_name));
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

	m_ui.applyProfile->setEnabled(isEditingProfile());
	m_ui.renameProfile->setEnabled(isEditingProfile());
	m_ui.deleteProfile->setEnabled(isEditingProfile());
	m_ui.restoreDefaults->setEnabled(isEditingGlobalSettings());
}

void ControllerSettingsWindow::updateListDescription(u32 global_slot, ControllerBindingWidget* widget)
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
			item->setText(mtap_enabled ? (tr("Controller Port %1%2\n%3").arg(port + 1).arg(s_mtap_slot_names[slot]).arg(display_name))
			//: Controller Port is an official term from Sony. Find the official translation for your language inside the console's manual.
			                           : tr("Controller Port %1\n%2").arg(port + 1).arg(display_name));
			item->setIcon(widget->getIcon());
			break;
		}
	}
}

void ControllerSettingsWindow::updateListDescription(u32 port, USBDeviceWidget* widget)
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

void ControllerSettingsWindow::refreshProfileList()
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

void ControllerSettingsWindow::switchProfile(const QString& name)
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
