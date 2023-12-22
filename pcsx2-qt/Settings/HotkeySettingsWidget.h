// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtWidgets/QWidget>
#include <QtCore/QMap>
#include <array>
#include <vector>

class QScrollArea;
class QGridLayout;
class QVBoxLayout;

class ControllerSettingsWindow;

class HotkeySettingsWidget : public QWidget
{
	Q_OBJECT

public:
	HotkeySettingsWidget(QWidget* parent, ControllerSettingsWindow* dialog);
	~HotkeySettingsWidget();

private:
	void createUi();
	void createButtons();

	ControllerSettingsWindow* m_dialog;
	QScrollArea* m_scroll_area = nullptr;
	QWidget* m_container = nullptr;
	QVBoxLayout* m_layout = nullptr;

	QMap<QString, QGridLayout*> m_categories;
};
