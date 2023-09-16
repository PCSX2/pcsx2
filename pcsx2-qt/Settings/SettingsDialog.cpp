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

#include "MainWindow.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "Settings/AchievementSettingsWidget.h"
#include "Settings/AdvancedSettingsWidget.h"
#include "Settings/AudioSettingsWidget.h"
#include "Settings/BIOSSettingsWidget.h"
#include "Settings/DEV9SettingsWidget.h"
#include "Settings/DebugSettingsWidget.h"
#include "Settings/EmulationSettingsWidget.h"
#include "Settings/FolderSettingsWidget.h"
#include "Settings/GameCheatSettingsWidget.h"
#include "Settings/GameFixSettingsWidget.h"
#include "Settings/GameListSettingsWidget.h"
#include "Settings/GamePatchSettingsWidget.h"
#include "Settings/GameSummaryWidget.h"
#include "Settings/GraphicsSettingsWidget.h"
#include "Settings/HotkeySettingsWidget.h"
#include "Settings/InterfaceSettingsWidget.h"
#include "Settings/MemoryCardSettingsWidget.h"
#include "SettingsDialog.h"

#include "pcsx2/Achievements.h"
#include "pcsx2/GameList.h"
#include "pcsx2/Host.h"
#include "pcsx2/INISettingsInterface.h"

#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QTextEdit>

static QList<SettingsDialog*> s_open_game_properties_dialogs;

SettingsDialog::SettingsDialog(QWidget* parent)
	: QDialog(parent)
	, m_disc_crc(0)
{
	setupUi(nullptr);
}

SettingsDialog::SettingsDialog(QWidget* parent, std::unique_ptr<INISettingsInterface> sif, const GameList::Entry* game,
	std::string serial, u32 disc_crc, QString filename)
	: QDialog(parent)
	, m_sif(std::move(sif))
	, m_filename(std::move(filename))
	, m_game_list_filename(game ? game->path : std::string())
	, m_serial(std::move(serial))
	, m_disc_crc(disc_crc)
{
	setupUi(game);

	s_open_game_properties_dialogs.push_back(this);
}

SettingsInterface* SettingsDialog::getSettingsInterface() const
{
	return m_sif.get();
}

