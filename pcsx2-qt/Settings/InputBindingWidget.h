// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "pcsx2/Config.h"
#include "pcsx2/Input/InputManager.h"

#include <QtWidgets/QPushButton>
#include <optional>
#include <utility>
#include <vector>

class QTimer;

class ControllerSettingsWindow;
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
	InputVibrationBindingWidget(QWidget* parent, ControllerSettingsWindow* dialog, std::string section_name, std::string key_name);
	~InputVibrationBindingWidget();

	void setKey(ControllerSettingsWindow* dialog, std::string section_name, std::string key_name);

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

	ControllerSettingsWindow* m_dialog;
};
