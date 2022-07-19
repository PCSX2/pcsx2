/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team, 987123879113
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

#include <QtCore/QTimer>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <algorithm>

#include "Settings/Python2BindingWidget.h"
#include "Settings/ControllerSettingsDialog.h"
#include "Settings/ControllerSettingWidgetBinder.h"
#include "Settings/InputBindingWidget.h"
#include "Settings/SettingsDialog.h"
#include "EmuThread.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"

#include "common/StringUtil.h"
#include "pcsx2/HostSettings.h"
#include "pcsx2/PAD/Host/PAD.h"

#include "pcsx2/GS/GSIntrin.h" // _BitScanForward

#include "pcsx2/USB/usb-python2/inputs/Python2QtInputManager.h"


Python2BindingWidget::Python2BindingWidget(QWidget* parent, ControllerSettingsDialog* dialog)
	: QWidget(parent)
	, m_dialog(dialog)
{
	m_ui.setupUi(this);
	Python2QtInputManager::LoadMapping();

	connect(m_ui.gameTypeFilter, &QComboBox::currentIndexChanged, this, [this](int index) {
		m_ui.inputList->clear();

		if (s_python2_system_info[index].bindings == nullptr)
		{
			for (auto system_entry : s_python2_system_info)
			{
				if (system_entry.bindings == nullptr)
					continue;

				for (u32 i = 0; i < system_entry.num_bindings; i++)
				{
					auto entry = system_entry.bindings[i];

					if (entry.type != PAD::ControllerBindingType::Button)
						continue;

					QListWidgetItem* item = new QListWidgetItem();
					item->setText(tr("%1 - %2").arg(QString::fromStdString(system_entry.name)).arg(QString::fromStdString(entry.display_name)));
					item->setData(Qt::UserRole, QString::fromStdString(entry.name));
					m_ui.inputList->addItem(item);
				}
			}
		}
		else
		{
			for (u32 i = 0; i < s_python2_system_info[index].num_bindings; i++)
			{
				auto entry = s_python2_system_info[index].bindings[i];

				if (entry.type != PAD::ControllerBindingType::Button)
					continue;

				QListWidgetItem* item = new QListWidgetItem();
				item->setText(QString::fromStdString(entry.display_name));
				item->setText(tr("%1 - %2").arg(QString::fromStdString(s_python2_system_info[index].name)).arg(QString::fromStdString(entry.display_name)));
				item->setData(Qt::UserRole, QString::fromStdString(entry.name));
				m_ui.inputList->addItem(item);
			}
		}

		m_ui.inputList->setCurrentRow(0);
	});

	// Inputs tab
	connect(m_ui.bindKey, &QPushButton::clicked, this, [this]() { startListeningForInput(TIMEOUT_FOR_SINGLE_BINDING, false); });
	connect(m_ui.unbindKey, &QPushButton::clicked, this, [this]() { unbindKeyClicked(m_ui.keybindList); });

	for (std::size_t i = 0; i < std::size(s_python2_system_info); i++)
	{
		auto input_entry = s_python2_system_info[i];
		m_ui.gameTypeFilter->addItem(QString::fromStdString(input_entry.name), (int)i);
	}
	m_ui.gameTypeFilter->setCurrentIndex(0);

	// Analog Inputs tab
	connect(m_ui.bindKeyAnalog, &QPushButton::clicked, this, [this]() { startListeningForInput(TIMEOUT_FOR_SINGLE_BINDING, true); });
	connect(m_ui.unbindKeyAnalog, &QPushButton::clicked, this, [this]() { unbindKeyClicked(m_ui.keybindListAnalogs); });
	connect(m_ui.inputListAnalogs, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
		// Disable motor scale slider
		m_ui.Deadzone->setDisabled(true);
		m_ui.AxisScale->setDisabled(true);

		m_ui.Deadzone->blockSignals(true);
		m_ui.Deadzone->setValue(m_ui.Deadzone->minimum());
		m_ui.Deadzone->blockSignals(false);

		m_ui.AxisScale->blockSignals(true);
		m_ui.AxisScale->setValue(m_ui.AxisScale->minimum());
		m_ui.AxisScale->blockSignals(false);
	});
	connect(m_ui.keybindListAnalogs, &QTableWidget::itemClicked, this, [this](QTableWidgetItem* item) {
		// Enable motor scale slider
		auto currentRow = m_ui.keybindListAnalogs->currentRow();

		if (currentRow == -1)
			return;

		auto deadzoneItem = m_ui.keybindListAnalogs->item(currentRow, 1);
		auto axisScaleItem = m_ui.keybindListAnalogs->item(currentRow, 2);

		m_ui.Deadzone->blockSignals(true);
		m_ui.Deadzone->setValue(deadzoneItem->data(Qt::UserRole).toDouble() * static_cast<double>(m_ui.Deadzone->maximum()));
		m_ui.Deadzone->blockSignals(false);

		m_ui.AxisScale->blockSignals(true);
		m_ui.AxisScale->setValue(axisScaleItem->data(Qt::UserRole).toDouble() * static_cast<double>(m_ui.AxisScale->maximum()));
		m_ui.AxisScale->blockSignals(false);

		m_ui.Deadzone->setDisabled(false);
		m_ui.AxisScale->setDisabled(false);
	});
	connect(m_ui.Deadzone, &QSlider::valueChanged, this, [this](int value) {
		auto currentRow = m_ui.keybindListAnalogs->currentRow();

		if (currentRow == -1)
			return;

		auto selectedItem = m_ui.keybindListAnalogs->item(currentRow, 0);
		uint uniqueId = selectedItem->data(Qt::UserRole).toUInt();
		auto val = static_cast<double>(value) / static_cast<double>(m_ui.Deadzone->maximum());

		for (auto &bind : Python2QtInputManager::GetCurrentMappings()) {
			if (bind.uniqueId == uniqueId) {
				bind.analogDeadzone = val;
			}
		}

		saveAndRefresh();

		m_ui.keybindListAnalogs->setCurrentItem(
			m_ui.keybindListAnalogs->item(currentRow, 0)
		);
	});
	connect(m_ui.AxisScale, &QSlider::valueChanged, this, [this](int value) {
		auto currentRow = m_ui.keybindListAnalogs->currentRow();

		if (currentRow == -1)
			return;

		auto selectedItem = m_ui.keybindListAnalogs->item(currentRow, 0);
		uint uniqueId = selectedItem->data(Qt::UserRole).toUInt();
		auto val = static_cast<double>(value) / static_cast<double>(m_ui.AxisScale->maximum());

		for (auto &bind : Python2QtInputManager::GetCurrentMappings()) {
			if (bind.uniqueId == uniqueId) {
				bind.analogSensitivity = val;
			}
		}

		saveAndRefresh();

		m_ui.keybindListAnalogs->setCurrentItem(
			m_ui.keybindListAnalogs->item(currentRow, 0)
		);
	});

	for (auto system_entry : s_python2_system_info)
	{
		if (system_entry.bindings == nullptr)
			continue;

		for (u32 i = 0; i < system_entry.num_bindings; i++)
		{
			auto entry = system_entry.bindings[i];

			if (entry.type != PAD::ControllerBindingType::Axis && entry.type != PAD::ControllerBindingType::HalfAxis)
				continue;

			QListWidgetItem* item = new QListWidgetItem();
			item->setText(tr("%1 - %2").arg(QString::fromStdString(system_entry.name)).arg(QString::fromStdString(entry.display_name)));
			item->setData(Qt::UserRole, QString::fromStdString(entry.name));
			m_ui.inputListAnalogs->addItem(item);
		}
	}
	m_ui.inputListAnalogs->setCurrentRow(0);
	m_ui.Deadzone->setDisabled(true);
	m_ui.AxisScale->setDisabled(true);

	// Motors tab
	connect(m_ui.bindKeyMotor, &QPushButton::clicked, this, &Python2BindingWidget::onBindKeyMotorClicked);
	connect(m_ui.unbindKeyMotor, &QPushButton::clicked, this, [this]() { unbindKeyClicked(m_ui.keybindListMotors); });
	connect(m_ui.motorList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
		// Disable motor scale slider
		m_ui.MotorScale->setDisabled(true);

		m_ui.MotorScale->blockSignals(true);
		m_ui.MotorScale->setValue(m_ui.MotorScale->minimum());
		m_ui.MotorScale->blockSignals(false);
	});
	connect(m_ui.keybindListMotors, &QTableWidget::itemClicked, this, [this](QTableWidgetItem* item) {
		// Enable motor scale slider
		auto currentRow = m_ui.keybindListMotors->currentRow();

		if (currentRow == -1)
			return;

		auto motorScaleItem = m_ui.keybindListMotors->item(currentRow, 1);

		m_ui.MotorScale->blockSignals(true);
		m_ui.MotorScale->setValue(motorScaleItem->data(Qt::UserRole).toDouble() * static_cast<double>(m_ui.MotorScale->maximum()));
		m_ui.MotorScale->blockSignals(false);

		m_ui.MotorScale->setDisabled(false);
	});
	connect(m_ui.MotorScale, &QSlider::valueChanged, this, [this](int value) {
		auto currentRow = m_ui.keybindListMotors->currentRow();

		if (currentRow == -1)
			return;

		auto selectedItem = m_ui.keybindListMotors->item(currentRow, 0);
		uint uniqueId = selectedItem->data(Qt::UserRole).toUInt();
		auto val = static_cast<double>(value) / static_cast<double>(m_ui.MotorScale->maximum());

		for (auto &bind : Python2QtInputManager::GetCurrentMappings()) {
			if (bind.uniqueId == uniqueId) {
				bind.motorScale = val;
			}
		}

		saveAndRefresh();

		m_ui.keybindListMotors->setCurrentItem(
			m_ui.keybindListMotors->item(currentRow, 0)
		);
	});

	for (auto system_entry : s_python2_system_info)
	{
		if (system_entry.bindings == nullptr)
			continue;

		for (u32 i = 0; i < system_entry.num_bindings; i++)
		{
			auto entry = system_entry.bindings[i];

			if (entry.type != PAD::ControllerBindingType::Motor)
				continue;

			QListWidgetItem* item = new QListWidgetItem();
			item->setText(tr("%1 - %2").arg(QString::fromStdString(system_entry.name)).arg(QString::fromStdString(entry.display_name)));
			item->setData(Qt::UserRole, QString::fromStdString(entry.name));
			m_ui.motorList->addItem(item);
		}
	}
	m_ui.motorList->setCurrentRow(0);
	m_ui.MotorScale->setDisabled(true);

	refreshUi();
}

