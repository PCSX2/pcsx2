// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "ui_InputBindingDialog.h"

#include "pcsx2/Config.h"
#include "pcsx2/Input/InputManager.h"

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

	void onSensitivityChanged(int value);
	void onDeadzoneChanged(int value);

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
