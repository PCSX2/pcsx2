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

#include "PrecompiledHeader.h"

#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <algorithm>

#include "Settings/ControllerBindingWidgets.h"
#include "Settings/ControllerSettingsDialog.h"
#include "Settings/ControllerSettingWidgetBinder.h"
#include "Settings/SettingsDialog.h"
#include "EmuThread.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"

#include "common/StringUtil.h"
#include "pcsx2/HostSettings.h"
#include "pcsx2/PAD/Host/PAD.h"

ControllerBindingWidget::ControllerBindingWidget(QWidget* parent, ControllerSettingsDialog* dialog, u32 port)
	: QWidget(parent)
	, m_dialog(dialog)
	, m_config_section(StringUtil::StdStringFromFormat("Pad%u", port + 1u))
	, m_port_number(port)
{
	m_ui.setupUi(this);
	populateControllerTypes();
	onTypeChanged();

	ControllerSettingWidgetBinder::BindWidgetToInputProfileString(m_dialog->getProfileSettingsInterface(),
		m_ui.controllerType, m_config_section, "Type", PAD::GetDefaultPadType(port));

	connect(m_ui.controllerType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ControllerBindingWidget::onTypeChanged);
	connect(m_ui.automaticBinding, &QPushButton::clicked, this, &ControllerBindingWidget::doAutomaticBinding);
	connect(m_ui.clearBindings, &QPushButton::clicked, this, &ControllerBindingWidget::doClearBindings);
}

ControllerBindingWidget::~ControllerBindingWidget() = default;

QIcon ControllerBindingWidget::getIcon() const
{
	return m_current_widget->getIcon();
}

void ControllerBindingWidget::populateControllerTypes()
{
	for (const auto& [name, display_name] : PAD::GetControllerTypeNames())
		m_ui.controllerType->addItem(QString::fromStdString(display_name), QString::fromStdString(name));
}

void ControllerBindingWidget::onTypeChanged()
{
	const bool is_initializing = (m_current_widget == nullptr);
	m_controller_type = m_dialog->getStringValue(m_config_section.c_str(), "Type", PAD::GetDefaultPadType(m_port_number));

	if (!is_initializing)
	{
		m_ui.verticalLayout->removeWidget(m_current_widget);
		delete m_current_widget;
		m_current_widget = nullptr;
	}

	const int index = m_ui.controllerType->findData(QString::fromStdString(m_controller_type));
	if (index >= 0 && index != m_ui.controllerType->currentIndex())
	{
		QSignalBlocker sb(m_ui.controllerType);
		m_ui.controllerType->setCurrentIndex(index);
	}

	if (m_controller_type == "DualShock2")
		m_current_widget = ControllerBindingWidget_DualShock2::createInstance(this);
	else
		m_current_widget = new ControllerBindingWidget_Base(this);

	m_ui.verticalLayout->addWidget(m_current_widget, 1);

	// no need to do this on first init, only changes
	if (!is_initializing)
		m_dialog->updateListDescription(m_port_number, this);
}

void ControllerBindingWidget::doAutomaticBinding()
{
	QMenu menu(this);
	bool added = false;

	for (const QPair<QString, QString>& dev : m_dialog->getDeviceList())
	{
		// we set it as data, because the device list could get invalidated while the menu is up
		QAction* action = menu.addAction(QStringLiteral("%1 (%2)").arg(dev.first).arg(dev.second));
		action->setData(dev.first);
		connect(action, &QAction::triggered, this, [this, action]() {
			doDeviceAutomaticBinding(action->data().toString());
		});
		added = true;
	}

	if (!added)
	{
		QAction* action = menu.addAction(tr("No devices available"));
		action->setEnabled(false);
	}

	menu.exec(QCursor::pos());
}

void ControllerBindingWidget::doClearBindings()
{
	if (QMessageBox::question(QtUtils::GetRootWidget(this), tr("Clear Bindings"),
			tr("Are you sure you want to clear all bindings for this controller? This action cannot be undone.")) != QMessageBox::Yes)
	{
		return;
	}

	if (m_dialog->isEditingGlobalSettings())
	{
		auto lock = Host::GetSettingsLock();
		PAD::ClearPortBindings(*Host::Internal::GetBaseSettingsLayer(), m_port_number);
	}
	else
	{
		PAD::ClearPortBindings(*m_dialog->getProfileSettingsInterface(), m_port_number);
	}

	saveAndRefresh();
}

void ControllerBindingWidget::doDeviceAutomaticBinding(const QString& device)
{
	std::vector<std::pair<GenericInputBinding, std::string>> mapping = InputManager::GetGenericBindingMapping(device.toStdString());
	if (mapping.empty())
	{
		QMessageBox::critical(QtUtils::GetRootWidget(this), tr("Automatic Binding"),
			tr("No generic bindings were generated for device '%1'").arg(device));
		return;
	}

	bool result;
	if (m_dialog->isEditingGlobalSettings())
	{
		auto lock = Host::GetSettingsLock();
		result = PAD::MapController(*Host::Internal::GetBaseSettingsLayer(), m_port_number, mapping);
	}
	else
	{
		result = PAD::MapController(*m_dialog->getProfileSettingsInterface(), m_port_number, mapping);
		m_dialog->getProfileSettingsInterface()->Save();
		g_emu_thread->reloadInputBindings();
	}

	// force a refresh after mapping
	if (result)
		saveAndRefresh();
}

