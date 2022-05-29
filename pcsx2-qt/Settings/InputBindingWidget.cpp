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

#include <QtCore/QTimer>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QInputDialog>
#include <cmath>
#include <sstream>

#include "pcsx2/HostSettings.h"

#include "pcsx2/GS/GSIntrin.h" // _BitScanForward

#include "EmuThread.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "Settings/ControllerSettingsDialog.h"
#include "Settings/InputBindingDialog.h"
#include "Settings/InputBindingWidget.h"

InputBindingWidget::InputBindingWidget(QWidget* parent)
	: QPushButton(parent)
{
	connect(this, &QPushButton::clicked, this, &InputBindingWidget::onClicked);
}

InputBindingWidget::InputBindingWidget(QWidget* parent, std::string section_name, std::string key_name)
	: QPushButton(parent)
{
	setMinimumWidth(225);
	setMaximumWidth(225);

	connect(this, &QPushButton::clicked, this, &InputBindingWidget::onClicked);

	setKey(std::move(section_name), std::move(key_name));
}

InputBindingWidget::~InputBindingWidget()
{
	Q_ASSERT(!isListeningForInput());
}

void InputBindingWidget::setKey(std::string section_name, std::string key_name)
{
	m_section_name = std::move(section_name);
	m_key_name = std::move(key_name);
	m_bindings = Host::GetBaseStringListSetting(m_section_name.c_str(), m_key_name.c_str());
	updateText();
}

void InputBindingWidget::updateText()
{
	if (m_bindings.empty())
	{
		setText(QString());
	}
	else if (m_bindings.size() > 1)
	{
		setText(tr("%n bindings", "", static_cast<int>(m_bindings.size())));

		// keep the full thing for the tooltip
		std::stringstream ss;
		bool first = true;
		for (const std::string& binding : m_bindings)
		{
			if (first)
				first = false;
			else
				ss << "\n";
			ss << binding;
		}
		setToolTip(QString::fromStdString(ss.str()));
	}
	else
	{
		QString binding_text(QString::fromStdString(m_bindings[0]));
		setToolTip(binding_text);

		// fix up accelerators, and if it's too long, ellipsise it
		if (binding_text.contains('&'))
			binding_text = binding_text.replace(QStringLiteral("&"), QStringLiteral("&&"));
		if (binding_text.length() > 35)
			binding_text = binding_text.left(35).append(QStringLiteral("..."));
		setText(binding_text);
	}
}

bool InputBindingWidget::eventFilter(QObject* watched, QEvent* event)
{
	const QEvent::Type event_type = event->type();

	// if the key is being released, set the input
	if (event_type == QEvent::KeyRelease || event_type == QEvent::MouseButtonPress)
	{
		setNewBinding();
		stopListeningForInput();
		return true;
	}
	else if (event_type == QEvent::KeyPress)
	{
		const QKeyEvent* key_event = static_cast<const QKeyEvent*>(event);
		m_new_bindings.push_back(InputManager::MakeHostKeyboardKey(key_event->key()));
		return true;
	}
	else if (event_type == QEvent::MouseButtonPress)
	{
		unsigned long button_index;
		if (_BitScanForward(&button_index, static_cast<u32>(static_cast<const QMouseEvent*>(event)->button())))
			m_new_bindings.push_back(InputManager::MakeHostMouseButtonKey(button_index));
		return true;
	}
	else if (event_type == QEvent::MouseButtonDblClick)
	{
		// just eat double clicks
		return true;
	}

	return false;
}

bool InputBindingWidget::event(QEvent* event)
{
	if (event->type() == QEvent::MouseButtonRelease)
	{
		QMouseEvent* mev = static_cast<QMouseEvent*>(event);
		if (mev->button() == Qt::LeftButton && mev->modifiers() & Qt::ShiftModifier)
		{
			openDialog();
			return false;
		}
	}

	return QPushButton::event(event);
}

void InputBindingWidget::mouseReleaseEvent(QMouseEvent* e)
{
	if (e->button() == Qt::RightButton)
	{
		clearBinding();
		return;
	}

	QPushButton::mouseReleaseEvent(e);
}

void InputBindingWidget::setNewBinding()
{
	if (m_new_bindings.empty())
		return;

	const std::string new_binding(
		InputManager::ConvertInputBindingKeysToString(m_new_bindings.data(), m_new_bindings.size()));
	if (!new_binding.empty())
	{
		QtHost::SetBaseStringSettingValue(m_section_name.c_str(), m_key_name.c_str(), new_binding.c_str());
		g_emu_thread->reloadInputBindings();
	}

	m_bindings.clear();
	m_bindings.push_back(std::move(new_binding));
}

void InputBindingWidget::clearBinding()
{
	m_bindings.clear();
	QtHost::RemoveBaseSettingValue(m_section_name.c_str(), m_key_name.c_str());
	g_emu_thread->reloadInputBindings();
	updateText();
}

void InputBindingWidget::reloadBinding()
{
	m_bindings = Host::GetBaseStringListSetting(m_section_name.c_str(), m_key_name.c_str());
	updateText();
}

void InputBindingWidget::onClicked()
{
	if (m_bindings.size() > 1)
	{
		openDialog();
		return;
	}

	if (isListeningForInput())
		stopListeningForInput();

	startListeningForInput(TIMEOUT_FOR_SINGLE_BINDING);
}

