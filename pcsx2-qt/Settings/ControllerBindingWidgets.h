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

#include "ui_ControllerBindingWidget.h"
#include "ui_ControllerBindingWidget_DualShock2.h"

class InputBindingWidget;
class ControllerSettingsDialog;
class ControllerBindingWidget_Base;

class ControllerBindingWidget final : public QWidget
{
	Q_OBJECT

public:
	ControllerBindingWidget(QWidget* parent, ControllerSettingsDialog* dialog, u32 port);
	~ControllerBindingWidget();

	__fi ControllerSettingsDialog* getDialog() const { return m_dialog; }
	__fi const std::string& getConfigSection() const { return m_config_section; }
	__fi const std::string& getControllerType() const { return m_controller_type; }
	__fi u32 getPortNumber() const { return m_port_number; }

private Q_SLOTS:
	void onTypeChanged();
	void doAutomaticBinding();
	void doClearBindings();

private:
	void populateControllerTypes();
	void doDeviceAutomaticBinding(const QString& device);
	void saveAndRefresh();

	Ui::ControllerBindingWidget m_ui;

	ControllerSettingsDialog* m_dialog;

	std::string m_config_section;
	std::string m_controller_type;
	u32 m_port_number;

	ControllerBindingWidget_Base* m_current_widget = nullptr;
};

class ControllerBindingWidget_Base : public QWidget
{
	Q_OBJECT

public:
	ControllerBindingWidget_Base(ControllerBindingWidget* parent);
	virtual ~ControllerBindingWidget_Base();

	__fi ControllerSettingsDialog* getDialog() const { return static_cast<ControllerBindingWidget*>(parent())->getDialog(); }
	__fi const std::string& getConfigSection() const { return static_cast<ControllerBindingWidget*>(parent())->getConfigSection(); }
	__fi const std::string& getControllerType() const { return static_cast<ControllerBindingWidget*>(parent())->getControllerType(); }
	__fi u32 getPortNumber() const { return static_cast<ControllerBindingWidget*>(parent())->getPortNumber(); }

protected:
	void initBindingWidgets();
};

class ControllerBindingWidget_DualShock2 final : public ControllerBindingWidget_Base
{
	Q_OBJECT

public:
	ControllerBindingWidget_DualShock2(ControllerBindingWidget* parent);
	~ControllerBindingWidget_DualShock2();

	static ControllerBindingWidget_Base* createInstance(ControllerBindingWidget* parent);

private:
	Ui::ControllerBindingWidget_DualShock2 m_ui;
};
