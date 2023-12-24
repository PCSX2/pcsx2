// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtWidgets/QWidget>
#include <QtCore/QMap>
#include <array>
#include <vector>

#include "ColorPickerButton.h"

#include "ui_ControllerGlobalSettingsWidget.h"
#include "ui_ControllerLEDSettingsDialog.h"
#include "ui_ControllerMouseSettingsDialog.h"

class ControllerSettingsWindow;

class ControllerGlobalSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	ControllerGlobalSettingsWidget(QWidget* parent, ControllerSettingsWindow* dialog);
	~ControllerGlobalSettingsWidget();

	void addDeviceToList(const QString& identifier, const QString& name);
	void removeDeviceFromList(const QString& identifier);

Q_SIGNALS:
	void bindingSetupChanged();

private Q_SLOTS:
	void updateSDLOptionsEnabled();
	void ledSettingsClicked();
	void mouseSettingsClicked();

private:
	Ui::ControllerGlobalSettingsWidget m_ui;
	ControllerSettingsWindow* m_dialog;
};

class ControllerLEDSettingsDialog : public QDialog
{
	Q_OBJECT

public:
	ControllerLEDSettingsDialog(QWidget* parent, ControllerSettingsWindow* dialog);
	~ControllerLEDSettingsDialog();

private:
	void linkButton(ColorPickerButton* button, u32 player_id);

	Ui::ControllerLEDSettingsDialog m_ui;
	ControllerSettingsWindow* m_dialog;
};

class ControllerMouseSettingsDialog : public QDialog
{
	Q_OBJECT

public:
	ControllerMouseSettingsDialog(QWidget* parent, ControllerSettingsWindow* dialog);
	~ControllerMouseSettingsDialog();

private:
	Ui::ControllerMouseSettingsDialog m_ui;
};