Python2BindingWidget::~Python2BindingWidget() = default;

QIcon Python2BindingWidget::getIcon() const
{
	return QIcon::fromTheme("artboard-2-line");
}

void Python2BindingWidget::unbindKeyClicked(QTableWidget* tableWidget)
{
	auto currentSelectionRow = tableWidget->currentRow();
	auto currentSelectionCol = tableWidget->currentColumn();

	if (currentSelectionRow == -1)
		return;

	for (int i = 0; i < tableWidget->rowCount(); i++)
	{
		auto selectedItem = tableWidget->item(i, 0);

		if (!selectedItem->isSelected())
			continue;

		uint uniqueId = selectedItem->data(Qt::UserRole).toUInt();

		Python2QtInputManager::RemoveMappingByUniqueId(uniqueId);
	}

	if (currentSelectionRow - 1 >= 0 && currentSelectionRow - 1 < tableWidget->rowCount())
		tableWidget->setCurrentCell(currentSelectionRow - 1, currentSelectionCol);
	else if (tableWidget->rowCount() > 0)
		tableWidget->setCurrentCell(0, currentSelectionCol);

	saveAndRefresh();
}

void Python2BindingWidget::onBindKeyMotorClicked()
{
	auto full_key = m_ui.motorList->currentItem()->data(Qt::UserRole).toString();

	QInputDialog dialog(QtUtils::GetRootWidget(this));

	QStringList input_options(m_dialog->getVibrationMotors());
	if (input_options.isEmpty())
	{
		QMessageBox::critical(QtUtils::GetRootWidget(this), tr("Error"), tr("No devices with vibration motors were detected."));
		return;
	}

	QInputDialog input_dialog(this);
	input_dialog.setWindowTitle(full_key);
	input_dialog.setLabelText(tr("Select vibration motor for %1.").arg(full_key));
	input_dialog.setInputMode(QInputDialog::TextInput);
	input_dialog.setOptions(QInputDialog::UseListViewForComboBoxItems);
	input_dialog.setComboBoxEditable(false);
	input_dialog.setComboBoxItems(std::move(input_options));
	input_dialog.setTextValue(QString::fromStdString("Test Value"));
	if (input_dialog.exec() == 0)
		return;

	auto new_binding = input_dialog.textValue().toStdString();
	double motorScale = 0;
	if (QSlider* widget = findChild<QSlider*>(QStringLiteral("MotorScale")); widget)
	{
		motorScale = static_cast<float>(widget->value()) / static_cast<float>(widget->maximum());
	}

	if (Python2QtInputManager::AddNewBinding(full_key.toStdString(), new_binding, 0, 0, motorScale)) {
		saveAndRefresh();
	}
}

