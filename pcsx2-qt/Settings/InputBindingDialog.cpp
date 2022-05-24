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
#include "Settings/InputBindingDialog.h"
#include <QtCore/QTimer>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>

// _BitScanForward()
#include "pcsx2/GS/GSIntrin.h"

InputBindingDialog::InputBindingDialog(std::string section_name, std::string key_name,
	std::vector<std::string> bindings, QWidget* parent)
	: QDialog(parent)
	, m_section_name(std::move(section_name))
	, m_key_name(std::move(key_name))
	, m_bindings(std::move(bindings))
{
	m_ui.setupUi(this);
	m_ui.title->setText(
		tr("Bindings for %1 %2").arg(QString::fromStdString(m_section_name)).arg(QString::fromStdString(m_key_name)));
	m_ui.buttonBox->button(QDialogButtonBox::Close)->setText(tr("Close"));

	connect(m_ui.addBinding, &QPushButton::clicked, this, &InputBindingDialog::onAddBindingButtonClicked);
	connect(m_ui.removeBinding, &QPushButton::clicked, this, &InputBindingDialog::onRemoveBindingButtonClicked);
	connect(m_ui.clearBindings, &QPushButton::clicked, this, &InputBindingDialog::onClearBindingsButtonClicked);
	connect(m_ui.buttonBox, &QDialogButtonBox::rejected, [this]() { done(0); });
	updateList();
}

InputBindingDialog::~InputBindingDialog()
{
	Q_ASSERT(!isListeningForInput());
}

bool InputBindingDialog::eventFilter(QObject* watched, QEvent* event)
{
	const QEvent::Type event_type = event->type();

	// if the key is being released, set the input
	if (event_type == QEvent::KeyRelease || event_type == QEvent::MouseButtonPress)
	{
		addNewBinding();
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

void InputBindingDialog::onInputListenTimerTimeout()
{
	m_input_listen_remaining_seconds--;
	if (m_input_listen_remaining_seconds == 0)
	{
		stopListeningForInput();
		return;
	}

	m_ui.status->setText(tr("Push Button/Axis... [%1]").arg(m_input_listen_remaining_seconds));
}

void InputBindingDialog::startListeningForInput(u32 timeout_in_seconds)
{
	m_new_bindings.clear();
	m_input_listen_timer = new QTimer(this);
	m_input_listen_timer->setSingleShot(false);
	m_input_listen_timer->start(1000);

	m_input_listen_timer->connect(m_input_listen_timer, &QTimer::timeout, this,
		&InputBindingDialog::onInputListenTimerTimeout);
	m_input_listen_remaining_seconds = timeout_in_seconds;
	m_ui.status->setText(tr("Push Button/Axis... [%1]").arg(m_input_listen_remaining_seconds));
	m_ui.addBinding->setEnabled(false);
	m_ui.removeBinding->setEnabled(false);
	m_ui.clearBindings->setEnabled(false);
	m_ui.buttonBox->setEnabled(false);

	installEventFilter(this);
	grabKeyboard();
	grabMouse();
	hookInputManager();
}

void InputBindingDialog::stopListeningForInput()
{
	m_ui.status->clear();
	m_ui.addBinding->setEnabled(true);
	m_ui.removeBinding->setEnabled(true);
	m_ui.clearBindings->setEnabled(true);
	m_ui.buttonBox->setEnabled(true);

	delete m_input_listen_timer;
	m_input_listen_timer = nullptr;

	unhookInputManager();
	releaseMouse();
	releaseKeyboard();
	removeEventFilter(this);
}

void InputBindingDialog::addNewBinding()
{
	if (m_new_bindings.empty())
		return;

	const std::string new_binding(
		InputManager::ConvertInputBindingKeysToString(m_new_bindings.data(), m_new_bindings.size()));
	if (!new_binding.empty())
	{
		if (std::find(m_bindings.begin(), m_bindings.end(), new_binding) != m_bindings.end())
			return;

		m_ui.bindingList->addItem(QString::fromStdString(new_binding));
		m_bindings.push_back(std::move(new_binding));
		saveListToSettings();
	}
}

void InputBindingDialog::onAddBindingButtonClicked()
{
	if (isListeningForInput())
		stopListeningForInput();

	startListeningForInput(TIMEOUT_FOR_BINDING);
}

void InputBindingDialog::onRemoveBindingButtonClicked()
{
	const int row = m_ui.bindingList->currentRow();
	if (row < 0 || static_cast<size_t>(row) >= m_bindings.size())
		return;

	m_bindings.erase(m_bindings.begin() + row);
	delete m_ui.bindingList->takeItem(row);
	saveListToSettings();
}

void InputBindingDialog::onClearBindingsButtonClicked()
{
	m_bindings.clear();
	m_ui.bindingList->clear();
	saveListToSettings();
}

void InputBindingDialog::updateList()
{
	m_ui.bindingList->clear();
	for (const std::string& binding : m_bindings)
		m_ui.bindingList->addItem(QString::fromStdString(binding));
}

void InputBindingDialog::saveListToSettings()
{
	if (!m_bindings.empty())
		QtHost::SetBaseStringListSettingValue(m_section_name.c_str(), m_key_name.c_str(), m_bindings);
	else
		QtHost::RemoveBaseSettingValue(m_section_name.c_str(), m_key_name.c_str());

	g_emu_thread->reloadInputBindings();
}

void InputBindingDialog::inputManagerHookCallback(InputBindingKey key, float value)
{
	const float abs_value = std::abs(value);

	for (InputBindingKey other_key : m_new_bindings)
	{
		if (other_key.MaskDirection() == key.MaskDirection())
		{
			if (abs_value < 0.5f)
			{
				// if this key is in our new binding list, it's a "release", and we're done
				addNewBinding();
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

void InputBindingDialog::hookInputManager()
{
	InputManager::SetHook([this](InputBindingKey key, float value) {
		QMetaObject::invokeMethod(this, "inputManagerHookCallback", Qt::QueuedConnection, Q_ARG(InputBindingKey, key),
			Q_ARG(float, value));
		return InputInterceptHook::CallbackResult::StopProcessingEvent;
	});
}

void InputBindingDialog::unhookInputManager()
{
	InputManager::RemoveHook();
}
