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

#include "pcsx2/SIO/Pad/PadTypes.h"

#include <QtWidgets/QWidget>

#include <span>

#include "ui_ControllerBindingWidget.h"
#include "ui_ControllerBindingWidget_DualShock2.h"
#include "ui_ControllerBindingWidget_Guitar.h"
#include "ui_ControllerMacroWidget.h"
#include "ui_ControllerMacroEditWidget.h"
#include "ui_USBDeviceWidget.h"

class InputBindingWidget;
class ControllerSettingsDialog;
class ControllerCustomSettingsWidget;
class ControllerMacroWidget;
class ControllerMacroEditWidget;
class ControllerBindingWidget_Base;

class USBBindingWidget;

class ControllerBindingWidget final : public QWidget
{
	Q_OBJECT

public:
	ControllerBindingWidget(QWidget* parent, ControllerSettingsDialog* dialog, u32 port);
	~ControllerBindingWidget();

	QIcon getIcon() const;

	__fi ControllerSettingsDialog* getDialog() const { return m_dialog; }
	__fi const std::string& getConfigSection() const { return m_config_section; }
	__fi const std::string& getControllerType() const { return m_controller_type; }
	__fi u32 getPortNumber() const { return m_port_number; }

private Q_SLOTS:
	void onTypeChanged();
	void onAutomaticBindingClicked();
	void onClearBindingsClicked();
	void onBindingsClicked();
	void onSettingsClicked();
	void onMacrosClicked();

private:
	void populateControllerTypes();
	void updateHeaderToolButtons();
	void doDeviceAutomaticBinding(const QString& device);

	Ui::ControllerBindingWidget m_ui;

	ControllerSettingsDialog* m_dialog;

	std::string m_config_section;
	std::string m_controller_type;
	u32 m_port_number;

	ControllerBindingWidget_Base* m_bindings_widget = nullptr;
	ControllerCustomSettingsWidget* m_settings_widget = nullptr;
	ControllerMacroWidget* m_macros_widget = nullptr;
};


//////////////////////////////////////////////////////////////////////////

class ControllerMacroWidget : public QWidget
{
	Q_OBJECT

public:
	ControllerMacroWidget(ControllerBindingWidget* parent);
	~ControllerMacroWidget();

	void updateListItem(u32 index);

private:
	static constexpr u32 NUM_MACROS = Pad::NUM_MACRO_BUTTONS_PER_CONTROLLER;

	void createWidgets(ControllerBindingWidget* parent);

	Ui::ControllerMacroWidget m_ui;
	ControllerSettingsDialog* m_dialog;
	std::array<ControllerMacroEditWidget*, NUM_MACROS> m_macros;
};

//////////////////////////////////////////////////////////////////////////

class ControllerMacroEditWidget : public QWidget
{
	Q_OBJECT

public:
	ControllerMacroEditWidget(ControllerMacroWidget* parent, ControllerBindingWidget* bwidget, u32 index);
	~ControllerMacroEditWidget();

	QString getSummary() const;

private Q_SLOTS:
	void onPressureChanged();
	void onDeadzoneChanged();
	void onSetFrequencyClicked();
	void updateBinds();

private:
	void modFrequency(s32 delta);
	void updateFrequency();
	void updateFrequencyText();

	Ui::ControllerMacroEditWidget m_ui;

	ControllerMacroWidget* m_parent;
	ControllerBindingWidget* m_bwidget;
	u32 m_index;

	std::vector<const InputBindingInfo*> m_binds;
	u32 m_frequency = 0;
};

//////////////////////////////////////////////////////////////////////////

class ControllerCustomSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	ControllerCustomSettingsWidget(std::span<const SettingInfo> settings, std::string config_section, std::string config_prefix,
		const char* translation_ctx, ControllerSettingsDialog* dialog, QWidget* parent_widget);
	~ControllerCustomSettingsWidget();

private Q_SLOTS:
	void restoreDefaults();

private:
	void createSettingWidgets(const char* translation_ctx, QWidget* widget_parent, QGridLayout* layout);

	std::span<const SettingInfo> m_settings;
	std::string m_config_section;
	std::string m_config_prefix;
	ControllerSettingsDialog* m_dialog;
};

//////////////////////////////////////////////////////////////////////////


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

	virtual QIcon getIcon() const;

protected:
	void initBindingWidgets();
};

class ControllerBindingWidget_DualShock2 final : public ControllerBindingWidget_Base
{
	Q_OBJECT

public:
	ControllerBindingWidget_DualShock2(ControllerBindingWidget* parent);
	~ControllerBindingWidget_DualShock2();

	QIcon getIcon() const override;

	static ControllerBindingWidget_Base* createInstance(ControllerBindingWidget* parent);

private:
	Ui::ControllerBindingWidget_DualShock2 m_ui;
};

class ControllerBindingWidget_Guitar final : public ControllerBindingWidget_Base
{
	Q_OBJECT

public:
	ControllerBindingWidget_Guitar(ControllerBindingWidget* parent);
	~ControllerBindingWidget_Guitar();

	QIcon getIcon() const override;

	static ControllerBindingWidget_Base* createInstance(ControllerBindingWidget* parent);

private:
	Ui::ControllerBindingWidget_Guitar m_ui;
};

//////////////////////////////////////////////////////////////////////////

class USBDeviceWidget final : public QWidget
{
	Q_OBJECT

public:
	USBDeviceWidget(QWidget* parent, ControllerSettingsDialog* dialog, u32 port);
	~USBDeviceWidget();

	QIcon getIcon() const;

	__fi ControllerSettingsDialog* getDialog() const { return m_dialog; }
	__fi const std::string& getConfigSection() const { return m_config_section; }
	__fi const std::string& getDeviceType() const { return m_device_type; }
	__fi u32 getPortNumber() const { return m_port_number; }

private Q_SLOTS:
	void onTypeChanged();
	void onSubTypeChanged(int new_index);
	void onAutomaticBindingClicked();
	void onClearBindingsClicked();
	void onBindingsClicked();
	void onSettingsClicked();

private:
	void populateDeviceTypes();
	void populatePages();
	void updateHeaderToolButtons();
	void doDeviceAutomaticBinding(const QString& device);

	Ui::USBDeviceWidget m_ui;

	ControllerSettingsDialog* m_dialog;

	std::string m_config_section;
	std::string m_device_type;
	u32 m_device_subtype;
	u32 m_port_number;

	USBBindingWidget* m_bindings_widget = nullptr;
	ControllerCustomSettingsWidget* m_settings_widget = nullptr;
};

class USBBindingWidget : public QWidget
{
	Q_OBJECT

public:
	USBBindingWidget(USBDeviceWidget* parent);
	~USBBindingWidget() override;

	__fi ControllerSettingsDialog* getDialog() const { return static_cast<USBDeviceWidget*>(parent())->getDialog(); }
	__fi const std::string& getConfigSection() const { return static_cast<USBDeviceWidget*>(parent())->getConfigSection(); }
	__fi const std::string& getDeviceType() const { return static_cast<USBDeviceWidget*>(parent())->getDeviceType(); }
	__fi u32 getPortNumber() const { return static_cast<USBDeviceWidget*>(parent())->getPortNumber(); }

	QIcon getIcon() const;

	static USBBindingWidget* createInstance(const std::string& type, u32 subtype, std::span<const InputBindingInfo> bindings, USBDeviceWidget* parent);

protected:
	std::string getBindingKey(const char* binding_name) const;

	void createWidgets(std::span<const InputBindingInfo> bindings);
	void bindWidgets(std::span<const InputBindingInfo> bindings);
};
