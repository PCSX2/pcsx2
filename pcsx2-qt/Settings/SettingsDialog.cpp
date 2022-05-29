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

#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#include "pcsx2/HostSettings.h"
#include "pcsx2/Frontend/GameList.h"
#include "pcsx2/Frontend/INISettingsInterface.h"

#include "EmuThread.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "SettingsDialog.h"

#include "AdvancedSystemSettingsWidget.h"
#include "AudioSettingsWidget.h"
#include "BIOSSettingsWidget.h"
#include "EmulationSettingsWidget.h"
#include "GameSummaryWidget.h"
#include "GameFixSettingsWidget.h"
#include "GameListSettingsWidget.h"
#include "GraphicsSettingsWidget.h"
#include "DEV9SettingsWidget.h"
#include "HotkeySettingsWidget.h"
#include "InterfaceSettingsWidget.h"
#include "MemoryCardSettingsWidget.h"
#include "SystemSettingsWidget.h"

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QTextEdit>

static QList<SettingsDialog*> s_open_game_properties_dialogs;

SettingsDialog::SettingsDialog(QWidget* parent)
	: QDialog(parent)
	, m_game_crc(0)
{
	setupUi(nullptr);
}

SettingsDialog::SettingsDialog(std::unique_ptr<SettingsInterface> sif, const GameList::Entry* game, u32 game_crc)
	: QDialog()
	, m_sif(std::move(sif))
	, m_game_crc(game_crc)
{
	setupUi(game);

	s_open_game_properties_dialogs.push_back(this);
}

void SettingsDialog::setupUi(const GameList::Entry* game)
{
	const bool show_advanced_settings = true;

	m_ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// We don't include interface/game list/bios settings from per-game settings.
	if (!isPerGameSettings())
	{
		addWidget(m_interface_settings = new InterfaceSettingsWidget(this, m_ui.settingsContainer), tr("Interface"),
			QStringLiteral("settings-3-line"),
			tr("<strong>Interface Settings</strong><hr>These options control how the software looks and behaves.<br><br>Mouse over an option for "
			   "additional information."));
		addWidget(m_game_list_settings = new GameListSettingsWidget(this, m_ui.settingsContainer), tr("Game List"),
			QStringLiteral("folder-settings-line"),
			tr("<strong>Game List Settings</strong><hr>The list above shows the directories which will be searched by PCSX2 to populate the game "
			   "list. Search directories can be added, removed, and switched to recursive/non-recursive."));
		addWidget(m_bios_settings = new BIOSSettingsWidget(this, m_ui.settingsContainer), tr("BIOS"), QStringLiteral("hard-drive-2-line"),
			tr("<strong>BIOS Settings</strong><hr>"));
	}
	else
	{
		if (game)
		{
			addWidget(new GameSummaryWidget(game, this, m_ui.settingsContainer), tr("Summary"), QStringLiteral("file-list-line"),
				tr("<strong>Summary</strong><hr>Eventually this will be where we can see patches and compute hashes/verify dumps/etc."));
		}

		m_ui.restoreDefaultsButton->setVisible(false);
	}

	// Common to both per-game and global settings.
	addWidget(m_emulation_settings = new EmulationSettingsWidget(this, m_ui.settingsContainer), tr("Emulation"), QStringLiteral("dashboard-line"),
		tr("<strong>Emulation Settings</strong><hr>"));
	addWidget(m_system_settings = new SystemSettingsWidget(this, m_ui.settingsContainer), tr("System"), QStringLiteral("artboard-2-line"),
		tr("<strong>System Settings</strong><hr>These options determine the configuration of the simulated console.<br><br>Mouse over an option for "
		   "additional information."));

	if (show_advanced_settings)
	{
		addWidget(m_advanced_system_settings = new AdvancedSystemSettingsWidget(this, m_ui.settingsContainer), tr("Advanced System"),
			QStringLiteral("artboard-2-line"), tr("<strong>Advanced System Settings</strong><hr>"));

		// Only show the game fixes for per-game settings, there's really no reason to be setting them globally.
		if (isPerGameSettings())
		{
			addWidget(m_game_fix_settings_widget = new GameFixSettingsWidget(this, m_ui.settingsContainer), tr("Game Fix"),
				QStringLiteral("close-line"), tr("<strong>Game Fix Settings</strong><hr>"));
		}
	}

	addWidget(m_graphics_settings = new GraphicsSettingsWidget(this, m_ui.settingsContainer), tr("Graphics"), QStringLiteral("brush-line"),
		tr("<strong>Graphics Settings</strong><hr>"));
	addWidget(m_audio_settings = new AudioSettingsWidget(this, m_ui.settingsContainer), tr("Audio"), QStringLiteral("volume-up-line"),
		tr("<strong>Audio Settings</strong><hr>These options control the audio output of the console. Mouse over an option for additional "
		   "information."));

	// for now, memory cards aren't settable per-game
	if (!isPerGameSettings())
	{
		addWidget(m_memory_card_settings = new MemoryCardSettingsWidget(this, m_ui.settingsContainer), tr("Memory Cards"),
			QStringLiteral("sd-card-line"), tr("<strong>Memory Card Settings</strong><hr>"));
	}
	
	addWidget(m_dev9_settings = new DEV9SettingsWidget(this, m_ui.settingsContainer), tr("Network & HDD"), QStringLiteral("dashboard-line"),
		tr("<strong>Network & HDD Settings</strong><hr>These options control the network connectivity and internal HDD storage of the console.<br><br>"
		   "Mouse over an option for additional information."));

	m_ui.settingsCategory->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	m_ui.settingsCategory->setCurrentRow(0);
	m_ui.settingsContainer->setCurrentIndex(0);
	m_ui.helpText->setText(m_category_help_text[0]);
	connect(m_ui.settingsCategory, &QListWidget::currentRowChanged, this, &SettingsDialog::onCategoryCurrentRowChanged);
	connect(m_ui.closeButton, &QPushButton::clicked, this, &SettingsDialog::accept);
	connect(m_ui.restoreDefaultsButton, &QPushButton::clicked, this, &SettingsDialog::onRestoreDefaultsClicked);

	// TODO: Remove this once they're implemented.
	m_ui.restoreDefaultsButton->setVisible(false);
}