void ControllerBindingWidget::saveAndRefresh()
{
	onTypeChanged();
	QtHost::QueueSettingsSave();
	g_emu_thread->applySettings();
}

//////////////////////////////////////////////////////////////////////////

ControllerBindingWidget_Base::ControllerBindingWidget_Base(ControllerBindingWidget* parent)
	: QWidget(parent)
{
}

ControllerBindingWidget_Base::~ControllerBindingWidget_Base()
{
}

QIcon ControllerBindingWidget_Base::getIcon() const
{
	return QIcon::fromTheme("artboard-2-line");
}

void ControllerBindingWidget_Base::initBindingWidgets()
{
	const std::string& type = getControllerType();
	const std::string& config_section = getConfigSection();
	std::vector<std::string> bindings(PAD::GetControllerBinds(type));
	SettingsInterface* sif = getDialog()->getProfileSettingsInterface();

	for (std::string& binding : bindings)
	{
		InputBindingWidget* widget = findChild<InputBindingWidget*>(QString::fromStdString(binding));
		if (!widget)
		{
			Console.Error("(ControllerBindingWidget_Base) No widget found for '%s' (%.*s)",
				binding.c_str(), static_cast<int>(type.size()), type.data());
			continue;
		}

		widget->initialize(sif, config_section, std::move(binding));
	}

	const PAD::VibrationCapabilities vibe_caps = PAD::GetControllerVibrationCapabilities(type);
	switch (vibe_caps)
	{
		case PAD::VibrationCapabilities::LargeSmallMotors:
		{
			InputVibrationBindingWidget* widget = findChild<InputVibrationBindingWidget*>(QStringLiteral("LargeMotor"));
			if (widget)
				widget->setKey(getDialog(), config_section, "LargeMotor");

			widget = findChild<InputVibrationBindingWidget*>(QStringLiteral("SmallMotor"));
			if (widget)
				widget->setKey(getDialog(), config_section, "SmallMotor");
		}
		break;

		case PAD::VibrationCapabilities::SingleMotor:
		{
			InputVibrationBindingWidget* widget = findChild<InputVibrationBindingWidget*>(QStringLiteral("Motor"));
			if (widget)
				widget->setKey(getDialog(), config_section, "Motor");
		}
		break;

		case PAD::VibrationCapabilities::NoVibration:
		default:
			break;
	}

	if (QSlider* widget = findChild<QSlider*>(QStringLiteral("Deadzone")); widget)
	{
		const float range = static_cast<float>(widget->maximum());
		QLabel* label = findChild<QLabel*>(QStringLiteral("DeadzoneLabel"));
		if (label)
		{
			connect(widget, &QSlider::valueChanged, this, [range, label](int value) {
				label->setText(tr("%1%").arg((static_cast<float>(value) / range) * 100.0f, 0, 'f', 0));
			});
		}

		ControllerSettingWidgetBinder::BindWidgetToInputProfileNormalized(sif, widget, config_section, "Deadzone", range,
			PAD::DEFAULT_STICK_DEADZONE);
	}

	if (QSlider* widget = findChild<QSlider*>(QStringLiteral("AxisScale")); widget)
	{
		// position 1.0f at the halfway point
		const float range = static_cast<float>(widget->maximum()) * 0.5f;
		QLabel* label = findChild<QLabel*>(QStringLiteral("AxisScaleLabel"));
		if (label)
		{
			connect(widget, &QSlider::valueChanged, this, [range, label](int value) {
				label->setText(tr("%1%").arg((static_cast<float>(value) / range) * 100.0f, 0, 'f', 0));
			});
		}

		ControllerSettingWidgetBinder::BindWidgetToInputProfileNormalized(sif, widget, config_section, "AxisScale", range,
			PAD::DEFAULT_STICK_SCALE);
	}

	if (QDoubleSpinBox* widget = findChild<QDoubleSpinBox*>(QStringLiteral("SmallMotorScale")); widget)
		ControllerSettingWidgetBinder::BindWidgetToInputProfileFloat(sif, widget, config_section, "SmallMotorScale", 1.0f);
	if (QDoubleSpinBox* widget = findChild<QDoubleSpinBox*>(QStringLiteral("LargeMotorScale")); widget)
		ControllerSettingWidgetBinder::BindWidgetToInputProfileFloat(sif, widget, config_section, "LargeMotorScale", 1.0f);
}

ControllerBindingWidget_DualShock2::ControllerBindingWidget_DualShock2(ControllerBindingWidget* parent)
	: ControllerBindingWidget_Base(parent)
{
	m_ui.setupUi(this);
	initBindingWidgets();
}

ControllerBindingWidget_DualShock2::~ControllerBindingWidget_DualShock2()
{
}

QIcon ControllerBindingWidget_DualShock2::getIcon() const
{
	return QIcon::fromTheme("gamepad-line");
}

ControllerBindingWidget_Base* ControllerBindingWidget_DualShock2::createInstance(ControllerBindingWidget* parent)
{
	return new ControllerBindingWidget_DualShock2(parent);
}

//////////////////////////////////////////////////////////////////////////
