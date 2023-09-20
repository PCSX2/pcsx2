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
#include "QtUtils.h"
#include "Settings/ControllerSettingWidgetBinder.h"
#include "Settings/InputBindingDialog.h"
#include "Settings/InputBindingWidget.h"
#include <QtCore/QTimer>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>

#include "fmt/format.h"

#include <bit>

InputBindingDialog::InputBindingDialog(SettingsInterface* sif, InputBindingInfo::Type bind_type, std::string section_name,
	std::string key_name, std::vector<std::string> bindings, QWidget* parent)
	: QDialog(parent)
	, m_sif(sif)
	, m_bind_type(bind_type)
	, m_section_name(std::move(section_name))
	, m_key_name(std::move(key_name))
	, m_bindings(std::move(bindings))
{
	m_ui.setupUi(this);
	m_ui.title->setText(tr("Bindings for %1 %2").arg(QString::fromStdString(m_section_name)).arg(QString::fromStdString(m_key_name)));
	m_ui.buttonBox->button(QDialogButtonBox::Close)->setText(tr("Close"));

	connect(m_ui.addBinding, &QPushButton::clicked, this, &InputBindingDialog::onAddBindingButtonClicked);
	connect(m_ui.removeBinding, &QPushButton::clicked, this, &InputBindingDialog::onRemoveBindingButtonClicked);
	connect(m_ui.clearBindings, &QPushButton::clicked, this, &InputBindingDialog::onClearBindingsButtonClicked);
	connect(m_ui.buttonBox, &QDialogButtonBox::rejected, [this]() { done(0); });
	updateList();

	// Only show the sensitivity controls for binds where it's applicable.
	if (bind_type == InputBindingInfo::Type::Button || bind_type == InputBindingInfo::Type::Axis ||
		bind_type == InputBindingInfo::Type::HalfAxis)
	{
		ControllerSettingWidgetBinder::BindWidgetToInputProfileNormalized(
			sif, m_ui.sensitivity, m_section_name, fmt::format("{}Scale", m_key_name), 100.0f, 1.0f);
		ControllerSettingWidgetBinder::BindWidgetToInputProfileNormalized(
			sif, m_ui.deadzone, m_section_name, fmt::format("{}Deadzone", m_key_name), 100.0f, 0.0f);

		connect(m_ui.sensitivity, &QSlider::valueChanged, this, &InputBindingDialog::onSensitivityChanged);
		connect(m_ui.deadzone, &QSlider::valueChanged, this, &InputBindingDialog::onDeadzoneChanged);

		onSensitivityChanged(m_ui.sensitivity->value());
		onDeadzoneChanged(m_ui.deadzone->value());
	}
	else
	{
		m_ui.verticalLayout->removeWidget(m_ui.sensitivityWidget);
		delete m_ui.sensitivityWidget;
		m_ui.sensitivityWidget = nullptr;
	}
}

InputBindingDialog::~InputBindingDialog()
{
	Q_ASSERT(!isListeningForInput());
}

