// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Settings/HotkeySettingsWidget.h"
#include "Settings/ControllerSettingsWindow.h"
#include "InputBindingWidget.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"

#include "pcsx2/Input/InputManager.h"

#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QVBoxLayout>

HotkeySettingsWidget::HotkeySettingsWidget(QWidget* parent, ControllerSettingsWindow* dialog)
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