void Python2BindingWidget::refreshUi()
{
	refreshInputBindingList();
	refreshInputAnalogBindingList();
	refreshOutputMotorBindingList();
}

void Python2BindingWidget::saveAndRefresh()
{
	saveMapping();
	refreshUi();
}

std::string Python2BindingWidget::getKeybindDisplayName(std::string keybind)
{
	for (auto system_entry : s_python2_system_info)
	{
		if (system_entry.bindings == nullptr)
			continue;

		for (u32 i = 0; i < system_entry.num_bindings; i++)
		{
			if (std::string(system_entry.bindings[i].name) == keybind)
				return (tr("%1 - %2").arg(system_entry.name).arg(system_entry.bindings[i].display_name)).toStdString();
		}
	}

	return keybind;
}

void Python2BindingWidget::refreshInputBindingList()
{
	m_ui.keybindList->clearContents();
	m_ui.keybindList->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_ui.keybindList->setAlternatingRowColors(true);
	m_ui.keybindList->setShowGrid(false);
	m_ui.keybindList->verticalHeader()->hide();
	m_ui.keybindList->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_ui.keybindList->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	m_ui.keybindList->horizontalHeader()->setStretchLastSection(true);
	m_ui.keybindList->setRowCount(0);

	for (auto &entry : Python2QtInputManager::GetCurrentMappings())
	{
		if (entry.input_type != PAD::ControllerBindingType::Button)
			continue;

		const int row = m_ui.keybindList->rowCount();
		m_ui.keybindList->insertRow(row);

		QTableWidgetItem* item = new QTableWidgetItem();
		item->setText(QString::fromStdString(getKeybindDisplayName(entry.keybind)));
		item->setFlags(item->flags() & ~(Qt::ItemIsEditable));
		item->setData(Qt::UserRole, entry.uniqueId);
		m_ui.keybindList->setItem(row, 0, item);

		QCheckBox* cb = new QCheckBox(m_ui.keybindList);
		cb->setChecked(entry.isOneshot);
		connect(cb, &QCheckBox::stateChanged, this, [this, &entry](int state) {
			entry.isOneshot = state == Qt::Checked;
			saveMapping();
		});

		QWidget* cbw = new QWidget();
		QHBoxLayout* hbox = new QHBoxLayout(cbw);
		hbox->addWidget(cb);
		hbox->setAlignment(Qt::AlignCenter);
		hbox->setContentsMargins(0, 0, 0, 0);

		m_ui.keybindList->setCellWidget(row, 1, cbw);

		QTableWidgetItem* item2 = new QTableWidgetItem();
		item2->setText(QString::fromStdString(entry.inputKey));
		item2->setFlags(item->flags() & ~(Qt::ItemIsEditable));
		m_ui.keybindList->setItem(row, 2, item2);
	}
}

