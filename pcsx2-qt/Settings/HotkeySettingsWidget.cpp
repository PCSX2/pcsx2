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
#include <QtWidgets/QVBoxLayout>

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

	m_scroll_area = new QScrollArea(this);
	m_container = new QWidget(m_scroll_area);
	m_layout = new QVBoxLayout(m_container);
	m_scroll_area->setWidget(m_container);
	m_scroll_area->setWidgetResizable(true);
	m_scroll_area->setBackgroundRole(QPalette::Base);

	createButtons();

	layout->addWidget(m_scroll_area, 0, 0, 1, 1);

	setLayout(layout);
}

void HotkeySettingsWidget::createButtons()
{
	const std::vector<const HotkeyInfo*> hotkeys(InputManager::GetHotkeyList());
	for (const HotkeyInfo* hotkey : hotkeys)
	{
		const QString category(qApp->translate("Hotkeys", hotkey->category));

		auto iter = m_categories.find(category);
		if (iter == m_categories.end())
		{
			QLabel* label = new QLabel(category, m_container);
			QFont label_font(label->font());
			label_font.setPointSizeF(14.0f);
			label->setFont(label_font);
			m_layout->addWidget(label);

			QLabel* line = new QLabel(m_container);
			line->setFrameShape(QFrame::HLine);
			line->setFixedHeight(4);
			m_layout->addWidget(line);

			QGridLayout* layout = new QGridLayout();
			layout->setContentsMargins(0, 0, 0, 0);
			m_layout->addLayout(layout);
			iter = m_categories.insert(category, layout);
		}

		QGridLayout* layout = *iter;
		const int target_row = layout->count() / 2;

		QLabel* label = new QLabel(qApp->translate("Hotkeys", hotkey->display_name), m_container);
		layout->addWidget(label, target_row, 0);

		InputBindingWidget* bind = new InputBindingWidget(
			m_container, m_dialog->getProfileSettingsInterface(), InputBindingInfo::Type::Button, "Hotkeys", hotkey->name);
		bind->setMinimumWidth(300);
		layout->addWidget(bind, target_row, 1);
	}

	// Fill remaining space.
	m_layout->addStretch(1);
}
