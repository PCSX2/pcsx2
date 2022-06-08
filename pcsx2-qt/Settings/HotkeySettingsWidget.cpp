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
#include "Settings/HotkeySettingsWidget.h"
#include "Settings/ControllerSettingsDialog.h"
#include "InputBindingWidget.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QTabWidget>

HotkeySettingsWidget::HotkeySettingsWidget(QWidget* parent, ControllerSettingsDialog* dialog)
	: QWidget(parent)
	, m_dialog(dialog)
{
	createUi();
}

HotkeySettingsWidget::~HotkeySettingsWidget() = default;

void HotkeySettingsWidget::createUi()
{
	QGridLayout* layout = new QGridLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	m_tab_widget = new QTabWidget(this);

	createButtons();

	layout->addWidget(m_tab_widget, 0, 0, 1, 1);

	setLayout(layout);
}

void HotkeySettingsWidget::createButtons()
{
	const std::vector<const HotkeyInfo*> hotkeys(InputManager::GetHotkeyList());
	for (const HotkeyInfo* hotkey : hotkeys)
	{
		const auto category = qApp->translate("Hotkeys", hotkey->category);

		auto iter = m_categories.find(category);
		if (iter == m_categories.end())
		{
			QScrollArea* scroll = new QScrollArea(m_tab_widget);
			QWidget* container = new QWidget(scroll);
			QVBoxLayout* vlayout = new QVBoxLayout(container);
			QGridLayout* layout = new QGridLayout();
			layout->setContentsMargins(0, 0, 0, 0);
			vlayout->addLayout(layout);
			vlayout->addStretch(1);
			iter = m_categories.insert(category, Category{container, layout});
			scroll->setWidget(container);
			scroll->setWidgetResizable(true);
			scroll->setBackgroundRole(QPalette::Base);
			scroll->setFrameShape(QFrame::NoFrame);
			m_tab_widget->addTab(scroll, category);
		}

		QWidget* container = iter->container;
		QGridLayout* layout = iter->layout;
		const int target_row = layout->count() / 2;

		layout->addWidget(new QLabel(qApp->translate("Hotkeys", hotkey->display_name), container), target_row, 0);
		layout->addWidget(new InputBindingWidget(container, m_dialog->getProfileSettingsInterface(), "Hotkeys", hotkey->name), target_row, 1);
	}
}