void Python2BindingWidget::refreshInputAnalogBindingList()
{
	m_ui.keybindListAnalogs->clearContents();
	m_ui.keybindListAnalogs->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_ui.keybindListAnalogs->setAlternatingRowColors(true);
	m_ui.keybindListAnalogs->setShowGrid(false);
	m_ui.keybindListAnalogs->verticalHeader()->hide();
	m_ui.keybindListAnalogs->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_ui.keybindListAnalogs->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	m_ui.keybindListAnalogs->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	m_ui.keybindListAnalogs->horizontalHeader()->setStretchLastSection(true);
	m_ui.keybindListAnalogs->setRowCount(0);

	for (auto entry : Python2QtInputManager::GetCurrentMappings())
	{
		if (entry.input_type != PAD::ControllerBindingType::Axis && entry.input_type != PAD::ControllerBindingType::HalfAxis)
			continue;

		const int row = m_ui.keybindListAnalogs->rowCount();
		m_ui.keybindListAnalogs->insertRow(row);

		QTableWidgetItem* item = new QTableWidgetItem();
		item->setText(QString::fromStdString(getKeybindDisplayName(entry.keybind)));
		item->setFlags(item->flags() & ~(Qt::ItemIsEditable));
		item->setData(Qt::UserRole, entry.uniqueId);
		m_ui.keybindListAnalogs->setItem(row, 0, item);

		QTableWidgetItem* item2 = new QTableWidgetItem();
		item2->setText(tr("%1%").arg(entry.analogDeadzone * 100));
		item2->setData(Qt::UserRole, entry.analogDeadzone);
		item2->setFlags(item->flags() & ~(Qt::ItemIsEditable));
		m_ui.keybindListAnalogs->setItem(row, 1, item2);

		QTableWidgetItem* item3 = new QTableWidgetItem();
		item3->setText(tr("%1%").arg(entry.analogSensitivity * 100));
		item3->setData(Qt::UserRole, entry.analogSensitivity);
		item3->setFlags(item->flags() & ~(Qt::ItemIsEditable));
		m_ui.keybindListAnalogs->setItem(row, 2, item3);

		QTableWidgetItem* item4 = new QTableWidgetItem();
		item4->setText(QString::fromStdString(entry.inputKey));
		item4->setFlags(item->flags() & ~(Qt::ItemIsEditable));
		m_ui.keybindListAnalogs->setItem(row, 3, item4);
	}
}