void InputBindingWidget::onInputListenTimerTimeout()
{
	m_input_listen_remaining_seconds--;
	if (m_input_listen_remaining_seconds == 0)
	{
		stopListeningForInput();
		return;
	}

	setText(tr("Push Button/Axis... [%1]").arg(m_input_listen_remaining_seconds));
}

void InputBindingWidget::startListeningForInput(u32 timeout_in_seconds)
{
	m_new_bindings.clear();
	m_input_listen_timer = new QTimer(this);
	m_input_listen_timer->setSingleShot(false);
	m_input_listen_timer->start(1000);

	m_input_listen_timer->connect(m_input_listen_timer, &QTimer::timeout, this,
		&InputBindingWidget::onInputListenTimerTimeout);
	m_input_listen_remaining_seconds = timeout_in_seconds;
	setText(tr("Push Button/Axis... [%1]").arg(m_input_listen_remaining_seconds));

	installEventFilter(this);
	grabKeyboard();
	grabMouse();
	hookInputManager();
}

void InputBindingWidget::stopListeningForInput()
{
	updateText();
	delete m_input_listen_timer;
	m_input_listen_timer = nullptr;
	std::vector<InputBindingKey>().swap(m_new_bindings);

	unhookInputManager();
	releaseMouse();
	releaseKeyboard();
	removeEventFilter(this);
}

void InputBindingWidget::inputManagerHookCallback(InputBindingKey key, float value)
{
	const float abs_value = std::abs(value);

	for (InputBindingKey other_key : m_new_bindings)
	{
		if (other_key.MaskDirection() == key.MaskDirection())
		{
			if (abs_value < 0.5f)
			{
				// if this key is in our new binding list, it's a "release", and we're done
				setNewBinding();
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
		key_to_add.negative = (value < 0.0f);
		m_new_bindings.push_back(key_to_add);
	}
}

void InputBindingWidget::hookInputManager()
{
	InputManager::SetHook([this](InputBindingKey key, float value) {
		QMetaObject::invokeMethod(this, "inputManagerHookCallback", Qt::QueuedConnection, Q_ARG(InputBindingKey, key),
			Q_ARG(float, value));
		return InputInterceptHook::CallbackResult::StopProcessingEvent;
	});
}

void InputBindingWidget::unhookInputManager()
{
	InputManager::RemoveHook();
}

void InputBindingWidget::openDialog()
{
	InputBindingDialog binding_dialog(m_section_name, m_key_name, m_bindings, QtUtils::GetRootWidget(this));
	binding_dialog.exec();
	reloadBinding();
}

InputVibrationBindingWidget::InputVibrationBindingWidget(QWidget* parent)
{
	connect(this, &QPushButton::clicked, this, &InputVibrationBindingWidget::onClicked);
}

InputVibrationBindingWidget::InputVibrationBindingWidget(QWidget* parent, ControllerSettingsDialog* dialog, std::string section_name, std::string key_name)
{
	setMinimumWidth(225);
	setMaximumWidth(225);

	connect(this, &QPushButton::clicked, this, &InputVibrationBindingWidget::onClicked);

	setKey(dialog, std::move(section_name), std::move(key_name));
}

InputVibrationBindingWidget::~InputVibrationBindingWidget()
{
}

void InputVibrationBindingWidget::setKey(ControllerSettingsDialog* dialog, std::string section_name, std::string key_name)
{
	m_dialog = dialog;
	m_section_name = std::move(section_name);
	m_key_name = std::move(key_name);
	m_binding = Host::GetBaseStringSettingValue(m_section_name.c_str(), m_key_name.c_str());
	setText(QString::fromStdString(m_binding));
}

void InputVibrationBindingWidget::clearBinding()
{
	m_binding = {};
	QtHost::RemoveBaseSettingValue(m_section_name.c_str(), m_key_name.c_str());
	g_emu_thread->reloadInputBindings();
	setText(QString());
}

void InputVibrationBindingWidget::onClicked()
{
	QInputDialog dialog(QtUtils::GetRootWidget(this));

	const QString full_key(QStringLiteral("%1/%2").arg(QString::fromStdString(m_section_name)).arg(QString::fromStdString(m_key_name)));
	const QString current(QString::fromStdString(m_binding));
	QStringList input_options(m_dialog->getVibrationMotors());
	if (!current.isEmpty() && input_options.indexOf(current) < 0)
		input_options.append(current);

	QInputDialog input_dialog(this);
	input_dialog.setWindowTitle(full_key);
	input_dialog.setLabelText(tr("Select vibration motor for %1.").arg(full_key));
	input_dialog.setInputMode(QInputDialog::TextInput);
	input_dialog.setOptions(QInputDialog::UseListViewForComboBoxItems);
	input_dialog.setComboBoxEditable(false);
	input_dialog.setComboBoxItems(std::move(input_options));
	input_dialog.setTextValue(current);
	if (input_dialog.exec() == 0)
		return;

	const QString new_value(input_dialog.textValue());
	m_binding = new_value.toStdString();
	QtHost::SetBaseStringSettingValue(m_section_name.c_str(), m_key_name.c_str(), m_binding.c_str());
	setText(new_value);
}

void InputVibrationBindingWidget::mouseReleaseEvent(QMouseEvent* e)
{
	if (e->button() == Qt::RightButton)
	{
		clearBinding();
		return;
	}

	QPushButton::mouseReleaseEvent(e);
}