SettingsDialog::~SettingsDialog()
{
	if (isPerGameSettings())
		s_open_game_properties_dialogs.removeOne(this);
}

void SettingsDialog::closeEvent(QCloseEvent*)
{
	// we need to clean up ourselves, since we're not modal
	if (isPerGameSettings())
		deleteLater();
}

void SettingsDialog::setCategory(const char* category)
{
	// the titles in the category list will be translated.
	const QString translated_category(qApp->translate("SettingsDialog", category));

	for (int i = 0; i < m_ui.settingsCategory->count(); i++)
	{
		if (translated_category == m_ui.settingsCategory->item(i)->text())
		{
			// will also update the visible widget
			m_ui.settingsCategory->setCurrentRow(i);
			break;
		}
	}
}

void SettingsDialog::onCategoryCurrentRowChanged(int row)
{
	m_ui.settingsContainer->setCurrentIndex(row);
	m_ui.helpText->setText(m_category_help_text[row]);
}

void SettingsDialog::onRestoreDefaultsClicked()
{
	if (QMessageBox::question(this, tr("Confirm Restore Defaults"),
			tr("Are you sure you want to restore the default settings? Any preferences will be lost."), QMessageBox::Yes,
			QMessageBox::No) != QMessageBox::Yes)
	{
		return;
	}

	// TODO
}

