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
#include "QtUtils.h"
#include "SettingWidgetBinder.h"

ControllerGlobalSettingsWidget::ControllerGlobalSettingsWidget(QWidget* parent, ControllerSettingsDialog* dialog)
	: QWidget(parent)
{
	m_ui.setupUi(this);

	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.enableSDLSource, "InputSources", "SDL", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.enableSDLEnhancedMode, "InputSources", "SDLControllerEnhancedMode", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(nullptr, m_ui.enableXInputSource, "InputSources", "XInput", false);

	connect(m_ui.enableSDLSource, &QCheckBox::stateChanged, this, &ControllerGlobalSettingsWidget::updateSDLOptionsEnabled);
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
