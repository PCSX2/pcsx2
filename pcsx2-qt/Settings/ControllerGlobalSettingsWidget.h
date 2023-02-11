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

#include <QtWidgets/QWidget>
#include <QtCore/QMap>
#include <array>
#include <vector>

#include "ColorPickerButton.h"

#include "ui_ControllerGlobalSettingsWidget.h"
#include "ui_ControllerLEDSettingsDialog.h"
#include "ui_ControllerMouseSettingsDialog.h"

class ControllerSettingsDialog;

class ControllerGlobalSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	ControllerGlobalSettingsWidget(QWidget* parent, ControllerSettingsDialog* dialog);
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
	ControllerSettingsDialog* m_dialog;
};

class ControllerLEDSettingsDialog : public QDialog
{
	Q_OBJECT

public:
	ControllerLEDSettingsDialog(QWidget* parent, ControllerSettingsDialog* dialog);
	~ControllerLEDSettingsDialog();

private:
	void linkButton(ColorPickerButton* button, u32 player_id);

	Ui::ControllerLEDSettingsDialog m_ui;
	ControllerSettingsDialog* m_dialog;
};

class ControllerMouseSettingsDialog : public QDialog
{
	Q_OBJECT

public:
	ControllerMouseSettingsDialog(QWidget* parent, ControllerSettingsDialog* dialog);
	~ControllerMouseSettingsDialog();

private:
	Ui::ControllerMouseSettingsDialog m_ui;
};
