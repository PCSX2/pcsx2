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
#include "ui_InputBindingDialog.h"
#include "pcsx2/Config.h"
#include "pcsx2/Frontend/InputManager.h"
#include <QtWidgets/QDialog>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class SettingsInterface;

class InputBindingDialog : public QDialog
{
	Q_OBJECT

public:
	InputBindingDialog(SettingsInterface* sif, InputBindingInfo::Type bind_type, std::string section_name, std::string key_name,
		std::vector<std::string> bindings, QWidget* parent);
	~InputBindingDialog();

protected Q_SLOTS:
	void onAddBindingButtonClicked();
	void onRemoveBindingButtonClicked();
	void onClearBindingsButtonClicked();
	void onInputListenTimerTimeout();
	void inputManagerHookCallback(InputBindingKey key, float value);

protected:
	enum : u32
	{
		TIMEOUT_FOR_BINDING = 5
	};

	virtual bool eventFilter(QObject* watched, QEvent* event) override;

	virtual void startListeningForInput(u32 timeout_in_seconds);
	virtual void stopListeningForInput();

	bool isListeningForInput() const { return m_input_listen_timer != nullptr; }
	void addNewBinding();

	void updateList();
	void saveListToSettings();

	void hookInputManager();
	void unhookInputManager();

	Ui::InputBindingDialog m_ui;

	SettingsInterface* m_sif;
	InputBindingInfo::Type m_bind_type;
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
