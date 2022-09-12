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

#include "Frontend/InputManager.h"
#include "Settings/ControllerGlobalSettingsWidget.h"
#include "Settings/ControllerSettingsDialog.h"
#include "Settings/ControllerSettingWidgetBinder.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"

ControllerGlobalSettingsWidget::ControllerGlobalSettingsWidget(QWidget* parent, ControllerSettingsDialog* dialog)
	: QWidget(parent)
	, m_dialog(dialog)
{
	m_ui.setupUi(this);

	SettingsInterface* sif = dialog->getProfileSettingsInterface();

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.enableSDLSource, "InputSources", "SDL", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.enableSDLEnhancedMode, "InputSources", "SDLControllerEnhancedMode", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.enableXInputSource, "InputSources", "XInput", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.enableMouseMapping, "UI", "EnableMouseMapping", false);
	ControllerSettingWidgetBinder::BindWidgetToInputProfileBool(sif, m_ui.multitapPort1, "Pad", "MultitapPort1", false);
	ControllerSettingWidgetBinder::BindWidgetToInputProfileBool(sif, m_ui.multitapPort2, "Pad", "MultitapPort2", false);
	ControllerSettingWidgetBinder::BindWidgetToInputProfileFloat(sif, m_ui.pointerXScale, "Pad", "PointerXScale", 8.0f);
	ControllerSettingWidgetBinder::BindWidgetToInputProfileFloat(sif, m_ui.pointerYScale, "Pad", "PointerYScale", 8.0f);

	if (dialog->isEditingProfile())
	{
		m_ui.useProfileHotkeyBindings->setChecked(m_dialog->getBoolValue("Pad", "UseProfileHotkeyBindings", false));
		connect(m_ui.useProfileHotkeyBindings, &QCheckBox::stateChanged, this, [this](int new_state) {
			m_dialog->setBoolValue("Pad", "UseProfileHotkeyBindings", (new_state == Qt::Checked));
			emit bindingSetupChanged();
		});
	}
	else
	{
		// remove profile options from the UI.
		m_ui.mainLayout->removeWidget(m_ui.profileSettings);
		m_ui.profileSettings->deleteLater();
		m_ui.profileSettings = nullptr;
	}

	connect(m_ui.enableSDLSource, &QCheckBox::stateChanged, this, &ControllerGlobalSettingsWidget::updateSDLOptionsEnabled);
	for (QCheckBox* cb : {m_ui.multitapPort1, m_ui.multitapPort2})
		connect(cb, &QCheckBox::stateChanged, this, [this]() { emit bindingSetupChanged(); });

	connect(m_ui.pointerXScale, &QSlider::valueChanged, this, [this](int value) { m_ui.pointerXScaleLabel->setText(QStringLiteral("%1").arg(value)); });
	connect(m_ui.pointerYScale, &QSlider::valueChanged, this, [this](int value) { m_ui.pointerYScaleLabel->setText(QStringLiteral("%1").arg(value)); });
	m_ui.pointerXScaleLabel->setText(QStringLiteral("%1").arg(m_ui.pointerXScale->value()));
	m_ui.pointerYScaleLabel->setText(QStringLiteral("%1").arg(m_ui.pointerYScale->value()));

	updateSDLOptionsEnabled();
}

ControllerGlobalSettingsWidget::~ControllerGlobalSettingsWidget() = default;

void ControllerGlobalSettingsWidget::addDeviceToList(const QString& identifier, const QString& name)
{
	QListWidgetItem* item = new QListWidgetItem();
	item->setText(QStringLiteral("%1: %2").arg(identifier).arg(name));
	item->setData(Qt::UserRole, identifier);
	m_ui.deviceList->addItem(item);
}

void ControllerGlobalSettingsWidget::removeDeviceFromList(const QString& identifier)
{
	const int count = m_ui.deviceList->count();
	for (int i = 0; i < count; i++)
	{
		QListWidgetItem* item = m_ui.deviceList->item(i);
		if (item->data(Qt::UserRole) != identifier)
			continue;

		delete m_ui.deviceList->takeItem(i);
		break;
	}
}

void ControllerGlobalSettingsWidget::updateSDLOptionsEnabled()
{
	const bool enabled = m_ui.enableSDLSource->isChecked();
	m_ui.enableSDLEnhancedMode->setEnabled(enabled);
}
