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

#include "EmuThread.h"
#include "QtHost.h"
#include "Settings/ControllerSettingsDialog.h"
#include "Settings/ControllerGlobalSettingsWidget.h"
#include "Settings/ControllerBindingWidgets.h"
#include "Settings/HotkeySettingsWidget.h"

#include "pcsx2/Sio.h"

#include "common/Assertions.h"

#include <array>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QTextEdit>

ControllerSettingsDialog::ControllerSettingsDialog(QWidget* parent /* = nullptr */)
	: QDialog(parent)
{
	m_ui.setupUi(this);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// These are preset in the ui file.
	m_global_settings = new ControllerGlobalSettingsWidget(m_ui.settingsContainer, this);
	m_ui.settingsContainer->addWidget(m_global_settings);
	m_hotkey_settings = new HotkeySettingsWidget(m_ui.settingsContainer, this);
	m_ui.settingsContainer->addWidget(m_hotkey_settings);

	// add remainder of ports
	createPortWidgets();

	m_ui.settingsCategory->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	connect(m_ui.settingsCategory, &QListWidget::currentRowChanged, this, &ControllerSettingsDialog::onCategoryCurrentRowChanged);
	connect(m_ui.buttonBox, &QDialogButtonBox::rejected, this, &ControllerSettingsDialog::close);

	connect(g_emu_thread, &EmuThread::onInputDevicesEnumerated, this, &ControllerSettingsDialog::onInputDevicesEnumerated);
	connect(g_emu_thread, &EmuThread::onInputDeviceConnected, this, &ControllerSettingsDialog::onInputDeviceConnected);
	connect(g_emu_thread, &EmuThread::onInputDeviceDisconnected, this, &ControllerSettingsDialog::onInputDeviceDisconnected);
	connect(g_emu_thread, &EmuThread::onVibrationMotorsEnumerated, this, &ControllerSettingsDialog::onVibrationMotorsEnumerated);

	connect(m_global_settings, &ControllerGlobalSettingsWidget::multitapModeChanged, this, &ControllerSettingsDialog::createPortWidgets);

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
			m_ui.settingsCategory->setCurrentRow(3);
			break;

		default:
			break;
	}
}

void ControllerSettingsDialog::onCategoryCurrentRowChanged(int row)
{
	m_ui.settingsContainer->setCurrentIndex(row);
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
		const std::string key_str(InputManager::ConvertInputBindingKeyToString(key));
		if (!key_str.empty())
			m_vibration_motors.push_back(QString::fromStdString(key_str));
	}
}

void ControllerSettingsDialog::createPortWidgets()
{
	// shouldn't mess with it with something else visible
	{
		QSignalBlocker sb(m_ui.settingsContainer);
		QSignalBlocker sb2(m_ui.settingsCategory);
		m_ui.settingsContainer->setCurrentIndex(0);
		m_ui.settingsCategory->setCurrentRow(0);
	}

	// remove all except global and hotkeys (i.e. first and last)
	pxAssert(m_ui.settingsCategory->count() == m_ui.settingsContainer->count());
	while (m_ui.settingsContainer->count() > 2)
	{
		delete m_ui.settingsCategory->takeItem(1);

		QWidget* widget = m_ui.settingsContainer->widget(1);
		m_ui.settingsContainer->removeWidget(widget);
		delete widget;
	}

	// because we can't insert and shuffle everything forward, we need to temporarily remove hotkeys
	QListWidgetItem* const hotkeys_row = m_ui.settingsCategory->takeItem(1);
	QWidget* const hotkeys_widget = m_ui.settingsContainer->widget(1);
	m_ui.settingsContainer->removeWidget(hotkeys_widget);

	// load mtap settings
	const std::array<bool, 2> mtap_enabled = {{Host::GetBaseBoolSettingValue("EmuCore", "MultitapPort0_Enabled", false),
		Host::GetBaseBoolSettingValue("EmuCore", "MultitapPort1_Enabled", false)}};

	// we reorder things a little to make it look less silly for mtap
	static constexpr const std::array<char, 4> mtap_slot_names = {{'A', 'B', 'C', 'D'}};
	static constexpr const std::array<u32, MAX_PORTS> mtap_port_order = {{0, 2, 3, 4, 1, 5, 6, 7}};

	// create the ports
	for (u32 global_slot : mtap_port_order)
	{
		const bool is_mtap_port = sioPadIsMultitapSlot(global_slot);
		const auto [port, slot] = sioConvertPadToPortAndSlot(global_slot);
		if (is_mtap_port && !mtap_enabled[port])
			continue;

		QListWidgetItem* item = new QListWidgetItem();
		item->setText(mtap_enabled[port] ?
						  (tr("Controller Port %1%2").arg(port + 1).arg(mtap_slot_names[slot])) :
                          tr("Controller Port %1").arg(port + 1));
		item->setIcon(QIcon::fromTheme("gamepad-line"));
		m_ui.settingsCategory->addItem(item);

		m_port_bindings[global_slot] = new ControllerBindingWidget(m_ui.settingsContainer, this, global_slot);
		m_ui.settingsContainer->addWidget(m_port_bindings[global_slot]);
	}

	// and re-insert hotkeys
	m_ui.settingsCategory->addItem(hotkeys_row);
	m_ui.settingsContainer->addWidget(hotkeys_widget);
}