void SettingsDialog::addWidget(QWidget* widget, QString title, QString icon, QString help_text)
{
	const int index = m_ui.settingsCategory->count();

	QListWidgetItem* item = new QListWidgetItem(m_ui.settingsCategory);
	item->setText(title);
	if (!icon.isEmpty())
		item->setIcon(QIcon::fromTheme(icon));

	m_ui.settingsContainer->addWidget(widget);

	m_category_help_text[index] = std::move(help_text);
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

bool SettingsDialog::getEffectiveBoolValue(const char* section, const char* key, bool default_value) const
{
	bool value;
	if (m_sif && m_sif->GetBoolValue(section, key, &value))
		return value;
	else
		return Host::GetBaseBoolSettingValue(section, key, default_value);
}

int SettingsDialog::getEffectiveIntValue(const char* section, const char* key, int default_value) const
{
	int value;
	if (m_sif && m_sif->GetIntValue(section, key, &value))
		return value;
	else
		return Host::GetBaseIntSettingValue(section, key, default_value);
}

float SettingsDialog::getEffectiveFloatValue(const char* section, const char* key, float default_value) const
{
	float value;
	if (m_sif && m_sif->GetFloatValue(section, key, &value))
		return value;
	else
		return Host::GetBaseFloatSettingValue(section, key, default_value);
}

std::string SettingsDialog::getEffectiveStringValue(const char* section, const char* key, const char* default_value) const
{
	std::string value;
	if (!m_sif || !m_sif->GetStringValue(section, key, &value))
		value = Host::GetBaseStringSettingValue(section, key, default_value);
	return value;
}

std::optional<bool> SettingsDialog::getBoolValue(const char* section, const char* key, std::optional<bool> default_value) const
{
	std::optional<bool> value;
	if (m_sif)
	{
		bool bvalue;
		if (m_sif->GetBoolValue(section, key, &bvalue))
			value = bvalue;
		else
			value = default_value;
	}
	else
	{
		value = Host::GetBaseBoolSettingValue(section, key, default_value.value_or(false));
	}

	return value;
}

std::optional<int> SettingsDialog::getIntValue(const char* section, const char* key, std::optional<int> default_value) const
{
	std::optional<int> value;
	if (m_sif)
	{
		int ivalue;
		if (m_sif->GetIntValue(section, key, &ivalue))
			value = ivalue;
		else
			value = default_value;
	}
	else
	{
		value = Host::GetBaseIntSettingValue(section, key, default_value.value_or(0));
	}

	return value;
}

std::optional<float> SettingsDialog::getFloatValue(const char* section, const char* key, std::optional<float> default_value) const
{
	std::optional<float> value;
	if (m_sif)
	{
		float fvalue;
		if (m_sif->GetFloatValue(section, key, &fvalue))
			value = fvalue;
		else
			value = default_value;
	}
	else
	{
		value = Host::GetBaseFloatSettingValue(section, key, default_value.value_or(0.0f));
	}

	return value;
}

std::optional<std::string> SettingsDialog::getStringValue(const char* section, const char* key, std::optional<const char*> default_value) const
{
	std::optional<std::string> value;
	if (m_sif)
	{
		std::string svalue;
		if (m_sif->GetStringValue(section, key, &svalue))
			value = std::move(svalue);
		else if (default_value.has_value())
			value = default_value.value();
	}
	else
	{
		value = Host::GetBaseStringSettingValue(section, key, default_value.value_or(""));
	}

	return value;
}

void SettingsDialog::setBoolSettingValue(const char* section, const char* key, std::optional<bool> value)
{
	if (m_sif)
	{
		value.has_value() ? m_sif->SetBoolValue(section, key, value.value()) : m_sif->DeleteValue(section, key);
		m_sif->Save();
	}
	else
	{
		value.has_value() ? QtHost::SetBaseBoolSettingValue(section, key, value.value()) : QtHost::RemoveBaseSettingValue(section, key);
	}

	g_emu_thread->applySettings();
}

void SettingsDialog::setIntSettingValue(const char* section, const char* key, std::optional<int> value)
{
	if (m_sif)
	{
		value.has_value() ? m_sif->SetIntValue(section, key, value.value()) : m_sif->DeleteValue(section, key);
		m_sif->Save();
	}
	else
	{
		value.has_value() ? QtHost::SetBaseIntSettingValue(section, key, value.value()) : QtHost::RemoveBaseSettingValue(section, key);
	}

	g_emu_thread->applySettings();
}

void SettingsDialog::setFloatSettingValue(const char* section, const char* key, std::optional<float> value)
{
	if (m_sif)
	{
		value.has_value() ? m_sif->SetFloatValue(section, key, value.value()) : m_sif->DeleteValue(section, key);
		m_sif->Save();
	}
	else
	{
		value.has_value() ? QtHost::SetBaseFloatSettingValue(section, key, value.value()) : QtHost::RemoveBaseSettingValue(section, key);
	}

	g_emu_thread->applySettings();
}

void SettingsDialog::setStringSettingValue(const char* section, const char* key, std::optional<const char*> value)
{
	if (m_sif)
	{
		value.has_value() ? m_sif->SetStringValue(section, key, value.value()) : m_sif->DeleteValue(section, key);
		m_sif->Save();
	}
	else
	{
		value.has_value() ? QtHost::SetBaseStringSettingValue(section, key, value.value()) : QtHost::RemoveBaseSettingValue(section, key);
	}

	g_emu_thread->applySettings();
}

void SettingsDialog::openGamePropertiesDialog(const GameList::Entry* game, const std::string_view& serial, u32 crc)
{
	// check for an existing dialog with this crc
	for (SettingsDialog* dialog : s_open_game_properties_dialogs)
	{
		if (dialog->m_game_crc == crc)
		{
			dialog->show();
			dialog->setFocus();
			return;
		}
	}

	std::string filename(VMManager::GetGameSettingsPath(serial, crc));
	std::unique_ptr<INISettingsInterface> sif = std::make_unique<INISettingsInterface>(std::move(filename));
	if (FileSystem::FileExists(sif->GetFileName().c_str()))
		sif->Load();

	const QString window_title(tr("%1 [%2]")
								   .arg(game ? QtUtils::StringViewToQString(game->title) : QStringLiteral("<UNKNOWN>"))
								   .arg(QtUtils::StringViewToQString(Path::GetFileName(sif->GetFileName()))));

	SettingsDialog* dialog = new SettingsDialog(std::move(sif), game, crc);
	dialog->setWindowTitle(window_title);
	dialog->show();
}