bool InputBindingDialog::eventFilter(QObject* watched, QEvent* event)
{
	const QEvent::Type event_type = event->type();

	// if the key is being released, set the input
	if (event_type == QEvent::KeyRelease || event_type == QEvent::MouseButtonRelease)
	{
		addNewBinding();
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
		if (const u32 button_mask = static_cast<u32>(static_cast<const QMouseEvent*>(event)->button()))
			m_new_bindings.push_back(InputManager::MakePointerButtonKey(0, std::countr_zero(button_mask)));
		return true;
	}
	else if (event_type == QEvent::Wheel)
	{
		const QPoint delta_angle(static_cast<QWheelEvent*>(event)->angleDelta());
		const float dx = std::clamp(static_cast<float>(delta_angle.x()) / QtUtils::MOUSE_WHEEL_DELTA, -1.0f, 1.0f);
		if (dx != 0.0f)
		{
			InputBindingKey key(InputManager::MakePointerAxisKey(0, InputPointerAxis::WheelX));
			key.modifier = dx < 0.0f ? InputModifier::Negate : InputModifier::None;
			m_new_bindings.push_back(key);
		}

		const float dy = std::clamp(static_cast<float>(delta_angle.y()) / QtUtils::MOUSE_WHEEL_DELTA, -1.0f, 1.0f);
		if (dy != 0.0f)
		{
			InputBindingKey key(InputManager::MakePointerAxisKey(0, InputPointerAxis::WheelY));
			key.modifier = dy < 0.0f ? InputModifier::Negate : InputModifier::None;
			m_new_bindings.push_back(key);
		}

		if (dx != 0.0f || dy != 0.0f)
		{
			addNewBinding();
			stopListeningForInput();
		}

		return true;
	}
	else if (event_type == QEvent::MouseMove && m_mouse_mapping_enabled)
	{
		// if we've moved more than a decent distance from the center of the widget, bind it.
		// this is so we don't accidentally bind to the mouse if you bump it while reaching for your pad.
		static constexpr const s32 THRESHOLD = 50;
		const QPoint diff(static_cast<QMouseEvent*>(event)->globalPosition().toPoint() - m_input_listen_start_position);
		bool has_one = false;

		if (std::abs(diff.x()) >= THRESHOLD)
		{
			InputBindingKey key(InputManager::MakePointerAxisKey(0, InputPointerAxis::X));
			key.modifier = diff.x() < 0 ? InputModifier::Negate : InputModifier::None;
			m_new_bindings.push_back(key);
			has_one = true;
		}
		if (std::abs(diff.y()) >= THRESHOLD)
		{
			InputBindingKey key(InputManager::MakePointerAxisKey(0, InputPointerAxis::Y));
			key.modifier = diff.y() < 0 ? InputModifier::Negate : InputModifier::None;
			m_new_bindings.push_back(key);
			has_one = true;
		}

		if (has_one)
		{
			addNewBinding();
			stopListeningForInput();
			return true;
		}
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
	m_value_ranges.clear();
	m_new_bindings.clear();
	m_mouse_mapping_enabled = InputBindingWidget::isMouseMappingEnabled(m_sif);
	m_input_listen_start_position = QCursor::pos();
	m_input_listen_timer = new QTimer(this);
	m_input_listen_timer->setSingleShot(false);
	m_input_listen_timer->start(1000);

	m_input_listen_timer->connect(m_input_listen_timer, &QTimer::timeout, this, &InputBindingDialog::onInputListenTimerTimeout);
	m_input_listen_remaining_seconds = timeout_in_seconds;
	m_ui.status->setText(tr("Push Button/Axis... [%1]").arg(m_input_listen_remaining_seconds));
	m_ui.addBinding->setEnabled(false);
	m_ui.removeBinding->setEnabled(false);
	m_ui.clearBindings->setEnabled(false);
	m_ui.buttonBox->setEnabled(false);

	installEventFilter(this);
	grabKeyboard();
	grabMouse();
	setMouseTracking(true);
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
	setMouseTracking(false);
	removeEventFilter(this);
}

void InputBindingDialog::addNewBinding()
{
	if (m_new_bindings.empty())
		return;

	const std::string new_binding(InputManager::ConvertInputBindingKeysToString(m_bind_type, m_new_bindings.data(), m_new_bindings.size()));
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
	if (m_sif)
	{
		if (!m_bindings.empty())
			m_sif->SetStringList(m_section_name.c_str(), m_key_name.c_str(), m_bindings);
		else
			m_sif->DeleteValue(m_section_name.c_str(), m_key_name.c_str());
		m_sif->Save();
		g_emu_thread->reloadGameSettings();
	}
	else
	{
		if (!m_bindings.empty())
			Host::SetBaseStringListSettingValue(m_section_name.c_str(), m_key_name.c_str(), m_bindings);
		else
			Host::RemoveBaseSettingValue(m_section_name.c_str(), m_key_name.c_str());
		Host::CommitBaseSettingChanges();
		g_emu_thread->reloadInputBindings();
	}
}

void InputBindingDialog::inputManagerHookCallback(InputBindingKey key, float value)
{
	if (!isListeningForInput())
		return;

	float initial_value = value;
	float min_value = value;
	auto it = std::find_if(m_value_ranges.begin(), m_value_ranges.end(), [key](const auto& it) { return it.first.bits == key.bits; });
	if (it != m_value_ranges.end())
	{
		initial_value = it->second.first;
		min_value = it->second.second = std::min(it->second.second, value);
	}
	else
	{
		m_value_ranges.emplace_back(key, std::make_pair(initial_value, min_value));
	}

	const float abs_value = std::abs(value);
	const bool reverse_threshold = (key.source_subtype == InputSubclass::ControllerAxis && initial_value > 0.5f);

	for (InputBindingKey& other_key : m_new_bindings)
	{
		if (other_key.MaskDirection() == key.MaskDirection())
		{
			// for pedals, we wait for it to go back to near its starting point to commit the binding
			if ((reverse_threshold ? ((initial_value - value) <= 0.25f) : (abs_value < 0.5f)))
			{
				// did we go the full range?
				if (reverse_threshold && initial_value > 0.5f && min_value <= -0.5f)
					other_key.modifier = InputModifier::FullAxis;

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
	if ((reverse_threshold ? (abs_value < 0.5f) : (abs_value >= 0.5f)))
	{
		InputBindingKey key_to_add = key;
		key_to_add.modifier = (value < 0.0f && !reverse_threshold) ? InputModifier::Negate : InputModifier::None;
		key_to_add.invert = reverse_threshold;
		m_new_bindings.push_back(key_to_add);
	}
}

void InputBindingDialog::onSensitivityChanged(int value)
{
	m_ui.sensitivityValue->setText(tr("%1%").arg(value));
}

void InputBindingDialog::onDeadzoneChanged(int value)
{
	m_ui.deadzoneValue->setText(tr("%1%").arg(value));
}

void InputBindingDialog::hookInputManager()
{
	InputManager::SetHook([this](InputBindingKey key, float value) {
		QMetaObject::invokeMethod(this, "inputManagerHookCallback", Qt::QueuedConnection, Q_ARG(InputBindingKey, key), Q_ARG(float, value));
		return InputInterceptHook::CallbackResult::StopProcessingEvent;
	});
}

void InputBindingDialog::unhookInputManager()
{
	InputManager::RemoveHook();
}
