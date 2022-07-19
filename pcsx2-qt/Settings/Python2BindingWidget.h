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

#pragma once

#include <QtWidgets/QWidget>

#include "ui_Python2BindingWidget.h"

#include "pcsx2/Frontend/InputManager.h"

class InputBindingWidget;
class ControllerSettingsDialog;

struct Python2KeyMapping;

class Python2BindingWidget final : public QWidget
{
	Q_OBJECT

public:
	Python2BindingWidget(QWidget* parent, ControllerSettingsDialog* dialog);
	~Python2BindingWidget();

	QIcon getIcon() const;

	__fi ControllerSettingsDialog* getDialog() const { return m_dialog; }

private Q_SLOTS:
	void unbindKeyClicked(QTableWidget *tableWidget);

	void onBindKeyMotorClicked();

	void onInputListenTimerTimeout();
	void inputManagerHookCallback(InputBindingKey key, float value);

protected:
	enum : u32
	{
		TIMEOUT_FOR_SINGLE_BINDING = 5
	};

	void refreshUi();
	void saveAndRefresh();

	void refreshInputBindingList();
	void refreshInputAnalogBindingList();
	void refreshOutputMotorBindingList();

	virtual bool eventFilter(QObject* watched, QEvent* event) override;

	virtual void startListeningForInput(u32 timeout_in_seconds, bool isAnalog);
	virtual void stopListeningForInput();

	bool isListeningForInput() const { return m_input_listen_timer != nullptr; }
	void setNewInputBinding();

	void hookInputManager();
	void unhookInputManager();

	void saveMapping();

	std::string getKeybindDisplayName(std::string keybind);

	Ui::Python2BindingWidget m_ui;

	ControllerSettingsDialog* m_dialog;

	std::vector<std::string> m_bindings;
	std::vector<InputBindingKey> m_new_bindings;
	QTimer* m_input_listen_timer = nullptr;
	u32 m_input_listen_remaining_seconds = 0;
	bool m_input_listen_analog = false;
};