void Python2BindingWidget::refreshOutputMotorBindingList()
{
	m_ui.keybindListMotors->clearContents();
	m_ui.keybindListMotors->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_ui.keybindListMotors->setAlternatingRowColors(true);
	m_ui.keybindListMotors->setShowGrid(false);
	m_ui.keybindListMotors->verticalHeader()->hide();
	m_ui.keybindListMotors->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_ui.keybindListMotors->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	m_ui.keybindListMotors->horizontalHeader()->setStretchLastSection(true);
	m_ui.keybindListMotors->setRowCount(0);

	for (auto entry : Python2QtInputManager::GetCurrentMappings())
	{
		if (entry.input_type != PAD::ControllerBindingType::Motor)
			continue;

		const int row = m_ui.keybindListMotors->rowCount();
		m_ui.keybindListMotors->insertRow(row);

		QTableWidgetItem* item = new QTableWidgetItem();
		item->setText(QString::fromStdString(getKeybindDisplayName(entry.keybind)));
		item->setFlags(item->flags() & ~(Qt::ItemIsEditable));
		item->setData(Qt::UserRole, entry.uniqueId);
		m_ui.keybindListMotors->setItem(row, 0, item);

		QTableWidgetItem* item2 = new QTableWidgetItem();
		item2->setText(tr("%1%").arg(entry.motorScale * 100));
		item2->setData(Qt::UserRole, entry.motorScale);
		item2->setFlags(item->flags() & ~(Qt::ItemIsEditable));
		m_ui.keybindListMotors->setItem(row, 1, item2);

		QTableWidgetItem* item3 = new QTableWidgetItem();
		item3->setText(QString::fromStdString(entry.inputKey));
		item3->setFlags(item->flags() & ~(Qt::ItemIsEditable));
		m_ui.keybindListMotors->setItem(row, 2, item3);
	}
}

