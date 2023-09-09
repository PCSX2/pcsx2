/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

#include "pcsx2/Config.h"
#include "pcsx2/Input/InputManager.h"

#include <QtWidgets/QPushButton>
#include <optional>
#include <utility>
#include <vector>

class QTimer;

class ControllerSettingsDialog;
class SettingsInterface;

class InputBindingWidget : public QPushButton
{
	Q_OBJECT

public:
	InputBindingWidget(QWidget* parent);
	InputBindingWidget(
		QWidget* parent, SettingsInterface* sif, InputBindingInfo::Type bind_type, std::string section_name, std::string key_name);
	~InputBindingWidget();

	static bool isMouseMappingEnabled(SettingsInterface* sif);

	void initialize(SettingsInterface* sif, InputBindingInfo::Type bind_type, std::string section_name, std::string key_name);

public Q_SLOTS:
	void clearBinding();
	void reloadBinding();

protected Q_SLOTS:
	void onClicked();
	void onInputListenTimerTimeout();
	void inputManagerHookCallback(InputBindingKey key, float value);

protected:
	enum : u32
	{
		TIMEOUT_FOR_SINGLE_BINDING = 5,
		TIMEOUT_FOR_ALL_BINDING = 10
	};

	virtual bool eventFilter(QObject* watched, QEvent* event) override;
	virtual bool event(QEvent* event) override;
	virtual void mouseReleaseEvent(QMouseEvent* e) override;

	virtual void startListeningForInput(u32 timeout_in_seconds);
	virtual void stopListeningForInput();
	virtual void openDialog();

	bool isListeningForInput() const { return m_input_listen_timer != nullptr; }
	void setNewBinding();
	void updateText();

	void hookInputManager();
	void unhookInputManager();

	SettingsInterface* m_sif = nullptr;
	InputBindingInfo::Type m_bind_type = InputBindingInfo::Type::Unknown;
	std::string m_section_name;
	std::string m_key_name;
	std::vector<std::string> m_bindings;
	std::vector<InputBindingKey> m_new_bindings;
	std::vector<std::pair<InputBindingKey, std::pair<float, float>>> m_value_ranges;
	QTimer* m_input_listen_timer = nullptr;
	u32 m_input_listen_remaining_seconds = 0;
	QPoint m_input_listen_start_position{};
	bool m_mouse_mapping_enabled = false;
};

class InputVibrationBindingWidget : public QPushButton
{
	Q_OBJECT

public:
	InputVibrationBindingWidget(QWidget* parent);
	InputVibrationBindingWidget(QWidget* parent, ControllerSettingsDialog* dialog, std::string section_name, std::string key_name);
	~InputVibrationBindingWidget();

	void setKey(ControllerSettingsDialog* dialog, std::string section_name, std::string key_name);

public Q_SLOTS:
	void clearBinding();

protected Q_SLOTS:
	void onClicked();

protected:
	virtual void mouseReleaseEvent(QMouseEvent* e) override;

private:
	std::string m_section_name;
	std::string m_key_name;
	std::string m_binding;

	ControllerSettingsDialog* m_dialog;
};
