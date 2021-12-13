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

#include "QtHost.h"
#include "SettingsDialog.h"

#include "AdvancedSystemSettingsWidget.h"
#include "BIOSSettingsWidget.h"
#include "EmulationSettingsWidget.h"
#include "GameFixSettingsWidget.h"
#include "GameListSettingsWidget.h"
#include "GraphicsSettingsWidget.h"
#include "HotkeySettingsWidget.h"
#include "InterfaceSettingsWidget.h"
#include "SystemSettingsWidget.h"

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QTextEdit>

static constexpr char DEFAULT_SETTING_HELP_TEXT[] = "";

SettingsDialog::SettingsDialog(QWidget* parent /* = nullptr */)
	: QDialog(parent)
{
	m_ui.setupUi(this);
	setCategoryHelpTexts();

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	m_interface_settings = new InterfaceSettingsWidget(m_ui.settingsContainer, this);
	m_game_list_settings = new GameListSettingsWidget(m_ui.settingsContainer, this);
	m_bios_settings = new BIOSSettingsWidget(m_ui.settingsContainer, this);
	m_emulation_settings = new EmulationSettingsWidget(m_ui.settingsContainer, this);
	m_system_settings = new SystemSettingsWidget(m_ui.settingsContainer, this);
	m_advanced_system_settings = new AdvancedSystemSettingsWidget(m_ui.settingsContainer, this);
	m_game_fix_settings_widget = new GameFixSettingsWidget(m_ui.settingsContainer, this);
	m_graphics_settings = new GraphicsSettingsWidget(m_ui.settingsContainer, this);

	m_ui.settingsContainer->insertWidget(static_cast<int>(Category::InterfaceSettings), m_interface_settings);
	m_ui.settingsContainer->insertWidget(static_cast<int>(Category::GameListSettings), m_game_list_settings);
	m_ui.settingsContainer->insertWidget(static_cast<int>(Category::BIOSSettings), m_bios_settings);
	m_ui.settingsContainer->insertWidget(static_cast<int>(Category::EmulationSettings), m_emulation_settings);
	m_ui.settingsContainer->insertWidget(static_cast<int>(Category::SystemSettings), m_system_settings);
	m_ui.settingsContainer->insertWidget(static_cast<int>(Category::AdvancedSystemSettings), m_advanced_system_settings);
	m_ui.settingsContainer->insertWidget(static_cast<int>(Category::GameFixSettings), m_game_fix_settings_widget);
	m_ui.settingsContainer->insertWidget(static_cast<int>(Category::GraphicsSettings), m_graphics_settings);

	m_ui.settingsCategory->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	m_ui.settingsCategory->setCurrentRow(0);
	m_ui.settingsContainer->setCurrentIndex(0);
	m_ui.helpText->setText(m_category_help_text[0]);
	connect(m_ui.settingsCategory, &QListWidget::currentRowChanged, this, &SettingsDialog::onCategoryCurrentRowChanged);
	connect(m_ui.closeButton, &QPushButton::clicked, this, &SettingsDialog::accept);
	connect(m_ui.restoreDefaultsButton, &QPushButton::clicked, this, &SettingsDialog::onRestoreDefaultsClicked);
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::setCategoryHelpTexts()
{
	m_category_help_text[static_cast<int>(Category::InterfaceSettings)] =
		tr("<strong>Interface Settings</strong><hr>These options control how the software looks and behaves.<br><br>Mouse "
		   "over "
		   "an option for additional information.");
	m_category_help_text[static_cast<int>(Category::SystemSettings)] =
		tr("<strong>System Settings</strong><hr>These options determine the configuration of the simulated "
		   "console.<br><br>Mouse over an option for additional information.");
	m_category_help_text[static_cast<int>(Category::GameListSettings)] =
		tr("<strong>Game List Settings</strong><hr>The list above shows the directories which will be searched by "
		   "PCSX2 to populate the game list. Search directories can be added, removed, and switched to "
		   "recursive/non-recursive.");
	m_category_help_text[static_cast<int>(Category::AudioSettings)] =
		tr("<strong>Audio Settings</strong><hr>These options control the audio output of the console. Mouse over an option "
		   "for additional information.");
}

void SettingsDialog::setCategory(Category category)
{
	if (category >= Category::Count)
		return;

	m_ui.settingsCategory->setCurrentRow(static_cast<int>(category));
}

void SettingsDialog::onCategoryCurrentRowChanged(int row)
{
	Q_ASSERT(row < static_cast<int>(Category::Count));
	m_ui.settingsContainer->setCurrentIndex(row);
	m_ui.helpText->setText(m_category_help_text[row]);
}

void SettingsDialog::onRestoreDefaultsClicked()
{
	if (QMessageBox::question(this, tr("Confirm Restore Defaults"),
			tr("Are you sure you want to restore the default settings? Any preferences will be lost."),
			QMessageBox::Yes, QMessageBox::No) != QMessageBox::Yes)
	{
		return;
	}

	// TODO
}

void SettingsDialog::registerWidgetHelp(QObject* object, QString title, QString recommended_value, QString text)
{
	// construct rich text with formatted description
	QString full_text;
	full_text += "<table width='100%' cellpadding='0' cellspacing='0'><tr><td><strong>";
	full_text += title;
	full_text += "</strong></td><td align='right'><strong>";
	full_text += tr("Recommended Value");
	full_text += ": </strong>";
	full_text += recommended_value;
	full_text += "</td></table><hr>";
	full_text += text;

	m_widget_help_text_map[object] = std::move(full_text);
	object->installEventFilter(this);
}

bool SettingsDialog::eventFilter(QObject* object, QEvent* event)
{
	if (event->type() == QEvent::Enter)
	{
		auto iter = m_widget_help_text_map.constFind(object);
		if (iter != m_widget_help_text_map.end())
		{
			m_current_help_widget = object;
			m_ui.helpText->setText(iter.value());
		}
	}
	else if (event->type() == QEvent::Leave)
	{
		if (m_current_help_widget)
		{
			m_current_help_widget = nullptr;
			m_ui.helpText->setText(m_category_help_text[m_ui.settingsCategory->currentRow()]);
		}
	}

	return QDialog::eventFilter(object, event);
}