void Python2BindingWidget::onInputListenTimerTimeout()
{
	m_input_listen_remaining_seconds--;
	if (m_input_listen_remaining_seconds == 0)
	{
		stopListeningForInput();
		return;
	}

	if (m_input_listen_analog)
	{
		m_ui.bindKeyAnalog->setText(tr("Press axis button... [%1]").arg(m_input_listen_remaining_seconds));
	}
	else
	{
		m_ui.bindKey->setText(tr("Press button... [%1]").arg(m_input_listen_remaining_seconds));
	}
}

void Python2BindingWidget::startListeningForInput(u32 timeout_in_seconds, bool isAnalog)
{
	if (isListeningForInput())
		stopListeningForInput();

	m_new_bindings.clear();

	m_input_listen_analog = isAnalog;
	m_input_listen_timer = new QTimer(this);
	m_input_listen_timer->setSingleShot(false);
	m_input_listen_timer->start(1000);
	m_input_listen_timer->connect(m_input_listen_timer, &QTimer::timeout, this, &Python2BindingWidget::onInputListenTimerTimeout);

	m_input_listen_remaining_seconds = timeout_in_seconds;

	if (m_input_listen_analog)
	{
		m_ui.bindKeyAnalog->setText(tr("Press axis button... [%1]").arg(m_input_listen_remaining_seconds));
	}
	else
	{
		m_ui.bindKey->setText(tr("Press button... [%1]").arg(m_input_listen_remaining_seconds));
	}

	installEventFilter(this);
	grabKeyboard();
	grabMouse();
	setMouseTracking(true);
	hookInputManager();
}

void Python2BindingWidget::stopListeningForInput()
{
	delete m_input_listen_timer;
	m_input_listen_timer = nullptr;

	std::vector<InputBindingKey>().swap(m_new_bindings);

	unhookInputManager();
	setMouseTracking(false);
	releaseMouse();
	releaseKeyboard();
	removeEventFilter(this);

	if (m_input_listen_analog)
	{
		m_ui.bindKeyAnalog->setText(tr("Bind"));
	}
	else
	{
		m_ui.bindKey->setText(tr("Bind"));
	}
}

void Python2BindingWidget::inputManagerHookCallback(InputBindingKey key, float value)
{
	const float abs_value = std::abs(value);

	if (m_input_listen_analog && key.source_subtype != InputSubclass::PointerAxis && key.source_subtype != InputSubclass::ControllerAxis)
		return;

	for (InputBindingKey other_key : m_new_bindings)
	{
		if (other_key.MaskDirection() == key.MaskDirection())
		{
			if (abs_value < 0.5f)
			{
				// if this key is in our new binding list, it's a "release", and we're done
				setNewInputBinding();
				stopListeningForInput();
				return;
			}

			// otherwise, keep waiting
			return;
		}
	}

	// new binding, add it to the list, but wait for a decent distance first, and then wait for release
	if (abs_value >= 0.5f)
	{
		InputBindingKey key_to_add = key;
		m_new_bindings.push_back(key_to_add);
	}
}

void Python2BindingWidget::hookInputManager()
{
	InputManager::SetHook([this](InputBindingKey key, float value) {
		QMetaObject::invokeMethod(this, "inputManagerHookCallback", Qt::QueuedConnection, Q_ARG(InputBindingKey, key),
			Q_ARG(float, value));
		return InputInterceptHook::CallbackResult::StopProcessingEvent;
	});
}

void Python2BindingWidget::unhookInputManager()
{
	InputManager::RemoveHook();
}