void SettingsDialog::setupUi(const GameList::Entry* game)
{
	const bool show_advanced_settings = QtHost::ShouldShowAdvancedSettings();

	m_ui.setupUi(this);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	if (isPerGameSettings())
	{
		QString summary = tr("<strong>Summary</strong><hr>This page shows details about the selected game. Changing the Input "
							 "Profile will set the controller binding scheme for this game to whichever profile is chosen, instead "
							 "of the default (Shared) configuration. The track list and dump verification can be used to determine "
							 "if your disc image matches a known good dump. If it does not match, the game may be broken.");
		if (game)
		{
			addWidget(new GameSummaryWidget(game, this, m_ui.settingsContainer), tr("Summary"),
				QStringLiteral("file-list-line"), std::move(summary));
		}
		else
		{
			QLabel* placeholder_label =
				new QLabel(tr("Summary is unavailable for files not present in game list."), m_ui.settingsContainer);
			placeholder_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
			addWidget(placeholder_label, tr("Summary"), QStringLiteral("file-list-line"), std::move(summary));
		}

		m_ui.restoreDefaultsButton->setVisible(false);
		m_ui.footerLayout->removeWidget(m_ui.restoreDefaultsButton);
		m_ui.restoreDefaultsButton->deleteLater();
		m_ui.restoreDefaultsButton = nullptr;
	}
	else
	{
		m_ui.copyGlobalSettingsButton->setVisible(false);
		m_ui.footerLayout->removeWidget(m_ui.copyGlobalSettingsButton);
		m_ui.copyGlobalSettingsButton->deleteLater();
		m_ui.copyGlobalSettingsButton = nullptr;

		m_ui.clearGameSettingsButton->setVisible(false);
		m_ui.footerLayout->removeWidget(m_ui.clearGameSettingsButton);
		m_ui.clearGameSettingsButton->deleteLater();
		m_ui.clearGameSettingsButton = nullptr;
	}

	addWidget(m_interface_settings = new InterfaceSettingsWidget(this, m_ui.settingsContainer), tr("Interface"),
		QStringLiteral("interface-line"),
		tr("<strong>Interface Settings</strong><hr>These options control how the software looks and behaves.<br><br>Mouse over an option "
		   "for additional information."));

	// We don't include game list/bios settings in per-game settings.
	if (!isPerGameSettings())
	{
		addWidget(m_game_list_settings = new GameListSettingsWidget(this, m_ui.settingsContainer), tr("Game List"),
			QStringLiteral("folder-open-line"),
			tr("<strong>Game List Settings</strong><hr>The list above shows the directories which will be searched by PCSX2 to populate "
			   "the game list. Search directories can be added, removed, and switched to recursive/non-recursive."));
		addWidget(m_bios_settings = new BIOSSettingsWidget(this, m_ui.settingsContainer), tr("BIOS"), QStringLiteral("chip-line"),
			tr("<strong>BIOS Settings</strong><hr>Configure your BIOS here.<br><br>Mouse over an option for additional information."));
	}

	// Common to both per-game and global settings.
	addWidget(m_emulation_settings = new EmulationSettingsWidget(this, m_ui.settingsContainer), tr("Emulation"),
		QStringLiteral("emulation-line"),
		tr("<strong>Emulation Settings</strong><hr>These options determine the configuration of frame pacing and game "
		   "settings.<br><br>Mouse over an option for additional information."));

	if (isPerGameSettings())
	{
		addWidget(m_game_patch_settings_widget = new GamePatchSettingsWidget(this, m_ui.settingsContainer),
			tr("Patches"), QStringLiteral("band-aid-line"),
			tr("<strong>Patches</strong><hr>This section allows you to select optional patches to apply to the game, "
			   "which may provide performance, visual, or gameplay improvements."));
		addWidget(m_game_cheat_settings_widget = new GameCheatSettingsWidget(this, m_ui.settingsContainer),
			tr("Cheats"), QStringLiteral("cheats-line"),
			tr("<strong>Cheats</strong><hr>This section allows you to select which cheats you wish to enable. You "
			   "cannot enable/disable cheats without labels for old-format pnach files, those will automatically "
			   "activate if the main cheat enable option is checked."));
	}

	// Only show the game fixes for per-game settings, there's really no reason to be setting them globally.
	if (show_advanced_settings && isPerGameSettings())
	{
		addWidget(m_game_fix_settings_widget = new GameFixSettingsWidget(this, m_ui.settingsContainer), tr("Game Fixes"),
			QStringLiteral("tools-line"),
			tr("<strong>Game Fixes Settings</strong><hr>Game Fixes can work around incorrect emulation in some titles.<br>However, they can "
			   "also cause problems in games if used incorrectly.<br>It is best to leave them all disabled unless advised otherwise."));
	}

	addWidget(m_graphics_settings = new GraphicsSettingsWidget(this, m_ui.settingsContainer), tr("Graphics"), QStringLiteral("image-fill"),
		tr("<strong>Graphics Settings</strong><hr>These options determine the configuration of the graphical output.<br><br>Mouse over an "
		   "option for additional information."));
	addWidget(m_audio_settings = new AudioSettingsWidget(this, m_ui.settingsContainer), tr("Audio"), QStringLiteral("volume-up-line"),
		tr("<strong>Audio Settings</strong><hr>These options control the audio output of the console.<br><br>Mouse over an option for "
		   "additional information."));

	addWidget(m_memory_card_settings = new MemoryCardSettingsWidget(this, m_ui.settingsContainer), tr("Memory Cards"),
		QStringLiteral("memcard-line"),
		tr("<strong>Memory Card Settings</strong><hr>Create and configure Memory Cards here.<br><br>Mouse over an option for "
		   "additional information."));

	addWidget(m_dev9_settings = new DEV9SettingsWidget(this, m_ui.settingsContainer), tr("Network & HDD"), QStringLiteral("global-line"),
		tr("<strong>Network & HDD Settings</strong><hr>These options control the network connectivity and internal HDD storage of the "
		   "console.<br><br>Mouse over an option for additional information."));

	if (!isPerGameSettings())
	{
		addWidget(m_folder_settings = new FolderSettingsWidget(this, m_ui.settingsContainer), tr("Folders"),
			QStringLiteral("folder-settings-line"),
			tr("<strong>Folder Settings</strong><hr>These options control where PCSX2 will save runtime data files."));
	}

	{
		QString title = tr("Achievements");
		QString icon_text(QStringLiteral("trophy-line"));
		QString help_text =
			tr("<strong>Achievements Settings</strong><hr>"
			   "These options control the RetroAchievements implementation in PCSX2, allowing you to earn achievements in your games.");
		if (Achievements::IsUsingRAIntegration())
		{
			QLabel* placeholder_label =
				new QLabel(tr("RAIntegration is being used, built-in RetroAchievements support is disabled."), m_ui.settingsContainer);
			placeholder_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
			addWidget(placeholder_label, std::move(title), std::move(icon_text), std::move(help_text));
		}
		else
		{
			addWidget((m_achievement_settings = new AchievementSettingsWidget(this, m_ui.settingsContainer)), std::move(title),
				std::move(icon_text), std::move(help_text));
		}
	}

	if (show_advanced_settings)
	{
		addWidget(m_advanced_settings = new AdvancedSettingsWidget(this, m_ui.settingsContainer), tr("Advanced"),
			QStringLiteral("warning-line"),
			tr("<strong>Advanced Settings</strong><hr>These are advanced options to determine the configuration of the simulated "
			   "console.<br><br>Mouse over an option for additional information."));
		addWidget(m_debug_settings = new DebugSettingsWidget(this, m_ui.settingsContainer), tr("Debug"),
			QStringLiteral("heart-monitor-line"),
			tr("<strong>Debug Settings</strong><hr>These are options which can be used to log internal information about the application. "
			   "<strong>Do not modify unless you know what you are doing</strong>, it will cause significant slowdown, and can waste large "
			   "amounts of disk space."));
	}

	m_ui.settingsCategory->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	m_ui.settingsCategory->setCurrentRow(0);
	m_ui.settingsContainer->setCurrentIndex(0);
	m_ui.helpText->setText(m_category_help_text[0]);
	connect(m_ui.settingsCategory, &QListWidget::currentRowChanged, this, &SettingsDialog::onCategoryCurrentRowChanged);
	connect(m_ui.closeButton, &QPushButton::clicked, this, &SettingsDialog::close);
	if (m_ui.restoreDefaultsButton)
		connect(m_ui.restoreDefaultsButton, &QPushButton::clicked, this, &SettingsDialog::onRestoreDefaultsClicked);
	if (m_ui.copyGlobalSettingsButton)
		connect(m_ui.copyGlobalSettingsButton, &QPushButton::clicked, this, &SettingsDialog::onCopyGlobalSettingsClicked);
	if (m_ui.clearGameSettingsButton)
		connect(m_ui.clearGameSettingsButton, &QPushButton::clicked, this, &SettingsDialog::onClearSettingsClicked);
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

QString SettingsDialog::getCategory() const
{
	return m_ui.settingsCategory->item(m_ui.settingsCategory->currentRow())->text();
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
	QMessageBox msgbox(this);
	msgbox.setIcon(QMessageBox::Question);
	msgbox.setWindowTitle(tr("Confirm Restore Defaults"));
	msgbox.setText(tr("Are you sure you want to restore the default settings? Any preferences will be lost."));

	QCheckBox* ui_cb = new QCheckBox(tr("Reset UI Settings"), &msgbox);
	msgbox.setCheckBox(ui_cb);
	msgbox.addButton(QMessageBox::Yes);
	msgbox.addButton(QMessageBox::No);
	msgbox.setDefaultButton(QMessageBox::Yes);
	if (msgbox.exec() != QMessageBox::Yes)
		return;

	g_main_window->resetSettings(ui_cb->isChecked());
}

void SettingsDialog::onCopyGlobalSettingsClicked()
{
	if (!isPerGameSettings())
		return;

	if (QMessageBox::question(this, tr("PCSX2 Settings"),
			tr("The configuration for this game will be replaced by the current global settings.\n\nAny current setting values will be "
			   "overwritten.\n\nDo you want to continue?"),
			QMessageBox::Yes, QMessageBox::No) != QMessageBox::Yes)
	{
		return;
	}

	{
		auto lock = Host::GetSettingsLock();
		Pcsx2Config::CopyConfiguration(m_sif.get(), *Host::Internal::GetBaseSettingsLayer());
	}
	m_sif->Save();
	g_emu_thread->reloadGameSettings();

	QMessageBox::information(reopen(), tr("PCSX2 Settings"), tr("Per-game configuration copied from global settings."));
}

void SettingsDialog::onClearSettingsClicked()
{
	if (!isPerGameSettings())
		return;

	if (QMessageBox::question(this, tr("PCSX2 Settings"),
			tr("The configuration for this game will be cleared.\n\nAny current setting values will be lost.\n\nDo you want to continue?"),
			QMessageBox::Yes, QMessageBox::No) != QMessageBox::Yes)
	{
		return;
	}

	Pcsx2Config::ClearConfiguration(m_sif.get());
	m_sif->Save();
	g_emu_thread->reloadGameSettings();

	QMessageBox::information(reopen(), tr("PCSX2 Settings"), tr("Per-game configuration cleared."));
}

SettingsDialog* SettingsDialog::reopen()
{
	// This doesn't work for global settings, because MainWindow maintains a pointer.
	if (!m_sif)
		return this;

	close();

	std::unique_ptr<INISettingsInterface> new_sif = std::make_unique<INISettingsInterface>(m_sif->GetFileName());
	if (FileSystem::FileExists(new_sif->GetFileName().c_str()))
		new_sif->Load();

	auto lock = GameList::GetLock();
	const GameList::Entry* game = m_game_list_filename.empty() ? nullptr : GameList::GetEntryForPath(m_game_list_filename.c_str());

	SettingsDialog* dlg = new SettingsDialog(g_main_window, std::move(new_sif), game, m_serial, m_disc_crc, m_filename);
	dlg->QDialog::setWindowTitle(windowTitle());
	dlg->setModal(false);
	dlg->show();

	return dlg;
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
	if (!object)
		return;

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

void SettingsDialog::setWindowTitle(const QString& title)
{
	if (m_filename.isEmpty())
	{
		QDialog::setWindowTitle(title);
	}
	else
	{
		QDialog::setWindowTitle(QStringLiteral("%1 [%2]").arg(title, m_filename));
	}
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

std::optional<std::string> SettingsDialog::getStringValue(
	const char* section, const char* key, std::optional<const char*> default_value) const
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
		g_emu_thread->reloadGameSettings();
	}
	else
	{
		value.has_value() ? Host::SetBaseBoolSettingValue(section, key, value.value()) : Host::RemoveBaseSettingValue(section, key);
		Host::CommitBaseSettingChanges();
		g_emu_thread->applySettings();
	}
}

void SettingsDialog::setIntSettingValue(const char* section, const char* key, std::optional<int> value)
{
	if (m_sif)
	{
		value.has_value() ? m_sif->SetIntValue(section, key, value.value()) : m_sif->DeleteValue(section, key);
		m_sif->Save();
		g_emu_thread->reloadGameSettings();
	}
	else
	{
		value.has_value() ? Host::SetBaseIntSettingValue(section, key, value.value()) : Host::RemoveBaseSettingValue(section, key);
		Host::CommitBaseSettingChanges();
		g_emu_thread->applySettings();
	}
}

void SettingsDialog::setFloatSettingValue(const char* section, const char* key, std::optional<float> value)
{
	if (m_sif)
	{
		value.has_value() ? m_sif->SetFloatValue(section, key, value.value()) : m_sif->DeleteValue(section, key);
		m_sif->Save();
		g_emu_thread->reloadGameSettings();
	}
	else
	{
		value.has_value() ? Host::SetBaseFloatSettingValue(section, key, value.value()) : Host::RemoveBaseSettingValue(section, key);
		Host::CommitBaseSettingChanges();
		g_emu_thread->applySettings();
	}
}

void SettingsDialog::setStringSettingValue(const char* section, const char* key, std::optional<const char*> value)
{
	if (m_sif)
	{
		value.has_value() ? m_sif->SetStringValue(section, key, value.value()) : m_sif->DeleteValue(section, key);
		m_sif->Save();
		g_emu_thread->reloadGameSettings();
	}
	else
	{
		value.has_value() ? Host::SetBaseStringSettingValue(section, key, value.value()) : Host::RemoveBaseSettingValue(section, key);
		Host::CommitBaseSettingChanges();
		g_emu_thread->applySettings();
	}
}

bool SettingsDialog::containsSettingValue(const char* section, const char* key) const
{
	if (m_sif)
		return m_sif->ContainsValue(section, key);
	else
		return Host::ContainsBaseSettingValue(section, key);
}

void SettingsDialog::removeSettingValue(const char* section, const char* key)
{
	if (m_sif)
	{
		m_sif->DeleteValue(section, key);
		m_sif->Save();
		g_emu_thread->reloadGameSettings();
	}
	else
	{
		Host::RemoveBaseSettingValue(section, key);
		Host::CommitBaseSettingChanges();
		g_emu_thread->applySettings();
	}
}

void SettingsDialog::openGamePropertiesDialog(const GameList::Entry* game, const std::string_view& title, std::string serial, u32 disc_crc)
{
	// check for an existing dialog with this crc
	for (SettingsDialog* dialog : s_open_game_properties_dialogs)
	{
		if (dialog->m_disc_crc == disc_crc)
		{
			dialog->show();
			dialog->setFocus();
			return;
		}
	}

	std::string filename(VMManager::GetGameSettingsPath(serial, disc_crc));
	std::unique_ptr<INISettingsInterface> sif = std::make_unique<INISettingsInterface>(filename);
	if (FileSystem::FileExists(sif->GetFileName().c_str()))
		sif->Load();

	SettingsDialog* dialog = new SettingsDialog(g_main_window, std::move(sif), game, std::move(serial), disc_crc, QtUtils::StringViewToQString(Path::GetFileName(filename)));
	dialog->setWindowTitle(QtUtils::StringViewToQString(title));
	dialog->setModal(false);
	dialog->show();
}