bool Python2BindingWidget::eventFilter(QObject* watched, QEvent* event)
{
	if (m_input_listen_analog)
		return false;

	const QEvent::Type event_type = event->type();

	// if the key is being released, set the input
	if (event_type == QEvent::KeyRelease || event_type == QEvent::MouseButtonRelease)
	{
		setNewInputBinding();
		stopListeningForInput();
		return true;
	}
	else if (event_type == QEvent::KeyPress)
	{
		const QKeyEvent* key_event = static_cast<const QKeyEvent*>(event);
		m_new_bindings.push_back(InputManager::MakeHostKeyboardKey(QtUtils::KeyEventToCode(key_event)));
		return true;
	}
	else if (event_type == QEvent::MouseButtonPress || event_type == QEvent::MouseButtonDblClick)
	{
		// double clicks get triggered if we click bind, then click again quickly.
		unsigned long button_index;
		if (_BitScanForward(&button_index, static_cast<u32>(static_cast<const QMouseEvent*>(event)->button())))
			m_new_bindings.push_back(InputManager::MakePointerButtonKey(0, button_index));
		return true;
	}
	else if (event_type == QEvent::Wheel)
	{
		const QPoint delta_angle(static_cast<QWheelEvent*>(event)->angleDelta());
		const float dx = std::clamp(static_cast<float>(delta_angle.x()) / QtUtils::MOUSE_WHEEL_DELTA, -1.0f, 1.0f);
		if (dx != 0.0f)
		{
			InputBindingKey key(InputManager::MakePointerAxisKey(0, InputPointerAxis::WheelX));
			key.negative = (dx < 0.0f);
			m_new_bindings.push_back(key);
		}

		const float dy = std::clamp(static_cast<float>(delta_angle.y()) / QtUtils::MOUSE_WHEEL_DELTA, -1.0f, 1.0f);
		if (dy != 0.0f)
		{
			InputBindingKey key(InputManager::MakePointerAxisKey(0, InputPointerAxis::WheelY));
			key.negative = (dy < 0.0f);
			m_new_bindings.push_back(key);
		}

		if (dx != 0.0f || dy != 0.0f)
		{
			setNewInputBinding();
			stopListeningForInput();
		}

		return true;
	}

	return false;
}

void Python2BindingWidget::setNewInputBinding()
{
	if (m_new_bindings.empty())
		return;

	const std::string new_binding(InputManager::ConvertInputBindingKeysToString(m_new_bindings.data(), m_new_bindings.size()));

	std::string full_key;
	float analogDeadzone = 0;
	float analogSensitivity = 0;

	if (m_input_listen_analog)
	{
		full_key = m_ui.inputListAnalogs->currentItem()->data(Qt::UserRole).toString().toStdString();

		if (QSlider* widget = findChild<QSlider*>(QStringLiteral("Deadzone")); widget)
		{
			analogDeadzone = static_cast<float>(widget->value()) / static_cast<float>(widget->maximum());
		}

		if (QSlider* widget = findChild<QSlider*>(QStringLiteral("AxisScale")); widget)
		{
			analogSensitivity = static_cast<float>(widget->value()) / static_cast<float>(widget->maximum());
		}
	}
	else
	{
		full_key = m_ui.inputList->currentItem()->data(Qt::UserRole).toString().toStdString();
	}

	if (Python2QtInputManager::AddNewBinding(full_key, new_binding, analogDeadzone, analogSensitivity, 0)) {
		saveAndRefresh();
	}
}

void Python2BindingWidget::saveMapping()
{
	auto lock = Host::GetSettingsLock();
	SettingsInterface* si = Host::Internal::GetBaseSettingsLayer();
	const std::string section = "Python2";

	// Clear all keybinds in Python2 section
	si->ClearSection(section.c_str());

	// Recreate Python2 section
	for (auto entry : Python2QtInputManager::GetCurrentMappings())
	{
		if (entry.input_type == PAD::ControllerBindingType::Button)
		{
			si->AddToStringList(
                section.c_str(),
                entry.keybind.c_str(),
                StringUtil::StdStringFromFormat("%s|%d", entry.inputKey.c_str(), entry.isOneshot).c_str()
            );
		}
		else if (entry.input_type == PAD::ControllerBindingType::Axis || entry.input_type == PAD::ControllerBindingType::HalfAxis)
		{
			si->AddToStringList(
                section.c_str(),
                entry.keybind.c_str(),
                StringUtil::StdStringFromFormat("%s|%lf|%lf", entry.inputKey.c_str(), entry.analogDeadzone, entry.analogSensitivity).c_str()
            );
		}
		else if (entry.input_type == PAD::ControllerBindingType::Motor)
		{
			si->AddToStringList(
                section.c_str(),
                entry.keybind.c_str(),
                StringUtil::StdStringFromFormat("%s|%lf", entry.inputKey.c_str(), entry.motorScale).c_str()
            );
		}
	}

	QtHost::QueueSettingsSave();
}